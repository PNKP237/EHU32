const char* ssid = "EHU32-OTA";
const char* password = "ehu32updater";
volatile bool OTA_running=0, OTA_finished=0, OTA_progressing=0;
#ifndef DEBUG
// initialize OTA functionality as a way to update firmware; this disables A2DP functionality!
void OTA_start(){
  //twai_stop();
  a2dp_sink.end(true);
  vTaskDelay(pdMS_TO_TICKS(500));
  if (!WiFi.softAP(ssid, password)) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP.restart();
  } else {
    IPAddress myIP = WiFi.softAPIP();
    ArduinoOTA
      .setMdnsEnabled(false)
      .setRebootOnSuccess(true)
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else
          type = "filesystem";
          OTA_progressing=1;
      })
      .onProgress([](unsigned int progress, unsigned int total) {           // gives visual updates on the 
        //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        if(((progress / (total / 100))%10)==0){
          unsigned int progress_val=progress / (total / 100);
          char progress_text[32];
          snprintf(progress_text, sizeof(progress_text), "Updating... %d%%", progress_val);
          writeTextToDisplay(1, nullptr, progress_text, nullptr);
        }
      })
      .onError([](ota_error_t error) {
        char err_reason[32];
        switch(error){
          case OTA_AUTH_ERROR:{
            snprintf(err_reason, sizeof(err_reason), "Not authenticated");
            break;
          }
          case OTA_BEGIN_ERROR:{
            snprintf(err_reason, sizeof(err_reason), "Error starting");
            break;
          }
          case OTA_CONNECT_ERROR:{
            snprintf(err_reason, sizeof(err_reason), "Connection problem");
            break;
          }
          case OTA_RECEIVE_ERROR:{
            snprintf(err_reason, sizeof(err_reason), "Error receiving");
            break;
          }
          case OTA_END_ERROR:{
            snprintf(err_reason, sizeof(err_reason), "Couldn't apply update");
            break;
          }
          default: break;
        }
        writeTextToDisplay(1, "Error updating", err_reason, "Resetting...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        ESP.restart();
      })
      .onEnd([]() {
        prefs_clear();    // reset settings, ensures any new setup changes/optimizations will be useful
        OTA_finished=1;
        OTA_progressing=0;
      });
    ArduinoOTA.begin();
    OTA_running=1;
  }
}

void OTA_Handle(){
  unsigned long time_started=0;
  while(1){
    while(!checkFlag(OTA_begin)){
      vTaskDelay(1000);
    }
    if(!OTA_running){
      OTA_start();
      time_started=millis();
    }
    ArduinoOTA.handle();
    if(!OTA_progressing){                     // timeout after 10 minutes of no OTA start
      if((time_started+600000)<millis()){
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
      }
    }
    if(OTA_running && !OTA_progressing && !OTA_finished){ // keeps the internal watchdog happy when not doing anything
      vTaskDelay(1);
    }
    if(checkFlag(OTA_abort) && OTA_running && !OTA_progressing && !OTA_finished){  // provides a way to reset the ESP if stuck in OTA
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
    if(OTA_finished){
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
  }
}
#endif