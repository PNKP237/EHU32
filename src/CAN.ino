#include "driver/twai.h"
void OTAhandleTask(void* pvParameters);

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
            if(Recvd_CAN_MSG.data[0]==0x10 && (Recvd_CAN_MSG.data[1]<0x40 || (Recvd_CAN_MSG.data[1]<0x4F && Recvd_CAN_MSG.data[2]==0x50))){       // we check if the total payload of radio's message is small, if yes assume it's an Aux message
              DEBUG_PRINTLN("CAN: Received display update, trying to block");
              twai_transmit(&Msg_PreventDisplayUpdate, pdMS_TO_TICKS(10));  // radio blocking msg has to be transmitted ASAP, which is why we skip the queue
              twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(10));    // read stats
              if(alerts_triggered & TWAI_ALERT_TX_SUCCESS){
                CAN_flowCtlFail=0;
                DEBUG_PRINTLN("CAN: Blocked successfully");
              } else {
                CAN_flowCtlFail=1;
                DEBUG_PRINTLN("CAN: Blocking failed!");
              }
              if(disp_mode==0) vTaskResume(canDisplayTaskHandle); // only retransmit the msg for audio metadata mode
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
        if(bt_connected && RxMsg.data[0]==0x0){
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
        if((RxMsg.data[0]==0x0) && (RxMsg.data[1]==0x17) && (RxMsg.data[2]<0x02)){          // FIXME!
          vTaskResume(canAirConMacroTaskHandle);   // start AC macro
        }
        break;
      }
      case 0x2C1: if(CAN_MessageReady) xTaskNotifyGive(canDisplayTaskHandle);  // let the display update task know that the data is ready to be transmitted
                  break;
      case 0x501: {                                         // CD30MP3 goes to sleep -> disable bluetooth connectivity
        if(a2dp_started && RxMsg.data[3]==0x18){
          a2dp_shutdown();
        }
        break;
      }
      case 0x546: {                             // display measurement blocks (used as a fallback)
          switch(RxMsg.data[0]){              // measurement block ID -> update data which the message is referencing
            case 0x0B:  {             // 0x0B references coolant temps
              int CAN_data_coolant=RxMsg.data[5]-40;
              snprintf(voltage_buffer, sizeof(voltage_buffer), "No additional data available");
              snprintf(coolant_buffer, sizeof(coolant_buffer), "Coolant temp: %d%c%cC   ", CAN_data_coolant, 0xC2, 0xB0);
              //snprintf(speed_buffer, sizeof(speed_buffer), "ECC not present"); // -> speed received as part of the 0x4E8 msg
              CAN_coolant_recvd=1;
              break;
            }
            default:    break;
          }
          if(CAN_coolant_recvd && CAN_speed_recvd){
            CAN_speed_recvd=0;
            CAN_coolant_recvd=0;
            CAN_new_dataSet_recvd=1;
          }
        break;
      }
      case 0x548: {                             // AC measurement blocks
          switch(RxMsg.data[0]){              // measurement block ID -> update data which the message is referencing
            case 0x07:  {             // 0x10 references battery voltage
              CAN_data_voltage=RxMsg.data[2];
              CAN_data_voltage/=10;
              snprintf(voltage_buffer, sizeof(voltage_buffer), "Voltage: %.1f V  ", CAN_data_voltage);
              CAN_voltage_recvd=1;
              break;
            }
            case 0x10:  {             // 0x10 references coolant temps
              unsigned short raw_coolant=(RxMsg.data[3]<<8 | RxMsg.data[4]);
              CAN_data_coolant=raw_coolant;
              CAN_data_coolant/=10;
              snprintf(coolant_buffer, sizeof(coolant_buffer), "Coolant temp: %.1f%c%cC   ", CAN_data_coolant, 0xC2, 0xB0);
              CAN_coolant_recvd=1;
              break;
            }
            case 0x11:  {             // 0x11 references RPMs and speed
              CAN_data_rpm=(RxMsg.data[1]<<8 | RxMsg.data[2]);
              CAN_data_speed=RxMsg.data[4];
              snprintf(speed_buffer, sizeof(speed_buffer), "%d km/h %d RPM     ", CAN_data_speed, CAN_data_rpm);
              CAN_speed_recvd=1;
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
        break;
      }
      case 0x4E8: {                               // this provides speed and RPMs right from the bus
        if(disp_mode==1 && !ECC_present){
          CAN_data_rpm=(RxMsg.data[2]<<8 | RxMsg.data[3]);
          CAN_data_speed=RxMsg.data[4];
          snprintf(speed_buffer, sizeof(speed_buffer), "%d km/h %d RPM     ", CAN_data_speed, CAN_data_rpm);
          CAN_speed_recvd=1;
        }
        break;
      }
      case 0x6C1: {                                         // radio requests a display update
        if(!a2dp_started){                                // if the total payload is less than 0x40 bytes the message will not be sent (so FM radio or other stuff isn't overwritten)
          ehu_started=1;                    // start the bluetooth A2DP service after first radio display call
          disp_mode=0;
        } else if(a2dp_started && !ehu_started){
          a2dp_sink.reconnect();
          ehu_started=1;
        }
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
  int TxSuccess=0;
  while(1){
    xQueueReceive(canTxQueue, &TxMessage, portMAX_DELAY);
    TxMessage.extd=0;
    TxMessage.rtr=0;
    TxMessage.ss=0;
    TxMessage.self=0;
    DEBUG_PRINTF("%03X # %02X %02X %02X %02X %02X %02X %02X %02X", TxMessage.identifier, TxMessage.data[0], TxMessage.data[1], TxMessage.data[2], TxMessage.data[3], TxMessage.data[4], TxMessage.data[5], TxMessage.data[6], TxMessage.data[7]);
    if(twai_transmit(&TxMessage, pdMS_TO_TICKS(10)) == ESP_OK) {
      DEBUG_PRINT(" Q:OK ");
    } else {
      DEBUG_PRINT("Q:FAIL ");
      TxSuccess=0;
      CAN_prevTxFail=1;
    }
    int alert_result=twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(10));    // read stats
    if(alert_result==ESP_OK){
        DEBUG_PRINT("AR:OK ");
      if(alerts_triggered & TWAI_ALERT_TX_SUCCESS){
        DEBUG_PRINTLN("TX:OK ");
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
        case 'S': {
          Serial.print("Simulating CAN msgs");
          twai_message_t FakeMeasurementMsg;
          FakeMeasurementMsg.identifier=0x548;
          FakeMeasurementMsg.data[0]=0x07;
          FakeMeasurementMsg.data[2]=144;
          xQueueSend(canRxQueue, &FakeMeasurementMsg, portMAX_DELAY);
          FakeMeasurementMsg.data[0]=0x10;
          FakeMeasurementMsg.data[3]=0x25;
          FakeMeasurementMsg.data[4]=0x10;
          xQueueSend(canRxQueue, &FakeMeasurementMsg, portMAX_DELAY);
          FakeMeasurementMsg.data[0]=0x11;
          FakeMeasurementMsg.data[1]=0x25;
          FakeMeasurementMsg.data[2]=0x10;
          FakeMeasurementMsg.data[4]=0x25;
          xQueueSend(canRxQueue, &FakeMeasurementMsg, portMAX_DELAY);
          break;
        }
        case 'd': {
          Serial.print("CURRENT FLAGS CAN: ");
          Serial.printf("CAN_MessageReady=%d CAN_prevTxFail=%d, DIS_forceUpdate=%d, ECC_present=%d \n", CAN_MessageReady, CAN_prevTxFail, DIS_forceUpdate, ECC_present);
          Serial.print("CURRENT FLAGS BODY: ");
          Serial.printf("CAN_voltage_recvd=%d CAN_coolant_recvd=%d, CAN_speed_recvd=%d, CAN_new_dataSet_recvd=%d \n", CAN_voltage_recvd, CAN_coolant_recvd, CAN_speed_recvd, CAN_new_dataSet_recvd);
          Serial.printf("CanMsgSemaphore state: %d \n", checkMutexState());
          Serial.printf("TxQueue items: %d, RxQueue items: %d \n", uxQueueMessagesWaiting(canTxQueue), uxQueueMessagesWaiting(canRxQueue));
          break;
        }
        default: break;
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
#endif

// this task waits for a flow control packet from the display
void canDisplayTask(void *pvParameters){
  while(1){
    if(xSemaphoreTake(CAN_MsgSemaphore, pdMS_TO_TICKS(100))==pdTRUE){          // if the buffer is being accessed, block indefinitely
      if(CAN_flowCtlFail){                 // failed transmitting flow control before the display resulting in an error frame, wait for a bit before sending it again, skip queue
        vTaskDelay(pdMS_TO_TICKS(1));
        twai_transmit(&Msg_PreventDisplayUpdate, pdMS_TO_TICKS(10));  // hope for the best and send flow control again
        CAN_flowCtlFail=0;
        vTaskDelay(pdMS_TO_TICKS(20));
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      sendMultiPacket();                    // send multi packet is blocking
      vTaskDelay(pdMS_TO_TICKS(2));
      if(CAN_prevTxFail){      // if sendMultiPacket failed, resend
        sendMultiPacket();
      }
      //xEventGroupWaitBits(CAN_Events, CAN_MessageReady, pdFALSE, pdFALSE, portMAX_DELAY);  // this waits until CAN_MessageReady is set by the transmit function (only in case of a successful TX)
      if(CAN_MessageReady && !CAN_prevTxFail){  // possibly not needed anymore?        
        DEBUG_PRINTLN("CAN: Now waiting for 2C1...");
        xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);      // waiting for a notification from the canProcessTask once a flow control frame is received
        sendMultiPacketData();
        xTaskNotifyStateClear(NULL);
      } else {
        vTaskDelay(10);
      }
      xSemaphoreGive(CAN_MsgSemaphore);           // release the semaphore
    }
    vTaskSuspend(NULL);
  }
}

// this task provides asynchronous simulation of button presses on the AC panel, quickly toggling AC
void canAirConMacroTask(void *pvParameters){
  while(1){
    vTaskDelay(100);
    xQueueSend(canTxQueue, &Msg_ACmacro_down, portMAX_DELAY);
    vTaskDelay(100);
    xQueueSend(canTxQueue, &Msg_ACmacro_press, portMAX_DELAY);
    vTaskDelay(100);
    xQueueSend(canTxQueue, &Msg_ACmacro_release, portMAX_DELAY);
    vTaskDelay(100);
    xQueueSend(canTxQueue, &Msg_ACmacro_up, portMAX_DELAY);
    vTaskDelay(100);
    xQueueSend(canTxQueue, &Msg_ACmacro_up, portMAX_DELAY);
    vTaskDelay(100);
    xQueueSend(canTxQueue, &Msg_ACmacro_press, portMAX_DELAY);
    vTaskDelay(100);
    xQueueSend(canTxQueue, &Msg_ACmacro_release, portMAX_DELAY);
    vTaskSuspend(NULL);
  }
}

// loads formatted UTF16 data into CAN_MsgArray and labels the messages; needs to know how many bytes to load into the array; afterwards this array is ready to be transmitted with sendMultiPacket()
void prepareMultiPacket(int bytes_processed, char* buffer_to_read){             // longer CAN messages are split into appropriately labeled packets, starting with 0x21 up to 0x2F, then rolling back to 0x20
  for(int i=1; i<64; i++){          // only the message labels need clearing, leftover data in the buffer is fine - even factory radios do that
    CAN_MsgArray[i][0]=0x00;
  }
  CAN_MsgArray[0][0]=0x10;                      // this is a special case
  int bytecounter=0, packetcounter=0;
  for(int i=0;i<64 && bytecounter<bytes_processed; i++){              // fill the rest of the packets with data from utf16buffer
    for(int j=1;j<8 && bytecounter<bytes_processed;j++){
      CAN_MsgArray[i][j]=buffer_to_read[bytecounter];                              // read split UTF-16 chars into the array byte by byte
      bytecounter++;
      //DEBUG_PRINTF("Processing message: Packet i=%d, byte j=%d, byte number=%d out of %d \n", i, j, bytecounter, bytes_processed);
    }
    if(i!=0){   // 1st packet requires 0x10;
      CAN_MsgArray[i][0]=0x21+packetcounter;                                     // label consecutive packets appropriately, rolls back to 0x20 in case of long payloads
      if(CAN_MsgArray[i][0]==0x2F){
        packetcounter=-1;                         // message label has to roll over to 0x20
      } else {
        packetcounter++;
      }
    }
  }
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
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  DEBUG_PRINTLN();
  if(CAN_prevTxFail){
    DIS_forceUpdate=1;
    CAN_prevTxFail=0;
  }
}

// function to queue a frame requesting measurement blocks
void requestMeasurementBlocks(){
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
    disp_mode=0;
    DIS_forceUpdate=1;
  }
}

void canActionEhuButton2(){         // printing speed+rpm, coolant and voltage from measurement blocks
  if(disp_mode!=1){
    disp_mode=1;
    disp_mode_changed=1;
    DEBUG_PRINTLN("DISP_MODE: Switching to vehicle data...");
  }
}

void canActionEhuButton3(){
  if(disp_mode!=2){
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