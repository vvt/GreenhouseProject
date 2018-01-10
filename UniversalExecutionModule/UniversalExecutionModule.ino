#include <avr/io.h>
#include <avr/interrupt.h>
#include "Common.h"
#include "LowLevel.h"
#include "OneWireSlave.h"
//----------------------------------------------------------------------------------------------------------------
/*
Прошивка для универсального модуля, предназначена для выноса
состояния контроллера по 1-Wire.

Также поддерживается работа по RS-485, для включения этой возможности
надо раскомментировать USE_RS485_GATE.

Также поддерживается работа по радиоканалу, используя модуль nRF24L01+,
для этой возможности раскомментируйте USE_NRF.

Также поддерживается возможность использования обратной связи по положению
окон и срабатывания концевиков, для этой возможности раскомментируйте USE_FEEDBACK.

ВНИМАНИЕ!

Если возможность обратной связи не используется (директива USE_FEEDBACK закомментирована) -
надо ногу разрешения приёма микросхемы преобразователя RS485 принудительно посадить на землю, чтобы не занимать лишний пин МК!


ВНИМАНИЕ!

RS-485 работает через аппаратный UART (RX0 и TX0 ардуины)!
Перед прошивкой выдёргивать контакты модуля RS-485 из пинов аппаратного UART!

*/
//----------------------------------------------------------------------------------------------------------------
// ВНИМАНИЕ! Пин номер 2 - не занимать, через него работает 1-Wire!
//----------------------------------------------------------------------------------------------------------------



//----------------------------------------------------------------------------------------------------------------
// НАСТРОЙКИ ОБРАТНОЙ СВЯЗИ
//----------------------------------------------------------------------------------------------------------------
#define USE_FEEDBACK // закомментировать, если не нужен функционал обратной связи (4 канала положения окон + 
// состояние концевиков открытия и закрытия каждого окна)
//----------------------------------------------------------------------------------------------------------------
#define WINDOWS_SERVED 4 // Сколько окон обслуживается (максимум - 4)
//----------------------------------------------------------------------------------------------------------------
#define FEEDBACK_UPDATE_INTERVAL 1000 // интервал между обновлениями статусов окон. Каждое окно обновляет свой статус
// через этот интервал, таким образом полный цикл обновления равняется FEEDBACK_UPDATE_INTERVAL*WINDOWS_SERVED.
// мы не можем читать информацию прямо в процессе обработки входящего по RS485 паката, поэтому делаем слепок
// состояния через определённые промежутки времени.
//----------------------------------------------------------------------------------------------------------------
// настройки MCP23017
//----------------------------------------------------------------------------------------------------------------
#define COUNT_OF_MCP23017_EXTENDERS 2 // сколько расширителей портов MCP23017 используется
//----------------------------------------------------------------------------------------------------------------
// адреса расширителей MCP23017, через запятую, кол-вом COUNT_OF_MCP23017_EXTENDERS
// 0 - первый адрес 0x20, 1 - второй адрес 0x21 и т.п.
#define MCP23017_ADDRESSES 4,5
//----------------------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------------------
// настройки адресации модуля обратной связи в системе
//----------------------------------------------------------------------------------------------------------------
/*
 адресация осуществляется путём чтения определённых каналов микросхему MCP23017 или путём регистрации на контроллере.
 Если используется адресация через MCP23017 - каждый их этих каналов заведён на переключатель, всего каналов - 4.
 Таким образом, кол-во адресов - 16, т.е. можно использовать максимально 16 модулей.
 */

// закомментировать эту строчку, если не нужна адресация переключателями на плате - 
// в этом случае адресация будет осуществляться через регистрацию в контроллере
//#define ADDRESS_THROUGH_MCP 

#define ADDRESS_MCP_NUMBER 0 // номер микросхемы MCP23017, обслуживающей каналы адресации, на шине I2C
#define ADDRESS_CHANNEL1 1 // номер канала микросхемы MCP23017 для первого бита адреса
#define ADDRESS_CHANNEL2 2 // номер канала микросхемы MCP23017 для второго бита адреса
#define ADDRESS_CHANNEL3 3 // номер канала микросхемы MCP23017 для третьего бита адреса
#define ADDRESS_CHANNEL4 4 // номер канала микросхемы MCP23017 для четвёртого бита адреса

