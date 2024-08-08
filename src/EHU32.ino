#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "driver/twai.h"
#include <WiFi.h>
// defining DEBUG enables Serial I/O for simulating button presses or faking measurement blocks through a separate RTOS task
//#define DEBUG

#ifndef DEBUG
#include <WiFiAP.h>
#include <ArduinoOTA.h>
#endif

// internal CAN queue constants
#define QUEUE_LENGTH 100
#define MSG_SIZE sizeof(twai_message_t)

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

// pin definitions
const int PCM_MUTE_CTL=23;            // this pin controls PCM5102s soft-mute function
// RTOS stuff
TaskHandle_t canReceiveTaskHandle, canDisplayTaskHandle, canProcessTaskHandle, canTransmitTaskHandle, canWatchdogTaskHandle, canAirConMacroTaskHandle;
QueueHandle_t canRxQueue, canTxQueue;
SemaphoreHandle_t CAN_MsgSemaphore=NULL;
// TWAI driver stuff
uint32_t alerts_triggered;
twai_status_info_t status_info;
// CAN related flags
volatile bool DIS_forceUpdate=0, CAN_MessageReady=0, CAN_prevTxFail=0, CAN_flowCtlFail=0, CAN_speed_recvd=0, CAN_coolant_recvd=0, CAN_voltage_recvd=0, CAN_new_dataSet_recvd=0, CAN_measurements_requested=0, disp_mode_changed=0;
// measurement related flags
volatile bool ECC_present=0;
// global bluetooth flags
volatile bool ehu_started=0, a2dp_started=0, bt_connected=0, bt_state_changed=0, bt_audio_playing=0, audio_state_changed=0, OTA_begin=0;
// body data
int CAN_data_speed=0, CAN_data_rpm=0;
float CAN_data_coolant=0, CAN_data_voltage=0;
// data buffers
char DisplayMsg[512], CAN_MsgArray[64][8], title_buffer[64], artist_buffer[64], album_buffer[64];
char coolant_buffer[32], speed_buffer[32], voltage_buffer[32];
// display mode 0 -> song metadata and general status messages, 1 -> body data, 2 -> single-line body data, -1 -> prevent screen updates
int disp_mode=-1;
// time to compare against
unsigned long last_millis=0, last_millis_req=0, last_millis_disp=0;

void canReceiveTask(void* pvParameters);
void canTransmitTask(void* pvParameters);
void canProcessTask(void* pvParameters);
void canDisplayTask(void* pvParameters);
void canWatchdogTask(void* pvParameters);
void canAirConMacroTask(void* pvParameters);
void OTAhandleTask(void* pvParameters);
void prepareMultiPacket(int bytes_processed, char* buffer_to_read);
int processDisplayMessage(char* upper_line_buffer, char* middle_line_buffer, char* lower_line_buffer);
void sendMultiPacket();
void sendMultiPacketData();

void setup(){
  pinMode(PCM_MUTE_CTL, OUTPUT);
  digitalWrite(PCM_MUTE_CTL, HIGH);
  vTaskDelay(pdMS_TO_TICKS(100));
  DEBUG_SERIAL(921600);                 // serial comms for debug

  twai_init();
  CAN_MsgSemaphore=xSemaphoreCreateMutex();  // as stuff is done asynchronously, we need to make sure that the message will not be transmitted when its being written to
  canRxQueue=xQueueCreate(QUEUE_LENGTH, MSG_SIZE);
  canTxQueue=xQueueCreate(QUEUE_LENGTH, MSG_SIZE);
  // FreeRTOS tasks
  xTaskCreate(canReceiveTask, "CANbusReceiveTask", 4096, NULL, 1, &canReceiveTaskHandle);
  xTaskCreate(canTransmitTask, "CANbusTransmitTask", 4096, NULL, 2, &canTransmitTaskHandle);
  xTaskCreate(canProcessTask, "CANbusMessageProcessor", 8192, NULL, 3, &canProcessTaskHandle);
  xTaskCreate(canDisplayTask, "DisplayUpdateTask", 8192, NULL, 3, &canDisplayTaskHandle);
  xTaskCreatePinnedToCore(canWatchdogTask, "WatchdogTask", 2048, NULL, 20, &canWatchdogTaskHandle, 0);
  vTaskSuspend(canDisplayTaskHandle);
  #ifdef DEBUG 
  xTaskCreate(CANsimTask, "CANbusSimulateEvents", 2048, NULL, 4, NULL);       // allows to simulate button presses through serial
  #endif
  xTaskCreate(canAirConMacroTask, "AirConMacroTask", 2048, NULL, 10, &canAirConMacroTaskHandle);
  vTaskSuspend(canAirConMacroTaskHandle);       // Aircon macro task exists solely to execute simulated button presses asynchronously, as such it is only started when needed
}

