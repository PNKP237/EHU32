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
                      Msg_MeasurementRequestDIS={ .identifier=0x246, .data_length_code=8, .data={0x07, 0xAA, 0x03, 0x01, 0x0B, 0x0B, 0x0B, 0x0B}},
                      Msg_MeasurementRequestECC={ .identifier=0x248, .data_length_code=7, .data={0x06, 0xAA, 0x01, 0x01, 0x07, 0x10, 0x11}};

// can't initialize the values of the union inside the twai_message_t type struct, which is why it's defined here, then the transmit task sets the .ss flag
twai_message_t Msg_PreventDisplayUpdate={ .identifier=0x2C1, .data_length_code=8, .data={0x30, 0x0, 0x7F, 0, 0, 0, 0, 0}};

// initializing CAN communication
void twai_init(){
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_4, TWAI_MODE_NORMAL);         // CAN bus set up
  g_config.rx_queue_len=40;
  g_config.tx_queue_len=5;
  g_config.intr_flags=(ESP_INTR_FLAG_LEVEL1 & ESP_INTR_FLAG_IRAM);
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
  static twai_message_t Recvd_CAN_MSG;
  Msg_PreventDisplayUpdate.extd=0; Msg_PreventDisplayUpdate.ss=1; Msg_PreventDisplayUpdate.self=0; Msg_PreventDisplayUpdate.rtr=0;
  while(1){
    if(twai_receive(&Recvd_CAN_MSG, portMAX_DELAY)==ESP_OK){
      switch(Recvd_CAN_MSG.identifier){
        case 0x6C1: {
          if(disp_mode!=-1){            // don't bother checking the data if there's no need to update the display
            if(Recvd_CAN_MSG.data[0]==0x10 && (Recvd_CAN_MSG.data[2]==0x40 || Recvd_CAN_MSG.data[2]==0xC0) && Recvd_CAN_MSG.data[5]==0x03 && (disp_mode!=0 || CAN_allowDisplay)){       // another task processes the data since we can't do that here
              DEBUG_PRINTLN("CAN: Received display update, trying to block");
              twai_transmit(&Msg_PreventDisplayUpdate, pdMS_TO_TICKS(30));  // radio blocking msg has to be transmitted ASAP, which is why we skip the queue
              twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(10));    // read stats
              if(alerts_triggered & TWAI_ALERT_TX_SUCCESS){
                CAN_flowCtlFail=0;
                DEBUG_PRINTLN("CAN: Blocked successfully");
              } else {
                CAN_flowCtlFail=1;
                DEBUG_PRINTLN("CAN: Blocking failed!");
              }
              if(disp_mode==0 || disp_mode==2) vTaskResume(canDisplayTaskHandle); // only retransmit the msg for audio metadata mode and single line coolant, since these don't update frequently
            }
          }
        }
        case 0x201:
        case 0x206:
        case 0x208:
        case 0x2C1:
        case 0x501:
        case 0x546:
        case 0x548:
        case 0x4E8:
        case 0x6C8:
          xQueueSend(canRxQueue, &Recvd_CAN_MSG, portMAX_DELAY);
          break;
        default: break;
      }
    }
  }
}

