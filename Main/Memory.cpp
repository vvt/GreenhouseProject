#include "Memory.h"
#include "Globals.h"
//--------------------------------------------------------------------------------------------------------------------------------
#if EEPROM_USED_MEMORY == EEPROM_BUILTIN
  #include <EEPROM.h>
#elif EEPROM_USED_MEMORY == EEPROM_AT24C32
  #include "AT24CX.h"
  AT24C32* memoryBank;
#elif EEPROM_USED_MEMORY == EEPROM_AT24C64
  #include "AT24CX.h"
  AT24C64* memoryBank;
#elif EEPROM_USED_MEMORY == EEPROM_AT24C128
  #include "AT24CX.h"
  AT24C128* memoryBank;
#elif EEPROM_USED_MEMORY == EEPROM_AT24C256
  #include "AT24CX.h"
  AT24C256* memoryBank;
#elif EEPROM_USED_MEMORY == EEPROM_AT24C512
  #include "AT24CX.h"
  AT24C512* memoryBank;
#endif
//--------------------------------------------------------------------------------------------------------------------------------
void MemInit()
{
#if EEPROM_USED_MEMORY == EEPROM_BUILTIN
  // не надо инициализировать дополнительно
#elif EEPROM_USED_MEMORY == EEPROM_AT24C32
   memoryBank = new AT24C32();
#elif EEPROM_USED_MEMORY == EEPROM_AT24C64
  memoryBank = new AT24C64();
#elif EEPROM_USED_MEMORY == EEPROM_AT24C128
 memoryBank = new AT24C128();
#elif EEPROM_USED_MEMORY == EEPROM_AT24C256
 memoryBank = new AT24C256();
#elif EEPROM_USED_MEMORY == EEPROM_AT24C512
  memoryBank = new AT24C512();
#endif  
}
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t MemRead(unsigned int address)
{
  #if EEPROM_USED_MEMORY == EEPROM_BUILTIN
    return EEPROM.read(address);
  #elif EEPROM_USED_MEMORY == EEPROM_AT24C32
     return memoryBank->read(address);
  #elif EEPROM_USED_MEMORY == EEPROM_AT24C64
    return memoryBank->read(address);
  #elif EEPROM_USED_MEMORY == EEPROM_AT24C128
   return memoryBank->read(address);
  #elif EEPROM_USED_MEMORY == EEPROM_AT24C256
   return memoryBank->read(address);
  #elif EEPROM_USED_MEMORY == EEPROM_AT24C512
    return memoryBank->read(address);
  #endif      

}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWrite(unsigned int address, uint8_t val)
{
  #if EEPROM_USED_MEMORY == EEPROM_BUILTIN
    EEPROM.write(address, val);
  #elif EEPROM_USED_MEMORY == EEPROM_AT24C32
     memoryBank->write(address,val);
  #elif EEPROM_USED_MEMORY == EEPROM_AT24C64
    memoryBank->write(address,val);
  #elif EEPROM_USED_MEMORY == EEPROM_AT24C128
   memoryBank->write(address,val);
  #elif EEPROM_USED_MEMORY == EEPROM_AT24C256
   memoryBank->write(address,val);
  #elif EEPROM_USED_MEMORY == EEPROM_AT24C512
    memoryBank->write(address,val);
  #endif
}
//--------------------------------------------------------------------------------------------------------------------------------
