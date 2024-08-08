const char* ssid = "EHU32-OTA";
const char* password = "ehu32updater";
volatile bool OTA_running=0, OTA_Finished=0;
#ifndef DEBUG
// initialize OTA functionality as a way to update firmware; this disables CAN and A2DP functionality!
void OTA_start(){
  twai_stop();
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
      })
      .onEnd([]() {
        OTA_Finished=1;
      });
    ArduinoOTA.begin();
  }
}

void OTA_Handle(){
  while(1){
    while(!OTA_begin){
      vTaskDelay(1000);
    }
    if(!OTA_running){
      OTA_start();
    }
    ArduinoOTA.handle();
    if(OTA_Finished){
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
  }
}
#endif