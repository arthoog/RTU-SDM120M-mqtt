//MOD-WIFI
#include <arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "AP-elfjes.h"
#include "mqtt.h"
#include <Streaming.h>
#include <string.h>
#include <dictionary.h> //inlined version
#include <cppQueue.h>

//===============================================================
const char* mqtt_server = "192.168.1.40"; //elite";

boolean dbg = false;
byte  aktief = 1;
long pollInterval = 10000;
bool goCycle=false;

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
bool bezig      = false;
u8 adr          = 1; //huidig MBadres

int devnr       = -1;
String devNaam  = "";
int devAdres    = -1; //16 bit, u8 nodig maar dan is -1 = ongeldig

int stap        = 0;
int mberror     = 0;

int wrRelaisPB  = -1;

//bool coils[20];
uint16_t regs[16]; //er worden er maar 2 gebruikt

// //===============================================================
// bool cbWrite(Modbus::ResultCode event, uint16_t transactionId, void* data) 
// {
// #ifdef ESP8266
//   Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
// #elif ESP32
//   Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
// #else
//   Serial.print("Request result: 0x");
//   Serial.print(event, HEX);
// #endif
//   return true;
// }

//---------------------------------------------------------------
typedef struct strRec {
	// uint16_t	mbAdr;      //modbusadres
    // char        type;       // 'I'=inputregister 'H'=holdingregister
    // long        adres;
    // char        datatype;   // van het adres. F=float32 'I'=int16
	float	    waarde;     //
    String      Naam;
    String      Rest;
} wrQueueRec;

cppQueue	wrqueue(sizeof(wrQueueRec), 10, FIFO);	// Instantiate queue

wrQueueRec writerec;

//---------------------------------------------------------------
typedef struct {
    char type; //bank, 
    long adres;
    char datatype; //F=float
    int decimalen;
    String naam;
    // long interval;
} SDM120M ;

SDM120M dataray[] = 
{   {'I', 0x00, 'F', 1, "Spanning"}      // 0
,   {'I', 0x06, 'F', 1 ,"Stroom"}        // 1
,   {'I', 0x0c, 'F', 1, "Vermogen"}      // 2
,   {'I', 0x12, 'F', 1, "SchijnVerm"}    // 3
,   {'I', 0x18, 'F', 1, "BlindVerm"}     // 4
,   {'I', 0x1e, 'F', 2, "CosPhi"}        // 5
,   {'I', 0x46, 'F', 3, "Frequentie"}    // 6
,   {'I', 0x48, 'F', 3, "kWhToelev"}     // 7
,   {'I', 0x4a, 'F', 3, "kWhTeruglev"}   // 8
,   {'H', 0x0c, 'F', 0, "RelaisPB"}      // 9
,   {'H', 0x14, 'F', 0, "mom/adr"}       //10   MeterID
,   {'-', 0x00, ' ', 1, ""}              // End of data
};

// set/.. is een algemene setting. later */set/# of all/set/#
// wr/.. is een schrijfcommando. transient. wordt weer verwijderd. later */wr/#
// SDM120M/<naam>/wr/adr commando om adres te wijzigen (en gebruik adres= SDM120M/<naam>/mom/adr) als gelukt schrijf ../mom/adr
// SDM120M/*/set/interval
// SDM120M/<naam>/wr/adr    

Dictionary &devadrs = *(new Dictionary(20));
Dictionary &devsettings = *(new Dictionary(20));

//---------------------------------------------------------------
bool cbWrIreg(Modbus::ResultCode event, uint16_t transactionId, void* data) 
    {
    //Serial.print(regs[0],HEX);
    //Serial.print(regs[1],HEX);
    //Serial.print(" ");

    if (event != 0) //Serial.printf_P("Request result: 0x%02X, Mem: %d\r\n", event, ESP.getFreeHeap());
    {
       Serial << millis() << "\tRequest result: 0x" << event << " Mem: " << ESP.getFreeHeap() << endl;
       //delay voor herstel modbus
        mberror++;
        // stap--;
        // delay(1000); //modbus een seconde rust geven.
        delay(100);
    }
    else //TODO nu alleen nog maar Floats
    {
        SDM120M dat = dataray[stap-1];
        pubifchanged(devNaam + "/" + dat.naam, String(floatfromregs(),dat.decimalen));
    }

    bezig = false;
    return true;
    }

