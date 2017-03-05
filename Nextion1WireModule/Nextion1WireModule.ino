/*

Модуль поддержки дисплея Nextion по шине 1-Wire

*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include "NextionController.h"
#include "LowLevel.h"
#include "OneWireSlave.h"
//----------------------------------------------------------------------------------------------------------------
// НАСТРОЙКИ
//----------------------------------------------------------------------------------------------------------------
#define NEXTION_SERIAL_SPEED 9600 // скорость работы с Serial для Nextion 
//----------------------------------------------------------------------------------------------------------------
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// \/
//----------------------------------------------------------------------------------------------------------------
// ДАЛЕЕ ИДУТ СЛУЖЕБНЫЕ НАСТРОЙКИ И КОД - МЕНЯТЬ С ПОЛНЫМ ПОНИМАНИЕМ ТОГО, ЧТО ХОДИМ СДЕЛАТЬ !!!
//----------------------------------------------------------------------------------------------------------------
#define UNUSED(expr) do { (void)(expr); } while (0)
//----------------------------------------------------------------------------------------------------------------
// уникальный ID модуля
//----------------------------------------------------------------------------------------------------------------
#define RF_MODULE_ID 200
//----------------------------------------------------------------------------------------------------------------
//Структура передаваемая мастеру и обратно
//----------------------------------------------------------------------------------------------------------------
struct sensorData
{
    byte type;
    byte data[2];
};
//----------------------------------------------------------------------------------------------------------------
typedef enum
{
  StateTemperature = 1, // есть температурные датчики
  StateLuminosity = 4, // есть датчики освещенности
  StateHumidity = 8 // есть датчики влажности
  
} ModuleStates; // вид состояния
//----------------------------------------------------------------------------------------------------------------
typedef struct
{
  byte packet_type; // тип пакета
  byte packet_subtype; // подтип пакета
  byte config; // конфигурация
  byte controller_id; // ID контроллера, к которому привязан модуль
  byte rf_id; // уникальный идентификатор модуля
  byte reserved[3]; // резерв, добитие до 24 байт
  byte controllerStatus;
  byte nextionStatus1;
  byte nextionStatus2;  
  byte openTemperature; // температура открытия окон
  byte closeTemperature; // температура закрытия окон
  byte dataCount; // кол-во записанных показаний с датчиков 
  sensorData data[5];

  byte crc8;
    
} t_scratchpad;
//----------------------------------------------------------------------------------------------------------------
t_scratchpad scratchpadS, savedScratchpad;//, scratchpadToSend;
volatile char* scratchpad = (char *)&scratchpadS; //что бы обратиться к scratchpad как к линейному массиву

volatile bool scratchpadReceivedFromMaster = false; // флаг, что мы должны обновить данные в Nextion

//----------------------------------------------------------------------------------------------------------------
// Настройки 1-Wire
Pin oneWireData(2); // на втором пине у нас висит 1-Wire
const byte owROM[7] = { 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }; // адрес датчика, менять не обязательно, т.к. у нас не честный 1-Wire
// команды 1-Wire
const byte COMMAND_START_CONVERSION = 0x44; // запустить конвертацию
const byte COMMAND_READ_SCRATCHPAD = 0xBE; // попросили отдать скратчпад мастеру
const byte COMMAND_WRITE_SCRATCHPAD = 0x4E; // попросили записать скратчпад, следом пойдёт скратчпад
const byte COMMAND_SAVE_SCRATCHPAD = 0x25; // попросили сохранить скратчпад в EEPROM
enum DeviceState {
  DS_WaitingReset,
  DS_WaitingCommand,
  DS_ReadingScratchpad,
  DS_SendingScratchpad
};
volatile DeviceState state = DS_WaitingReset;
volatile byte scratchpadWritePtr = 0; // указатель на байт в скратчпаде, куда надо записать пришедший от мастера байт
volatile byte scratchpadNumOfBytesReceived = 0; // сколько байт прочитали от мастера
volatile bool firstTimeReceiveScratchpad = true; // флаг, что мы в первый раз получили скратчпад от мастера, и нам надо обновить Nextion
//----------------------------------------------------------------------------------------------------------------
void ReadROM()
{

    memset((void*)&scratchpadS,0,sizeof(scratchpadS));

    // пишем свой уникальный ID
    scratchpadS.rf_id = RF_MODULE_ID; 
    scratchpadS.packet_type = 2; // говорим, что это тип пакета - дисплей Nextion

    RecalcScratchpadChecksum();
    memcpy((void*)&savedScratchpad,(void*)&scratchpadS,sizeof(scratchpadS));
    
}
//----------------------------------------------------------------------------------------------------------------
void WriteROM()
{
   RecalcScratchpadChecksum();
}
//----------------------------------------------------------------------------------------------------------------
NextionController nextion;
volatile unsigned long rotationTimer = 0; // таймер авторотации
const unsigned int ROTATION_INTERVAL = 7000; // интервал авторотации
volatile bool isDisplaySleep = false;
volatile int8_t currentSensorIndex = -1;
//----------------------------------------------------------------------------------------------------------------
void displayNextSensorData(int8_t dir)
{
  if(isDisplaySleep)
    return;  

  if(!scratchpadS.dataCount) // нет данных с датчиков
    return;

  currentSensorIndex += dir; // прибавляем направление
  if(currentSensorIndex < 0)
  {
     // надо искать последний элемент
     currentSensorIndex = scratchpadS.dataCount-1;  
  }

  if(currentSensorIndex >= scratchpadS.dataCount)
    currentSensorIndex = 0;

  if(currentSensorIndex < 0)
    currentSensorIndex = 0;

  byte type = scratchpadS.data[currentSensorIndex].type;
  switch(type)
  {
      case StateTemperature:
      {
        Temperature t; t.Value = scratchpadS.data[currentSensorIndex].data[1]; t.Fract = scratchpadS.data[currentSensorIndex].data[0];
        nextion.showTemperature(t);
      }
      break;

      case StateHumidity:
      {
        Temperature t; t.Value = scratchpadS.data[currentSensorIndex].data[1]; t.Fract = scratchpadS.data[currentSensorIndex].data[0];
        nextion.showHumidity(t);
      }
      break;
    
      case StateLuminosity:
      {
        long lum = 0;
        memcpy(&lum,scratchpadS.data[currentSensorIndex].data,2); 
        nextion.showLuminosity(lum);

      }
      break;

  } // switch
}
//----------------------------------------------------------------------------------------------------------------
void RecalcScratchpadChecksum() {

  scratchpadS.crc8 = OneWireSlave::crc8((const byte*) scratchpad,sizeof(scratchpadS)-1);
}
//----------------------------------------------------------------------------------------------------------------
void nSleep(NextionAbstractController* Sender)
{
  UNUSED(Sender);
  scratchpadS.controllerStatus |= 64;
  isDisplaySleep = true;

   RecalcScratchpadChecksum();

}
//----------------------------------------------------------------------------------------------------------------
void nWake(NextionAbstractController* Sender)
{
  UNUSED(Sender);
  scratchpadS.controllerStatus &= ~64;
  isDisplaySleep = false;

  RecalcScratchpadChecksum();
}
//----------------------------------------------------------------------------------------------------------------
void nString(NextionAbstractController* Sender, const char* str)
{
  UNUSED(Sender);

  if(!strcmp_P(str,(const char*)F("w_open")))
  {
    // попросили открыть окна
    scratchpadS.nextionStatus1 |= 2;
    RecalcScratchpadChecksum();
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("w_close")))
  {
    // попросили закрыть окна
    scratchpadS.nextionStatus1 |= 1;
    RecalcScratchpadChecksum();
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("w_auto")))
  {
    // попросили перевести в автоматический режим окон
    scratchpadS.nextionStatus1 |= 4;
    RecalcScratchpadChecksum();
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("w_manual")))
  {
    // попросили перевести в ручной режим работы окон
    scratchpadS.nextionStatus1 |= 8;
    RecalcScratchpadChecksum();
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("wtr_on")))
  {
    // попросили включить полив
    scratchpadS.nextionStatus1 |= 16;
    RecalcScratchpadChecksum();
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("wtr_off")))
  {
    // попросили выключить полив
    scratchpadS.nextionStatus1 |= 32;
    RecalcScratchpadChecksum();
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("wtr_auto")))
  {
    // попросили перевести в автоматический режим работы полива
    scratchpadS.nextionStatus1 |= 64;
    RecalcScratchpadChecksum();
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("wtr_manual")))
  {
    // попросили перевести в ручной режим работы полива
    scratchpadS.nextionStatus1 |= 128;
    RecalcScratchpadChecksum();
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("lht_on")))
  {
    // попросили включить досветку
    scratchpadS.nextionStatus2 |= 1;
    RecalcScratchpadChecksum();
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("lht_off")))
  {
    // попросили выключить досветку
    scratchpadS.nextionStatus2 |= 2;
    RecalcScratchpadChecksum();
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("lht_auto")))
  {
    // попросили перевести досветку в автоматический режим
    scratchpadS.nextionStatus2 |= 4;
    RecalcScratchpadChecksum();
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("lht_manual")))
  {
    // попросили перевести досветку в ручной режим
    scratchpadS.nextionStatus2 |= 8;
    RecalcScratchpadChecksum();
    return;
  }
  
 if(!strcmp_P(str,(const char*)F("topen_down")))
 {
    // листают температуру открытия вниз
    scratchpadS.nextionStatus2 |= 32; 
    RecalcScratchpadChecksum();   
    return;
  }  

 if(!strcmp_P(str,(const char*)F("topen_up")))
  {
    // листают температуру открытия вверх
    scratchpadS.nextionStatus2 |= 16;
    RecalcScratchpadChecksum();
    return;
  }

 if(!strcmp_P(str,(const char*)F("tclose_down")))
  {
    // листают температуру закрытия вниз
   scratchpadS.nextionStatus2 |= 128;
   RecalcScratchpadChecksum();
    return;
  }  

 if(!strcmp_P(str,(const char*)F("tclose_up")))
  {
    // листают температуру закрытия вверх
    scratchpadS.nextionStatus2 |= 64;
    RecalcScratchpadChecksum();
    return;
  }

 if(!strcmp_P(str,(const char*)F("prev")))
  {
    rotationTimer = millis();
    displayNextSensorData(-1);
    return;
  }

 if(!strcmp_P(str,(const char*)F("next")))
  {
    rotationTimer = millis();
    displayNextSensorData(1);
    return;
  }
  
}
//----------------------------------------------------------------------------------------------------------------
void owReceive(OneWireSlave::ReceiveEvent evt, byte data);
//----------------------------------------------------------------------------------------------------------------
void setup()
{
    Serial.begin(NEXTION_SERIAL_SPEED);

    NextionSubscribeStruct ss;
    ss.OnStringReceived = nString;
    ss.OnSleep = nSleep;
    ss.OnWakeUp = nWake;
    nextion.subscribe(ss);
     
    nextion.begin(&Serial,NULL);
    
    nextion.setWaitTimerInterval();
    nextion.setSleepDelay();
    nextion.setWakeOnTouch();
    nextion.setEchoMode();
  
    ReadROM();

  RecalcScratchpadChecksum();
  
  OWSlave.setReceiveCallback(&owReceive);
  OWSlave.begin(owROM, oneWireData.getPinNumber());        

}
//----------------------------------------------------------------------------------------------------------------
void owSendDone(bool error) {
  UNUSED(error);
 // закончили посылать скратчпад мастеру
 state = DS_WaitingReset;
}
//----------------------------------------------------------------------------------------------------------------
void owReceive(OneWireSlave::ReceiveEvent evt, byte data)
{  
 switch (evt)
  {
  case OneWireSlave::RE_Byte:
    switch (state)
    {

     case DS_ReadingScratchpad: // читаем скратчпад от мастера

        // увеличиваем кол-во прочитанных байт
        scratchpadNumOfBytesReceived++;

        // пишем в скратчпад принятый байт
        scratchpad[scratchpadWritePtr] = data;
        // увеличиваем указатель записи
        scratchpadWritePtr++;

        // проверяем, всё ли прочитали
        if(scratchpadNumOfBytesReceived >= sizeof(scratchpadS)) {
          // всё прочитали, сбрасываем состояние на ожидание резета
          state = DS_WaitingReset;
          scratchpadNumOfBytesReceived = 0;
          scratchpadWritePtr = 0;
          scratchpadReceivedFromMaster = true; // говорим, что мы получили скратчпад от мастера
        }
        
     break; // DS_ReadingScratchpad
      
    case DS_WaitingCommand:
      switch (data)
      {
      case COMMAND_START_CONVERSION: // запустить конвертацию
        state = DS_WaitingReset;
        break;

      case COMMAND_READ_SCRATCHPAD: // попросили отдать скратчпад мастеру
        state = DS_SendingScratchpad;
//        OWSlave.beginWrite((const byte*)&scratchpadToSend, sizeof(scratchpadToSend), owSendDone);
        OWSlave.beginWrite((const byte*)scratchpad, sizeof(scratchpadS), owSendDone);
        break;

      case COMMAND_WRITE_SCRATCHPAD:  // попросили записать скратчпад, следом пойдёт скратчпад
          state = DS_ReadingScratchpad; // ждём скратчпада
          scratchpadWritePtr = 0;
          scratchpadNumOfBytesReceived = 0;
        break;

        case COMMAND_SAVE_SCRATCHPAD: // сохраняем скратчпад в память
          state = DS_WaitingReset;
          WriteROM();
        break;

      } // switch (data)
      break; // case DS_WaitingCommand

      case DS_WaitingReset:
      break;

      case DS_SendingScratchpad:
      break;
    } // switch(state)
    break; 

  case OneWireSlave::RE_Reset:
    state = DS_WaitingCommand;
    break;

  case OneWireSlave::RE_Error:
    state = DS_WaitingReset;
    break;
    
  } // switch (evt)
}
//----------------------------------------------------------------------------------------------------------------
void loop()
{

  // проверяем, надо ли обновить Nextion
  if(scratchpadReceivedFromMaster)
  {
    // получили скратчпад от мастера
    RecalcScratchpadChecksum();

      if(firstTimeReceiveScratchpad || (savedScratchpad.controllerStatus & 1) != (scratchpadS.controllerStatus & 1))
      {
        nextion.notifyWindowState(scratchpadS.controllerStatus & 1);
      }

      if(firstTimeReceiveScratchpad || (savedScratchpad.controllerStatus & 2) != (scratchpadS.controllerStatus & 2))
      {
        nextion.notifyWindowMode(scratchpadS.controllerStatus & 2);
      }

      if(firstTimeReceiveScratchpad || (savedScratchpad.controllerStatus & 4) != (scratchpadS.controllerStatus & 4))
      {
        nextion.notifyWaterState(scratchpadS.controllerStatus & 4);
      }

      if(firstTimeReceiveScratchpad || (savedScratchpad.controllerStatus & 8) != (scratchpadS.controllerStatus & 8) )
      {
        nextion.notifyWaterMode(scratchpadS.controllerStatus & 8);
      }

      if(firstTimeReceiveScratchpad || (savedScratchpad.controllerStatus & 16) != (scratchpadS.controllerStatus & 16))
      {
        nextion.notifyLightState(scratchpadS.controllerStatus & 16);
      }

      if(firstTimeReceiveScratchpad || (savedScratchpad.controllerStatus & 32) != (scratchpadS.controllerStatus & 32))
      {
        nextion.notifyLightMode(scratchpadS.controllerStatus & 32);
      }

      if(firstTimeReceiveScratchpad || savedScratchpad.openTemperature != scratchpadS.openTemperature)
      {
        nextion.showOpenTemp(scratchpadS.openTemperature);
      }

      if(firstTimeReceiveScratchpad || savedScratchpad.closeTemperature != scratchpadS.closeTemperature)
      {
        nextion.showCloseTemp(scratchpadS.closeTemperature);
      }

      memcpy((void*)&savedScratchpad,(void*)&scratchpadS,sizeof(scratchpadS));
      scratchpadReceivedFromMaster = false;
      firstTimeReceiveScratchpad = false;
      
  } // scratchpadReceivedFromMaster

  // крутим показания на экране ожидания
  unsigned long curMillis = millis();
  if( (curMillis - rotationTimer) > NEXTION_ROTATION_INTERVAL)
  {
    rotationTimer = curMillis;
    displayNextSensorData(1);
  }  

  nextion.update();

}
//----------------------------------------------------------------------------------------------------------------
