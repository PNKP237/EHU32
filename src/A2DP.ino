#include <A2DPVolumeControl.h>
I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);
A2DPNoVolumeControl noVolumeControl;

// updates the buffers
void avrc_metadata_callback(uint8_t md_type, const uint8_t *data2) {  // fills the song title buffer with data, updates text_lenght with the amount of chars
  xSemaphoreTake(BufferSemaphore, portMAX_DELAY);      // take the semaphore as a way to prevent the buffers being accessed elsewhere
  switch(md_type){
    case 0x1: memset(title_buffer, 0, sizeof(title_buffer));
              snprintf(title_buffer, sizeof(title_buffer), "%s", data2);
              //DEBUG_PRINTF("\nA2DP: Received title: \"%s\"", data2);
              setFlag(md_title_recvd);
              break;
    case 0x2: memset(artist_buffer, 0, sizeof(artist_buffer));
              snprintf(artist_buffer, sizeof(artist_buffer), "%s", data2);
              //DEBUG_PRINTF("\nA2DP: Received artist: \"%s\"", data2);
              setFlag(md_artist_recvd);
              break;
    case 0x4: memset(album_buffer, 0, sizeof(album_buffer));
              snprintf(album_buffer, sizeof(album_buffer), "%s", data2);
              //DEBUG_PRINTF("\nA2DP: Received album: \"%s\"", data2);
              setFlag(md_album_recvd);
              break;
    default:  break;
  }
  xSemaphoreGive(BufferSemaphore);
  if(checkFlag(md_title_recvd) && checkFlag(md_artist_recvd) && checkFlag(md_album_recvd)){
    setFlag(DIS_forceUpdate);                                                      // lets the eventHandler task know that there's new data to be written to the display
    clearFlag(md_title_recvd);
    clearFlag(md_artist_recvd);
    clearFlag(md_album_recvd);
  }
}

// a2dp bt connection callback
void a2dp_connection_state_changed(esp_a2d_connection_state_t state, void *ptr){    // callback for bluetooth connection state change
  if(state==2){                                                                     // state=0 -> disconnected, state=1 -> connecting, state=2 -> connected
    setFlag(bt_connected);
  } else {
    clearFlag(bt_connected);
  }
  setFlag(bt_state_changed);
}

// a2dp audio state callback
void a2dp_audio_state_changed(esp_a2d_audio_state_t state, void *ptr){  // callback for audio playing/stopped
  if(state==2){                                                         //  state=1 -> stopped, state=2 -> playing
    setFlag(bt_audio_playing);
  } else {
    clearFlag(bt_audio_playing);
  }
  setFlag(audio_state_changed);
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
  a2dp_sink.set_volume_control(&noVolumeControl);
  a2dp_sink.set_reconnect_delay(500);
  a2dp_sink.set_auto_reconnect(true, 2000);

  a2dp_sink.start("EHU32");         // setting up bluetooth audio sink
  setFlag(a2dp_started);
  DEBUG_PRINTLN("A2DP: Started!");
  disp_mode=0;                      // set display mode to audio metadata on boot
  writeTextToDisplay(1, "EHU32 v0.9.5", "Bluetooth on", "Waiting for connection...");
}

// handles events such as connecion/disconnection and audio play/pause
void A2DP_EventHandler(){
  if(checkFlag(ehu_started) && !checkFlag(a2dp_started)){             // this enables bluetooth A2DP service only after the radio is started
    a2dp_init();
  }

  if(checkFlag(DIS_forceUpdate) && disp_mode==0 && checkFlag(CAN_allowAutoRefresh) && checkFlag(bt_audio_playing)){                       // handles data processing for A2DP AVRC data events
    writeTextToDisplay();
  }

  if(checkFlag(bt_state_changed) && disp_mode==0){                                   // mute external DAC when not playing
    if(checkFlag(bt_connected)){
      a2dp_sink.set_volume(127);        // workaround to ensure max volume being applied on successful connection
      writeTextToDisplay(1, "", "Bluetooth connected", (char*)a2dp_sink.get_peer_name());
    } else {
      writeTextToDisplay(1, "", "Bluetooth disconnected", "");
    }
    clearFlag(bt_state_changed);
  }

  if(checkFlag(audio_state_changed) && checkFlag(bt_connected) && disp_mode==0){      // mute external DAC when not playing; bt_connected ensures no "Connected, paused" is displayed, seems that the audio_state_changed callback comes late
    if(checkFlag(bt_audio_playing)){
      digitalWrite(PCM_MUTE_CTL, HIGH);
      setFlag(DIS_forceUpdate);              // force reprinting of audio metadata when the music is playing
    } else {
      digitalWrite(PCM_MUTE_CTL, LOW);
      writeTextToDisplay(1, "Bluetooth connected", "Paused", "");
    }
    clearFlag(audio_state_changed);
  }
}

// ID 0x501 DB3 0x18 indicates imminent shutdown of the radio and display; disconnect from source
void a2dp_shutdown(){
  vTaskSuspend(canMessageDecoderTaskHandle);
  //vTaskSuspend(canWatchdogTaskHandle);
  ESP.restart();                            // very crude workaround until I find a better way to deal with reconnection problems after end() is called
  delay(1000);
  a2dp_sink.disconnect();
  a2dp_sink.end();
  clearFlag(ehu_started);                            // so it is possible to restart and reconnect the source afterwards in the rare case radio is shutdown but ESP32 is still powered up
  clearFlag(a2dp_started);                           // while extremely unlikely to happen in the vehicle, this comes handy for debugging on my desk setup
  DEBUG_PRINTLN("CAN: EHU went down! Disconnecting A2DP.");
}