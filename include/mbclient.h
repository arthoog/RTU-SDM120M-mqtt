#pragma once

#include <Arduino.h>

bool    mbclient_isServer();
int     mbclient_bezig();
bool    mbclient_lastOK();
bool    mbclient_errors();
void    mbclient_resetErrors();
float   mbclient_readFloat(int mbadr, char type, uint16 start, void(*restloopers)(void));
bool    mbclient_writeFloat(float waarde, int mbadr, char type, uint16 start, void(*restloopers)(void));
void    mbclient_setup(uint32_t bps, int8_t rxpin, int8_t txpin, int8_t txenbpin=-1, bool direct=true);
void    mbclient_loop();
