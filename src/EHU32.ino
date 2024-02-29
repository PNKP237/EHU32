/*
  Setting up Arduino IDE for compilation:
  Board: ESP32 Dev Module
  Events run: on core 0
  Arduino runs: on core 1
  Partition scheme: Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)
*/

#include "BluetoothA2DPSink.h"
#include "driver/twai.h"
#include <WiFi.h>
#include <WiFiAP.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

bool DEBUGGING_ON=0;
// pin definitions
const int PCM_MUTE_CTL=23;            // this pin controls PCM5102s soft-mute function
// CAN buffers
uint32_t alerts_triggered;
static twai_message_t RxMessage, TxMessage;
twai_status_info_t status_info;
// CAN related flags
bool DIS_forceUpdate=0, DIS_autoupdate=0, CAN_MessageReady=0, CAN_prevTxFail=0, CAN_speed_recvd=0, CAN_coolant_recvd=0, CAN_voltage_recvd=0, CAN_new_dataSet_recvd=0;
// body data
int CAN_data_speed=0, CAN_data_rpm=0;
float CAN_data_coolant=0, CAN_data_voltage=0;
// global bluetooth flags
bool ehu_started=0, a2dp_started=0, bt_connected=0, bt_state_changed=0, bt_audio_playing=0, audio_state_changed=0;
// data buffers
static char utf16buffer[384], utf16_title[128], utf16_artist[128], utf16_album[128], CAN_MsgArray[64][8], title_buffer[64], artist_buffer[64], album_buffer[64], coolant_buffer[32], speed_buffer[32], voltage_buffer[32];
// display mode 0 -> song metadata, 1 -> body data, -1 -> prevent screen updates
int disp_mode=3;
bool disp_mode_changed=0, disp_mode_changed_with_delay=0;
// time to compare against
unsigned int last_millis=0;

void sendMultiPacketData();

void setup() {
  pinMode(PCM_MUTE_CTL, OUTPUT);
  digitalWrite(PCM_MUTE_CTL, HIGH);
  delay(100);
  if(DEBUGGING_ON) Serial.begin(921600);                 // serial comms for debug
  twai_init();
}

// processes data based on the current value of disp_mode or prints one-off messages by specifying the data in arguments; message is then transmitted right away
void processDataBuffer(bool disp_mode_override=0, char* up_line_text=nullptr, char* mid_line_text=nullptr, char* low_line_text=nullptr){             // disp_mode_override exists as a simple way to print one-off messages (like board status, errors and such)
  if(!CAN_MessageReady){                      // only prepare new buffers once the previously prepared message has been sent. DIS_forceUpdate is still 1, so this function should be called again next loop
    if(!disp_mode_override){
      if(disp_mode==0 && (album_buffer[0]!='\0' || title_buffer[0]!='\0' || artist_buffer[0]!='\0')){
        prepareMultiPacket(utf8_conversion(album_buffer, title_buffer, artist_buffer));               // prepare a 3-line message (audio Title, Album and Artist)
      }
      if(disp_mode==1){
        prepareMultiPacket(utf8_conversion(coolant_buffer, speed_buffer, voltage_buffer));               // prepare a vehicle dat 
      }
    } else {                                   // overriding buffers, making sure to switch disp_mode_changed_with delay so the message stays there for a while
      prepareMultiPacket(utf8_conversion(up_line_text, mid_line_text, low_line_text));
      disp_mode_changed_with_delay=1;
      last_millis=millis();
      DIS_autoupdate=0;
    }
    sendMultiPacket();                  // sends the stored CAN message
    DIS_forceUpdate=0;
  }
}

void loop() { 
  twai_get_status_info(&status_info);
  if(status_info.state==TWAI_STATE_BUS_OFF){
    if(DEBUGGING_ON) Serial.println("CAN: DETECTED BUS OFF. TRYING TO RECOVER -> REINSTALLING");
    //twai_initiate_recovery();                     // twai_initiate_recovery(); leads to hard crashes - it's something that ESP-IDF need to fix
    if(twai_driver_uninstall()==ESP_OK){
      if(DEBUGGING_ON) Serial.println("CAN: TWAI DRIVER UNINSTALL OK");
    } else {
      if(DEBUGGING_ON) Serial.println("CAN: TWAI DRIVER UNINSTALL FAIL!!! Rebooting...");          // total fail - just reboot at this point
      delay(100);
      ESP.restart();
    }
    twai_init();
  }
  if(CAN_prevTxFail){             // assume the message wasn't received properly -> retransmit the whole message again in case of a single fail
    sendMultiPacket();
    CAN_prevTxFail=0;
  }
  if(CAN_MessageReady){       // CAN_MessageReady is set after the display has been requested is sent. This waits for the display to reply with an ACK (id 0x2C1)
    delay(5);                // waits a bit because sometimes stuff happens so fast we get the ACK meant for the radio, we need to wait for the radio to stop talking
    RxMessage.identifier=0x0;                         // workaround for debugging when the bus is not on
    while(!RxMessage.identifier==0x2C1){
      if(DEBUGGING_ON) Serial.println("CAN: Waiting for 0x2C1 ACK...");
      while(twai_receive(&RxMessage, pdMS_TO_TICKS(5))!=ESP_OK){        // wait for the desired message
        delay(1);                                                         // I've honestly tried everything, refreshing status and alerts. This shit just doesn't work properly
      }
    }
    sendMultiPacketData();
  } else {                                   // this could use a rewrite
    if(DEBUGGING_ON && status_info.msgs_to_rx!=0){Serial.printf("CAN: Got messages %d messages in RX queue!\n", status_info.msgs_to_rx);}
    canReceive();             // read data from RX buffer
  }

  if(disp_mode_changed_with_delay){             // ensure the "one-off" status message is displayed for some time, then go back to regular messages based on disp_mode
    DIS_autoupdate=0;
    if((last_millis+3000)<millis()){
      disp_mode_changed=1;
      disp_mode_changed_with_delay=0;
      DIS_autoupdate=1;
    }
  }
  
  if(disp_mode_changed){                      // force update the display in case of switching context (speed display to AVRC metadata or the other way)
    DIS_forceUpdate=1;
    disp_mode_changed=0;
  }
  
  if(disp_mode==1){                           // if running in measurement block mode, check time and if enough time has elapsed ask for new data
    if((last_millis+250)<millis()){
      requestMeasurementBlocks();
      last_millis=millis();
    }
    if(CAN_new_dataSet_recvd){               // print new data if it has arrived
      CAN_new_dataSet_recvd=0;
      processDataBuffer();
    }
  }
  
  if(DIS_forceUpdate && disp_mode==0){                       // handles data processing for A2DP AVRC data events
    processDataBuffer();
  }
  
  A2DP_EventHandler();          // process bluetooth and audio flags set by interrupt callbacks
}