//----------------------------------------------------------------------------------------------------------------
// настройки привязок управления каналами активности инклинометров на шине I2C 
// ( управление каналами I2C осуществляется через микросхему PCA9516A)
// ВНИМАНИЕ! кол-во записей - равно WINDOWS_SERVED !!!
// записи - через запятую, одна запись имеет формат { MCP_NUMBER, CHANNEL_NUMBER }, где
// MCP_NUMBER - номер микросхемы по порядку (0 - первая микросхема, 1 - вторая и т.п.),
// CHANNEL_NUMBER - номер канала микросхемы, который обслуживает инклинометр
//----------------------------------------------------------------------------------------------------------------
// по умолчанию -  MCP23017 номер 0 (адрес на шине из MCP23017_ADDRESSES - 4).
// каналы управления линиями инклинометров - 5,6,7,8
#define MCP23017_INCLINOMETER_SETTINGS {0,5}, {0,6}, {0,7}, {0,8} 
//----------------------------------------------------------------------------------------------------------------
// настройки привязок концевиков крайних положений
// концевики крайних положений обслуживаются через MCP23017, их количество равно WINDOWS_SERVED,
// для каждого окна - два концевика на открытие и закрытие.
// записи - через запятую, каждая запись имеет формат { MCP_NUMBER, MCP_CHANNEL_OPEN_SWITCH, MCP_CHANNEL_CLOSE_SWITCH }, где
// MCP_NUMBER - номер микросхемы по порядку (0 - первая микросхема, 1 - вторая и т.п.),
// MCP_CHANNEL_OPEN_SWITCH - канал микросхемы, с которого читается уровень концевика открытия
// MCP_CHANNEL_CLOSE_SWITCH - канал микросхемы, с которого читается уровень концевика закрытия
//----------------------------------------------------------------------------------------------------------------
// по умолчанию -  MCP23017 номер 1 (адрес на шине из MCP23017_ADDRESSES - 5).
// каналы считывания позиций концевиков - 0,1     2,3      4,5      6,7
#define MCP23017_SWITCH_SETTINGS        {0,0,1}, {0,2,3}, {0,4,5}, {0,6,7} 
//----------------------------------------------------------------------------------------------------------------
// настройки управления приёмом/передачей RS485
//----------------------------------------------------------------------------------------------------------------
#define RS485_MCP23017_NUMBER 0 // номер микросхемы MCP23017, через которую управляем направлением приём/передача по RS485
#define RS485_MCP23017_CNANNEL 9 // номер канала микросхемы MCP23017, через который управляем направлением приём/передача по RS485

#define USE_DIRECT_RS485_DE_PIN // если раскомментировано, то управление пином приёма/передачи RS485 будет идти по пину ниже,
// иначе - по каналу MCP23017 (настройки чуть выше)
#define DIRECT_RS485_PIN 4 // номер пина для прямого управления переключением приём/передача RS485
//----------------------------------------------------------------------------------------------------------------
// настройки уровней
//----------------------------------------------------------------------------------------------------------------
#define INCLINOMETER_CHANNEL_OFF LOW // уровень, нужный для выключения канала I2C инклинометров, не участвующих в опросе позиции конкретного окна
#define INCLINOMETER_CHANNEL_ON HIGH // уровень, нужный для включения канала I2C инклинометров, не участвующих в опросе позиции конкретного окна

#define OPEN_SWITCH_TRIGGERED_LEVEL HIGH // уровень, при котором концевик открытия считается сработавшим
#define CLOSE_SWITCH_TRIGGERED_LEVEL HIGH // уровень, при котором концевик закрытия считается сработавшим
//----------------------------------------------------------------------------------------------------------------
// КОНЕЦ НАСТРОЕК ОБРАТНОЙ СВЯЗИ
//----------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------
// настройки RS-485
//----------------------------------------------------------------------------------------------------------------
#define USE_RS485_GATE // закомментировать, если не нужна работа через RS-485
#define RS485_SPEED 57600 // скорость работы по RS-485
//----------------------------------------------------------------------------------------------------------------
// настройки nRF
//----------------------------------------------------------------------------------------------------------------
//#define USE_NRF // закомментировать, если не надо работать через nRF.
/*
 nRF для своей работы занимает следующие пины: 9,10,11,12,13. 
 Следите за тем, чтобы номера пинов не пересекались в слотах, или с RS-485.
 */
#define NRF_CE_PIN 9 // номер пина CE для модуля nRF
#define NRF_CSN_PIN 10 // номер пина CSN для модуля nRF
#define DEFAULT_RF_CHANNEL 19 // номер канала для nRF по умолчанию
//#define NRF_AUTOACK_INVERTED // раскомментировать эту строчку здесь и в главной прошивке, если у вас они не коннектятся. 
// Иногда auto aсk в китайских модулях имеет инвертированное значение.
//----------------------------------------------------------------------------------------------------------------
// настройки
//----------------------------------------------------------------------------------------------------------------
#define ROM_ADDRESS (void*) 0 // по какому адресу у нас настройки?
//----------------------------------------------------------------------------------------------------------------
// настройки инициализации, привязка слотов к пинам и первоначальному состоянию
//----------------------------------------------------------------------------------------------------------------
#define RELAY_ON HIGH// уровень для включения нагрузки на канале
#define RELAY_OFF LOW // уровень для выключения нагрузки на канале
//----------------------------------------------------------------------------------------------------------------
// настройки привязки номеров локальных пинов к слотам контроллера.
// нужны для трансляции типа слота в конкретный пин, на которому будет управление нагрузкой
//----------------------------------------------------------------------------------------------------------------
/* 
Пины для платы исполнительного модуля

 D6
 A0
 A1
 D3
 A2
 A3
 A4
 A5
 
 По умолчанию на пинах - низкий уровень !!!
 */