// this task monitors radio messages and resets the program if the radio goes to sleep or CAN dies
void canWatchdogTask(void *pvParameters){
  static BaseType_t notifResult;
  while(1){
    if(ehu_started){
      notifResult=xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(15000));           // wait for a notification that display packet from the radio unit has been received
      if(notifResult==pdFAIL){            // if the notification has not been received in the specified timeframe (radio sends its display messages each 5s, specified timeout of 15s for safety) we assume the radio is off
        DEBUG_PRINTLN("WATCHDOG: Triggering software reset...");
        vTaskDelay(pdMS_TO_TICKS(100));
        a2dp_shutdown();      // this or disp_mode=-1?
      } else {
        DEBUG_PRINTLN("WATCHDOG: Reset successful.");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// processes data based on the current value of disp_mode or prints one-off messages by specifying the data in arguments; message is then transmitted right away
void writeTextToDisplay(bool disp_mode_override=0, char* up_line_text=nullptr, char* mid_line_text=nullptr, char* low_line_text=nullptr){             // disp_mode_override exists as a simple way to print one-off messages (like board status, errors and such)
  xSemaphoreTake(CAN_MsgSemaphore, portMAX_DELAY);      // take the semaphore as a way to prevent any transmission when the message structure is being written
  if(!disp_mode_override){
    if(disp_mode==0 && (album_buffer[0]!='\0' || title_buffer[0]!='\0' || artist_buffer[0]!='\0')){
      prepareMultiPacket(processDisplayMessage(album_buffer, title_buffer, artist_buffer), DisplayMsg);               // prepare a 3-line message (audio Title, Album and Artist)
    } else {
      if(disp_mode==1){
        prepareMultiPacket(processDisplayMessage(coolant_buffer, speed_buffer, voltage_buffer), DisplayMsg);               // vehicle data buffer
      }
      if(disp_mode==2){
        prepareMultiPacket(processDisplayMessage(nullptr, coolant_buffer, nullptr), DisplayMsg);               // vehicle data buffer
      }
    }
  } else {                                   // overriding buffers
    prepareMultiPacket(processDisplayMessage(up_line_text, mid_line_text, low_line_text), DisplayMsg);
  }
  xSemaphoreGive(CAN_MsgSemaphore);
  DIS_forceUpdate=0;
  vTaskResume(canDisplayTaskHandle);        // resume display task, it will suspend itself after its job is done
}

// loop will process interrupt flags and print text based on what happened
void loop(){
  if(OTA_begin){
    disp_mode=-1;
    writeTextToDisplay(1, "OTA initiated", "BT off, OTA on", "Waiting for FW...");
    vTaskDelay(1000);
    vTaskSuspend(canReceiveTaskHandle);
    vTaskSuspend(canTransmitTaskHandle);
    vTaskSuspend(canProcessTaskHandle);
    vTaskSuspend(canDisplayTaskHandle);
    vTaskSuspend(canWatchdogTaskHandle);    // so I added the watchdog but forgot to suspend it when starting OTA. result? Couldn't update it inside the car and had to take the radio unit out to do it manually
    #ifndef DEBUG
    OTA_Handle();
    #endif
  }

  if(disp_mode==1 && ehu_started){                           // if running in measurement block mode, check time and if enough time has elapsed ask for new data
    if((last_millis_req+250)<millis()){
      requestMeasurementBlocks();
      last_millis_req=millis();
    }
    if(((last_millis_disp+250)<millis()) && CAN_new_dataSet_recvd){               // print new data if it has arrived
      CAN_new_dataSet_recvd=0;
      writeTextToDisplay();
      last_millis_disp=millis();
    }
  }

  if(disp_mode==2 && ehu_started){                  // single line measurement mode only provides coolant temp.
    if((last_millis_req+3000)<millis()){        // since we're only updating coolant data, I'd say that 3 secs is a fair update rate
      requestMeasurementBlocks();
      last_millis_req=millis();
    }
    if(((last_millis_disp+3000)<millis()) && CAN_coolant_recvd){
      CAN_coolant_recvd=0;
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

  if(DIS_forceUpdate && disp_mode==0){                       // handles data processing for A2DP AVRC data events
    writeTextToDisplay();
  }
  
  A2DP_EventHandler();          // process bluetooth and audio flags set by interrupt callbacks
}