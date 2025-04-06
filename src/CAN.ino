#include "driver/twai.h"

void OTAhandleTask(void* pvParameters);

// CAN-related variables
volatile uint8_t canISO_frameSpacing=1;   // simple implementation of ISO 15765-2 variable frame spacing, based on flow control frames by the receiving node

// defining static CAN frames for simulation
const twai_message_t  simulate_scroll_up={ .identifier=0x201, .data_length_code=3, .data={0x08, 0x6A, 0x01}},
                      simulate_scroll_down={ .identifier=0x201, .data_length_code=3, .data={0x08, 0x6A, 0xFF}},
                      simulate_scroll_press={ .identifier=0x206, .data_length_code=3, .data={0x01, 0x84, 0x0}},
                      simulate_scroll_release={ .identifier=0x206, .data_length_code=3, .data={0x0, 0x84, 0x02}},
                      Msg_ACmacro_down={ .identifier=0x208, .data_length_code=3, .data={0x08, 0x16, 0x01}},
                      Msg_ACmacro_up={ .identifier=0x208, .data_length_code=3, .data={0x08, 0x16, 0xFF}},
                      Msg_ACmacro_press={ .identifier=0x208, .data_length_code=3, .data={0x01, 0x17, 0x0}},
                      Msg_ACmacro_release={ .identifier=0x208, .data_length_code=3, .data={0x0, 0x17, 0x02}},
                      Msg_MeasurementRequestDIS={ .identifier=0x246, .data_length_code=7, .data={0x06, 0xAA, 0x01, 0x01, 0x0B, 0x0E, 0x13}},
                      Msg_MeasurementRequestECC={ .identifier=0x248, .data_length_code=7, .data={0x06, 0xAA, 0x01, 0x01, 0x07, 0x10, 0x11}},
                      Msg_VoltageRequestDIS={ .identifier=0x246, .data_length_code=5, .data={0x04, 0xAA, 0x01, 0x01, 0x13}},
                      Msg_CoolantRequestDIS={ .identifier=0x246, .data_length_code=5, .data={0x04, 0xAA, 0x01, 0x01, 0x0B}},
                      Msg_CoolantRequestECC={ .identifier=0x248, .data_length_code=5, .data={0x04, 0xAA, 0x01, 0x01, 0x10}};

// can't initialize the values of the union inside the twai_message_t type struct, which is why it's defined here, then the transmit task sets the .ss flag
twai_message_t  Msg_PreventDisplayUpdate={ .identifier=0x2C1, .data_length_code=8, .data={0x30, 0x0, 0x7F, 0, 0, 0, 0, 0}},
                Msg_AbortTransmission={ .identifier=0x2C1, .data_length_code=8, .data={0x32, 0x0, 0, 0, 0, 0, 0, 0}};     // can have unforseen consequences such as resets! use as last resort

// initializing CAN communication
void twai_init(){
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_4, TWAI_MODE_NORMAL);         // CAN bus set up
  g_config.rx_queue_len=40;
  g_config.tx_queue_len=5;
  g_config.intr_flags=(ESP_INTR_FLAG_NMI & ESP_INTR_FLAG_IRAM);   // run the TWAI driver at the highest possible priority
  twai_timing_config_t t_config =  {.brp = 42, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false};    // set CAN prescalers and time quanta for 95kbit
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    DEBUG_PRINT("\nCAN/TWAI SETUP => "); 
  if(twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
      DEBUG_PRINT("DRV_INSTALL: OK ");
  } else {
      DEBUG_PRINT("DRV_INST: FAIL ");
  }
  if (twai_start() == ESP_OK) {
      DEBUG_PRINT("DRV_START: OK ");
  } else {
      DEBUG_PRINT("DRV_START: FAIL ");
  }
  uint32_t alerts_to_enable=TWAI_ALERT_TX_SUCCESS;
  if(twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK){
      DEBUG_PRINTLN("ALERTS: OK \n");
  } else {
      DEBUG_PRINTLN("ALERTS: FAIL \n");
  }
}

