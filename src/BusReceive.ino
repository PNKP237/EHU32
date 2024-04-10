// initialize CAN BUS
void twai_init(){
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_4, TWAI_MODE_NORMAL);         // CAN bus set up
  g_config.rx_queue_len=40;
  g_config.tx_queue_len=5;
  g_config.intr_flags=(ESP_INTR_FLAG_LEVEL1 & ESP_INTR_FLAG_IRAM);
  twai_timing_config_t t_config =  {.brp = 42, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false};    // set CAN prescalers and time quanta for 95kbit
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  TxMessage.extd=0;                      // TxMessage settings - not extended ID CAN packet
  TxMessage.rtr=0;                       // not retransmission packet
  TxMessage.ss=0;                        // don't transmit the packet as a single shot -> less chance of an error this way
  
  // CAN SETUP
  if(DEBUGGING_ON) Serial.print("\nCAN/TWAI SETUP => "); 
  if(twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
      if(DEBUGGING_ON) Serial.print("DRV_INSTALL: OK ");
  } else {
      if(DEBUGGING_ON) Serial.print("DRV_INST: FAIL ");
  }
  if (twai_start() == ESP_OK) {
      if(DEBUGGING_ON) Serial.print("DRV_START: OK ");
  } else {
      if(DEBUGGING_ON) Serial.print("DRV_START: FAIL ");
  }
  uint32_t alerts_to_enable=TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_RX_DATA;
  if(twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK){
      if(DEBUGGING_ON) Serial.print("ALERTS: OK ");
  } else {
      if(DEBUGGING_ON) Serial.print("ALERTS: FAIL \n");
  }
}

void sendPacket(int id, char can_send_buffer[8], int dlc);

// process incoming CAN messages until there is none left in the buffer; picks appropriate action based on message identifier
void canReceive(){                                               // logical filter based on the CAN ID
  while(twai_receive(&RxMessage, pdMS_TO_TICKS(10)==ESP_OK && !CAN_MessageReady)){
    switch (RxMessage.identifier){
      case 0x201: canDecodeEhuButtons();
                  break;
      case 0x206: canDecodeWheel();
                  break;
      case 0x208: canDecodeAC();
                  break;
      case 0x501: a2dp_shutdown();
                  break;
      case 0x548: if(disp_mode==1) canUpdateBodyData();
                  break;
      case 0x4ec: if(disp_mode==3) canUpdateCoolant();       // temporarily unused, but should work without ECC module
                  break;
      case 0x6c1: canUpdateDisplay();
                  break;
      default:    break;
    }
  }
}

