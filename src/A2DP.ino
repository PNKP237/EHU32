I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

volatile bool md_album_recvd=0, md_artist_recvd=0, md_title_recvd=0;

// updates the buffers
void avrc_metadata_callback(uint8_t md_type, const uint8_t *data2) {  // fills the song title buffer with data, updates text_lenght with the amount of chars
  switch(md_type){
    case 0x1: memset(title_buffer, 0, sizeof(title_buffer));
              snprintf(title_buffer, sizeof(title_buffer), "%s", data2);
              DEBUG_PRINTF("\nA2DP: Received title: \"%s\"", data2);
              md_title_recvd=1;
              break;
    case 0x2: memset(artist_buffer, 0, sizeof(artist_buffer));
              snprintf(artist_buffer, sizeof(artist_buffer), "%s", data2);
              DEBUG_PRINTF("\nA2DP: Received artist: \"%s\"", data2);
              md_artist_recvd=1;
              break;
    case 0x4: memset(album_buffer, 0, sizeof(album_buffer));
              snprintf(album_buffer, sizeof(album_buffer), "%s", data2);
              DEBUG_PRINTF("\nA2DP: Received album: \"%s\"", data2);
              md_album_recvd=1;
              break;
    default:  break;
  }
  if(md_title_recvd && md_artist_recvd && md_album_recvd){
    DIS_forceUpdate=1;                                                      // lets the main loop() know that there's a new song title in the buffer
    md_title_recvd=0;
    md_artist_recvd=0;
    md_album_recvd=0;
  }
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
  auto i2s_conf=i2s.defaultConfig();
  i2s_conf.pin_bck=26;
  i2s_conf.pin_ws=25;
  i2s_conf.pin_data=22;
  i2s.begin(i2s_conf);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM);
  a2dp_sink.set_on_connection_state_changed(a2dp_connection_state_changed);
  a2dp_sink.set_on_audio_state_changed(a2dp_audio_state_changed);

  #ifndef DEBUG
  a2dp_sink.set_auto_reconnect(true);
  #endif

  a2dp_sink.start("EHU32");         // setting up bluetooth audio sink
  a2dp_started=1;
  DEBUG_PRINTLN("A2DP: Started!");
  disp_mode=0;                      // set display mode to audio metadata on boot
  writeTextToDisplay(1, "EHU32 v0.9rc started!", "Bluetooth on", "Waiting for connection...");
}

// handles events such as connecion/disconnection and audio play/pause
void A2DP_EventHandler(){
  if(ehu_started && !a2dp_started){             // this enables bluetooth A2DP service only after the radio is started
    a2dp_init();
  }
  
  if(audio_state_changed && bt_connected){      // mute external DAC when not playing; bt_connected ensures no "Connected, paused" is displayed, seems that the audio_state_changed callback comes late
    if(bt_audio_playing){
      digitalWrite(PCM_MUTE_CTL, HIGH);
      DIS_forceUpdate=1;              // force reprinting of audio metadata when the music is playing
    } else {
      digitalWrite(PCM_MUTE_CTL, LOW);
      writeTextToDisplay(1, "Bluetooth connected", "", "Paused");
    }
    audio_state_changed=0;
  }

  if(bt_state_changed){                                   // mute external DAC when not playing
    if(bt_connected){
      writeTextToDisplay(1, "Bluetooth connected", "", (char*)a2dp_sink.get_peer_name());
    } else {
      writeTextToDisplay(1, "Bluetooth disconnected", "", "");
    }
    bt_state_changed=0;
  }
}

// ID 0x501 DB3 0x18 indicates imminent shutdown of the radio and display; disconnect from source
void a2dp_shutdown(){
  a2dp_sink.disconnect();
  a2dp_sink.end();
  ehu_started=0;                            // so it is possible to restart and reconnect the source afterwards in the rare case radio is shutdown but ESP32 is still powered up
  a2dp_started=0;                           // while extremely unlikely to happen in the vehicle, this comes handy for debugging on my desk setup
  DEBUG_PRINTLN("CAN: EHU went down! Disconnecting A2DP.");
}

void a2dp_end(){
  a2dp_sink.disconnect();
  a2dp_sink.end();
  a2dp_started=0;
  DEBUG_PRINTLN("A2DP: Stopped!");
}