// this task only reads CAN messages, filters them and enqueues them to be decoded ansynchronously. 0x6C1 is a special case, as the radio message has to be blocked ASAP
void canReceiveTask(void *pvParameters){
  static twai_message_t Recvd_CAN_MSG, DummyFirstFrame={ .identifier=0x6C1, .data_length_code=8, .data={0x10, 0xA7, 0x50, 0x00, 0xA4, 0x03, 0x10, 0x13}};
  uint32_t alerts_FlowCtl;        // separate buffer for the flow control alerts, prevents race conditions with the other task which also uses alerts
  bool allowDisplayBlocking=0, firstAckReceived=0, overwriteAttemped=0;
  uint32_t flowCtlUsed=(displayMsgIdentifier-0x400);  // set from memory, if using 0x6C0 flow control will be 0x2C0 etc.
  Msg_PreventDisplayUpdate.extd=0; Msg_PreventDisplayUpdate.ss=1; Msg_PreventDisplayUpdate.self=0; Msg_PreventDisplayUpdate.rtr=0;
  Msg_AbortTransmission.extd=0; Msg_AbortTransmission.ss=1; Msg_AbortTransmission.self=0; Msg_AbortTransmission.rtr=0;
  while(1){
    allowDisplayBlocking=checkFlag(CAN_allowAutoRefresh);   // checking earlier improves performance, there's very little time to send that message, otherwise we get error frames (because of the same ID)
    if(twai_receive(&Recvd_CAN_MSG, portMAX_DELAY)==ESP_OK){
      switch(Recvd_CAN_MSG.identifier){
        case 0x6C1: {
          if(disp_mode!=-1){            // don't bother checking the data if there's no need to update the display
            if(Recvd_CAN_MSG.data[0]==0x10 && (Recvd_CAN_MSG.data[2]==0x40 || Recvd_CAN_MSG.data[2]==0xC0) && Recvd_CAN_MSG.data[5]==0x03 && (disp_mode!=0 || allowDisplayBlocking)){       // another task processes the data since we can't do that here
              twai_transmit(&Msg_PreventDisplayUpdate, pdMS_TO_TICKS(30));  // radio blocking msg has to be transmitted ASAP, which is why we skip the queue
              DEBUG_PRINTLN("CAN: Received display update, trying to block");
              twai_read_alerts(&alerts_FlowCtl, pdMS_TO_TICKS(10));    // read stats to a local alert buffer
              if(alerts_FlowCtl & TWAI_ALERT_TX_SUCCESS){
                clearFlag(CAN_flowCtlFail);
                DEBUG_PRINTLN("CAN: Blocked successfully");
              } else {
                setFlag(CAN_flowCtlFail);                     // lets the display task know that we failed blocking the display TX and as such the display task shall wait
                DEBUG_PRINTLN("CAN: Blocking failed!");
              }
              overwriteAttemped=1;    // if the display message retransmission was intended to mask the radio's message
              if(eTaskGetState(canDisplayTaskHandle)==eRunning){
                if(Recvd_CAN_MSG.data[0]==0x10) setFlag(CAN_abortMultiPacket);    // let the transmission task know that the radio has transmissed a new first frame -> any ongoing transmission is no longer valid
              }
              vTaskResume(canDisplayTaskHandle); // only retransmit the msg for audio metadata mode and single line coolant, since these don't update frequently
            }
          }
        }
        case 0x201:
        case 0x206:
        case 0x208:
        case 0x501:
        case 0x546:
        case 0x548:
        case 0x4E8:
        case 0x6C8:
          xQueueSend(canRxQueue, &Recvd_CAN_MSG, portMAX_DELAY);      // queue the message contents to be read at a later time
          break;
        case 0x2C1:{        // this attempts to invalidate the radio's display call with identifier 0x6C1
          if(flowCtlUsed==0x2C1){       // old/backup logic for radio messages on 0x6C1
            if(firstAckReceived || !overwriteAttemped){          // disregard the first flow control frame meant for the radio unit ONLY if it was a result of retransmission to mask the radio's display update
              waitForFlag(CAN_MessageReady, pdMS_TO_TICKS(20));   // this is blocking a lot of stuff so gotta find a sweet spot for how long to block for
              if(Recvd_CAN_MSG.data[0]==0x30){
                xTaskNotifyGive(canDisplayTaskHandle);          // let the display update task know that the data is ready to be transmitted
                xQueueSend(canRxQueue, &Recvd_CAN_MSG, portMAX_DELAY);
                if(firstAckReceived) firstAckReceived=0;             // reset it to be ready for the next one
                if(overwriteAttemped) overwriteAttemped=0;
              }
            } else {
              firstAckReceived=1;   // flow control not meant for EHU32, set the switch and wait for the second one
            }
          }
          if(overwriteAttemped && flowCtlUsed!=0x2C1){
            twai_transmit(&DummyFirstFrame, pdMS_TO_TICKS(100));  // transmit a dummy frame ASAP to invalidate previous display call
            DEBUG_PRINTLN("CAN: Attempting to invalidate radio's screen call...");
            overwriteAttemped=0;
          }
          break;
        }
        default: break;
      }
      if(Recvd_CAN_MSG.identifier==flowCtlUsed && Recvd_CAN_MSG.identifier!=0x2C1 && Recvd_CAN_MSG.data[0]==0x30){ // can't be a switch case because it might be dynamic
        xTaskNotifyGive(canDisplayTaskHandle);
      }
    }
  }
}