//---------------------------------------------------------------
bool cbWriteHregPW(Modbus::ResultCode event, uint16_t transactionId, void* data) 
{
    // Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
    Serial << millis() << "\t>>>>>>>>>cbWriteHreg result: 0x" << event << " Mem: " << ESP.getFreeHeap() << endl;
    bezig = false;
    int16_t waarde = writerec.waarde;
    if (event==0) 
        if (writerec.Rest == "wr/adr")
            devadrs(writerec.Naam, String(waarde));
            //ook pub "mom/adr" ?
    return true;
}

//---------------------------------------------------------------
float floatfromregs() 
{
  uint16_t regsrev[2];
  regsrev[0] = regs[1];
  regsrev[1] = regs[0];
  return *(float*)regsrev;
}

//---------------------------------------------------------------
void initregsfromfloat(float y) 
{
    uint16_t regsrev[2];
    //float y = 100;
    *(float*)regsrev = y;
    regs[0] = regsrev[1];
    regs[1] = regsrev[0];
}

//===============================================================
unsigned long millisslotschaduw = 0xffffffff;
// signed long nextpoll = 0;
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

    //OTA spul
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

    //MQTT spul
    Serial << "#mqttserver=" << mqtt_server << endl;
    setCallback(mqtt_callback);
    mqtt_setup(mqtt_server, /*user*/ "", /*passw*/ "", "SDM120M", "SDM120M/#");

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

    // nextpoll = (signed long)millis()+pollInterval;
}

