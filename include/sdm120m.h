void sdm120m_setup(const char* mqtt_server);
void sdm120m_loop(void(*restloopers)(void));

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

// set/.. is een algemene setting. later */set/# of all/set/#
// wr/.. is een schrijfcommando. transient. wordt weer verwijderd. later */wr/#
// SDM120M/<naam>/wr/adr commando om adres te wijzigen (en gebruik adres= SDM120M/<naam>/mom/adr) als gelukt schrijf ../mom/adr
// SDM120M/*/set/interval
// SDM120M/<naam>/wr/adr    