// this task processes filtered CAN frames read from canRxQueue
void canProcessTask(void *pvParameters){
  static twai_message_t RxMsg;
  uint8_t payload_size=0, payload_bytes_queued=0, payload_type=0;
  while(1){
    xQueueReceive(canRxQueue, &RxMsg, portMAX_DELAY);     // receives data from the internal queue
    switch(RxMsg.identifier){
      case 0x201: {                                         // radio button decoder
        if(RxMsg.data[0]==0x01 && RxMsg.data[2]>=10){
          switch(RxMsg.data[1]){
            case 0x30:  canActionEhuButton0();      // CD30 has no '0' button!
                        break;
            case 0x31:  canActionEhuButton1();
                        break;
            case 0x32:  canActionEhuButton2();
                        break;
            case 0x33:  canActionEhuButton3();
                        break;
            case 0x34:  canActionEhuButton4();
                        break;
            case 0x35:  canActionEhuButton5();
                        break;
            case 0x36:  canActionEhuButton6();
                        break;
            case 0x37:  canActionEhuButton7();
                        break;
            case 0x38:  canActionEhuButton8();
                        break;
            case 0x39:  canActionEhuButton9();
                        break;
            default: break;
          }
        }
        break;
      }
      case 0x206: {                                  // decodes steering wheel buttons
        if(bt_connected && RxMsg.data[0]==0x0 && CAN_allowDisplay){                     // makes sure "Aux" is displayed, otherwise forward/next buttons will have no effect
          switch(RxMsg.data[1]){
            case 0x81:  if(bt_audio_playing){                 // upper left button (box with waves)
                          a2dp_sink.pause(); 
                        } else {
                          a2dp_sink.play();
                        }
                        break;
            case 0x91:  a2dp_sink.next();                     // upper right button (arrow up)
                        break;
            case 0x92:  a2dp_sink.previous();                 // lower right button (arrow down)
                        break;
            default:    break;
          }          
        }
        break;
      }
      case 0x208: {                               // AC panel button event
        if((RxMsg.data[0]==0x0) && (RxMsg.data[1]==0x17) && (RxMsg.data[2]<0x01)){          // FIXME!
          vTaskResume(canAirConMacroTaskHandle);   // start AC macro
        }
        break;
      }
      case 0x2C1: {
        if(CAN_MessageReady) xTaskNotifyGive(canDisplayTaskHandle);  // let the display update task know that the data is ready to be transmitted
        canISO_frameSpacing=RxMsg.data[2];            // dynamically adjust ISO 15765-2 frame spacing delay
        break;
      }
      case 0x501: {                                         // CD30MP3 goes to sleep -> disable bluetooth connectivity
        if(a2dp_started && RxMsg.data[3]==0x18){
          a2dp_shutdown();
        }
        break;
      }
      case 0x546: {                             // display measurement blocks (used as a fallback)
          if(disp_mode==1 || disp_mode==2){
            xSemaphoreTake(BufferSemaphore, portMAX_DELAY);
            DEBUG_PRINT("CAN: Got measurements from DIS: ");
            switch(RxMsg.data[0]){              // measurement block ID -> update data which the message is referencing, I may implement more cases in the future which is why switch is there
              case 0x0B:  {             // 0x0B references coolant temps
                DEBUG_PRINT("coolant\n");
                int CAN_data_coolant=RxMsg.data[5]-40;
                snprintf(voltage_buffer, sizeof(voltage_buffer), "No additional data available");
                snprintf(coolant_buffer, sizeof(coolant_buffer), "Coolant temp: %d%c%cC   ", CAN_data_coolant, 0xC2, 0xB0);
                //snprintf(speed_buffer, sizeof(speed_buffer), "ECC not present"); // -> speed received as part of the 0x4E8 msg
                CAN_coolant_recvd=1;
                #ifdef DEBUG
                CAN_speed_recvd=1;      // a workaround for my desktop setup (can't sniff the speed from 0x4E8)
                #endif
                break;
              }
              default:    break;
            }
            if(CAN_coolant_recvd && CAN_speed_recvd){
              CAN_speed_recvd=0;
              CAN_coolant_recvd=0;
              CAN_new_dataSet_recvd=1;
            }
            xSemaphoreGive(BufferSemaphore);  // let the message processing continue
          }
        break;
      }
      case 0x548: {                             // AC measurement blocks
          if(disp_mode==1 || disp_mode==2) xSemaphoreTake(BufferSemaphore, portMAX_DELAY);    // if we're in body data mode, take the semaphore to prevent the buffer being modified while the display message is being compiled
          DEBUG_PRINT("CAN: Got measurements from ECC: ");
          switch(RxMsg.data[0]){              // measurement block ID -> update data which the message is referencing
            case 0x07:  {             // 0x10 references battery voltage
              CAN_data_voltage=RxMsg.data[2];
              CAN_data_voltage/=10;
              snprintf(voltage_buffer, sizeof(voltage_buffer), "Voltage: %.1f V  ", CAN_data_voltage);
              CAN_voltage_recvd=1;
              DEBUG_PRINT("battery voltage\n");
              break;
            }
            case 0x10:  {             // 0x10 references coolant temps
              unsigned short raw_coolant=(RxMsg.data[3]<<8 | RxMsg.data[4]);
              CAN_data_coolant=raw_coolant;
              CAN_data_coolant/=10;
              snprintf(coolant_buffer, sizeof(coolant_buffer), "Coolant temp: %.1f%c%cC   ", CAN_data_coolant, 0xC2, 0xB0);
              CAN_coolant_recvd=1;
              DEBUG_PRINT("coolant\n");
              break;
            }
            case 0x11:  {             // 0x11 references RPMs and speed
              CAN_data_rpm=(RxMsg.data[1]<<8 | RxMsg.data[2]);
              CAN_data_speed=RxMsg.data[4];
              snprintf(speed_buffer, sizeof(speed_buffer), "%d km/h %d RPM     ", CAN_data_speed, CAN_data_rpm);
              CAN_speed_recvd=1;
              DEBUG_PRINT("speed and RPMs\n");
              break; 
            }
            default:    break;
          }
          if(CAN_voltage_recvd && CAN_coolant_recvd && CAN_speed_recvd){
            CAN_voltage_recvd=0;
            CAN_coolant_recvd=0;
            CAN_speed_recvd=0;
            CAN_new_dataSet_recvd=1;
          }
          if(disp_mode==1 || disp_mode==2) xSemaphoreGive(BufferSemaphore);  // let the message processing continue
        break;
      }
      case 0x4E8: {                               // this provides speed and RPMs right from the bus, only for 3-line measurement mode (disp_mode 1) IF there's no ECC module detected
        if((disp_mode==1) && !ECC_present){
          if(disp_mode==1) xSemaphoreTake(BufferSemaphore, portMAX_DELAY);
          CAN_data_rpm=(RxMsg.data[2]<<8 | RxMsg.data[3]);
          CAN_data_rpm/=4;                                // realized this thanks to testing done by @KingSilverHaze 
          if(RxMsg.data[6]=0x01){     // vehicle is standing still -> bytes 4 and 5 not updated, assume 0 km/h
            CAN_data_speed=0;           // when not moving, the speed value will not reflect 0 km/h
          } else {
            CAN_data_speed=(RxMsg.data[4]<<8 | RxMsg.data[5]);        // speed is a 16-bit integer multiplied by 128
            CAN_data_speed/=128;
          }
          snprintf(speed_buffer, sizeof(speed_buffer), "%d km/h %d RPM     ", CAN_data_speed, CAN_data_rpm);
          CAN_speed_recvd=1;
          if(disp_mode==1) xSemaphoreGive(BufferSemaphore);  // let the message processing continue
        }
        break;
      }
      case 0x6C1: {                                         // radio requests a display update
        if(!a2dp_started){
          ehu_started=1;                    // start the bluetooth A2DP service after first radio display call
          disp_mode=0;
        } else if(a2dp_started && !ehu_started){
          a2dp_sink.reconnect();
          ehu_started=1;
        }
        if(disp_mode==0){      // if not a consecutive frame, then we queue that data for decoding by another task
          if(RxMsg.data[0]==0x10 && (RxMsg.data[2]==0x40 || RxMsg.data[2]==0xC0)){
            payload_size=RxMsg.data[1]-6;
            payload_bytes_queued=0;         // reset the counter
            payload_type=RxMsg.data[5];     // this is a hack for CD70/DVD90, because they utilize an audio menu, they send messages "under the hood" which don't contain "Aux"
            if(payload_type==0x03) xQueueSend(canDispQueue, &payload_size, portMAX_DELAY);    // queue payload size decreased by 6 since we don't count the 6 bytes in first frame
          } else {
            for(int i=1; i<=7 && payload_bytes_queued<payload_size; i++){
              if(payload_type==0x03) xQueueSend(canDispQueue, &RxMsg.data[i], portMAX_DELAY);    // queue raw payload data, skipping consecutive frame index data
              payload_bytes_queued++;
            }
          }
          if(payload_type==0x03) vTaskResume(canMessageDecoderTaskHandle);           // resuming the task if it is already running is safe
        }
        xTaskNotifyGive(canWatchdogTaskHandle);    // reset the watchdog
        break;
      }
      case 0x6C8: {           // if any ECC module message is received, assume ECC is available to request measurement data from
        if(!ECC_present){
          ECC_present=1;
        }
        break;
      }
      default:    break;
    }
  }
}

