#include <avr/io.h>
#include <avr/interrupt.h>
#include "Common.h"
#include "LowLevel.h"
#include "OneWireSlave.h"
//----------------------------------------------------------------------------------------------------------------
/*
Прошивка для универсального модуля, предназначена для подключения
любого типа поддерживаемых датчиков и передачи с них показаний по шине 1-Wire.
Также поддерживается работа по RS-485, для включения этой возможности
надо раскомментировать USE_RS485_GATE.

ВНИМАНИЕ! Модуль при работе по RS-485 работает только на приём информации!
Это значит, что ногу разрешения приёма микросхемы преобразователя RS485
надо принудительно посадить на землю, чтобы не занимать лишний пин МК!

Также поддерживается работа по радиоканалу, используя модуль nRF24L01+,
для этой возможности раскомментируйте USE_NRF.

ВНИМАНИЕ!

RS-485 работает через аппаратный UART (RX0 и TX0 ардуины)!
Перед прошивкой выдёргивать контакты модуля RS-485 из пинов аппаратного UART!

*/
//----------------------------------------------------------------------------------------------------------------
// ВНИМАНИЕ! Пин номер 2 - не занимать, через него работает 1-Wire!
//----------------------------------------------------------------------------------------------------------------
// настройки RS-485
//----------------------------------------------------------------------------------------------------------------
#define USE_RS485_GATE // закомментировать, если не нужна работа через RS-485
#define RS485_SPEED 57600 // скорость работы по RS-485
//----------------------------------------------------------------------------------------------------------------
// настройки nRF
//----------------------------------------------------------------------------------------------------------------
#define USE_NRF // закомментировать, если не надо работать через nRF.
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
#define RELAY_ON LOW // уровень для включения реле
#define RELAY_OFF HIGH // уровень для выключения реле
//----------------------------------------------------------------------------------------------------------------
// настройки привязки номеров локальных пинов к слотам контроллера.
// нужны для трансляции типа слота в конкретный пин
//----------------------------------------------------------------------------------------------------------------
SlotSettings SLOTS[8] = 
{
  {3, RELAY_OFF} // пин 3, начальное состояние LOW
 ,{4, RELAY_OFF} // и т.д. 0 вместо номера пина - нет поддержки привязки канала к пину
 ,{A5, RELAY_OFF}
 ,{6, RELAY_OFF}
 ,{7, RELAY_OFF}
 ,{8, RELAY_OFF}
 ,{A0, RELAY_OFF}
 ,{A3, RELAY_OFF}
  
};
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
//#define _DEBUG // раскомментировать для отладочного режима
//----------------------------------------------------------------------------------------------------------------
t_scratchpad scratchpadS, scratchpadToSend;
volatile char* scratchpad = (char *)&scratchpadS; //что бы обратиться к scratchpad как к линейному массиву
volatile bool scratchpadReceivedFromMaster = false; // флаг, что мы получили данные с мастера
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
RS485Packet rs485Packet; // пакет, в который мы принимаем данные
volatile byte* rsPacketPtr = (byte*) &rs485Packet;
volatile byte  rs485WritePtr = 0; // указатель записи в пакет
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

    if(rs485Packet.type != RS485ControllerStatePacket) // пакет не c состоянием контроллера
      return;

     // теперь приводим пакет к нужному виду
     ControllerState* state = (ControllerState*) &(rs485Packet.data);
     UpdateFromControllerState(state);


    
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
  // тут настраиваем RS-485 на приём, если надо
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
  #ifdef USE_RS485_GATE // если сказано работать через RS-485 - работаем 
    Serial.begin(RS485_SPEED);
    InitRS485(); // настраиваем RS-485 на приём
 #endif

 #ifdef _DEBUG
  Serial.begin(57600);
  Serial.println(F("Debug mode..."));
 #endif
   
    ReadROM(); // читаем наши настройки

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


}
//----------------------------------------------------------------------------------------------------------------
