BluetoothA2DPSink a2dp_sink;

// updates the buffers
void avrc_metadata_callback(uint8_t md_type, const uint8_t *data2) {  // fills the song title buffer with data, updates text_lenght with the amount of chars
  switch(md_type){
    case 0x1: clear_buffer(title_buffer);
              snprintf(title_buffer, sizeof(title_buffer), "%s", data2);
              if(DEBUGGING_ON) Serial.printf("\nA2DP: Received title: \"%s\"", data2);
              break;
    case 0x2: clear_buffer(artist_buffer);
              snprintf(artist_buffer, sizeof(artist_buffer), "%s", data2);
              if(DEBUGGING_ON) Serial.printf("\nA2DP: Received artist: \"%s\"", data2);
              break;
    case 0x4: clear_buffer(album_buffer);
              snprintf(album_buffer, sizeof(album_buffer), "%s", data2);
              if(DEBUGGING_ON) Serial.printf("\nA2DP: Received album: \"%s\"", data2);
              break;
    default:  break;
  }
  DIS_forceUpdate=1;                                                      // lets the main loop() know that there's a new song title in the buffer
}

void a2dp_connection_state_changed(esp_a2d_connection_state_t state, void *ptr){    // callback for bluetooth connection state change
  if(state==2){                                                                     // state=0 -> disconnected, state=1 -> connecting, state=2 -> connected
    bt_connected=1;
  } else {
    bt_connected=0;
  }
  bt_state_changed=1;
}

void a2dp_audio_state_changed(esp_a2d_audio_state_t state, void *ptr){  // callback for audio playing/stopped
  if(state==2){                                                         //  state=1 -> stopped, state=2 -> playing
    bt_audio_playing=1;
  } else {
    bt_audio_playing=0;
  }
  audio_state_changed=1;
}

// start A2DP audio service
void a2dp_init(){
    const i2s_config_t i2s_config = {                   // ext dac BLCK=26  WS/LRCK=25  DOUT=22
    .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = (i2s_bits_per_sample_t)16,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 128,
    .use_apll = true,
    .tx_desc_auto_clear = true
  };
  a2dp_sink.set_i2s_config(i2s_config);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM);
  a2dp_sink.set_on_connection_state_changed(a2dp_connection_state_changed);
  a2dp_sink.set_on_audio_state_changed(a2dp_audio_state_changed);

  if(DEBUGGING_ON){
    a2dp_sink.set_auto_reconnect(false);
  } else {
    a2dp_sink.set_auto_reconnect(true);
  }
  
  a2dp_sink.start("EHU32");                                                       // setting up bluetooth audio sink
  a2dp_started=1;
  if(DEBUGGING_ON) Serial.println("A2DP: Started!");

  processDataBuffer(1, "EHU32 Started!", "Bluetooth on", "Waiting for connection...");
}

// handles events such as connecion/disconnection and audio play/pause
void A2DP_EventHandler(){
  if(ehu_started && !a2dp_started){             // this enables bluetooth A2DP service only after the radio is started
    a2dp_init();
  }
  
  if(audio_state_changed){                                   // mute external DAC when not playing
    if(bt_audio_playing){
      digitalWrite(PCM_MUTE_CTL, HIGH);
      DIS_autoupdate=1;
      if(disp_mode==3) disp_mode=0;
    } else {
      digitalWrite(PCM_MUTE_CTL, LOW);
      processDataBuffer(1, "Bluetooth connected", "", "Paused");
      DIS_autoupdate=0;
      if(disp_mode==0) disp_mode=3;
    }
    audio_state_changed=0;
  }

  if(bt_state_changed){                                   // mute external DAC when not playing
    if(bt_connected){
      processDataBuffer(1, "Bluetooth connected", "", (char*)a2dp_sink.get_peer_name());
      disp_mode=0;
      if(disp_mode==3) disp_mode=0;
    } else {
      processDataBuffer(1, "Bluetooth disconnected", "", "");
      DIS_autoupdate=0;
      if(disp_mode==0) disp_mode=3;
    }
    bt_state_changed=0;
  }
}

// ID 0x501 DB3 0x18 indicates imminent shutdown of the radio and display; disconnect from source
void a2dp_shutdown(){
  if(a2dp_started && RxMessage.data[3]==0x18){
    a2dp_sink.disconnect();
    ehu_started=0;                            // so it is possible to restart and reconnect the source afterwards in the rare case radio is shutdown but ESP32 is still powered up
  }
}