// reads button presses on the steering wheel; works only when bluetooth is connected
void canDecodeWheel(){
  if(DEBUGGING_ON) Serial.println("CAN: Decoding wheel buttons");
  if(RxMessage.data[0]==0x0 && bt_connected){	          // released have these buttons work only when bluetooth is connected
    switch(RxMessage.data[1]){
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
}

// macro for switching Air Conditioning on/off with a short single knob press
void canDecodeAC(){
  if(DEBUGGING_ON) Serial.println("CAN: Decoding AC buttons");
  if((RxMessage.data[0]==0x0) && (RxMessage.data[1]==0x17) && (RxMessage.data[2]<0x03)){	// knob held for short time
    int ac_dlc=3, button_delay=100;
    vTaskDelay(pdMS_TO_TICKS(button_delay));
    //down once
    char ac_buffer[8]={0x08, 0x16, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};                           // todo optimize this garbage
    sendPacket(0x208, ac_buffer, ac_dlc);
    vTaskDelay(pdMS_TO_TICKS(button_delay));
    //push button
    ac_buffer[0]=0x01;
    ac_buffer[1]=0x17;
    ac_buffer[2]=0x00;
    sendPacket(0x208, ac_buffer, ac_dlc);
    vTaskDelay(pdMS_TO_TICKS(button_delay));
    //release button with a time constant
    ac_buffer[0]=0x00;
    ac_buffer[2]=0x10;
    sendPacket(0x208, ac_buffer, ac_dlc);
    vTaskDelay(pdMS_TO_TICKS(button_delay));
    //turn down twice
    ac_buffer[0]=0x08;
    ac_buffer[1]=0x16;
    ac_buffer[2]=0x01;
    sendPacket(0x208, ac_buffer, ac_dlc);
    vTaskDelay(pdMS_TO_TICKS(button_delay));
    sendPacket(0x208, ac_buffer, ac_dlc);
    vTaskDelay(pdMS_TO_TICKS(button_delay));
    //push button
    ac_buffer[0]=0x01;
    ac_buffer[1]=0x17;
    ac_buffer[2]=0x00;
    sendPacket(0x208, ac_buffer, ac_dlc);
    vTaskDelay(pdMS_TO_TICKS(button_delay));
    //release button with a time constant
    ac_buffer[0]=0x00;
    ac_buffer[2]=0x10;
    sendPacket(0x208, ac_buffer, ac_dlc);
  }
}

// overwrites any messages written to the display by the factory radio; active only when DIS_autoupdate is on and disp_mode is not -1
void canUpdateDisplay(){
  if(!a2dp_started){                                // if the total payload is less than 0x40 bytes the message will not be sent (so FM radio or other stuff isn't overwritten)
    ehu_started=1;                    // start the bluetooth A2DP service after first radio display call
  } else if(a2dp_started && !ehu_started){
    a2dp_sink.reconnect();
    ehu_started=1;
  }
  if(DIS_autoupdate && disp_mode!=-1){            // don't bother checking the data if there's no need to update the display
    if(RxMessage.data[0]==0x10 && RxMessage.data[1]<0x40){       // we check if the total payload of radio's message is small, if yes assume it's an Aux message
      preventDisplayUpdate();             // this hack prevents radio from transmitting the entirety of an Aux message
      vTaskDelay(pdMS_TO_TICKS(10));
      sendMultiPacket();
    }
  }
}

// reads and applies some formatting to measurements taken by ECC (electronic climate control) module
void canUpdateBodyData(){
  if(DEBUGGING_ON) Serial.println("CAN: Got measurement data from 0x546!");
  switch(RxMessage.data[0]){              // mesurement block ID -> update data which the message is referencing
    case 0x07:  {
      CAN_data_voltage=RxMessage.data[2];
      CAN_data_voltage/=10;
      snprintf(voltage_buffer, sizeof(voltage_buffer), "Voltage: %.1f V  ", CAN_data_voltage);
      CAN_voltage_recvd=1;
      break;
    }
    case 0x10:  {
      unsigned short raw_coolant=(RxMessage.data[3]<<8 | RxMessage.data[4]);            //int currentRPM=RxMessage.data[3]<<8 | RxMessage.data[2];
      CAN_data_coolant=raw_coolant;
      CAN_data_coolant/=10;
      snprintf(coolant_buffer, sizeof(coolant_buffer), "Coolant temp: %.1f%cC   ", CAN_data_coolant, '°');
      CAN_coolant_recvd=1;
      break;
    }
    case 0x11:  {
      CAN_data_rpm=(RxMessage.data[1]<<8 | RxMessage.data[2]);
      CAN_data_speed=RxMessage.data[4];
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
}

// fallback to read coolant temperatures from ECU in case reading from ECC fails; to be implemented sometime in the future
void canUpdateCoolant(){
  CAN_data_coolant=(RxMessage.data[3]-40);
  CAN_coolant_recvd=1;
}

// decides what function to call when a button on the factory radio has been held down for about one second
void canDecodeEhuButtons(){
  if(RxMessage.data[0]==0x01 && RxMessage.data[2]>=10){
    switch(RxMessage.data[1]){
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
}

void canActionEhuButton0(){
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
    if(DEBUGGING_ON) Serial.println("DISP_MODE: Switching to vehicle data...");
  }
}

void canActionEhuButton3(){
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
  OTA_start();
}

void canActionEhuButton9(){
  if(disp_mode!=-1){
    disp_mode=-1;
    if(DEBUGGING_ON) Serial.println("Screen updates disabled");
  }
}