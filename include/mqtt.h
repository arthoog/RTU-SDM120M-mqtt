#pragma once
#include <arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
// #include <Streaming.h>
// #include <PrintEx.h>
#include <string.h>
#include <dictionary.h> //inlined version
// #include "test.h"

#define CALLBACKSIGNATURE void (*_callback)(char*, char*)
// void mqtt_setup(void);
void mqtt_setup(const char*, const char*, const char*, const char*, const char*);
void mqtt_loop(void);
void callback(char* topic, byte* payload, unsigned int payloadlen);
void reconnect();
void setCallback(CALLBACKSIGNATURE);
//pubsub
void pubifchanged(const char* topic, char* waarde);
void pubifchanged(const char* topic, String waarde);
void pubifchanged(String topic, String waarde);
void pub(const char* topic, const char* waarde, unsigned int plength);
void pub(const char* topic, const char* waarde);
void pub(String topic, String waarde);
void publong(String topic, long waarde);
char hexchar(long l, byte digit);
unsigned long hex2ulong(char* s, byte aantal);
// Dictionary *d = new Dictionary();
// Dictionary &d = *(new Dictionary(20));
// Dictionary &d;