//----------------------------------------------------------------------------------------------------------------
SlotSettings SLOTS[8] = 
{
  {3,   RELAY_OFF} // пин номер такой-то, начальное состояние RELAY_OFF
 ,{5,   RELAY_OFF} // и т.д. 0 вместо номера пина - нет поддержки привязки канала к пину
 ,{6,   RELAY_OFF}
 ,{7,   RELAY_OFF}
 ,{A0,  RELAY_OFF}
 ,{A1,  RELAY_OFF}
 ,{A2,  RELAY_OFF}
 ,{A3,  RELAY_OFF}
  
};
//----------------------------------------------------------------------------------------------------------------
//#define _DEBUG // раскомментировать для отладочного режима (плюётся в Serial, RS485, ясное дело, в таком режиме не работает)
//----------------------------------------------------------------------------------------------------------------
// Дальше лазить - неосмотрительно :)
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
t_scratchpad scratchpadS, scratchpadToSend;
volatile char* scratchpad = (char *)&scratchpadS; //что бы обратиться к scratchpad как к линейному массиву
volatile bool scratchpadReceivedFromMaster = false; // флаг, что мы получили данные с мастера
//----------------------------------------------------------------------------------------------------------------
// Настройки 1-Wire
//----------------------------------------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------------------------------------
RS485Packet rs485Packet; // пакет, в который мы принимаем данные
volatile byte* rsPacketPtr = (byte*) &rs485Packet;
volatile byte  rs485WritePtr = 0; // указатель записи в пакет
//----------------------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------------------
#ifdef USE_FEEDBACK
//----------------------------------------------------------------------------------------------------------------
#include "MCP23017.h"
//----------------------------------------------------------------------------------------------------------------
Adafruit_MCP23017* mcpExtenders[COUNT_OF_MCP23017_EXTENDERS] = {NULL};
byte mcpAddresses[COUNT_OF_MCP23017_EXTENDERS] = {MCP23017_ADDRESSES};
WindowStatus windowStatuses[WINDOWS_SERVED];
//----------------------------------------------------------------------------------------------------------------
void InitMCP23017()
{
  #ifdef _DEBUG
    Serial.println(F("Init MCP23017..."));
  #endif
  
  for(byte i=0;i<COUNT_OF_MCP23017_EXTENDERS;i++)
  {
    mcpExtenders[i] = new Adafruit_MCP23017;
    mcpExtenders[i]->begin(mcpAddresses[i]);
  }

  #ifdef _DEBUG
    Serial.println(F("MCP23017 inited!"));
  #endif
  
}
//----------------------------------------------------------------------------------------------------------------
byte moduleAddress = 0;
//----------------------------------------------------------------------------------------------------------------
void ReadModuleAddress()
{
  #ifdef _DEBUG
    Serial.println(F("Read module address..."));
  #endif

 #ifdef ADDRESS_THROUGH_MCP // адресуемся переключателями на плате

    Adafruit_MCP23017* mcp = mcpExtenders[ADDRESS_MCP_NUMBER];
  
    mcp->pinMode(ADDRESS_CHANNEL4,INPUT);
    byte bit1 = mcp->digitalRead(ADDRESS_CHANNEL4);
  
    mcp->pinMode(ADDRESS_CHANNEL3,INPUT);
    byte bit2 = mcp->digitalRead(ADDRESS_CHANNEL3);
  
    mcp->pinMode(ADDRESS_CHANNEL2,INPUT);
    byte bit3 = mcp->digitalRead(ADDRESS_CHANNEL2);
  
    mcp->pinMode(ADDRESS_CHANNEL1,INPUT);
    byte bit4 = mcp->digitalRead(ADDRESS_CHANNEL1);
  
    moduleAddress = bit1 | (bit2 << 1) | (bit3 << 2) | (bit4 << 3);

 #else
  // адресуемся чтением с конфига
  // старшие 4 бита байта config - это наш адрес
  moduleAddress = scratchpadS.config & 0xF0;
  if(moduleAddress > 15)
    moduleAddress = 0;  
 #endif

  #ifdef _DEBUG
    Serial.print(F("Module address is: "));
    Serial.println(moduleAddress);
  #endif    
}
//----------------------------------------------------------------------------------------------------------------
InclinometerSettings inclinometers[WINDOWS_SERVED] = {MCP23017_INCLINOMETER_SETTINGS};
FeedbackEndstop endstops[WINDOWS_SERVED] = { MCP23017_SWITCH_SETTINGS };
//----------------------------------------------------------------------------------------------------------------
void TurnInclinometerOff(InclinometerSettings& is)
{
 mcpExtenders[is.mcpNumber]->digitalWrite(is.mcpChannel,INCLINOMETER_CHANNEL_OFF); 
}
//----------------------------------------------------------------------------------------------------------------
void TurnInclinometerOn(InclinometerSettings& is)
{
  mcpExtenders[is.mcpNumber]->digitalWrite(is.mcpChannel,INCLINOMETER_CHANNEL_ON); 
}
//----------------------------------------------------------------------------------------------------------------
void TurnInclinometersOff()
{
  for(byte i=0;i<WINDOWS_SERVED;i++)
  {
      InclinometerSettings is = inclinometers[i];
      TurnInclinometerOff(is);
  }
}
//----------------------------------------------------------------------------------------------------------------
void GetWindowsStatus(byte windowNumber, byte& isCloseSwitchTriggered, byte& isOpenSwitchTriggered, byte& hasPosition, byte& position)
{
  WindowStatus* ws = &(windowStatuses[windowNumber]);
  isCloseSwitchTriggered = ws->isCloseSwitchTriggered;
  isOpenSwitchTriggered = ws->isOpenSwitchTriggered;
  hasPosition = ws->hasPosition;
  position = ws->position;
  
 
#ifdef _DEBUG
  Serial.print(F("Window #"));
  Serial.print(windowNumber);
  Serial.print(F(" status: hasPosition="));
  Serial.print(hasPosition);
  Serial.print(F("; position="));
  Serial.print(position);
  Serial.print(F("; close switch="));
  Serial.print(isCloseSwitchTriggered);
  Serial.print(F("; open switch="));
  Serial.println(isOpenSwitchTriggered);
#endif

}
//----------------------------------------------------------------------------------------------------------------
void UpdateWindowStatus(byte windowNumber)
{
  
  TurnInclinometersOff();
  InclinometerSettings inclinometer = inclinometers[windowNumber];

  // включаем инклинометр на шине I2C
  TurnInclinometerOn(inclinometer);
    
  //TODO: Тут актуальное чтение позиций окон!!!
  windowStatuses[windowNumber].hasPosition = 1;
  windowStatuses[windowNumber].position = (windowNumber+1)*5; // пока тупо, чисто для теста
  
  // теперь читаем позицию концевиков
  FeedbackEndstop endstop = endstops[windowNumber];
  
  Adafruit_MCP23017* mcp = mcpExtenders[endstop.mcpNumber];
  
  windowStatuses[windowNumber].isCloseSwitchTriggered = mcp->digitalRead(endstop.closeSwitchChannel) == CLOSE_SWITCH_TRIGGERED_LEVEL ? 1 : 0;
  windowStatuses[windowNumber].isOpenSwitchTriggered = mcp->digitalRead(endstop.openSwitchChannel) == OPEN_SWITCH_TRIGGERED_LEVEL ? 1 : 0; 
   
}
//----------------------------------------------------------------------------------------------------------------
void FillRS485PacketWithData(WindowFeedbackPacket* packet) // заполняем пакет обратной связи данными для RS485
{
  byte isCloseSwitchTriggered = 0;
  byte isOpenSwitchTriggered = 0;
  byte hasPosition = 0;
  byte position = 0;

  byte currentByteNumber = 0;
  int8_t currentBitNumber = 7;

  memset(packet->windowsStatus,0,20);
 
  
  for(int i=0;i<WINDOWS_SERVED;i++)
  {
    GetWindowsStatus(i, isCloseSwitchTriggered, isOpenSwitchTriggered, hasPosition, position);
    // тут мы получили состояния. У нас есть номер байта и номер бита,
    // с которого надо писать в поток. Как только бит исчерпан - переходим на следующий байт.
    // position  у нас занимает старшие биты, причём самый старший там - нам не нужен

    for(int k=6;k>=0;k--)
    {
      byte b = bitRead(position,k);
      packet->windowsStatus[currentByteNumber] |= (b << currentBitNumber);
      
      currentBitNumber--;
      if(currentBitNumber < 0)
      {
        currentBitNumber = 7;
        currentByteNumber++;
      }
      
    } // for

    // записали позицию, пишем информацию о том, есть ли позиция окна (третий по старшинству бит)
    packet->windowsStatus[currentByteNumber] |= (hasPosition << currentBitNumber);
    currentBitNumber--;
    if(currentBitNumber < 0)
    {
      currentBitNumber = 7;
      currentByteNumber++;
    }
    // теперь пишем информацию о концевике закрытия (второй бит)
    packet->windowsStatus[currentByteNumber] |= (isCloseSwitchTriggered << currentBitNumber);
    currentBitNumber--;
    if(currentBitNumber < 0)
    {
      currentBitNumber = 7;
      currentByteNumber++;
    }
    
    // теперь пишем информацию о концевике открытия (первый бит)
    packet->windowsStatus[currentByteNumber] |= (isOpenSwitchTriggered << currentBitNumber);
    currentBitNumber--;
    if(currentBitNumber < 0)
    {
      currentBitNumber = 7;
      currentByteNumber++;
    }
    
  } // for
 
}
//----------------------------------------------------------------------------------------------------------------
void ProcessFeedbackPacket()
{  
 
  WindowFeedbackPacket* packet = (WindowFeedbackPacket*) &(rs485Packet.data);
  
  // тут обрабатываем входящий пакет запроса о позиции окон
 
  if(packet->moduleNumber != moduleAddress)
  {      
    return; // пакет на для нас
  }

     rs485Packet.direction = RS485FromSlave;
     rs485Packet.type = RS485WindowsPositionPacket;
     packet->moduleNumber = moduleAddress;
     packet->windowsSupported = WINDOWS_SERVED;

     // тут заполняем пакет данными
     FillRS485PacketWithData(packet);     

     // подсчитываем CRC
     rs485Packet.crc8 = OneWireSlave::crc8((const byte*) &rs485Packet,sizeof(RS485Packet)-1 );

     #ifndef _DEBUG // в дебаг-режиме ничего не отсылаем
        // теперь переключаемся на передачу
        RS485Send();
        
        // пишем в порт данные
        Serial.write((const uint8_t *)&rs485Packet,sizeof(RS485Packet));
        
        // ждём окончания передачи
        RS485waitTransmitComplete();
        
        // переключаемся на приём
        RS485Receive();
      
        
    #endif  
   
      
}
//----------------------------------------------------------------------------------------------------------------
void RS485Receive()
{
  // переводим контроллер RS-485 на приём
  #ifdef USE_DIRECT_RS485_DE_PIN
    digitalWrite(DIRECT_RS485_PIN,LOW);
  #else
    mcpExtenders[RS485_MCP23017_NUMBER]->digitalWrite(RS485_MCP23017_CNANNEL,LOW);
  #endif
  
  #ifdef _DEBUG
    Serial.println(F("Switch RS485 to receive."));
  #endif  
}
//----------------------------------------------------------------------------------------------------------------
void RS485Send()
{
  // переводим контроллер RS-485 на передачу
  #ifdef USE_DIRECT_RS485_DE_PIN
    digitalWrite(DIRECT_RS485_PIN,HIGH);
  #else
    mcpExtenders[RS485_MCP23017_NUMBER]->digitalWrite(RS485_MCP23017_CNANNEL,HIGH);
  #endif
  
  #ifdef _DEBUG
    Serial.println(F("Switch RS485 to send."));
  #endif  
}
//----------------------------------------------------------------------------------------------------------------
void RS485waitTransmitComplete()
{
  // ждём завершения передачи по UART
  while(!(UCSR0A & _BV(TXC0) ));
}
//----------------------------------------------------------------------------------------------------------------
void InitEndstops()
{
  #ifdef _DEBUG
    Serial.println(F("Init endstops...."));
  #endif  

    for(byte i=0;i<WINDOWS_SERVED;i++)
    {
      FeedbackEndstop es = endstops[i];
      mcpExtenders[es.mcpNumber]->pinMode(es.openSwitchChannel,INPUT);
      mcpExtenders[es.mcpNumber]->pinMode(es.closeSwitchChannel,INPUT);
    } // for
  
  #ifdef _DEBUG
    Serial.println(F("Endstops inited."));
  #endif  
}
//----------------------------------------------------------------------------------------------------------------
void InitInclinometers()
{
  #ifdef _DEBUG
    Serial.println(F("Init inclinometers...."));
  #endif  

    for(byte i=0;i<WINDOWS_SERVED;i++)
    {
      InclinometerSettings is = inclinometers[i];
      mcpExtenders[is.mcpNumber]->pinMode(is.mcpChannel,OUTPUT);
      TurnInclinometerOff(is);
    } // for

    for(byte i=0;i<WINDOWS_SERVED;i++)
    {
      InclinometerSettings is = inclinometers[i];
      
      TurnInclinometerOn(is);

      //TODO: Тут инициализация инклинометра!!!!!
        #ifdef _DEBUG
          Serial.println(F("Init inclinometer - NOT IMPLEMENTED!!!"));
        #endif  
      
      TurnInclinometerOff(is);
    } // for
      
  
  #ifdef _DEBUG
    Serial.println(F("Inclinometers inited."));
  #endif    
}
//----------------------------------------------------------------------------------------------------------------
#endif // USE_FEEDBACK
//----------------------------------------------------------------------------------------------------------------
void UpdateFromControllerState(ControllerState* state)
{
     // у нас есть слепок состояния контроллера, надо искать в слотах привязки
     for(byte i=0;i<8;i++)
     {
        UniSlotData* slotData = &(scratchpadS.slots[i]);

        byte slotStatus = RELAY_OFF;
        byte slotType = slotData->slotType;
       
        if(slotType == 0 || slotType == 0xFF) // нет привязки
          continue;

        switch(slotType)
        {

            case slotWindowLeftChannel:
            {
              // состояние левого канала окна, в slotLinkedData - номер окна
              byte windowNumber = slotData->slotLinkedData;
              if(windowNumber < 16)
              {
                // окна у нас нумеруются от 0 до 15, всего 16 окон.
                // на каждое окно - два бита, для левого и правого канала.
                // следовательно, чтобы получить стартовый бит - надо номер окна
                // умножить на 2.
                byte bitNum = windowNumber*2;           
                if(state->WindowsState & (1 << bitNum))
                  slotStatus = RELAY_ON; // выставляем в слоте значение 1
              }
            }
            break;
    
            case slotWindowRightChannel:
            {
              // состояние левого канала окна, в slotLinkedData - номер окна
              byte windowNumber = slotData->slotLinkedData;
              if(windowNumber < 16)
              {
                // окна у нас нумеруются от 0 до 15, всего 16 окон.
                // на каждое окно - два бита, для левого и правого канала.
                // следовательно, чтобы получить стартовый бит - надо номер окна
                // умножить на 2.
                byte bitNum = windowNumber*2;
    
                // поскольку канал у нас правый - его бит идёт следом за левым.
                bitNum++;
                           
                if(state->WindowsState & (1 << bitNum))
                  slotStatus = RELAY_ON; // выставляем в слоте значение 1
              }
            }
            break;
    
            case slotWateringChannel:
            {
              // состояние канала полива, в slotLinkedData - номер канала полива
              byte wateringChannel = slotData->slotLinkedData;
              if(wateringChannel< 16)
              {
                if(state->WaterChannelsState & (1 << wateringChannel))
                  slotStatus = RELAY_ON; // выставляем в слоте значение 1
                  
              }
            }        
            break;
    
            case slotLightChannel:
            {
              // состояние канала досветки, в slotLinkedData - номер канала досветки
              byte lightChannel = slotData->slotLinkedData;
              if(lightChannel < 8)
              {
                if(state->LightChannelsState & (1 << lightChannel))
                  slotStatus = RELAY_ON; // выставляем в слоте значение 1
                  
              }
            }
            break;
    
            case slotPin:
            {
              // получаем статус пина
              byte pinNumber = slotData->slotLinkedData;
              byte byteNum = pinNumber/8;
              byte bitNum = pinNumber%8;

              slotStatus = LOW;
     
              if(byteNum < 16)
              {
                // если нужный бит с номером пина установлен - на пине высокий уровень
                if(state->PinsState[byteNum] & (1 << bitNum))
                  slotStatus = HIGH; // выставляем в слоте значение 1
              }
              
            }
            break;

            default:
              continue;
            
          } // switch


            // проверяем на изменения
             if(slotStatus != SLOTS[i].State)
              {
                 // состояние слота изменилось, запоминаем его
                SLOTS[i].State = slotStatus;
                
                if(SLOTS[i].Pin)
                {
                    digitalWrite(SLOTS[i].Pin, slotStatus);
                }
                
              } // if(slotStatus != SLOTS[i].State)

                    
     } // for  
}
//----------------------------------------------------------------------------------------------------------------
#ifdef USE_NRF
//----------------------------------------------------------------------------------------------------------------
uint64_t controllerStatePipe = 0xF0F0F0F0E0LL; // труба, с которой мы слушаем состояние контроллера
//----------------------------------------------------------------------------------------------------------------
#include "RF24.h"
RF24 radio(NRF_CE_PIN,NRF_CSN_PIN);
bool nRFInited = false;
//----------------------------------------------------------------------------------------------------------------
/*
int serial_putc( char c, FILE * ) {
  Serial.write( c );
  return c;
}

void printf_begin(void) {
  fdevopen( &serial_putc, 0 );
  Serial.begin(57600);
  Serial.println(F("Init nRF..."));
}
*/
//----------------------------------------------------------------------------------------------------------------
void initNRF()
{
  //printf_begin();
  
  // инициализируем nRF
  nRFInited = radio.begin();

  if(nRFInited)
  {
  delay(200); // чуть-чуть подождём

  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(scratchpadS.rf_id);
  radio.setRetries(15,15);
  radio.setPayloadSize(sizeof(NRFControllerStatePacket)); // у нас 30 байт на пакет
  radio.setCRCLength(RF24_CRC_16);
  radio.setAutoAck(
    #ifdef NRF_AUTOACK_INVERTED
      false
    #else
    true
    #endif
    );

  // открываем трубу состояния контроллера на прослушку
  radio.openReadingPipe(1,controllerStatePipe);
  radio.startListening(); // начинаем слушать

  //radio.printDetails();

 // Serial.println(F("Ready."));
  } // nRFInited
  
}
//----------------------------------------------------------------------------------------------------------------
void ProcessNRF()
{
  if(!nRFInited)
    return;
    
  static NRFControllerStatePacket nrfPacket; // наш пакет, в который мы принимаем данные с контроллера
  uint8_t pipe_num = 0; // из какой трубы пришло
  if(radio.available(&pipe_num))
  {
   // Serial.println(F("Got packet from nRF"));
    
    memset(&nrfPacket,0,sizeof(NRFControllerStatePacket));
    radio.read(&nrfPacket,sizeof(NRFControllerStatePacket));

    if(nrfPacket.controller_id == scratchpadS.controller_id)
    {
       // это пакет с нашего контроллера пришёл, обновляем данные
       byte checksum = OneWireSlave::crc8((const byte*) &nrfPacket,sizeof(NRFControllerStatePacket)-1);
       
       if(checksum == nrfPacket.crc8) // чексумма сошлась
       {
      //  Serial.println(F("Update from nRF"));
        UpdateFromControllerState(&(nrfPacket.state));
       }
    }
  }
}
//----------------------------------------------------------------------------------------------------------------
#endif // USE_NRF
//----------------------------------------------------------------------------------------------------------------
void ReadROM()
{
    memset((void*)&scratchpadS,0,sizeof(scratchpadS));
    eeprom_read_block((void*)&scratchpadS, ROM_ADDRESS, 29);

    // пишем номер канала по умолчанию
    if(scratchpadS.rf_id == 0xFF || scratchpadS.rf_id == 0)
      scratchpadS.rf_id = DEFAULT_RF_CHANNEL; 
      
    scratchpadS.packet_type = uniExecutionClient; // говорим, что это тип пакета - исполнительный модуль
    scratchpadS.packet_subtype = 0;

    // говорим, что никакой калибровки не поддерживаем
    scratchpadS.config &= ~2; // второй бит убираем по-любому


    #ifdef USE_FEEDBACK
      #ifndef ADDRESS_THROUGH_MCP
        // адресуемся из конфига, ставим второй бит
        scratchpadS.config |= 2;
      #endif
    #endif

}
//----------------------------------------------------------------------------------------------------------------
void WriteROM()
{
    eeprom_write_block( (void*)scratchpad,ROM_ADDRESS,29);
    memcpy(&scratchpadToSend,&scratchpadS,sizeof(scratchpadS));

    #ifdef USE_NRF
      if(nRFInited)
      {
      // переназначаем канал радио
      radio.stopListening();
      radio.setChannel(scratchpadS.rf_id);
      radio.startListening();
      }
    #endif


}
//----------------------------------------------------------------------------------------------------------------
#ifdef USE_RS485_GATE // сказали работать ещё и через RS-485
//----------------------------------------------------------------------------------------------------------------
/*
 Структура пакета, передаваемого по RS-495:
 
   0xAB - первый байт заголовка
   0xBA - второй байт заголовка

   данные, в зависимости от типа пакета
   
   0xDE - первый байт окончания
   0xAD - второй байт окончания

 
 */
