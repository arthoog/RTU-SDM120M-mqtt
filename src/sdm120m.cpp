#include <ModbusRTU.h>
#include "sdm120m.h"
#include "mqtt.h"
#include <dictionary.h> //inlined version
#include <cppQueue.h>
#include <Streaming.h>

//------forwards------------------------------------------------
// void mqtt_callback(char* topic, char* strpayload);
// float floatfromregs();

//local globals (private?)
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
uint16_t rdregs[16];    //Lezen.er worden er maar 2 gebruikt
uint16_t wrregs[2];     //voor schrijfakties

byte  aktief = 1;
long pollInterval = 10000;
unsigned long millisslotschaduw = 0xffffffff;
bool goCycle=false;

//===============================================================
#if defined(ESP8266)
 #include <SoftwareSerial.h>
 // SoftwareSerial S(D1, D2, false, 256);

 // receivePin, transmitPin, inverse_logic, bufSize, isrBufSize
 // connect RX to D2 (GPIO4, Arduino pin 4), TX to D1 (GPIO5, Arduino pin 4)
 SoftwareSerial S(4, 5);
#endif

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
typedef struct /*strRec*/ {
	float	    waarde;     //
    String      Naam;       //Strings in een struct. Spannend!! maar het gaat prima
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

Dictionary &devadrs = *(new Dictionary(20));
// Dictionary &devsettings = *(new Dictionary(20));

//---------------------------------------------------------------
float floatfromregs() 
{
  uint16_t regsrev[2];
  regsrev[0] = rdregs[1];
  regsrev[1] = rdregs[0];
  return *(float*)regsrev;
}

//---------------------------------------------------------------
void initregsfromfloat(float y) 
{
    uint16_t regsrev[2];
    *(float*)regsrev = y;
    wrregs[0] = regsrev[1];
    wrregs[1] = regsrev[0];
}

//---------------------------------------------------------------
bool cbRdIHreg(Modbus::ResultCode event, uint16_t transactionId, void* data) 
    {
    if (event != 0)
    {
       Serial << millis() << "\tRequest result: 0x" << event << " Mem: " << ESP.getFreeHeap() << endl;
        mberror++;
        delay(100); //voor herstel modbus
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

void mqtt_callback(char* topic, char* strpayload)
    {     
    bool dbg = false;
    if (dbg) Serial << millis() << "\tMQTTCB:" << topic << "=" << strpayload << endl;
    long intWaarde = atoi(strpayload);
        
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

    char* naam = strtok(topic,"/"); //copy pointer '/' will be overwritten by '\0' by next strtok
    char* rest = strtok(NULL,""); // get rest of string
    if (rest != NULL)
        {
        // Serial << millis() << "\tNaStrtok:" << naam << ": " << rest << "=" << String(intWaarde) << endl;

        //Nodig voor init. Op deze manier komt de polllijst van namen en adressen binnen
        if (strcmp(rest,"mom/adr") == 0) 
            {
            devadrs(naam, /*brute force*/String(intWaarde));
            Serial << millis() << "\tDEVADRS:" << naam << " " << rest << "=" << String(intWaarde) << "\tjson: " << devadrs.json() << endl;
            return;
            }
        //voor schrijven pulsbreedte (relais) van de Wh puls
        if (strcmp(rest,"wr/RelaisPB") == 0)
            {
            if (strlen(strpayload)>0)
                {
                Serial << millis() << "\tnieuwRelaisPB:" << naam << ": " << rest << "=" << String(intWaarde) << endl;
                wrQueueRec rec = {intWaarde, String(naam), String(rest) }; //of String(strpayload).toFloat
                wrqueue.push(&rec);
                }
            return;
            }
        //voor het aanpassen van het modbus-adres
        if (strcmp(rest,"wr/adr") == 0)
            {
            if (intWaarde > 1 && intWaarde < 240)
                {
                Serial << millis() << "\tnieuwMBadres:" << naam << ": " << rest << "=" << String(intWaarde) << endl;
                wrQueueRec rec = {intWaarde, String(naam), String(rest) }; //of String(strpayload).toFloat
                wrqueue.push(&rec);
                }
            return;
            }
        } //rest != NULL
    } //mqtt_callback

void sdm120m_setup(const char* mqtt_server)
{
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

    //MQTT spul
    Serial << "#mqttserver=" << mqtt_server << endl;
    setCallback(mqtt_callback);
    mqtt_setup(mqtt_server, /*user*/ "", /*passw*/ "", "SDM120M", "SDM120M/#");

}

void sdm120m_loop()
{
    mqtt_loop();

    if (!bezig && !mb.slave())
    {
        if (!wrqueue.isEmpty() && mberror<3)
        {
            if (wrqueue.pop(&writerec))
            {
                int16_t adr = devadrs[writerec.Naam].toInt();

                Serial << millis() << "\tPOP: " << "\tnaam:" << writerec.Naam << "\trest:" << writerec.Rest << "waarde:" << writerec.waarde << endl;

                if (writerec.Rest.equals("wr/adr")) //equalsIgnoreCase()?
                    {
                    int16_t waarde = writerec.waarde;
                    if (waarde > 1 && waarde < 240)
                        {
                        initregsfromfloat(writerec.waarde);
                        mb.writeHreg(adr, 0x14, wrregs, 2, cbWriteHregPW);
                        bezig = true;
                        pub(writerec.Naam + "/" + writerec.Rest, (char*)""); //wis topic //eigenlijk in callback
                        }
                    }
                if (writerec.Rest.equals("wr/RelaisPB")) //equalsIgnoreCase()?
                    {
                    initregsfromfloat(writerec.waarde);
                    mb.writeHreg(adr, 0x0c, wrregs, 2, cbWriteHregPW);
                    bezig = true;
                    pub(writerec.Naam + "/" + writerec.Rest, (char*)""); //wis topic //eigenlijk in callback
                    }
            }
        }
        unsigned long millisslot = millis()/(pollInterval); //10sec slot
        if (millisslot != millisslotschaduw)
        {
            if (devadrs.count()>0) goCycle = true;
            millisslotschaduw = millisslot;
        }

        if (goCycle)
        {
            if (devadrs.count()>0) //we hebben minimaal e'e'n device
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
                            mb.readIreg(devAdres, dat.adres, rdregs, 2, cbRdIHreg);
                            bezig = true;
                            break;
                        case 'H':
                            mb.readHreg(devAdres, dat.adres, rdregs, 2, cbRdIHreg);
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
};
