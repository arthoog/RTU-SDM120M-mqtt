#include <ModbusRTU.h>
#include <Streaming.h>
/*mb*/
ModbusRTU mb;
/*mb*/
bool bezig      = false;

int mberror     = 0;

//bool coils[20];
uint16_t rdregs[2];     // Lezen.er worden er maar 2 gebruikt
uint16_t wrregs[2];     // voor schrijfakties

//===============================================================
/*mb*/
#if defined(ESP8266)
 #include <SoftwareSerial.h>
 // SoftwareSerial S(D1, D2, false, 256);

 // receivePin, transmitPin, inverse_logic, bufSize, isrBufSize
 // connect RX to D2 (GPIO4, Arduino pin 4), TX to D1 (GPIO5, Arduino pin 4)
//  SoftwareSerial S(4, 5);
 SoftwareSerial S;
#endif

/*mb*/
Modbus::ResultCode lastResult;

//---------------------------------------------------------------
bool mbclient_isServer()
{
    return mb.slave();
}
//---------------------------------------------------------------
int mbclient_bezig()
{
    return bezig;
}
//---------------------------------------------------------------
bool mbclient_lastOK()
{
    return (lastResult == 0);
}
//---------------------------------------------------------------
bool mbclient_errors()
{
    return mberror;
}
//---------------------------------------------------------------
void mbclient_resetErrors()
{
    mberror = 0;
}
//---------------------------------------------------------------
void mbclient_loop()
{
    mb.task();
}
//---------------------------------------------------------------
float mbclient_floatfromregs() 
{
  uint16_t regsrev[2];
  regsrev[0] = rdregs[1];
  regsrev[1] = rdregs[0];
  return *(float*)regsrev;
}
float mbclient_floatfromregs(uint16_t* regs) 
{
  uint16_t regsrev[2];
  regsrev[0] = regs[1];
  regsrev[1] = regs[0];
  return *(float*)regsrev;
}

//---------------------------------------------------------------
void mbclient_initregsfromfloat(float y) 
{
    uint16_t regsrev[2];
    *(float*)regsrev = y;
    wrregs[0] = regsrev[1];
    wrregs[1] = regsrev[0];
}
void mbclient_initregsfromfloat(float y, uint16_t* regs) 
{
    uint16_t regsrev[2];
    *(float*)regsrev = y;
    regs[0] = regsrev[1];
    regs[1] = regsrev[0];
}

//---------------------------------------------------------------
/*mb*/
// bool cbRdIHreg(Modbus::ResultCode event, uint16_t transactionId, void* data) 
//     {
//     lastResult = event;
//     if (event != 0)
//     {
//        Serial << millis() << "\tRequest result: 0x" << event << " Mem: " << ESP.getFreeHeap() << endl;
//         mberror++;
//         delay(100); //voor herstel modbus
//     }
//     else //TODO nu alleen nog maar Floats
//     {
//         SDM120M dat = dataray[stap-1];
//         pubifchanged(devNaam + "/" + dat.naam, String(floatfromregs(),dat.decimalen));
//     }

//     bezig = false;
//     return true;
//     }
 
/*mb*/
bool cbRdIHregInlined(Modbus::ResultCode event, uint16_t transactionId, void* data) 
    {
    lastResult = event;
    if (event != 0)
    {
       Serial << millis() << "\tRequest result: 0x" << event << " Mem: " << ESP.getFreeHeap() << endl;
        mberror++;
        delay(100); //voor herstel modbus
    }
    else //TODO nu alleen nog maar Floats
    {
        // SDM120M dat = dataray[stap-1];
        // pubifchanged(devNaam + "/" + dat.naam, String(floatfromregs(),dat.decimalen));
    }

    bezig = false;
    return true;
    }
 
//---------------------------------------------------------------
/*mb*/
// bool cbWriteHregPW(Modbus::ResultCode event, uint16_t transactionId, void* data) 
// {
//     lastResult = event;
//     // Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
//     Serial << millis() << "\t>>>>>>>>>cbWriteHreg result: 0x" << event << " Mem: " << ESP.getFreeHeap() << endl;
//     bezig = false;
//     int16_t waarde = writerec.waarde;
//     if (event==0) 
//         if (writerec.Rest == "wr/adr")
//             devadrs(writerec.Naam, String(waarde));
//             //ook pub "mom/adr" ?
//     return true;
// }
//---------------------------------------------------------------
/*mb*/
bool cbWriteHregPWInlined(Modbus::ResultCode event, uint16_t transactionId, void* data) 
{
    lastResult = event;
    // Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
    Serial << millis() << "\t>>>>>>>>>cbWriteHreg result: 0x" << event << " Mem: " << ESP.getFreeHeap() << endl;
    bezig = false;
    // int16_t waarde = writerec.waarde;
    // if (event==0) 
    //     if (writerec.Rest == "wr/adr")
    //         devadrs(writerec.Naam, String(waarde));
    //         //ook pub "mom/adr" ?
    return true;
}

/*mb*/
float mbclient_readFloat(int mbadr, char type, uint16 start, void(*restloopers)(void))
{
    uint16_t regs[2];

    if (type == 'I')
         mb.readIreg(mbadr, start, regs, 2, cbRdIHregInlined);
    else if (type == 'H')
         mb.readHreg(mbadr, start, regs, 2, cbRdIHregInlined);
    bezig = true;
    while (bezig) {mb.task(); restloopers();}
    return (lastResult == 0) ? mbclient_floatfromregs(regs) : NULL;
}

/*mb*/
bool mbclient_writeFloat(float waarde, int mbadr, char type, uint16 start, void(*restloopers)(void))
{
    uint16_t regs[2];
    mbclient_initregsfromfloat(waarde, regs);

    if (type == 'H')
         mb.writeHreg(mbadr, start, regs, 2, cbWriteHregPWInlined);
    // else  if (type == 'I') bestaat niet. 'O'utput wel?
    else return false;
    bezig = true;
    while (bezig) {mb.task(); restloopers();}
    return (lastResult == 0) ;
}

void mbclient_setup(uint32_t bps, int8_t rxpin, int8_t txpin, int8_t txenbpin=-1, bool direct=true)
{
   //Modbus spul
    #if defined(ESP8266)
    // S.begin(bps, SWSERIAL_8N1);
    // S.begin(bps, SWSERIAL_8N1, rxpin, txpin/*, false, 128, 128*11*/);
    S.begin(bps, SWSERIAL_8N1, rxpin, txpin, false, 128/*, 128*11*/);
    //mb.begin(&S);
    mb.begin(&S, txenbpin, direct);
    #elif defined(ESP32)
    Serial1.begin(bps, SERIAL_8N1);
    mb.begin(&Serial1);
    #else
    Serial1.begin(bps, SERIAL_8N1);
    mb.begin(&Serial1);
    mb.setBaudrate(bps);
    #endif
    mb.master();


}