//----------------------------------------------------------------------------------------------------------------
bool GotRS485Packet()
{
  // проверяем, есть ли у нас валидный RS-485 пакет
  return rs485WritePtr > ( sizeof(RS485Packet)-1 );
}
//----------------------------------------------------------------------------------------------------------------
void ProcessRS485Packet()
{
  // обрабатываем входящий пакет. Тут могут возникнуть проблемы с синхронизацией
  // начала пакета, поэтому мы сначала ищем заголовок и убеждаемся, что он валидный. 
  // если мы нашли заголовок и он не в начале пакета - значит, с синхронизацией проблемы,
  // и мы должны сдвинуть заголовок в начало пакета, чтобы потом дочитать остаток.
  if(!(rs485Packet.header1 == 0xAB && rs485Packet.header2 == 0xBA))
  {
     // заголовок неправильный, ищем возможное начало пакета
     byte readPtr = 0;
     bool startPacketFound = false;
     while(readPtr < sizeof(RS485Packet))
     {
       if(rsPacketPtr[readPtr] == 0xAB)
       {
        startPacketFound = true;
        break;
       }
        readPtr++;
     } // while

     if(!startPacketFound) // не нашли начало пакета
     {
        rs485WritePtr = 0; // сбрасываем указатель чтения и выходим
        return;
     }

     if(readPtr == 0)
     {
      // стартовый байт заголовка найден, но он в нулевой позиции, следовательно - что-то пошло не так
        rs485WritePtr = 0; // сбрасываем указатель чтения и выходим
        return;       
     } // if

     // начало пакета найдено, копируем всё, что после него, перемещая в начало буфера
     byte writePtr = 0;
     byte bytesWritten = 0;
     while(readPtr < sizeof(RS485Packet) )
     {
      rsPacketPtr[writePtr++] = rsPacketPtr[readPtr++];
      bytesWritten++;
     }

     rs485WritePtr = bytesWritten; // запоминаем, куда писать следующий байт
     return;
         
  } // if
  else
  {
    // заголовок правильный, проверяем окончание
    if(!(rs485Packet.tail1 == 0xDE && rs485Packet.tail2 == 0xAD))
    {
      // окончание неправильное, сбрасываем указатель чтения и выходим
      rs485WritePtr = 0;
      return;
    }
    // данные мы получили, сразу обнуляем указатель записи, чтобы не забыть
    rs485WritePtr = 0;

    // проверяем контрольную сумму
    byte crc = OneWireSlave::crc8((const byte*) rsPacketPtr,sizeof(RS485Packet) - 1);
    if(crc != rs485Packet.crc8)
    {
      // не сошлось, игнорируем
      return;
    }


    // всё в пакете правильно, анализируем и выполняем
    // проверяем, наш ли пакет
    if(rs485Packet.direction != RS485FromMaster) // не от мастера пакет
      return;

    if(!(rs485Packet.type == RS485ControllerStatePacket 

    #ifdef USE_FEEDBACK
    || rs485Packet.type == RS485WindowsPositionPacket
    #endif
    
    )) // пакет не c состоянием контроллера
      return;


      if(rs485Packet.type == RS485ControllerStatePacket)
      {
       // теперь приводим пакет к нужному виду
       ControllerState* state = (ControllerState*) &(rs485Packet.data);
       UpdateFromControllerState(state);
      }
      #ifdef USE_FEEDBACK
      else if(rs485Packet.type == RS485WindowsPositionPacket)
      {
        ProcessFeedbackPacket();
      }
      #endif


    
  } // else
}
//----------------------------------------------------------------------------------------------------------------
void ProcessIncomingRS485Packets() // обрабатываем входящие пакеты по RS-485
{
  while(Serial.available())
  {
    rsPacketPtr[rs485WritePtr++] = (byte) Serial.read();
   
    if(GotRS485Packet())
      ProcessRS485Packet();
  } // while
  
}
//----------------------------------------------------------------------------------------------------------------
void InitRS485()
{
  memset(&rs485Packet,0,sizeof(RS485Packet));

  #ifdef USE_FEEDBACK

  #ifdef _DEBUG
    Serial.println(F("Init RS485 DE pin..."));
  #endif

  #ifdef USE_DIRECT_RS485_DE_PIN
    pinMode(DIRECT_RS485_PIN,OUTPUT);
  #else
    mcpExtenders[RS485_MCP23017_NUMBER]->pinMode(RS485_MCP23017_CNANNEL,OUTPUT);
  #endif
  
  RS485Receive();

  #ifdef _DEBUG
    Serial.println(F("RS485 DE pin inited!"));
  #endif
    
  #endif // USE_FEEDBACK

}
//----------------------------------------------------------------------------------------------------------------
#endif // USE_RS485_GATE
//----------------------------------------------------------------------------------------------------------------
void UpdateSlots1Wire()
{  
 #ifdef _DEBUG
  Serial.println(F("Update slots from 1-Wire..."));
 #endif
  
  for(byte i=0;i<8;i++)
  {
    byte slotType = scratchpadS.slots[i].slotType;
    if(slotType > 0 && slotType != 0xFF)
    {
      // на слот назначены настройки, надо обновить состояние связанного пина
      byte slotStatus =  scratchpadS.slots[i].slotStatus ? RELAY_ON : RELAY_OFF;
      if(slotType == slotPin)
        slotStatus = scratchpadS.slots[i].slotStatus ? HIGH : LOW;
      
      if(!(slotStatus == HIGH || slotStatus == LOW)) // записан мусор в статусе слота
        continue;
        
      if(slotStatus != SLOTS[i].State)
      {        
        // состояние изменилось
        SLOTS[i].State = slotStatus;
        
        if(SLOTS[i].Pin) {
           #ifdef _DEBUG
            Serial.print(F("Writing "));
            Serial.print(slotStatus);
            Serial.print(F(" to pin "));
            Serial.println(SLOTS[i].Pin);
           #endif          
            digitalWrite(SLOTS[i].Pin, slotStatus);
        }
      }
      
    } // if
    
  } // for
}
//----------------------------------------------------------------------------------------------------------------
void owReceive(OneWireSlave::ReceiveEvent evt, byte data);
//----------------------------------------------------------------------------------------------------------------
void setup()
{
 ReadROM(); // читаем наши настройки
 
 #ifdef USE_RS485_GATE // если сказано работать через RS-485 - работаем 
    Serial.begin(RS485_SPEED);
 #endif

 #ifdef _DEBUG
  #ifndef USE_RS485_GATE 
    Serial.begin(57600);
  #endif
  Serial.println(F("Debug mode..."));
 #endif
  
  #ifdef USE_FEEDBACK
    InitMCP23017(); // инициализируем расширители
    InitEndstops(); // инициализируем концевики
    InitInclinometers(); // инициализируем инклинометры
    ReadModuleAddress(); // читаем наш адрес

    #ifdef _DEBUG
      // тут тестовое заполнение пакета данными
      WindowFeedbackPacket* packet = (WindowFeedbackPacket*) &(rs485Packet.data);
      packet->moduleNumber = moduleAddress; 
  
      ProcessFeedbackPacket();
    #endif

  
  #endif
  
 #ifdef USE_RS485_GATE // если сказано работать через RS-485 - работаем 
    InitRS485(); // настраиваем RS-485 на приём
 #endif
  
   

    // настраиваем слоты, назначая им конкретные уровни при старте
    for(byte i=0;i<8;i++)
    {
      byte pin = SLOTS[i].Pin;
      if(pin)
      {
        Pin pPin(pin);
        pPin.outputMode();
        pPin.write(SLOTS[i].State);
      }
    } // for

    // настраиваем nRF
    #ifdef USE_NRF
    initNRF();
    #endif


  scratchpadS.crc8 = OneWireSlave::crc8((const byte*) scratchpad,sizeof(scratchpadS)-1);
  memcpy(&scratchpadToSend,&scratchpadS,sizeof(scratchpadS));
  
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
// обработчик прерывания на пине
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
        OWSlave.beginWrite((const byte*)&scratchpadToSend, sizeof(scratchpadToSend), owSendDone);
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
  if(scratchpadReceivedFromMaster)
  {
    // скратч был получен от мастера, тут можно что-то делать
    scratchpadReceivedFromMaster = false;

    UpdateSlots1Wire(); // обновляем состояние слотов
      
  } // scratchpadReceivedFromMaster

  #ifdef USE_RS485_GATE
    ProcessIncomingRS485Packets(); // обрабатываем входящие пакеты по RS-485
  #endif

  #ifdef USE_NRF
    ProcessNRF(); // обрабатываем входящие пакаты nRF
  #endif

  #ifdef USE_FEEDBACK

    static bool isFeedbackInited = false;
    if(!isFeedbackInited)
    {
      isFeedbackInited = true;
      for(byte i=0;i<WINDOWS_SERVED;i++)
      {
        UpdateWindowStatus(i);
      }
    }
    else
    {
  
      static unsigned long feedackTimer = millis();
      static int8_t cntr = -1;
      unsigned long curMillis = millis();
      
      if(curMillis - feedackTimer > FEEDBACK_UPDATE_INTERVAL)
      {
        
        feedackTimer = curMillis;
        cntr++;
        if(cntr >= WINDOWS_SERVED)
          cntr = 0;
  
          UpdateWindowStatus(cntr);
        
      } // if
    }  // else
  #endif // USE_FEEDBACK


}
//----------------------------------------------------------------------------------------------------------------