//---------------------------------------------------------------
void loop() 
{
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

    // gaat naat poll_MB()
    if (!bezig && !mb.slave())
    {
        if (false && mberror<3 && wrRelaisPB >=0)
        {
            //40013 Relay Pulse Width Float 00 0C
            bezig = true;
            initregsfromfloat(float(wrRelaisPB));
            Serial << "wrRelaisPB: " << floatfromregs() << endl;
            mb.writeHreg(adr, 0x0c, regs, 2, cbWriteHregPW);
            //bezig = false;
            wrRelaisPB = -1;
            pub((char*)"cmd/wrRelaisPB",(char*)""); //wis topic
        }

        if (!wrqueue.isEmpty() && mberror<3)
        {
            // wrQueueRec writerec;
            if (wrqueue.pop(&writerec))
            {   //devadrs[naam].toInt(), 'H', 0x0c, 'F',
                int16_t adr = devadrs[writerec.Naam].toInt();

                Serial << millis() << "\tPOP: " << "\tnaam:" << writerec.Naam << "\trest:" << writerec.Rest << "waarde:" << writerec.waarde << endl;

                if (writerec.Rest.equals("wr/adr")) //equalsIgnoreCase()?
                    {
                    int16_t waarde = writerec.waarde;
                    if (waarde > 1 && waarde < 240)
                        {
                        initregsfromfloat(writerec.waarde);
                        mb.writeHreg(adr, 0x14, regs, 2, cbWriteHregPW);
                        bezig = true;
                        pub(writerec.Naam + "/" + writerec.Rest, (char*)""); //wis topic //eigenlijk in callback
                        }
                    }
                if (writerec.Rest.equals("wr/RelaisPB")) //equalsIgnoreCase()?
                    {
                    initregsfromfloat(writerec.waarde);
                    mb.writeHreg(adr, 0x0c, regs, 2, cbWriteHregPW);
                    bezig = true;
                    pub(writerec.Naam + "/" + writerec.Rest, (char*)""); //wis topic //eigenlijk in callback
                    }
            }
        }
        unsigned long millisslot = millis()/(pollInterval); //10sec slot
        if (millisslot != millisslotschaduw)
        {
            goCycle = true;
            millisslotschaduw = millisslot;
        }

        if (goCycle)
        {
            // pas als klaar met cyclus millisslotschaduw = millisslot;

            if (false)
            {
                bezig = true; //op voorhand
                switch (++stap)
                {
                    // case 0:
                    //   break;
                    case 1:
                    //30001 Voltage Volts Float 00 00
                    mb.readIreg(adr, 0x00, regs, 2, cbWrIreg); 
                    break;
                    case 2:
                    //30007 Current Amps Float 00 06
                    mb.readIreg(adr, 0x06, regs, 2, cbWrIreg); 
                    break;
                    case 3:
                    //30013 Active power Watts Float 00 0C
                    mb.readIreg(adr, 0x0c, regs, 2, cbWrIreg); 
                    break;
                    case 4:
                    //30019 Apparent power VA Float 00 12
                    mb.readIreg(adr, 0x12, regs, 2, cbWrIreg); 
                    break;
                    case 5:
                    //30025 Reactive power VAr Float 00 18
                    mb.readIreg(adr, 0x18, regs, 2, cbWrIreg); 
                    break;
                    case 6:
                    //30031 Power factor None Float 00 1E
                    mb.readIreg(adr, 0x1E, regs, 2, cbWrIreg); 
                    break;
                    case 7:
                    //30071 Frequency Hz Float 00 46
                    mb.readIreg(adr, 0x46, regs, 2, cbWrIreg);
                    break; 
                    case 8:
                    //30073 Import active energy kWh Float 00 48
                    mb.readIreg(adr, 0x48, regs, 2, cbWrIreg);
                    break; 
                    case 9:
                    //30075 Export active energy kWh Float 00 4a
                    mb.readIreg(adr, 0x4a, regs, 2, cbWrIreg);
                    break; 
                    case 10:
                    //40013 Relay Pulse Width Float 00 0C
                    mb.readHreg(adr, 0x0c, regs, 2, cbWrIreg);
                    break; 
                    case 11:
                    //40021 Meter ID Float 00 14
                    mb.readHreg(adr, 0x14, regs, 2, cbWrIreg);
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
            else if (false && mberror>=3) //of dit gelijk doen. dan moet op beide plaatsen millisslot gelijk bepaald worden. of globaal.
            {
                mberror=0;
                stap = 0; 
                millisslotschaduw = millisslot; //klaar met dit tijdslot. zodra weer ongelijk de volgende
            }
            else if (devadrs.count()>0) //we hebben minimaal een device
            {
                if ((mberror>0)||(devAdres<0)||(devnr<0)) //ongeldig
                {
                    mberror = 0;
                    devnr++;
                    stap=0;
                    if (devnr>= devadrs.count()) 
                    {
                        devnr=-1; 
                        devAdres=-1;
                        goCycle = false;
                        devNaam = "";
                        // millisslotschaduw = millisslot; //klaar cyclus alle devices. klaar met dit tijdslot. zodra weer ongelijk de volgende
                    }
                    else
                    {
                        devNaam = devadrs(devnr); //geeft key
                        String devAdr = devadrs[devnr]; //geeft value
                        int t = devAdr.toInt();

                        //hier nog check op devAdr? Yep
                        if (t>0 && t<248) //geldig
                        {
                            devAdres = devAdr.toInt();
                            // Serial << millis() << "\t======== " << devnr << "\tdev: " << devNaam << "\tadr: " << devAdres << " ========" << endl;
                        }
                    }
                    Serial << millis() << "\tH:" << ESP.getFreeHeap() << "\t======== " << devnr << "\tdev: " << devNaam << "\tadr: " << devAdres << " ========" << endl;
                }

                if (devAdres>0 && devAdres<248) //geldig
                {
                    SDM120M dat = dataray[stap++];
                    // bezig = true;
                    switch (dat.type)
                    {
                        case 'I':
                            mb.readIreg(devAdres, dat.adres, regs, 2, cbWrIreg);
                            bezig = true;
                            break;
                        case 'H':
                            mb.readHreg(devAdres, dat.adres, regs, 2, cbWrIreg);
                            bezig = true;
                            break;
                        default: // '-'
                            // bezig = false;
                            devAdres = -1; //volgend device
                            stap = 0; 
                            // millisslotschaduw = millisslot; //klaar met dit tijdslot. zodra weer ongelijk de volgende
                    }
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
    long intWaarde = atoi(strpayload);
    if (strcmp(topic,"wr/RelaisPB") == 0) 
        {
        Serial << millis() << "\tMQTT:" << topic << "=" << intWaarde << endl;
        wrRelaisPB = intWaarde;
        return;
        }
        
    if (strcmp(topic,"set/interval") == 0) 
        {
        Serial << millis() << "\tMQTT:" << topic << "=" << intWaarde << endl;
        pollInterval = intWaarde;
        if (pollInterval < 1000) pollInterval = 1000;
        return;
        }
        
    if (strcmp(topic,"set/aktief") == 0) 
        {
        Serial << millis() << "\tMQTT:" << topic << "=" << intWaarde << endl;
        aktief = intWaarde;
        return;
        }


        //devadrs <naam> (SDM120M/)<naam>/mom/adr
    //split op '/' . Hoe?
    char* naam = strtok(topic,"/"); //copy pointer '/' will be overwritten by '\0' by next strtok
    char* rest = strtok(NULL,""); // get rest of string
    if (rest != NULL)
        {
        // Serial << millis() << "\tNaStrtok:" << naam << ": " << rest << "=" << String(intWaarde) << endl;
        if (strcmp(rest,"mom/adr") == 0) 
            {
            devadrs(naam, /*brute force*/String(intWaarde));
            Serial << millis() << "\tDEVADRS:" << naam << " " << rest << "=" << String(intWaarde) << "\tjson: " << devadrs.json() << endl;
            // Serial << "devadrs: " << devadrs.json() << endl;

            return;
            }
        if (strcmp(rest,"wr/RelaisPB") == 0) // verander dev's relais pulsbreedte
            {
            if (strlen(strpayload)>0)
                {
                //TODO
                //devadrs(naam, /*brute force*/String(intWaarde));
                Serial << millis() << "\tnieuwRelaisPB:" << naam << ": " << rest << "=" << String(intWaarde) << endl;
                
                //zet in queue
                // wrQueueRec rec = {devadrs[naam].toInt(), 'H', 0x0c, 'F', intWaarde, String(naam), String(rest) }; //of String(strpayload).toFloat
                wrQueueRec rec = {/*0, 'x', 0x0, 'x',*/ intWaarde, String(naam), String(rest) }; //of String(strpayload).toFloat
                wrqueue.push(&rec);

                //moet eigenlijk pas als write gelukt is.
                // pub(String(naam)+"/"+rest, (char*)""); //wis topic
                }
            return;
            }
        if (strcmp(rest,"wr/adr") == 0) // verander dev's modbusadres
            {
            if (intWaarde > 1 && intWaarde < 240)
                {
                //TODO
                //devadrs(naam, /*brute force*/String(intWaarde));
                Serial << millis() << "\tnieuwMBadres:" << naam << ": " << rest << "=" << String(intWaarde) << endl;
                
                //zet in queue
                // wrQueueRec rec = {devadrs[naam].toInt(), 'H', 0x0c, 'F', intWaarde, String(naam), String(rest) }; //of String(strpayload).toFloat
                wrQueueRec rec = {/*0, 'x', 0x0, 'x',*/ intWaarde, String(naam), String(rest) }; //of String(strpayload).toFloat
                wrqueue.push(&rec);

                //moet eigenlijk pas als write gelukt is.
                // pub(String(naam)+"/"+rest, (char*)""); //wis topic
                }
            return;
            }

        } //rest != NULL
        
    } //mqtt_callback