// this task receives CAN messages from canTxQueue and transmits them asynchronously
void canTransmitTask(void *pvParameters){
  static twai_message_t TxMessage;
  while(1){
    xQueueReceive(canTxQueue, &TxMessage, portMAX_DELAY);
    TxMessage.extd=0;
    TxMessage.rtr=0;
    TxMessage.ss=0;
    TxMessage.self=0;
    //DEBUG_PRINTF("%03X # %02X %02X %02X %02X %02X %02X %02X %02X", TxMessage.identifier, TxMessage.data[0], TxMessage.data[1], TxMessage.data[2], TxMessage.data[3], TxMessage.data[4], TxMessage.data[5], TxMessage.data[6], TxMessage.data[7]);
    if(twai_transmit(&TxMessage, pdMS_TO_TICKS(50)) == ESP_OK) {
      //DEBUG_PRINT(" Q:OK ");
    } else {
      //DEBUG_PRINT("Q:FAIL ");
      CAN_prevTxFail=1;
    }
    int alert_result=twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(10));    // read stats
    if(alert_result==ESP_OK){
        //DEBUG_PRINT("AR:OK ");
      if(alerts_triggered & TWAI_ALERT_TX_SUCCESS){
        //DEBUG_PRINTLN("TX:OK ");
        if(TxMessage.identifier==0x6C1 && TxMessage.data[0]==0x10){
          CAN_MessageReady=1;
        }
      } else {
        DEBUG_PRINTLN("TX:FAIL ");
      }
    } else {
        CAN_prevTxFail=1;
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
          Serial.printf("CAN_MessageReady=%d CAN_prevTxFail=%d, DIS_forceUpdate=%d, ECC_present=%d, ehu_started=%d \n", CAN_MessageReady, CAN_prevTxFail, DIS_forceUpdate, ECC_present, ehu_started);
          Serial.print("CURRENT FLAGS BODY: ");
          Serial.printf("CAN_voltage_recvd=%d CAN_coolant_recvd=%d, CAN_speed_recvd=%d, CAN_new_dataSet_recvd=%d \n", CAN_voltage_recvd, CAN_coolant_recvd, CAN_speed_recvd, CAN_new_dataSet_recvd);
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
  while(1){
    if(xSemaphoreTake(CAN_MsgSemaphore, portMAX_DELAY)==pdTRUE){          // if the buffer is being accessed, block indefinitely
      if(CAN_flowCtlFail){                 // failed transmitting flow control before the display resulting in an error frame, wait for a bit before sending it again, skip queue
        xQueueSend(canTxQueue, &Msg_PreventDisplayUpdate, portMAX_DELAY); // since we missed the first chance to block, we well do this once again through the usual queue as a normal packet
        CAN_flowCtlFail=0;                                                // Tx task will set the single shot flag to 0, since it'd be highly unlikely to run into another 0x2C1 frame
        vTaskDelay(pdMS_TO_TICKS(2));
      }
      vTaskDelay(pdMS_TO_TICKS(5));     // these delays are incredibly important for the proper operation of the transmission task
      sendMultiPacket();                    // send multi packet is blocking
      vTaskDelay(pdMS_TO_TICKS(2));
      if(CAN_prevTxFail){      // if sendMultiPacket failed, resend
        sendMultiPacket();
      }
      //xEventGroupWaitBits(CAN_Events, CAN_MessageReady, pdFALSE, pdFALSE, portMAX_DELAY);  // this waits until CAN_MessageReady is set by the transmit function (only in case of a successful TX)
      if(CAN_MessageReady && !CAN_prevTxFail){  // possibly not needed anymore? nope, still needed until I'm competent enough to implement EventGroups       
        DEBUG_PRINTLN("CAN: Now waiting for 2C1...");
        xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(100));
        sendMultiPacketData();
        xTaskNotifyStateClear(NULL);
      } else {
        vTaskDelay(10);
      }
      xSemaphoreGive(CAN_MsgSemaphore);           // release the semaphore
    }
    vTaskSuspend(NULL);   // have the display task stop itself
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
  uint8_t rxDisplay, payload_size=0, payload_bytes_received=0;
  const uint8_t AuxPattern[7]={0x6D, 0x00, 0x41, 0x00, 0x75, 0x00, 0x78};     // snippet of data to look for, allows for robust detection of "Aux" on all kinds of headunits
  int patternIndex=0;
  bool patternFound=0;
  while(1){
    xQueueReceive(canDispQueue, &payload_size, portMAX_DELAY);    // after starting from suspended state the first byte should be the payload size
    for(payload_bytes_received=0; payload_bytes_received<payload_size && !patternFound; payload_bytes_received++){   // read bytes as long as there is payload data OR "Aux" has been found
      xQueueReceive(canDispQueue, &rxDisplay, portMAX_DELAY);
      if(rxDisplay==AuxPattern[patternIndex]){
        patternIndex++;
        if(patternIndex==7){
          patternIndex=0;
          patternFound=1;
          DEBUG_PRINTLN("CAN: Found Aux string!");
        }
      } else {                  // start looking for aux once again
        patternIndex=0;
      }
    }
    if(patternFound){
      if(!CAN_allowDisplay){
        CAN_allowDisplay=1;
        DIS_forceUpdate=1;        // gotta force a buffer update here anyway since the metadata might be outdated
      }
      while(payload_bytes_received<payload_size){          // if there is any leftover data, wait for it and discard it, clearing the queue
        xQueueReceive(canDispQueue, &rxDisplay, portMAX_DELAY);
        payload_bytes_received++;
      }
    } else {
      if(CAN_allowDisplay) CAN_allowDisplay=0;
      DEBUG_PRINTLN("CAN: No Aux string here...");
    }
    patternFound=0;
    if(uxQueueMessagesWaiting(canDispQueue)==0){
      vTaskSuspend(NULL);     // suspend the task if there's no new data available, the canProcessTask will start it back up
    } else {
      DEBUG_PRINTLN("CAN: More data to process...");
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

// loads formatted UTF16 data into CAN_MsgArray and labels the messages; needs to know how many bytes to load into the array; afterwards this array is ready to be transmitted with sendMultiPacket()
void prepareMultiPacket(int bytesProcessed, char* buffer_to_read){             // longer CAN messages are split into appropriately labeled packets, starting with 0x21 up to 0x2F, then rolling back to 0x20
  int packetCount=bytesProcessed/7, bytesToProcess=bytesProcessed%7;
  unsigned char frameIndex=0x20;
  for(int i=0; i<packetCount; i++){
    CAN_MsgArray[i][0]=frameIndex;
    frameIndex=(frameIndex==0x2F)? 0x20: frameIndex+1;             // reset back to 0x20 after 0x2F, otherwise increment the frameIndex (consecutive frame index)
    memcpy(&CAN_MsgArray[i][1], &buffer_to_read[i*7], 7);       // copy 7 bytes at a time to fill consecutive frames with data
  }
  CAN_MsgArray[0][0]=0x10;            // first frame index is always 0x10
  if(bytesToProcess>0){                 // if there are bytes left to be processed but are not enough for a complete message, process them now
    CAN_MsgArray[packetCount][0]=frameIndex;
    memcpy(&CAN_MsgArray[packetCount][1], &buffer_to_read[packetCount*7], bytesToProcess);
    packetCount++;
  }
  CAN_MsgArray[packetCount+1][0]=0x0;      // remove the next frame label if there was any, as such it will not be transmitted
}

// sends a message request to the display
void sendMultiPacket(){     // main loop shall decide when to send the following data
  static twai_message_t MsgToTx;
  MsgToTx.identifier=0x6C1;
  MsgToTx.data_length_code=8;
  memcpy(MsgToTx.data, CAN_MsgArray[0], 8);
  CAN_prevTxFail=0;
  xQueueSend(canTxQueue, &MsgToTx, portMAX_DELAY);
}

// sends the rest of the message buffer in quick succession
void sendMultiPacketData(){   // should only be executed after the display acknowledges the incoming transmission
  CAN_MessageReady=0;                   // new buffers can now be prepared as the will be sent
  static twai_message_t MsgToTx;
  MsgToTx.identifier=0x6C1;
  MsgToTx.data_length_code=8;
  for(int i=1;i<64 && (CAN_MsgArray[i][0]!=0x00 && !CAN_prevTxFail);i++){                 // this loop will stop sending data once the next packet doesn't contain a label
    memcpy(MsgToTx.data, CAN_MsgArray[i], 8);
    xQueueSend(canTxQueue, &MsgToTx, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(canISO_frameSpacing));         // receiving node can request a variable frame spacing time, we take it into account here, so far I've seen BID request 2ms while GID/CID request 0ms (no delay)
  }
  DEBUG_PRINTLN();
  if(CAN_prevTxFail){
    DIS_forceUpdate=1;
    CAN_prevTxFail=0;
  }
}

// function to queue a frame requesting measurement blocks
void requestMeasurementBlocks(){
  DEBUG_PRINTLN("CAN: Requesting measurements...");
  if(ECC_present){              // request measurement blocks from the climate control module
    xQueueSend(canTxQueue, &Msg_MeasurementRequestECC, portMAX_DELAY);
  } else {
    xQueueSend(canTxQueue, &Msg_MeasurementRequestDIS, portMAX_DELAY);        // fallback if ECC is not present, then we read reduced data from the display
  }
}

// below functions are used to add additional functionality to longpresses on the radio panel
void canActionEhuButton0(){         // do not use for CD30! it does not have a "0" button
}

void canActionEhuButton1(){         // regular audio metadata mode
  if(disp_mode!=0){
    if(bt_audio_playing) disp_mode=0;   // we have to check whether the music is playing, else we the buffered song title just stays there
    DIS_forceUpdate=1;
  }
}

void canActionEhuButton2(){         // printing speed+rpm, coolant and voltage from measurement blocks
  if(disp_mode!=1){
    CAN_new_dataSet_recvd=0;
    disp_mode=1;
    disp_mode_changed=1;
    DEBUG_PRINTLN("DISP_MODE: Switching to vehicle data...");
  }
}

void canActionEhuButton3(){
  if(disp_mode!=2){
    CAN_new_dataSet_recvd=0;
    disp_mode=2;
    disp_mode_changed=1;
    DEBUG_PRINTLN("DISP_MODE: Switching to 1-line coolant...");
  }
}

void canActionEhuButton4(){
}

void canActionEhuButton5(){
}

void canActionEhuButton6(){
}

void canActionEhuButton7(){
}

void canActionEhuButton8(){
  OTA_begin=1;
}

void canActionEhuButton9(){
  if(disp_mode!=-1){
    disp_mode=-1;
    DEBUG_PRINTLN("Screen updates disabled");
  }
}