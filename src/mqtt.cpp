#include "mqtt.h"
#include <PrintEx.h>
using namespace ios;
#// #include <dictionary.h>
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* base_topic;
const char* sub_topic;
const char* pubsubuser;
const char* pubsubpassw;
unsigned long ChipID;
char strCID[10]; 

WiFiClient espClient;
PubSubClient pubsubclient(espClient);

void (*callbackFnc)(char*, char*);
CALLBACKSIGNATURE;

int mqttreconnects = 0;
// char vorig[20][40];
Dictionary &vorig = *(new Dictionary(20));

void mqtt_setup(const char* mqtt_server, const char* _user, const char* _passw, const char* b_topic, const char* s_topic)
{
    ChipID = ESP.getChipId();
    itoa(ChipID, strCID,16);

    base_topic = b_topic; //pointer copy
    sub_topic = s_topic; //pointer copy
    pubsubuser = _user;
    pubsubpassw = _passw;
    pubsubclient.setServer(mqtt_server, 1883);
    pubsubclient.setCallback(callback);
    reconnect();
}

void mqtt_loop(void)
    {
    if (!pubsubclient.connected()) 
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
 
    pubsubclient.loop();
    }

void reconnect() 
    {
    // Loop until we're reconnected
    while (!pubsubclient.connected()) 
        {
            Serial.print("#Attempting MQTT connection...");
            // Attempt to connect
            if (pubsubclient.connect(/*clientid*/strCID, pubsubuser, pubsubpassw)) 
            {
                Serial.println("#connected");
                if (pubsubclient.subscribe(sub_topic)) Serial << "#abbo op " << sub_topic << endl;
                else Serial.println("#abbo mislukt");
            } 
            else 
            {
                Serial.print("#failed, rc=");
                Serial.print(pubsubclient.state());
                Serial.println("# try again in 5 seconds");
                // Wait 5 seconds before retrying
                delay(5000); //TODO loopen zodat OTA werkt
            }
        }
    if (pubsubclient.connected()) 
        {
        // char strCID[10]; 
        // itoa(ChipID, strCID,16);
        //TODO
        // pubsubclient.publish("SDM120M/ChipID", strCID, strlen((char*)strCID), true);
        pub("ChipID", strCID);
        }
    }

void setCallback(CALLBACKSIGNATURE)
    {
    callbackFnc = _callback;
    }

void callback(char* topic, byte* payload, unsigned int payloadlen) 
    {
    //pas op, payload is niet met '\0' afgesloten
    bool dbg = false; 
    int intWaarde = 0;
    char strpayload[40];
    if (dbg) Serial.print("#Message arrived [");
    if (dbg) Serial.print(topic);
    if (dbg) Serial.print("] ");
    for (unsigned int i = 0; i < payloadlen; i++) 
    {
        char c = (char)payload[i];
        strpayload[i] = c;
        if (dbg) Serial.print(c);
        if (c>='0' && c <= '9') intWaarde = intWaarde*10 + (c)-'0';
    }
    strpayload[payloadlen]=0; //met \0 afsluiten
    if (dbg) Serial.printf("# payload_int=%d", intWaarde);
    if (dbg) Serial.printf(" strpayload=%s", strpayload);
    if (dbg) Serial.println(); //algemene regel afsluiten
  
    char topicPlus[40] = "";
    strcat(topicPlus, base_topic);
    strcat(topicPlus, "/");
    int len = strlen(topicPlus);

    if (strncmp(topic, topicPlus, len) == 0) topic+=len;
    else return;

    if (payloadlen>0) //anders is intWaarde 0
        if (callbackFnc) 
            (callbackFnc(topic, strpayload));
    }

//jammer dat ik EN itemnr EN topic moet opgeven. zou dict moeten hebben
void pubifchanged(const char* topic, const char* waarde)
    {
    // if (strcmp(waarde, vorig[itemnr])) // !=0 => verschil
    if (String(waarde) != vorig[topic])
        {
        pub(topic, waarde);
        // strcpy(vorig[itemnr], waarde);
        vorig(topic, waarde);
        }
    }

void pubifchanged(const char* topic, String waarde)
    {
    if (waarde != vorig[topic])
        {
        pub(topic, waarde);
        vorig(topic, waarde);
        }
    }

void pubifchanged(String topic, String waarde)
    {
    if (waarde != vorig[topic])
        {
        pub(topic, waarde);
        vorig(topic, waarde);
        }
    }

void pub(const char* topic, const char* waarde, unsigned int plength)
    {
    char topicPlus[40] = "";
    strcat(topicPlus, base_topic);
    strcat(topicPlus, "/");
    strcat(topicPlus,topic);
    pubsubclient.publish(/*(const char*)*/topicPlus, (const uint8_t*)waarde, plength, true);
    }

void pub(const char* topic, const char* waarde)
    {
    char topicPlus[40] = "";
    strcat(topicPlus, base_topic);
    strcat(topicPlus, "/");
    strcat(topicPlus,topic);
    pubsubclient.publish(topicPlus, waarde, true);
    }

void pub(String topic, String waarde)
    {
    String topicPlus = String(base_topic) + "/" + topic;
    char strtopic[40];  
    topicPlus.toCharArray(strtopic, 40);
    char strwaarde[40];  
    waarde.toCharArray(strwaarde,40);
    pubsubclient.publish((const char*)strtopic, (const uint8_t*)strwaarde, strlen(strwaarde), true);
    Serial << millis() << "\tPUB:" << strtopic << '\t' << strwaarde << endl;
    }

void pubG(String topic, String waarde)
    {
    char strtopic[40];  
    GString g(strtopic);
    g << base_topic << "/" << topic;
    // String topicPlus = String(base_topic) + "/" + topic;
    // topicPlus.toCharArray(strtopic,40);
    char strwaarde[40];  
    waarde.toCharArray(strwaarde,40);
    pubsubclient.publish((const char*)strtopic, (const uint8_t*)strwaarde, strlen(strwaarde), true);
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

