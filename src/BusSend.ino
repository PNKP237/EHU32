// transmits a CAN message with specified payload; sets CAN_prevTxFail in case there was a failure at any point
void sendPacket(int id, char can_send_buffer[8], int dlc=8){
  TxMessage.identifier=id;
  TxMessage.data_length_code=dlc;                      // CAN packets set to 8 bytes by default, can be overridden, for emulating button presses and such
  for(int i=0; i<dlc; i++){                             // load data into message, queue the message, then read alerts
    TxMessage.data[i]=can_send_buffer[i];
  }
  if (twai_transmit(&TxMessage, pdMS_TO_TICKS(20)) == ESP_OK) {
    if(DEBUGGING_ON) Serial.print("  Q:OK ");
  } else {
    if(DEBUGGING_ON) Serial.print("Q:FAIL ");
  }

  int alert_result=twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(10));    // read stats
  if(alert_result==ESP_OK){
      if(DEBUGGING_ON) Serial.print("AR:OK ");
    if(alerts_triggered & TWAI_ALERT_TX_SUCCESS){
      if(DEBUGGING_ON) Serial.print("TX:OK ");
    } else {
      if(DEBUGGING_ON) Serial.print("TX:FAIL ");
        CAN_prevTxFail=1;                           // the main loop will try to transmit the message again on the next iteration
    }
  } else {
      if(DEBUGGING_ON) Serial.print("AR:FAIL:");
      CAN_prevTxFail=1;
    if(alert_result==ESP_ERR_INVALID_ARG){
      if(DEBUGGING_ON) Serial.print("INV_ARG");
    }
    if(alert_result==ESP_ERR_INVALID_STATE){
      if(DEBUGGING_ON) Serial.print("INV_STATE");
    }
    if(alert_result==ESP_ERR_TIMEOUT){
      if(DEBUGGING_ON) Serial.print("TIMEOUT");
    }
  }
}

// loads formatted UTF16 data into CAN_MsgArray and labels the messages; needs to know how many bytes to load into the array; afterwards this array is ready to be transmitted with sendMultiPacket()
void prepareMultiPacket(int bytes_processed){             // longer CAN messages are split into appropriately labeled packets, starting with 0x21 up to 0x2F, then rolling back to 0x20
  for(int i=1; i<64; i++){          // only the message labels need clearing, leftover data in the buffer is fine - even factory radios do that
    CAN_MsgArray[i][0]=0x00;
  }
  CAN_MsgArray[0][0]=0x10;                      // this is a special case
  int bytecounter=0, packetcounter=0;
  for(int i=0;i<64 && bytecounter<bytes_processed; i++){              // fill the rest of the packets with data from utf16buffer
    for(int j=1;j<8 && bytecounter<bytes_processed;j++){
      CAN_MsgArray[i][j]=utf16buffer[bytecounter];                              // read split UTF-16 chars into the array byte by byte
      bytecounter++;
      //Serial.printf("Processing message: Packet i=%d, byte j=%d, byte number=%d out of %d \n", i, j, bytecounter, bytes_processed);
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

// sends a message request to the display, sets the switch to wait for ACK from 0x2C1
void sendMultiPacket(){     // main loop shall decide when to send the following data
  if(DEBUGGING_ON) sendPacketSerial(0x6C1, CAN_MsgArray[0]);
  sendPacket(0x6C1, CAN_MsgArray[0]);
  CAN_MessageReady=1;
}

// sends the rest of the message buffer in quick succession
void sendMultiPacketData(){   // should only be executed after the display acknowledges the incoming transmission
  for(int i=1;i<64 && (CAN_MsgArray[i][0]!=0x00 && !CAN_prevTxFail);i++){                 // this loop will stop sending data once the next packet doesn't contain a label
    if(DEBUGGING_ON) sendPacketSerial(0x6C1, CAN_MsgArray[i]);
    sendPacket(0x6C1, CAN_MsgArray[i]);
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  if(DEBUGGING_ON) Serial.println();
  CAN_MessageReady=0;                   // new buffers can now be prepared as the message has been sent
}

// debug, sends formatted CAN packets over serial ABC # B0 B1 B2 B3 B4 B5 B6 B7 with status
void sendPacketSerial(int id, char can_send_buffer[8]){
  Serial.println();
  Serial.printf("%03X # %02X %02X %02X %02X %02X %02X %02X %02X", id, can_send_buffer[0], can_send_buffer[1], can_send_buffer[2], can_send_buffer[3], can_send_buffer[4], can_send_buffer[5], can_send_buffer[6], can_send_buffer[7]);
}

// requests measurement blocks from ECC module using GMs diagnostic protocol
void requestMeasurementBlocks(){            // request from 0x248, 0x548 responds
  if(DEBUGGING_ON) Serial.println("CAN: Requesting measurement blocks from 0x246");
  char measurement_payload[8]={0x06, 0xAA, 0x01, 0x01, 0x07, 0x10, 0x11};
  sendPacket(0x248, measurement_payload, 7);
}