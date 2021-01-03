#include "mqtt.h"

//MQTT stuff
//=============================================================================

//const char* mqtt_server = "192.168.1.40"; //elite";
const char* mqtt_username = "";
const char* mqtt_password = "";
// const char* mqtt_topic = "SDM120M/#";
const char* mqtt__topic;
unsigned long ChipID;

extern WiFiClient espClient;

PubSubClient client(espClient);

void (*callbackFnc)(char*, char*);
CALLBACKSIGNATURE;

int mqttreconnects = 0;
char vorig[20][40];


void mqtt_setup()
{
  Serial.println("#SDM120MmqttOTA");
//   Serial << "#mqttserver=" << mqtt_server << endl;
//   ChipID = ESP.getChipId();
}
void mqtt_setup2(const char* mqtt_server, const char* topic)
{
    mqtt__topic = topic; //pointer copy
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    reconnect();
}

void mqtt_loop(void)
{
    if (!client.connected()) 
  {
    Serial.print("\nClient not connected");
    Serial.flush();
    if (mqttreconnects++ < 100)
    {
      Serial << endl << "Reconnect#" << mqttreconnects;
      reconnect();
      //setCallback is niet nodig. callbackFnc is nog actueel. 
      //setCallback(mqtt_callback);
    }
    else
    {
      Serial.print("\nRESTART!!");
      ESP.restart();
    }
  }
  
  client.loop();

}
void reconnect() 
  {
  // Loop until we're reconnected
  while (!client.connected()) 
    {
    Serial.print("#Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("SDM120M", mqtt_username, mqtt_password)) 
      {
      Serial.println("#connected");
      if (client.subscribe(mqtt__topic)) Serial << "#abbo op " << mqtt__topic << endl;
      else Serial.println("#abbo mislukt");
      } 
    else 
      {
      Serial.print("#failed, rc=");
      Serial.print(client.state());
      Serial.println("# try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000); //TODO loopen zodat OTA werkt
      }
    }
  if (client.connected()) 
    {
    char strCID[10]; 
    itoa(ChipID, strCID,16);
    //TODO
    // client.publish("SDM120M/ChipID", strCID, strlen((char*)strCID), true);
    pub("ChipID", strCID);
    }
  }

void setCallback(CALLBACKSIGNATURE)
{
    callbackFnc = _callback;
}

void callback(char* topic, byte* payload, unsigned int payloadlen) {
    //pas op, payload is niet met '\0' afgesloten
    bool dbg = false;
    int intWaarde = 0;
    char strpayload[40];
    if (dbg) Serial.print("#Message arrived [");
    if (dbg) Serial.print(topic);
    if (dbg) Serial.print("] ");
    for (unsigned int i = 0; i < payloadlen; i++) {
      char c = (char)payload[i];
      strpayload[i] = c;
      if (dbg) Serial.print(c);
      if (c>='0' && c <= '9') intWaarde = intWaarde*10 + (c)-'0';
    }
    strpayload[payloadlen]=0; //met \0 afsluiten
    if (dbg) Serial.printf("# payload_int=%d", intWaarde);
    if (dbg) Serial.printf(" strpayload=%s", strpayload);
    if (dbg) Serial.println(); //algemene regel afsluiten
  
    //we zijn geabbonneerd op "SDM120M/#"
    if (strncmp(topic,"SDM120M/",8) == 0) topic+=8;
    else return;

  //dit is de payload!!!

//  if (strcmp(topic,"setpoint") == 0) 
//  {
//    Serial << endl << "MQTT:" << topic << "=" << intWaarde;
//    String pl = "setpoint: " + String(strpayload);
//    setpoint = String(strpayload).toFloat();
//    if (setpoint < 18.0) setpoint = 18.0;
//    if (setpoint > 36.0) setpoint = 36.0;
//    pub("log", "setpoint="+String(setpoint));
//  }
    if (payloadlen>0) //anders is intWaarde 0
    {    
        if (callbackFnc) (callbackFnc(topic, strpayload));
        // if (strcmp(topic,"cmd/setRelaisPB") == 0) 
        // {
        //     Serial << "MQTT:" << topic << "=" << intWaarde << endl;
        //     setRelaisPB = intWaarde;
        // }
        
        // if (strcmp(topic,"cmd/interval") == 0) 
        // {
        //     Serial << "MQTT:" << topic << "=" << intWaarde << endl;
        //     interval = intWaarde;
        //     if (interval < 0) interval = 0;
        // }
        
        // if (strcmp(topic,"cmd/aktief") == 0) 
        // {
        //     Serial << "MQTT:" << topic << "=" << intWaarde << endl;
        //     aktief = intWaarde;
        // }
    }
}

//jammer dat ik EN itemnr EN topic moet opgeven. zou dict moeten hebben
void pubifchanged(int itemnr, const char* topic, const char* waarde)
{
  if (strcmp(waarde, vorig[itemnr])) // !=0 => verschil
  {
    pub(topic, waarde);
    strcpy(vorig[itemnr], waarde);
  }
}

void pubifchanged(int itemnr, const char* topic, String waarde)
{
  //if (strcmp(waarde, vorig[itemnr])) // !=0 => verschil
  if (waarde != String(vorig[itemnr]))
  {
    pub(topic, waarde);
    waarde.toCharArray(vorig[itemnr],40);
  }
}

void pubifchanged(int itemnr, String topic, String waarde)
{
  //if (strcmp(waarde, vorig[itemnr])) // !=0 => verschil
  if (waarde != String(vorig[itemnr]))
  {
    pub(topic, waarde);
    waarde.toCharArray(vorig[itemnr],40);
  }
}

void pub(const char* topic, const char* waarde, unsigned int plength)
{
  char topicPlus[40] = "SDM120M/";
  strcat(topicPlus,topic);
  client.publish(/*(const char*)*/topicPlus, (const uint8_t*)waarde, plength, true);
}

void pub(const char* topic, const char* waarde)
  {
  char topicPlus[40] = "SDM120M/";
  strcat(topicPlus,topic);
  client.publish(/*(const char*)*/topicPlus, /*(const uint8_t*)*/waarde, /*strlen(strwaarde),*/ true);
  }

void pub(String topic, String waarde)
  {
  //Serial << "PUB:" << topic << '\t' << waarde << endl;
  String topicPlus = "SDM120M/"+topic;
  char strtopic[40];  
  topicPlus.toCharArray(strtopic,40);
  char strwaarde[40];  
  waarde.toCharArray(strwaarde,40);
  client.publish((const char*)strtopic, (const uint8_t*)strwaarde, strlen(strwaarde), true);
  Serial << "PUB:" << strtopic << '\t' << strwaarde << endl;
  }

void publong(String topic, long waarde)
  {
      char pl[12];
      itoa(waarde, pl, 10);
      pub(topic, pl);
  }

char hexchar(long l, byte digit)
  {
  int dititval = (l>>(digit<<2)) & 0xf;
  if (dititval < 10) return '0'+dititval;
  else return 'a'-10+dititval;
  }
unsigned long hex2ulong(char* s, byte aantal)
  {
  unsigned long res = 0;
  for (int ix = 0; ix< aantal;ix++)
      { 
      char c = *s++;
      // hoeft niet if (c = '\0') break;
      if (c >= '0' && c <= '9') res = (res<<4) + c-'0';
      else if (c >= 'a' && c <= 'f') res = (res<<4) + 10 + c - 'a';
      else if (c >= 'A' && c <= 'F') res = (res<<4) + 10 + c - 'A';
      else break;  
      }
  return res;
  }

