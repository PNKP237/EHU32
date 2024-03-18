bool OTA_running=0;

const char* ssid = "EHU32-OTA";
const char* password = "ehu32updater";
volatile bool OTA_Finished=0;

// initialize OTA functionality as a way to update firmware; this disables CAN and A2DP functionality!
void OTA_start(){
  if(twai_stop()==ESP_OK){
    if(DEBUGGING_ON) Serial.println("TWAI: Stopped successfully.");
  }
  a2dp_sink.end(true);
  vTaskDelay(pdMS_TO_TICKS(500));
  if (!WiFi.softAP(ssid, password)) {
    if(DEBUGGING_ON) Serial.println("FAIL: Soft AP creation failed.");
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
        if(DEBUGGING_ON) Serial.println("OTA: Start updating " + type);
      })
      .onEnd([]() {
        if(DEBUGGING_ON) Serial.println("\nOTA: Finished!");
        OTA_Finished=1;
      })
      .onError([](ota_error_t error) {
        if(DEBUGGING_ON) Serial.printf("OTA: ERROR[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    if(DEBUGGING_ON) Serial.println("OTA: Ready!");
    while(1){
      ArduinoOTA.handle();
      if(OTA_Finished){
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
      }
    }
  }
}