// this task processes filtered CAN frames read from canRxQueue
void canProcessTask(void *pvParameters){
  static twai_message_t RxMsg;
  bool badVoltage_VectraC_bypass=0;
  while(1){
    xQueueReceive(canRxQueue, &RxMsg, portMAX_DELAY);     // receives data from the internal queue
    switch(RxMsg.identifier){
      case 0x201: {                                         // radio button decoder
        bool btn_state=RxMsg.data[0];
        unsigned int btn_ms_held=(RxMsg.data[2]*100);
        switch(RxMsg.data[1]){
          case 0x30:  canActionEhuButton0(btn_state, btn_ms_held);      // CD30 has no '0' button!
                      break;
          case 0x31:  canActionEhuButton1(btn_state, btn_ms_held);
                      break;
          case 0x32:  canActionEhuButton2(btn_state, btn_ms_held);
                      break;
          case 0x33:  canActionEhuButton3(btn_state, btn_ms_held);
                      break;
          case 0x34:  canActionEhuButton4(btn_state, btn_ms_held);
                      break;
          case 0x35:  canActionEhuButton5(btn_state, btn_ms_held);
                      break;
          case 0x36:  canActionEhuButton6(btn_state, btn_ms_held);
                      break;
          case 0x37:  canActionEhuButton7(btn_state, btn_ms_held);
                      break;
          case 0x38:  canActionEhuButton8(btn_state, btn_ms_held);
                      break;
          case 0x39:  canActionEhuButton9(btn_state, btn_ms_held);
                      break;
          default: break;
        }
        break;
      }
      case 0x206: {                                  // decodes steering wheel buttons
        if(checkFlag(bt_connected) && RxMsg.data[0]==0x0 && checkFlag(CAN_allowAutoRefresh)){                     // makes sure "Aux" is displayed, otherwise forward/next buttons will have no effect
          switch(RxMsg.data[1]){
            case 0x81:{
              if(!vehicle_UHP_present){   // only enable the play/pause functionality for vehicles without UHP otherwise it could conflict with the factory bluetooth hands-free
                if(checkFlag(bt_audio_playing)){                 // upper left button (box with waves)
                    a2dp_sink.pause(); 
                  } else {
                    a2dp_sink.play();
                  }
                }
              break;
            }
            case 0x91:{
              a2dp_sink.next();                     // upper right button (arrow up)
              break;
            }
            case 0x92:{
              a2dp_sink.previous();                 // lower right button (arrow down)
              break;
            }
            default:    break;
          }          
        }
        break;
      }
      case 0x208: {                               // AC panel button event
        if(RxMsg.data[0]==0x0 && RxMsg.data[1]==0x17 && RxMsg.data[2]<0x03){          // FIXME!
          vTaskResume(canAirConMacroTaskHandle);   // start AC macro
        }
        break;
      }
      case 0x2C1: {
        if(RxMsg.data[2]!=0 && canISO_frameSpacing!=RxMsg.data[2]) canISO_frameSpacing=RxMsg.data[2];            // adjust ISO 15765-2 frame spacing delay only if the receiving node calls for it
        break;
      }
      case 0x501: {                                         // CD30MP3 goes to sleep -> disable bluetooth connectivity
        if(checkFlag(a2dp_started) && RxMsg.data[3]==0x18){
          a2dp_shutdown();
        }
        break;
      }
      case 0x546: {                             // display measurement blocks (used as a fallback or for )
          if(disp_mode==1 || disp_mode==2){
            xSemaphoreTake(BufferSemaphore, portMAX_DELAY);
            DEBUG_PRINT("CAN: Got measurements from DIS: ");
            switch(RxMsg.data[0]){              // measurement block ID -> update data which the message is referencing, I may implement more cases in the future which is why switch is there
              case 0x0B:  {             // 0x0B references coolant temps
                DEBUG_PRINT("coolant\n");
                int CAN_data_coolant=RxMsg.data[5]-40;
                snprintf(voltage_buffer, sizeof(voltage_buffer), " ");
                snprintf(coolant_buffer, sizeof(coolant_buffer), "Coolant temp: %d%c%cC   ", CAN_data_coolant, 0xC2, 0xB0);
                setFlag(CAN_coolant_recvd);
                break;
              }
              case 0x0E: {
                DEBUG_PRINT("speed\n");
                int CAN_data_speed=(RxMsg.data[2]<<8 | RxMsg.data[3]);
                CAN_data_speed/=128;
                snprintf(speed_buffer, sizeof(speed_buffer), "%d km/h    ", CAN_data_speed);
                setFlag(CAN_speed_recvd);
                break;
              }
              case 0x13: {    // reading voltage from display, research courtesy of @xymetox
                DEBUG_PRINT("battery voltage\n");
                float CAN_data_voltage=RxMsg.data[6];
                CAN_data_voltage/=10;
                snprintf(voltage_buffer, sizeof(voltage_buffer), "Voltage: %.1f V  ", CAN_data_voltage);
                setFlag(CAN_voltage_recvd);
                break;
              }
              default:    break;
            }
            if(checkFlag(CAN_voltage_recvd) && checkFlag(CAN_coolant_recvd) && checkFlag(CAN_speed_recvd)){
              clearFlag(CAN_voltage_recvd);
              clearFlag(CAN_coolant_recvd);
              clearFlag(CAN_speed_recvd);
              setFlag(CAN_new_dataSet_recvd);
            }
            xSemaphoreGive(BufferSemaphore);  // let the message processing continue
          }
        break;
      }
      case 0x548: {                             // AC measurement blocks
          if(disp_mode==1 || disp_mode==2) xSemaphoreTake(BufferSemaphore, portMAX_DELAY);    // if we're in body data mode, take the semaphore to prevent the buffer being modified while the display message is being compiled
          DEBUG_PRINT("CAN: Got measurements from ECC: ");
          switch(RxMsg.data[0]){              // measurement block ID -> update data which the message is referencing
            case 0x07:  {             // 0x07 references battery voltage
              if(!badVoltage_VectraC_bypass){
                float CAN_data_voltage=RxMsg.data[2];
                CAN_data_voltage/=10;
                if(CAN_data_voltage>9 && CAN_data_voltage<16){
                  snprintf(voltage_buffer, sizeof(voltage_buffer), "Voltage: %.1f V  ", CAN_data_voltage);
                } else {            // we get erroneous readings, as such we'll switch to reading from display on the next measurement request
                  badVoltage_VectraC_bypass=1;
                }
                setFlag(CAN_voltage_recvd);
                DEBUG_PRINT("battery voltage\n");
              } else {
                xQueueSend(canTxQueue, &Msg_VoltageRequestDIS, pdMS_TO_TICKS(100));   // request just the voltage from Vectra's display because ECC voltage reading is erratic compared to Astra/Zafira
              }
              break;
            }
            case 0x10:  {             // 0x10 references coolant temps
              unsigned short raw_coolant=(RxMsg.data[3]<<8 | RxMsg.data[4]);
              float CAN_data_coolant=raw_coolant;
              CAN_data_coolant/=10;
              snprintf(coolant_buffer, sizeof(coolant_buffer), "Coolant temp: %.1f%c%cC   ", CAN_data_coolant, 0xC2, 0xB0);
              setFlag(CAN_coolant_recvd);
              DEBUG_PRINT("coolant\n");
              break;
            }
            case 0x11:  {             // 0x11 references RPMs and speed
              int CAN_data_rpm=(RxMsg.data[1]<<8 | RxMsg.data[2]);
              int CAN_data_speed=RxMsg.data[4];
              snprintf(speed_buffer, sizeof(speed_buffer), "%d km/h %d RPM     ", CAN_data_speed, CAN_data_rpm);
              setFlag(CAN_speed_recvd);
              DEBUG_PRINT("speed and RPMs\n");
              break; 
            }
            default:    break;
          }
          if(checkFlag(CAN_voltage_recvd) && checkFlag(CAN_coolant_recvd) && checkFlag(CAN_speed_recvd)){
            clearFlag(CAN_voltage_recvd);
            clearFlag(CAN_coolant_recvd);
            clearFlag(CAN_speed_recvd);
            setFlag(CAN_new_dataSet_recvd);
          }
          if(disp_mode==1 || disp_mode==2) xSemaphoreGive(BufferSemaphore);  // let the message processing continue
        break;
      }
      case 0x6C1: {                                         // radio requests a display update
        if(!checkFlag(a2dp_started)){
          setFlag(ehu_started);                    // start the bluetooth A2DP service after first radio display call
          disp_mode=0;
          vTaskResume(canMessageDecoderTaskHandle);       // begin decoding data from the display
        } else if(checkFlag(a2dp_started) && !checkFlag(ehu_started)){
          a2dp_sink.reconnect();
          setFlag(ehu_started);
        }
        if(disp_mode==0){      // if not a consecutive frame, then we queue that data for decoding by another task
          for(int i=1; i<=7; i++){
            xQueueSend(canDispQueue, &RxMsg.data[i], portMAX_DELAY);    // send a continuous byte stream
          }
        }
        xTaskNotifyGive(canWatchdogTaskHandle);    // reset the watchdog
        break;
      }
      case 0x6C8: {
        if(!checkFlag(ECC_present)) setFlag(ECC_present);            // adjust ISO 15765-2 frame spacing delay only if the receiving node calls for it
        break;
      }
      default:    break;
    }
  }
}

