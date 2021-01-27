//MOD-WIFI
#include <arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include "AP-elfjes.h"
// #include <ArduinoOTA.h>
#include "ota.h"
// #include "mqtt.h"
// #include <ModbusRTU.h>
#include "sdm120m.h"
#include <Streaming.h>
#include <string.h>
#include <dictionary.h> //inlined version
#include <cppQueue.h>
// #include "main.h"

//===============================================================
const char* mqtt_server = "192.168.1.40"; //elite";

// boolean dbg = false;

//===============================================================
void setup() 
{
    Serial.begin(115200);
    Serial.println("Booting");
    //Wifi spul
    WiFi.mode(WIFI_STA);
    Serial.println();
    Serial.print("#Connecting to ");
    Serial.print(ssid);

    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) 
    {
        Serial.println("Connection Failed! Rebooting...");
        delay(10000);
        ESP.restart();
    }
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    ota_setup("SDM120M");
    sdm120m_setup(mqtt_server);
}

//---------------------------------------------------------------
void rest_loop()
{
    ota_loop();
    // in sdm120m_loop()   mqtt_loop();
    // sdm120m_loop();
    yield();

}
void loop() //voor arduino
{
        sdm120m_loop(rest_loop);
}


