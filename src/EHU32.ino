#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "esp_sleep.h"
#include "driver/twai.h"
#include <Preferences.h>

// defining DEBUG enables Serial I/O for simulating button presses or faking measurement blocks through a separate RTOS task
//#define DEBUG

#ifndef DEBUG
#include <WiFi.h>
#include <WiFiAP.h>
#include <ArduinoOTA.h>
#endif

#ifdef DEBUG
#define DEBUG_SERIAL(X) Serial.begin(X)
#define DEBUG_PRINT(X) Serial.print(X)
#define DEBUG_PRINTLN(X) Serial.println(X)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_SERIAL(X)
#define DEBUG_PRINT(X)
#define DEBUG_PRINTLN(X)
#define DEBUG_PRINTF(...)
#endif

// defining available flags in the eventGroup
#define DIS_forceUpdate (1 << 0)           // call for the eventHandler to process the text buffers and instantly transmit the new message
#define CAN_MessageReady (1 << 1)
#define CAN_prevTxFail (1 << 2)
#define CAN_abortMultiPacket (1 << 3)
#define CAN_flowCtlFail (1 << 4)
#define CAN_speed_recvd (1 << 5)
#define CAN_coolant_recvd (1 << 6)
#define CAN_new_dataSet_recvd (1 << 7)
#define CAN_voltage_recvd (1 << 8)
#define CAN_measurements_requested (1 << 9)
#define disp_mode_changed (1 << 10)
#define CAN_allowAutoRefresh (1 << 11)          // otherwise means "Aux" has been detected
#define ECC_present (1 << 12)
#define ehu_started (1 << 13)
#define a2dp_started (1 << 14)
#define bt_connected (1 << 15)
#define bt_state_changed (1 << 16)
#define bt_audio_playing (1 << 17)
#define audio_state_changed (1 << 18)
#define md_album_recvd (1 << 19)
#define md_artist_recvd (1 << 20)
#define md_title_recvd (1 << 21)
#define OTA_begin (1 << 22)
#define OTA_abort (1 << 23)

// pin definitions
const int PCM_MUTE_CTL=23, PCM_ENABLE=27;            // D23 controls PCM5102s soft-mute function, D27 enables PCM5102s power
// RTOS stuff
TaskHandle_t canReceiveTaskHandle, canDisplayTaskHandle, canProcessTaskHandle, canTransmitTaskHandle, canWatchdogTaskHandle, canAirConMacroTaskHandle, canMessageDecoderTaskHandle, eventHandlerTaskHandle;
QueueHandle_t canRxQueue, canTxQueue, canDispQueue;
SemaphoreHandle_t CAN_MsgSemaphore=NULL, BufferSemaphore=NULL;
EventGroupHandle_t eventGroup;
// TWAI driver stuff
uint32_t alerts_triggered;
twai_status_info_t status_info;
uint32_t displayMsgIdentifier=0;
// data buffers
char DisplayMsg[1024], CAN_MsgArray[128][8], title_buffer[64], artist_buffer[64], album_buffer[64];
char coolant_buffer[32], speed_buffer[32], voltage_buffer[32];
// display mode 0 -> song metadata and general status messages, 1 -> body data, 2 -> single-line body data, -1 -> prevent screen updates
volatile int disp_mode=-1;
// time to compare against
unsigned long last_millis=0, last_millis_req=0, last_millis_disp=0, last_millis_aux=0;
// body data
bool vehicle_ECC_present, vehicle_UHP_present;

void canReceiveTask(void* pvParameters);
void canTransmitTask(void* pvParameters);
void canProcessTask(void* pvParameters);
void canDisplayTask(void* pvParameters);
void canWatchdogTask(void* pvParameters);
void canAirConMacroTask(void* pvParameters);
void OTAhandleTask(void* pvParameters);
void prepareMultiPacket(int bytes_processed, char* buffer_to_read);
int processDisplayMessage(char* upper_line_buffer, char* middle_line_buffer, char* lower_line_buffer);

