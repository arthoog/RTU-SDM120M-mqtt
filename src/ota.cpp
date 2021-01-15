// #include <arduino.h>
// #include <ESP8266WiFi.h>
// #include <ESP8266mDNS.h>
// #include <WiFiUdp.h>
#include <ArduinoOTA.h>

void ota_init()
{
    // ArduinoOTA.setPort(8266);                        // Port defaults to 8266
    ArduinoOTA.setHostname("SDM120M");                  // Hostname defaults to esp8266-[ChipID]
    //ArduinoOTA.setPassword((const char *)"geheim");   // No authentication by default
    ArduinoOTA.onStart([]() { Serial.println("Start"); });
    ArduinoOTA.onEnd([]()   { Serial.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) 
    { 
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) 
    {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();

}

void ota_loop()
{
   ArduinoOTA.handle();
}