// this task receives CAN messages from canTxQueue and transmits them asynchronously
void canTransmitTask(void *pvParameters){
  static twai_message_t TxMessage;
  int alert_result;
  while(1){
    xQueueReceive(canTxQueue, &TxMessage, portMAX_DELAY);
    TxMessage.extd=0;
    TxMessage.rtr=0;
    TxMessage.ss=0;
    TxMessage.self=0;
    //DEBUG_PRINTF("%03X # %02X %02X %02X %02X %02X %02X %02X %02X", TxMessage.identifier, TxMessage.data[0], TxMessage.data[1], TxMessage.data[2], TxMessage.data[3], TxMessage.data[4], TxMessage.data[5], TxMessage.data[6], TxMessage.data[7]);
    if(twai_transmit(&TxMessage, pdMS_TO_TICKS(50))==ESP_OK) {
      //DEBUG_PRINT(" Q:OK ");
    } else {
      //DEBUG_PRINT("Q:FAIL ");
      setFlag(CAN_prevTxFail);
      if(TxMessage.identifier==displayMsgIdentifier && (TxMessage.data[0]==0x10 || TxMessage.data[0]==0x11)) setFlag(CAN_abortMultiPacket);
    }
    alert_result=twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(10));    // read stats
    if(alert_result==ESP_OK){
        //DEBUG_PRINT("AR:OK ");
      if(alerts_triggered & TWAI_ALERT_TX_SUCCESS){
        if(TxMessage.identifier==displayMsgIdentifier && (TxMessage.data[0]==0x10 || TxMessage.data[0]==0x11)) setFlag(CAN_MessageReady); // let the display task know that the first frame has been transmitted and we're expecting flow control (0x2C1) frame now
        //DEBUG_PRINTLN("TX:OK ");
      } else {
        DEBUG_PRINTLN("TX:FAIL ");
        setFlag(CAN_prevTxFail);
      }
    } else {
        setFlag(CAN_prevTxFail);
        if(TxMessage.identifier==displayMsgIdentifier && (TxMessage.data[0]==0x10 || TxMessage.data[0]==0x11)) setFlag(CAN_abortMultiPacket);
        DEBUG_PRINT("AR:FAIL:");
      if(alert_result==ESP_ERR_INVALID_ARG){
        DEBUG_PRINTLN("INV_ARG");
      }
      if(alert_result==ESP_ERR_INVALID_STATE){
        DEBUG_PRINTLN("INV_STATE");
      }
      if(alert_result==ESP_ERR_TIMEOUT){
        DEBUG_PRINTLN("TIMEOUT");
      }
    }
  }
}