void setup(){
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 0);    // this will wake the ESP32 up if there's CAN activity
  pinMode(PCM_MUTE_CTL, OUTPUT);
  pinMode(PCM_ENABLE, OUTPUT);          // control PCM5102 power setting
  digitalWrite(PCM_MUTE_CTL, HIGH);
  digitalWrite(PCM_ENABLE, HIGH);
  DEBUG_SERIAL(921600);                 // serial comms for debug

  twai_init();      // sets up everything CAN-bus related
  twai_message_t testMsg;
  if(twai_receive(&testMsg, pdMS_TO_TICKS(100))!=ESP_OK){       // if there's no activity on the bus, assume the vehicle is off, go to sleep and wake up after 5 seconds
    DEBUG_PRINTLN("CAN inactive. Back to sleep!");
    #ifdef DEBUG
    vTaskDelay(pdMS_TO_TICKS(10));  // wait for a bit for the buffer to be transmitted when debugging
    #endif
    esp_deep_sleep_start();                     // enter deep sleep
  }

  digitalWrite(PCM_ENABLE, LOW);           // enable PCM5102 and wake SN65HVD230 up from standby (active low in case of KF50BD)
  
  Preferences settings;
  settings.begin("my-app", false);
  if(!settings.isKey("setupcomplete")){
    DEBUG_PRINTLN("CAN SETUP: Key does not exist! Creating keys");
    settings.clear();
    settings.putBool("setupcomplete", 0);
    settings.putBool("uhppresent", 0);
    settings.putBool("eccpresent", 0);
    settings.putUInt("identifier", 0);
  }
  bool init_setupComplete=settings.getBool("setupcomplete", 0);   // prefs init
  if(!init_setupComplete){     // this should only be executed on first boot
    while(twai_receive(&testMsg, portMAX_DELAY)!=ESP_OK && testMsg.identifier!=0x6C1 && (testMsg.data[2]!=0x40 || testMsg.data[2]!=0xC0)) {} // wait for the initial 0x6C1 c0/40 then start the timer
    unsigned long millis_init_start=millis();       // got 0x6C1, assume radio started now
    bool init_usedCANids[16];     // represents 0x6C0 to 0x6CF
    while(twai_receive(&testMsg, portMAX_DELAY)==ESP_OK && (millis_init_start+20000>millis())){ // keep receiving all msgs and log everything from 0x6C0 to 0x6CF for 10 secs
      if((testMsg.identifier & 0xFF0)==0x6C0 && !init_usedCANids[testMsg.identifier-0x6c0]){      // allows 0x6C0 to 0x6CF
        init_usedCANids[testMsg.identifier-0x6c0]=1;    // if got a hit, mark it the ID as in use
        if(testMsg.identifier==0x6C7){
          settings.putBool("uhppresent", 1);
          vehicle_UHP_present=1;
        }
        if(testMsg.identifier==0x6C8){      // ECC doesn't start until the key is at ignition so that's relatively pointless for now
          settings.putBool("eccpresent", 1);
          vehicle_ECC_present=1;
        }
        DEBUG_PRINTF("CAN SETUP: Marking 0x%03X as a CAN ID in use\n", testMsg.identifier);
      }
    }
    // finally, perform some test ISO 15765-2 first frame transmissions to see which CAN IDs the display will respond to. Avoid used IDs.
    // if the display does not respond to the unused IDs, check if 0x6C8 is present and use that, in the end just use 0x6C1 with additional logic for overwrite attempts
    DEBUG_PRINT("CAN SETUP: Attempting to test display responses to tested identifiers: ");
    twai_message_t testMsgTx={ .identifier=0x6C0, .data_length_code=8, .data={0x10, 0xA7, 0x40, 0x00, 0xA4, 0x03, 0x10, 0x13}};
    unsigned long millis_transmitted;
    for(int i=0; i<16 && displayMsgIdentifier==0; i++){
      if(init_usedCANids[i])  i++;  // skip IDs in active use
      testMsgTx.identifier=(0x6C0+i);
      DEBUG_PRINTF("0x%03X... ", testMsgTx.identifier);
      twai_transmit(&testMsgTx, pdMS_TO_TICKS(300));
      millis_transmitted=millis();
      while((millis_transmitted+1000>millis()) && displayMsgIdentifier==0){     // break out early if the display message identifier has been found
        if(twai_receive(&testMsg, pdMS_TO_TICKS(300))==ESP_OK){
          if(testMsg.identifier==(testMsgTx.identifier-0x400) && testMsg.data[0]==0x30){   // received a flow control frame, meaning the display has responded to a first frame (ids 0x2C0 to 0x2CF) and accepted the transmission (db0 0x30)
            displayMsgIdentifier=testMsgTx.identifier;    // save that ID
            DEBUG_PRINTF("got a response on 0x%03X!", testMsg.identifier);
          }
        }
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    if(displayMsgIdentifier==0){
      if(init_usedCANids[8]==1){
        displayMsgIdentifier=0x6C8;
        DEBUG_PRINTLN("\nCAN SETUP: Unable to find a valid unused CAN ID, but detected ECC -> using 0x6C8");
      } else {
        displayMsgIdentifier=0x6C1;
        DEBUG_PRINTLN("\nCAN SETUP: Unable to find a valid unused CAN ID. Falling back to stock -> using 0x6C1");
      }
    }
    DEBUG_PRINTLN("\nCAN SETUP: Saving the display message identifier to flash...");
    settings.putUInt("identifier", displayMsgIdentifier);    // saving that data to flash to use on next boot
    settings.putBool("setupcomplete", 1);
  } else {
    displayMsgIdentifier=settings.getUInt("identifier", 0);
    DEBUG_PRINTF("CAN SETUP: Get the display identifier from flash -> 0x%03X\n", displayMsgIdentifier);
    vehicle_ECC_present=settings.getBool("eccpresent", 0);
    vehicle_UHP_present=settings.getBool("uhppresent", 0);
  }
  if(displayMsgIdentifier==0){
    DEBUG_PRINTLN("CAN SETUP: identifier can't be 0, rerunning CAN setup...");
    settings.putBool("setupcomplete", 0);
    settings.end();
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP.restart();
  } else {
    settings.end();
  }

  CAN_MsgSemaphore=xSemaphoreCreateMutex();  // as stuff is done asynchronously, we need to make sure that the message will not be transmitted when its being written to
  BufferSemaphore=xSemaphoreCreateMutex();    // CAN_MsgSemaphore is used when encoding the message and transmitting it, while BufferSemaphore is used when acquiring new data or encoding the message
  canRxQueue=xQueueCreate(100, sizeof(twai_message_t));        // internal EHU32 queue for messages to be handled by the canProcessTask
  canTxQueue=xQueueCreate(100, sizeof(twai_message_t));        // internal EHU32 queue for messages to be transmitted
  canDispQueue=xQueueCreate(255, sizeof(uint8_t));        // queue used for handling of raw ISO 15765-2 data that's meant for the display (Aux string detection)
  eventGroup=xEventGroupCreate();       // just one eventGroup for now

  // FreeRTOS tasks
  xTaskCreatePinnedToCore(canReceiveTask, "CANbusReceiveTask", 4096, NULL, 1, &canReceiveTaskHandle, 1);
  xTaskCreatePinnedToCore(canTransmitTask, "CANbusTransmitTask", 4096, NULL, 1, &canTransmitTaskHandle, 0);
  xTaskCreatePinnedToCore(canProcessTask, "CANbusMessageProcessor", 8192, NULL, 2, &canProcessTaskHandle, 0);
  xTaskCreatePinnedToCore(canDisplayTask, "DisplayUpdateTask", 8192, NULL, 1, &canDisplayTaskHandle, 1);
  vTaskSuspend(canDisplayTaskHandle);
  xTaskCreatePinnedToCore(canWatchdogTask, "WatchdogTask", 2048, NULL, tskIDLE_PRIORITY, &canWatchdogTaskHandle, 0);
  xTaskCreatePinnedToCore(canMessageDecoder, "MessageDecoder", 2048, NULL, tskIDLE_PRIORITY, &canMessageDecoderTaskHandle, 0);
  vTaskSuspend(canMessageDecoderTaskHandle);
  #ifdef DEBUG 
  xTaskCreate(CANsimTask, "CANbusSimulateEvents", 2048, NULL, 21, NULL);       // allows to simulate button presses through serial
  #endif
  xTaskCreatePinnedToCore(canAirConMacroTask, "AirConMacroTask", 2048, NULL, 10, &canAirConMacroTaskHandle, 0);
  vTaskSuspend(canAirConMacroTaskHandle);       // Aircon macro task exists solely to execute simulated button presses asynchronously, as such it is only started when needed
  xTaskCreatePinnedToCore(eventHandlerTask, "eventHandler", 8192, NULL, 4, &eventHandlerTaskHandle, 1);
}

// this task monitors radio messages and resets the program if the radio goes to sleep or CAN dies
void canWatchdogTask(void *pvParameters){
  static BaseType_t notifResult;
  while(1){
    notifResult=xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(15000));           // wait for a notification that display packet from the radio unit has been received
    if(notifResult==pdFAIL){            // if the notification has not been received in the specified timeframe (radio sends its display messages each 5s, specified timeout of 15s for safety) we assume the radio is off
      DEBUG_PRINTLN("WATCHDOG: Triggering software reset...");
      vTaskDelay(pdMS_TO_TICKS(100));
      a2dp_shutdown();      // this or disp_mode=-1?
    } else {
      DEBUG_PRINTLN("WATCHDOG: Reset successful.");
      xTaskNotifyStateClear(NULL);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// below functions are used to simplify interaction with freeRTOS eventGroups
void setFlag(uint32_t bit){
  xEventGroupSetBits(eventGroup, bit);
}

// clears an event bit
void clearFlag(uint32_t bit){
  xEventGroupClearBits(eventGroup, bit);
}

// waits for an event bit to be set, blocking indefinitely if 2nd argument not provided
void waitForFlag(uint32_t bit, TickType_t ticksToWait=portMAX_DELAY){
  xEventGroupWaitBits(eventGroup, bit, pdFALSE, pdTRUE, ticksToWait);
}

// Check if a specific event bit is set (without blocking)
bool checkFlag(uint32_t bit){
  EventBits_t bits=xEventGroupGetBits(eventGroup);
  return (bits&bit)!=0;
}

// used to clear saved settings and go through the setup again on next reboot
void prefs_clear(){
  Preferences settings;
  settings.begin("my-app", false);
  settings.clear();
  settings.end();
}

// processes data based on the current value of disp_mode or prints one-off messages by specifying the data in arguments; message is then transmitted right away
// it acts as a bridge between UTF-8 text data and the resulting CAN messages meant to be transmitted to the display
void writeTextToDisplay(bool disp_mode_override=0, char* up_line_text=nullptr, char* mid_line_text=nullptr, char* low_line_text=nullptr){             // disp_mode_override exists as a simple way to print one-off messages (like board status, errors and such)
  DEBUG_PRINTLN("EVENTS: Refreshing buffer...");
  xSemaphoreTake(CAN_MsgSemaphore, portMAX_DELAY);      // take the semaphore as a way to prevent any transmission when the message structure is being written
  xSemaphoreTake(BufferSemaphore, portMAX_DELAY);       // we take both semaphores, since this task specifically interacts with both the internal data buffers and the CAN message buffer
  if(!disp_mode_override){
    if(disp_mode==0 && (album_buffer[0]!='\0' || title_buffer[0]!='\0' || artist_buffer[0]!='\0')){         // audio metadata mode
      prepareMultiPacket(processDisplayMessage(album_buffer, title_buffer, artist_buffer), DisplayMsg);               // prepare a 3-line message (audio Title, Album and Artist)
    } else {
      if(disp_mode==1){                                     // vehicle data mode (3-line)
        prepareMultiPacket(processDisplayMessage(coolant_buffer, speed_buffer, voltage_buffer), DisplayMsg);               // vehicle data buffer
      }
      if(disp_mode==2){                                     // coolant mode (1-line)
        prepareMultiPacket(processDisplayMessage(nullptr, coolant_buffer, nullptr), DisplayMsg);               // vehicle data buffer (single line)
      }
    }
  } else {                                   // overriding buffers
    prepareMultiPacket(processDisplayMessage(up_line_text, mid_line_text, low_line_text), DisplayMsg);
  }
  xSemaphoreGive(CAN_MsgSemaphore);
  xSemaphoreGive(BufferSemaphore);        // releasing semaphores
  vTaskResume(canDisplayTaskHandle);            // buffer has been updated, transmit 
  clearFlag(DIS_forceUpdate);
}

// this task handles events and output to display in context of events, such as new data in buffers or A2DP events
void eventHandlerTask(void *pvParameters){
  while(1){
    if(checkFlag(OTA_begin)){
      disp_mode=0;
      writeTextToDisplay(1, "Bluetooth off", "OTA Started", "Waiting for connection...");
      vTaskDelay(1000);
      vTaskSuspend(canWatchdogTaskHandle);    // so I added the watchdog but forgot to suspend it when starting OTA. result? Couldn't update it inside the car and had to take the radio unit out to do it manually
      #ifndef DEBUG
      OTA_Handle();
      #endif
    }

    if(disp_mode==1 && checkFlag(ehu_started)){                           // if running in measurement block mode, check time and if enough time has elapsed ask for new data
      if(checkFlag(disp_mode_changed)){
        clearFlag(disp_mode_changed);
        writeTextToDisplay(1, nullptr, "No data yet...", nullptr);      // print a status message that will stay if display/ecc are not responding
      }
      if((last_millis_req+400)<millis()){
        requestMeasurementBlocks();
        last_millis_req=millis();
      }
      if(((last_millis_disp+400)<millis()) && checkFlag(CAN_new_dataSet_recvd)){               // print new data if it has arrived
        clearFlag(CAN_new_dataSet_recvd);
        writeTextToDisplay();
        last_millis_disp=millis();
      }
    }

    if(disp_mode==2 && checkFlag(ehu_started)){                  // single line measurement mode only provides coolant temp.
      if(checkFlag(disp_mode_changed)){
        clearFlag(disp_mode_changed);
        writeTextToDisplay(1, nullptr, "No data yet...", nullptr);      // print a status message that will stay if display/ecc are not responding
      }
      if((last_millis_req+3000)<millis()){        // since we're only updating coolant data, I'd say that 3 secs is a fair update rate
        requestCoolantTemperature();
        last_millis_req=millis();
      }
      if(((last_millis_disp+3000)<millis()) && checkFlag(CAN_coolant_recvd)){
        clearFlag(CAN_coolant_recvd);
        writeTextToDisplay();
        last_millis_disp=millis();
      }
    }

    twai_get_status_info(&status_info);  
    // this will try to get CAN back up in case it fails
    if((status_info.state==TWAI_STATE_BUS_OFF) || (twai_get_status_info(&status_info)==ESP_ERR_INVALID_STATE)){
      DEBUG_PRINTLN("CAN: DETECTED BUS OFF. TRYING TO RECOVER -> REINSTALLING");
      vTaskSuspend(canReceiveTaskHandle);
      vTaskSuspend(canTransmitTaskHandle);
      vTaskSuspend(canProcessTaskHandle);
      vTaskSuspend(canDisplayTaskHandle);
      vTaskSuspend(canWatchdogTaskHandle);
      //twai_initiate_recovery();                     // twai_initiate_recovery(); leads to hard crashes - it's something that ESP-IDF need to fix
      twai_stop();
      if(twai_driver_uninstall()==ESP_OK){
        DEBUG_PRINTLN("CAN: TWAI DRIVER UNINSTALL OK");
      } else {
        DEBUG_PRINTLN("CAN: TWAI DRIVER UNINSTALL FAIL!!! Rebooting...");          // total fail - just reboot at this point
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP.restart();
      }
      vTaskDelay(100);
      twai_init();
      vTaskDelay(100);
      vTaskResume(canReceiveTaskHandle);
      vTaskResume(canTransmitTaskHandle);
      vTaskResume(canProcessTaskHandle);
      vTaskResume(canDisplayTaskHandle);
      vTaskResume(canWatchdogTaskHandle);
    }

    A2DP_EventHandler();          // process bluetooth and audio flags set by A2DP callbacks
    vTaskDelay(10);
  }
}

// loop will do nothing
void loop(){
  vTaskDelay(pdMS_TO_TICKS(1000));
}