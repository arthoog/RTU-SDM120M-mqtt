//MOD-WIFI
#include <arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "AP-elfjes.h"

#include "mqtt.h"
// #include <PubSubClient.h>

#include <Streaming.h>
#include <string.h>

//#include <OneWire.h> 
//#include <DallasTemperature.h>

//#define ONE_WIRE_BUS 14
//#define LED 2 //wemos d1 //13
//#define RELAIS 12

//OneWire oneWire(ONE_WIRE_BUS); 
//DallasTemperature sensors(&oneWire);

//===============================================================
// const char* mqtt_server = "192.168.1.40"; //elite";
// const char* mqtt_username = "";
// const char* mqtt_password = "";
// const char* mqtt_topic = "SDM120M/#";
// unsigned long ChipID;

// char vorig[20][40];

boolean dbg = false;
//byte  ch1 = 0;
byte  aktief = 1;
long interval = 10000;
//char* command[40];
int setRelaisPB = -1;
// int mqttreconnects = 0;
//long sched = 1000;
//float setpoint = 21.0;

WiFiClient espClient;
// PubSubClient client(espClient);

//===============================================================
#include <ModbusRTU.h>
#if defined(ESP8266)
 #include <SoftwareSerial.h>
 // SoftwareSerial S(D1, D2, false, 256);

 // receivePin, transmitPin, inverse_logic, bufSize, isrBufSize
 // connect RX to D2 (GPIO4, Arduino pin 4), TX to D1 (GPIO5, Arduino pin 4)
 SoftwareSerial S(4, 5);
#endif
//===============================================================
#include "main.h"
//===============================================================
ModbusRTU mb;
bool bezig = false;
int stap = 0;

bool coils[20];
uint16_t regs[16];

//===============================================================
bool cbWrite(Modbus::ResultCode event, uint16_t transactionId, void* data) {
#ifdef ESP8266
  Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
#elif ESP32
  Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
#else
  Serial.print("Request result: 0x");
  Serial.print(event, HEX);
#endif
  return true;
}

//---------------------------------------------------------------
bool cbWrIreg(Modbus::ResultCode event, uint16_t transactionId, void* data) {
  //Serial.print(regs[0],HEX);
  //Serial.print(regs[1],HEX);
  //Serial.print(" ");

  //if (stap ==  1) pub("Spanning",     String(floatfromregs(),1));
  if (stap ==  1) pubifchanged(stap, (char*)"Spanning",     String(floatfromregs(),1));
  if (stap ==  2) pubifchanged(stap, (char*)"Stroom",       String(floatfromregs(),1));
  if (stap ==  3) pubifchanged(stap, (char*)"Vermogen",     String(floatfromregs(),1));
  if (stap ==  4) pubifchanged(stap, (char*)"SchijnVerm",   String(floatfromregs(),1));
  if (stap ==  5) pubifchanged(stap, (char*)"BlindVerm",    String(floatfromregs(),1));
  if (stap ==  6) pubifchanged(stap, (char*)"CosPhi",       String(floatfromregs(),2));
  if (stap ==  7) pubifchanged(stap, (char*)"Frequentie",   String(floatfromregs(),3));
  if (stap ==  8) pubifchanged(stap, (char*)"kWhToelev",    String(floatfromregs(),3));
  if (stap ==  9) pubifchanged(stap, (char*)"kWhTeruglev",  String(floatfromregs(),3));
  if (stap == 10) pubifchanged(stap, (char*)"RelaisPB",     String(floatfromregs(),0));
  if (stap == 11) pubifchanged(stap, (char*)"MeterID",      String(floatfromregs(),0));

  bezig = false;
  return true;
}

//---------------------------------------------------------------
bool cbWriteHregPW(Modbus::ResultCode event, uint16_t transactionId, void* data) {
  Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
  bezig = false;
  return true;
}

//---------------------------------------------------------------
float floatfromregs() {
  uint16_t regsrev[2];
  regsrev[0] = regs[1];
  regsrev[1] = regs[0];
  return *(float*)regsrev;
}

//---------------------------------------------------------------
void initregsfromfloat(float y) {
  uint16_t regsrev[2];
  //float y = 100;
  *(float*)regsrev = y;
  regs[0] = regsrev[1];
  regs[1] = regsrev[0];
}

//===============================================================
unsigned long millisslotschaduw = 0xffffffff;
// signed long nextpoll = 0;
void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  //Wifi spul
  WiFi.mode(WIFI_STA);

  // Serial.println("#SDM120MmqttOTA");
  // Serial << "#mqttserver=" << mqtt_server << endl;
  // ChipID = ESP.getChipId();
  mqtt_setup();
  setCallback(mqtt_callback);

  // We start by connecting to a WiFi network
  Serial.println();
  /*if (dbg)*/ Serial.print("#Connecting to ");
  /*if (dbg)*/ Serial.print(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(10000);
    ESP.restart();
  }

  //OTA spul
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("SDM120M");

  // No authentication by default
  //ArduinoOTA.setPassword((const char *)"geheim");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //onewire
  //sensors.begin(); 

  //MQTT spul
  mqtt_setup2();
   
  //digitalWrite(RELAIS,HIGH); //uit?

  //Modbus spul
 #if defined(ESP8266)
  S.begin(2400, SWSERIAL_8N1);
  //mb.begin(&S);
  mb.begin(&S, /*pen 2 voor transmit enable*/ 2, /*direct*/true);
 #elif defined(ESP32)
  Serial1.begin(2400, SERIAL_8N1);
  mb.begin(&Serial1);
 #else
  Serial1.begin(2400, SERIAL_8N1);
  mb.begin(&Serial1);
  mb.setBaudrate(2400);
 #endif
  mb.master();

  // nextpoll = (signed long)millis()+interval;
}