#ifdef DEBUG
char* split_text[3];
char usage_stats[512];

bool checkMutexState(){
  if(xSemaphoreTake(CAN_MsgSemaphore, pdMS_TO_TICKS(1))==pdTRUE){
    xSemaphoreGive(CAN_MsgSemaphore);
    return 0;
  } else {
    return 1;
  }
}

void CANsimTask(void *pvParameters){
  while(1){
    if(Serial.available()>0){
      Serial.print("SERIAL: Action - ");
      char serial_input=Serial.read();
      switch(serial_input){
        case '2': Serial.print("UP\n");
                  xQueueSend(canTxQueue, &simulate_scroll_up, portMAX_DELAY);
                  break;
        case '8': Serial.print("DOWN\n");
                  xQueueSend(canTxQueue, &simulate_scroll_down, portMAX_DELAY);
                  break;
        case '6': Serial.print("UP\n");
                  xQueueSend(canTxQueue, &simulate_scroll_up, portMAX_DELAY);
                  break;
        case '4': Serial.print("DOWN\n");
                  xQueueSend(canTxQueue, &simulate_scroll_down, portMAX_DELAY);
                  break;
        case '5': Serial.print("PRESS\n");
                  xQueueSend(canTxQueue, &simulate_scroll_press, portMAX_DELAY);
                  vTaskDelay(pdMS_TO_TICKS(100));
                  xQueueSend(canTxQueue, &simulate_scroll_release, portMAX_DELAY);
                  break;
        case 'd': {
          Serial.print("CURRENT FLAGS CAN: ");
          Serial.printf("CAN_MessageReady=%d CAN_prevTxFail=%d, DIS_forceUpdate=%d, ECC_present=%d, ehu_started=%d \n", checkFlag(CAN_MessageReady), checkFlag(CAN_prevTxFail), checkFlag(DIS_forceUpdate), checkFlag(ECC_present), checkFlag(ehu_started));
          Serial.print("CURRENT FLAGS BODY: ");
          Serial.printf("CAN_voltage_recvd=%d CAN_coolant_recvd=%d, CAN_speed_recvd=%d, CAN_new_dataSet_recvd=%d \n", checkFlag(CAN_voltage_recvd), checkFlag(CAN_coolant_recvd), checkFlag(CAN_speed_recvd), checkFlag(CAN_new_dataSet_recvd));
          Serial.print("TIME AND STUFF: ");
          Serial.printf("last_millis_req=%lu last_millis_disp=%lu, millis=%lu \n", last_millis_req, last_millis_disp, millis());
          Serial.printf("CanMsgSemaphore state: %d \n", checkMutexState());
          Serial.printf("TxQueue items: %d, RxQueue items: %d \n", uxQueueMessagesWaiting(canTxQueue), uxQueueMessagesWaiting(canRxQueue));
          break;
        }
        case 'T': {             // print arbitrary text from serial to the display, max char count for each line is 35
          char inputBuffer[256];
          int bytesRead=Serial.readBytesUntil('\n', inputBuffer, 256);
          inputBuffer[bytesRead]='\0';
          serialStringSplitter(inputBuffer);
          disp_mode=0;
          writeTextToDisplay(1, split_text[0], split_text[1], split_text[2]);
          break;
        }
        case 'C': {
          prefs_clear();
          break;
        }
        default: break;
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void serialStringSplitter(char* input){
  char* text_in;
  int text_count=0;
  text_in=strtok(input, "|");
  while(text_in!=NULL && text_count<3){
    split_text[text_count]=text_in;
    text_count++;
    text_in=strtok(NULL, "|");
  }
  while(text_count<3){
    split_text[text_count]="";
    text_count++;
  }
}
#endif

// this task implements ISO 15765-2 (multi-packet transmission over CAN frames) in a crude, but hopefully robust way in order to send frames to the display
void canDisplayTask(void *pvParameters){
  static twai_message_t MsgToTx;
  MsgToTx.identifier=displayMsgIdentifier;
  MsgToTx.data_length_code=8;
  uint32_t notifResult;
  bool retryTx=0;
  while(1){
    retryTx=0;
    if(xSemaphoreTake(CAN_MsgSemaphore, portMAX_DELAY)==pdTRUE){          // if the buffer is being accessed, block indefinitely
      if(checkFlag(CAN_flowCtlFail)){
        vTaskDelay(pdMS_TO_TICKS(300));   // since we failed at flow control, wait for the radio to finish its business
      }
      clearFlag(CAN_prevTxFail);
      clearFlag(CAN_abortMultiPacket);      // new transmission, we clear these
      memcpy(MsgToTx.data, CAN_MsgArray[0], 8);
      xQueueSend(canTxQueue, &MsgToTx, portMAX_DELAY);
      DEBUG_PRINTLN("CAN: Now waiting for flow control frame...");
      if(xTaskNotifyWait(0, 0, NULL, portMAX_DELAY)==pdPASS){            // blocking execution until flow control from display arrives
        DEBUG_PRINTLN("CAN: Got flow control! Sending consecutive frames...");
        for(int i=1;i<64 && (CAN_MsgArray[i][0]!=0x00 && !checkFlag(CAN_prevTxFail) && !checkFlag(CAN_abortMultiPacket));i++){                 // this loop will stop sending data once the next packet doesn't contain a label
          memcpy(MsgToTx.data, CAN_MsgArray[i], 8);
          xQueueSend(canTxQueue, &MsgToTx, portMAX_DELAY);
          vTaskDelay(pdMS_TO_TICKS(canISO_frameSpacing));         // receiving node can request a variable frame spacing time, we take it into account here, so far I've seen BID request 2ms while GID/CID request 0ms (no delay)
        }
        clearFlag(CAN_MessageReady);  // clear this as fast as possible once we're done sending
        if(checkFlag(CAN_prevTxFail) || checkFlag(CAN_abortMultiPacket)){
          retryTx=1;                          // "queue up" to restart this task since something went wrong
          clearFlag(CAN_prevTxFail);
        }
        xTaskNotifyStateClear(NULL);
      } else {                      // fail 
        DEBUG_PRINTLN("CAN: Flow control frame has not been received in time, aborting");
        clearFlag(CAN_MessageReady);
        retryTx=1;
      }
      xSemaphoreGive(CAN_MsgSemaphore);           // release the semaphore
    }
    if(!retryTx) vTaskSuspend(NULL);   // have the display task stop itself only if there is no need to retransmit, else run once again
  }
}

// this task provides asynchronous simulation of button presses on the AC panel, quickly toggling AC
void canAirConMacroTask(void *pvParameters){
  while(1){
    vTaskDelay(pdMS_TO_TICKS(500));    // initial delay has to be extended, in some cases 100ms was not enough to have the AC menu appear on the screen, resulting in the inputs being dropped and often entering the vent config instead
    xQueueSend(canTxQueue, &Msg_ACmacro_down, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_press, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_release, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_up, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_up, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_press, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_release, portMAX_DELAY);
    vTaskSuspend(NULL);
  }
}

// this task monitors raw data contained within messages sent by the radio and looks for Aux string being printed to the display; rejects "Aux" in views such as "Audio Source" screen (CD70/DVD90)
void canMessageDecoder(void *pvParameters){
  uint8_t rxDisplay;
  const uint8_t AuxPattern[8]={0x00, 0x6D, 0x00, 0x41, 0x00, 0x75, 0x00, 0x78};     // snippet of data to look for, allows for robust detection of "Aux" on all kinds of headunits
  int patternIndex=0;
  bool patternFound=0;
  int currentIndex[6] = {0};
  const char patterns[6][17] = {           // this is a crutch for CD30/CD40 "SOUND" menu, required to be able to adjust fader/balance/bass/treble, otherwise EHU32 will block it from showing up
    {0, 0x6D, 0, 0x41, 0, 0x75, 0, 0x78},                // formatted Aux (left or center aligned). Weird formatting because the data is in UTF-16
    {0x46, 0, 0x61, 0, 0x64, 0, 0x65, 0, 0x72},                                      // Fader
    {0x42, 0, 0x61, 0, 0x6c, 0, 0x61, 0, 0x6e, 0, 0x63, 0, 0x65},                    // Balance
    {0x42, 0, 0x61, 0, 0x73, 0, 0x73},                                               // Bass
    {0x54, 0, 0x72, 0, 0x65, 0, 0x62, 0, 0x6c, 0, 0x65},                             // Treble
    {0x53, 0, 0x6f, 0, 0x75, 0, 0x6e, 0, 0x64, 0, 0x20, 0, 0x4f, 0, 0x66, 0, 0x66}   // Sound Off
  };
  const char patternLengths[6] = {8, 9, 13, 7, 11, 17};
  while(1){
    if(xQueueReceive(canDispQueue, &rxDisplay, portMAX_DELAY)==pdTRUE){         // wait for new data queued by the ProcessTask
      for(int i=0;i<6; i++){
          if(rxDisplay==patterns[i][currentIndex[i]]){
            currentIndex[i]++;
            if(currentIndex[i]==patternLengths[i]){
              switch(i){
                case 0:{          // formatting+Aux detected
                  patternFound=1;
                  last_millis_aux=millis();             // keep track of when was the last time Aux has been seen
                  DEBUG_PRINTLN("CAN Decode: Found Aux string!");
                  break;
                }
                case 1:   // either Fader, Balance, Bass, Treble or Sound Off
                case 2:
                case 3:
                case 4:
                case 5: patternFound=0;
                        clearFlag(CAN_allowAutoRefresh);        // we let the following message(s) through
              }
              for(int j=0; j<6; j++){
                  currentIndex[j]=0;
              }
              break;
            }
          } else {
            currentIndex[i] = 0;                  // no match, start anew
            if (rxDisplay == patterns[i][0]) {
              currentIndex[i] = 1;
            }
          }
      }
    }
    if(checkFlag(CAN_allowAutoRefresh) && !patternFound && (last_millis_aux+6000<millis())){
      clearFlag(CAN_allowAutoRefresh);    // Aux string has not appeared within the last 6 secs -> stop auto-updating the display
      DEBUG_PRINTLN("CAN Decode: Disabling display autorefresh...");
    } else {
      if(patternFound && !checkFlag(CAN_allowAutoRefresh)){
        setFlag(CAN_allowAutoRefresh);
        setFlag(DIS_forceUpdate);        // gotta force a buffer update here anyway since the metadata might be outdated (wouldn't wanna reprint old audio metadata right?)
        DEBUG_PRINTLN("CAN Decode: Enabling display autorefresh...");
      }
      patternFound=0;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// loads formatted UTF16 data into CAN_MsgArray and labels the messages; needs to know how many bytes to load into the array; afterwards this array is ready to be transmitted with sendMultiPacket()
void prepareMultiPacket(int bytesProcessed, char* buffer_to_read){             // longer CAN messages are split into appropriately labeled (the so called PCI) packets, starting with 0x21 up to 0x2F, then rolling back to 0x20
  int packetCount=bytesProcessed/7, bytesToProcess=bytesProcessed%7;
  unsigned char frameIndex=0x20;
  for(int i=0; i<packetCount; i++){
    CAN_MsgArray[i][0]=frameIndex;
    frameIndex=(frameIndex==0x2F)? 0x20: frameIndex+1;             // reset back to 0x20 after 0x2F, otherwise increment the frameIndex (consecutive frame index)
    memcpy(&CAN_MsgArray[i][1], &buffer_to_read[i*7], 7);       // copy 7 bytes at a time to fill consecutive frames with data
  }
  if(bytesProcessed<=255){
    CAN_MsgArray[0][0]=0x10;            // first frame index is always 0x10 for datasets smaller than 255 bytes
  } else {
    CAN_MsgArray[0][0]=0x11;
  }
  if(bytesToProcess>0){                 // if there are bytes left to be processed but are not enough for a complete message, process them now
    CAN_MsgArray[packetCount][0]=frameIndex;
    memcpy(&CAN_MsgArray[packetCount][1], &buffer_to_read[packetCount*7], bytesToProcess);
    packetCount++;
  }
  CAN_MsgArray[packetCount+1][0]=0x0;      // remove the next frame label if there was any, as such it will not be transmitted
}

// function to queue a frame requesting measurement blocks
void requestMeasurementBlocks(){
  DEBUG_PRINT("CAN: Requesting measurements from ");
  if(checkFlag(ECC_present)){              // request measurement blocks from the climate control module
    DEBUG_PRINTLN("climate control...");
    xQueueSend(canTxQueue, &Msg_MeasurementRequestECC, portMAX_DELAY);
  } else {
    DEBUG_PRINTLN("display...");
    xQueueSend(canTxQueue, &Msg_MeasurementRequestDIS, portMAX_DELAY);        // fallback if ECC is not present, then we read reduced data from the display
  }
}

// function to queue a frame requesting just the coolant data
void requestCoolantTemperature(){
  DEBUG_PRINT("CAN: Requesting coolant temperature from ");
  if(checkFlag(ECC_present)){              // request measurement blocks from the climate control module
    DEBUG_PRINTLN("climate control...");
    xQueueSend(canTxQueue, &Msg_CoolantRequestECC, portMAX_DELAY);
  } else {
    DEBUG_PRINTLN("display...");
    xQueueSend(canTxQueue, &Msg_CoolantRequestDIS, portMAX_DELAY);        // fallback if ECC is not present, then we read reduced data from the display
  }
}

// below functions are used to add additional functionality to longpresses on the radio panel
void canActionEhuButton0(bool btn_state, unsigned int btn_ms_held){         // do not use for CD30! it does not have a "0" button
}

// regular audio metadata mode
void canActionEhuButton1(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode!=0 && btn_ms_held>=500){
    disp_mode=0;   // we have to check whether the music is playing, else the buffered song title just stays there
    setFlag(DIS_forceUpdate);
  }
}

// measurement mode type 1, printing speed+rpm, coolant and voltage from measurement blocks
void canActionEhuButton2(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode!=1 && btn_ms_held>=500){
    clearFlag(CAN_new_dataSet_recvd);
    disp_mode=1;
    setFlag(disp_mode_changed);
    DEBUG_PRINTLN("DISP_MODE: Switching to vehicle data...");
  }
}

// measurement mode type 2, printing coolant from measurement blocks
void canActionEhuButton3(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode!=2 && btn_ms_held>=500){
    clearFlag(CAN_new_dataSet_recvd);
    disp_mode=2;
    setFlag(disp_mode_changed);
    DEBUG_PRINTLN("DISP_MODE: Switching to 1-line coolant...");
  }
}

// no action
void canActionEhuButton4(bool btn_state, unsigned int btn_ms_held){
}

// no action
void canActionEhuButton5(bool btn_state, unsigned int btn_ms_held){
}

// no action
void canActionEhuButton6(bool btn_state, unsigned int btn_ms_held){
}

// no action
void canActionEhuButton7(bool btn_state, unsigned int btn_ms_held){
}

// Start OTA to allow updating over Wi-Fi
void canActionEhuButton8(bool btn_state, unsigned int btn_ms_held){
  if(!checkFlag(OTA_begin) && btn_ms_held>=1000){
    setFlag(OTA_begin);
  } else {
    if(btn_ms_held>=5000) setFlag(OTA_abort);
  }
}

// holding the button for half a second disables EHU32 influencing the screen in any way, holding it for 5 whole seconds clears any saved settings and hard resets the ESP32
void canActionEhuButton9(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode!=-1 && btn_ms_held>=500){
    disp_mode=-1;
    DEBUG_PRINTLN("Screen updates disabled");
  }
  if(btn_ms_held>=5000){
    if(!checkFlag(OTA_begin)){
      vTaskDelay(pdMS_TO_TICKS(1000));
      prefs_clear();
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
  }
}