//---------------------------------------------------------------
void loop() {
  ArduinoOTA.handle();

  mqtt_loop();

  //Modbus spul

//30001 Voltage Volts Float 00 00
//30007 Current Amps Float 00 06
//30013 Active power Watts Float 00 0C
//30019 Apparent power VA Float 00 12
//30025 Reactive power VAr Float 00 18
//30031 Power factor None Float 00 1E
//30071 Frequency Hz Float 00 46
//30073 Import active energy kWh Float 00 48
//30075 Export active energy kWh Float 00 4A
//30077 Import reactive energy kvarh Float 00 4C
//30079 Export reactive energy kvarh Float 00 4E
//30343 Total active energy kWh Float 01 56
//30345 Total reactive energy Kvarh Float 01 58
//uint16_t ModbusRTU::readIreg(uint8_t slaveId, uint16_t offset, uint16_t* value, uint16_t numregs, cbTransaction cb)
//uint16_t ModbusRTU::readHreg(uint8_t slaveId, uint16_t offset, uint16_t* value, uint16_t numregs, cbTransaction cb)
//uint16_t ModbusRTU::writeHreg(uint8_t slaveId, uint16_t offset, uint16_t* value, uint16_t numregs, cbTransaction cb)        mb.readCoil(1, 1, coils, 20, cbWrite);

  // Serial << millis() << "\tstap=" << stap << "\tbezig=" << bezig << endl; 
  unsigned long millisslot = millis()/(interval); //2sec slot
  if (millisslot != millisslotschaduw)
  {
    // pas als klaar met cyclus millisslotschaduw = millisslot;

    if (!bezig && !mb.slave())
    {
        if (setRelaisPB >=0)
        {
            //40013 Relay Pulse Width Float 00 0C
            bezig = true;
            initregsfromfloat(float(setRelaisPB));
            Serial << "setRelaisPB: " << floatfromregs() << endl;
            mb.writeHreg(1, 0x0c, regs, 2, cbWriteHregPW);
            //bezig = false;
            setRelaisPB = -1;
            pub((char*)"cmd/setRelaisPB",(char*)""); //wis topic
        }
    }


    if (!bezig && !mb.slave()) 
    {
      bezig = true; //op voorhand
      switch (++stap)
      {
        // case 0:
        //   break;
        case 1:
          //30001 Voltage Volts Float 00 00
          mb.readIreg(1, 0x00, regs, 2, cbWrIreg); 
          break;
        case 2:
          //30007 Current Amps Float 00 06
          mb.readIreg(1, 0x06, regs, 2, cbWrIreg); 
          break;
        case 3:
          //30013 Active power Watts Float 00 0C
          mb.readIreg(1, 0x0c, regs, 2, cbWrIreg); 
          break;
        case 4:
          //30019 Apparent power VA Float 00 12
          mb.readIreg(1, 0x12, regs, 2, cbWrIreg); 
          break;
        case 5:
          //30025 Reactive power VAr Float 00 18
          mb.readIreg(1, 0x18, regs, 2, cbWrIreg); 
          break;
        case 6:
          //30031 Power factor None Float 00 1E
          mb.readIreg(1, 0x1E, regs, 2, cbWrIreg); 
          break;
        case 7:
          //30071 Frequency Hz Float 00 46
          mb.readIreg(1, 0x46, regs, 2, cbWrIreg);
          break; 
        case 8:
          //30073 Import active energy kWh Float 00 48
          mb.readIreg(1, 0x48, regs, 2, cbWrIreg);
          break; 
        case 9:
          //30075 Export active energy kWh Float 00 4a
          mb.readIreg(1, 0x4a, regs, 2, cbWrIreg);
          break; 
        case 10:
          //40013 Relay Pulse Width Float 00 0C
          mb.readHreg(1, 0x0c, regs, 2, cbWrIreg);
          break; 
        case 11:
          //40021 Meter ID Float 00 14
          mb.readHreg(1, 0x14, regs, 2, cbWrIreg);
          break; 
        default:
          bezig = false; //toch niet
          if (stap>20) 
          {
            stap = 0; 
            millisslotschaduw = millisslot; //klaar met dit tijdslot. zodra weer ongelijk de volgende
          }
      }
    }
  }
  mb.task();
  yield();
  // delay(1000);
}

void mqtt_callback(char* topic, char* strpayload)
    {     
      bool dbg = false;
      if (dbg) Serial << millis() << "\tMQTTCB:" << topic << "=" << strpayload << endl;
      int intWaarde = atoi(strpayload);
        if (strcmp(topic,"cmd/setRelaisPB") == 0) 
        {
            Serial << "MQTT:" << topic << "=" << intWaarde << endl;
            setRelaisPB = intWaarde;
        }
        
        if (strcmp(topic,"cmd/interval") == 0) 
        {
            Serial << "MQTT:" << topic << "=" << intWaarde << endl;
            interval = intWaarde;
            if (interval < 0) interval = 0;
        }
        
        if (strcmp(topic,"cmd/aktief") == 0) 
        {
            Serial << "MQTT:" << topic << "=" << intWaarde << endl;
            aktief = intWaarde;
        }
    }
