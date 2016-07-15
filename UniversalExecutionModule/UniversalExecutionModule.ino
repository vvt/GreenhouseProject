/*
Прошивка для универсального модуля, предназначена для подключения
любого типа поддерживаемых датчиков и передачи с них показаний по шине 1-Wire.
Также поддерживается работа по RS-485, для включения этой возможности
надо раскомментировать USE_RS485_GATE.

ВНИМАНИЕ! Модуль при работе по RS-485 работает только на приём информации!
Это значит, что ногу разрешения приёма микросхемы преобразователя RS485
надо принудительно посадить на землю, чтобы не занимать лишний пин МК!

ВНИМАНИЕ!

RS-485 работает через аппаратный UART (RX0 и TX0 ардуины)!
Перед прошивкой выдёргивать контакты модуля RS-485 из пинов аппаратного UART!

*/
//----------------------------------------------------------------------------------------------------------------
#define USE_RS485_GATE // закомментировать, если не нужна работа через RS-485
#define RS485_SPEED 57600 // скорость работы по RS-485
//----------------------------------------------------------------------------------------------------------------
#include <avr/io.h>
#include <avr/interrupt.h>
// настройки
//----------------------------------------------------------------------------------------------------------------
#define RF_MODULE_ID 1 // уникальный ID модуля
#define ROM_ADDRESS (void*) 5 // по какому адресу у нас настройки?
//----------------------------------------------------------------------------------------------------------------
// настройки инициализации, привязка слотов к пинам и первоначальному состоянию
//----------------------------------------------------------------------------------------------------------------
#define RELAY_ON LOW // уровень для включения реле
#define RELAY_OFF HIGH // уровень для выключения реле
//----------------------------------------------------------------------------------------------------------------
typedef struct
{
  byte Pin; // номер пина
  byte State; // состояние
  
} SlotSettings;
//----------------------------------------------------------------------------------------------------------------
// настройки привязки номеров локальных пинов к слотам контроллера.
// нужны для трансляции типа слота в конкретный пин
//----------------------------------------------------------------------------------------------------------------
SlotSettings SLOTS[8] = 
{
  {3, RELAY_OFF} // пин 13, встроенный светодиод, начальное состояние LOW
 ,{4, RELAY_OFF} // и т.д. 0 вместо номера пина - нет поддержки привязки канала к пину
 ,{5, RELAY_OFF}
 ,{6, RELAY_OFF}
 ,{7, RELAY_OFF}
 ,{8, RELAY_OFF}
 ,{9, RELAY_OFF}
 ,{12, RELAY_OFF}
  
};
//----------------------------------------------------------------------------------------------------------------
//Синонимы регистров управления прерываниями. Разные для ATTINY и ATMEGA
//----------------------------------------------------------------------------------------------------------------
#define GIMSK EIMSK
#define GIFR EIFR
//----------------------------------------------------------------------------------------------------------------
//Порт 1-wire  вывод PD2 он же PIN2 Ардуино
//----------------------------------------------------------------------------------------------------------------
#define OW_PORT PORTD //1 Wire Port
#define OW_PIN PIND //1 Wire Pin as number
#define OW_PORTN (1<<PIND2)  //Pin as bit in registers
#define OW_PINN (1<<PIND2)
#define OW_DDR DDRD  //pin direction register
//----------------------------------------------------------------------------------------------------------------
inline void OneWireSetLow()
{
  //set 1-Wire line to low
  OW_DDR|=OW_PINN;
  OW_PORT&=~OW_PORTN;
}
//----------------------------------------------------------------------------------------------------------------
inline void OneWireSendAck()
{
  OW_DDR&=~OW_PINN;
}
//----------------------------------------------------------------------------------------------------------------
inline void OneWireEnableInterrupt()
{
  GIMSK|=(1<<INT0);GIFR|=(1<<INTF0);
}
//----------------------------------------------------------------------------------------------------------------
inline void OneWireDisableInterrupt()
{
  GIMSK&=~(1<<INT0);
}
//----------------------------------------------------------------------------------------------------------------
inline void OneWireInterruptAtRisingEdge()
{
  MCUCR=(1<<ISC01)|(1<<ISC00);
}
//----------------------------------------------------------------------------------------------------------------
inline void OneWireInterruptAtFallingEdge()
{
  MCUCR=(1<<ISC01);
}
//----------------------------------------------------------------------------------------------------------------
inline bool OneWireIsInterruptEnabled()
{
  return (GIMSK&(1<<INT0))==(1<<INT0); 
}
//----------------------------------------------------------------------------------------------------------------
//Timer Interrupt
//----------------------------------------------------------------------------------------------------------------
// Используем 16 разрядный таймер. Остальные не отдала Ардуино.
//Делитель - 64. То есть каждый тик таймера - 1/4 микросекунды
//----------------------------------------------------------------------------------------------------------------
inline void TimerEnable()
{
  TIMSK1  |= (1<<TOIE1); 
  TIFR1|=(1<<TOV1);
}
//----------------------------------------------------------------------------------------------------------------
inline void TimerDisable()
{
  TIMSK1  &= ~(1<<TOIE1);
}
//----------------------------------------------------------------------------------------------------------------
inline void TimerSetTimeout(uint8_t tmio)
{
  TCNT1 = ~tmio;
}
//----------------------------------------------------------------------------------------------------------------
#define OnTimer ISR(TIMER1_OVF_vect) // процедура обработки прерывания по таймеру
//----------------------------------------------------------------------------------------------------------------
inline void PreInit()
{
//Initializations of AVR
  CLKPR=(1<<CLKPCE);
  CLKPR=0;/*9.6Mhz*/
  TIMSK1=0;
  GIMSK=(1<<INT0);/*set direct GIMSK register*/
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1B = (1 << CS10) | (1 << CS11);
}
//----------------------------------------------------------------------------------------------------------------
//Интервалы для 1-Wire все в формате Микросекунды/4
//----------------------------------------------------------------------------------------------------------------
#define OWT_MIN_RESET  400/4   // Минимальная длительность Reset
#define OWT_RESET_PRESENCE 30/4  //Сколько ждать от фронта завершения Reset до начала Present
#define OWT_PRESENCE 200/4  // Длительность имульса Present
#define OWT_READLINE 30/4        //  ждать от спада мастера до момента чтения линии 1-Wire line
#define OWT_LOWTIME 15/4         // Сколько удерживать LOW
//----------------------------------------------------------------------------------------------------------------
//Структура передаваемая мастеру и обратно
//----------------------------------------------------------------------------------------------------------------
typedef enum
{
  slotEmpty, // пустой слот, без настроек
  slotWindowLeftChannel, // настройки привязки к левому каналу одного окна
  slotWindowRightChannel, // настройки привязки к правому каналу одного окна
  slotWateringChannel, // настройки привязки к статусу канала полива 
  slotLightChannel, // настройки привязки к статусу канала досветки
  slotPin // настройки привязки к статусу пина
  
} UniSlotType; // тип слота, для которого указаны настройки
//----------------------------------------------------------------------------------------------------------------
struct UniSlotData
{
  byte slotType; // тип слота, одно из значений UniSlotType 
  byte slotLinkedData; // данные, привязанные к слоту мастером, должны хранится слейвом без изменений
  byte slotStatus; // статус слота (ВКЛ или ВЫКЛ)
    
};
//----------------------------------------------------------------------------------------------------------------
typedef struct
{
    byte packet_type;
    byte packet_subtype;
    byte config;
    byte controller_id;
    byte rf_id;
    
    UniSlotData slots[8]; // слоты настроек

    byte crc8;
    
} t_scratchpad;
//----------------------------------------------------------------------------------------------------------------
//States / Modes
//----------------------------------------------------------------------------------------------------------------
typedef enum
{
  stateSleep,
  stateReset,
  statePresence,
  stateReadCommand,
  stateReadScratchpad,
  stateWriteScratchpad,
  stateCheckReset,
  stateMeasure
    
} MachineStates;
//----------------------------------------------------------------------------------------------------------------
typedef enum
{
  uniSensorsClient = 1, // packet_type == 1
  uniNextionClient = 2, // packet_type == 2
  uniExecutionClient = 3 // packet_type == 3
  
} UniClientType; // тип клиента
//-------------------------------------------------------------------------------------------------------------------------------------------------------
const int sensePin = 2; // пин, на котором висит 1-Wire
t_scratchpad scratchpadS;
volatile char* scratchpad = (char *)&scratchpadS; //что бы обратиться к scratchpad как к линейному массиву

volatile uint8_t crcHolder; //CRC calculation

volatile uint8_t commandBuffer; //Входной буфер команды

volatile uint8_t bitPointer;  //pointer to current Bit
volatile uint8_t bytePointer; //pointer to current Byte

volatile MachineStates machineState; //state
volatile uint8_t workMode; //if 0 next bit that send the device is  0
volatile uint8_t actualBit; //current

volatile bool scratchpadReceivedFromMaster = false; // флаг, что мы получили данные с мастера
//----------------------------------------------------------------------------------------------------------------
//Write a bit after next falling edge from master
//its for sending a zero as soon as possible
#define OWW_NO_WRITE 2
#define OWW_WRITE_0 0
//----------------------------------------------------------------------------------------------------------------
void ReadROM()
{
    memset((void*)&scratchpadS,0,sizeof(scratchpadS));
    eeprom_read_block((void*)&scratchpadS, ROM_ADDRESS, 29);

    // пишем свой уникальный ID
    scratchpadS.rf_id = RF_MODULE_ID; 
    scratchpadS.packet_type = uniExecutionClient; // говорим, что это тип пакета - исполнительный модуль
    scratchpadS.packet_subtype = 0;

    // говорим, что никакой калибровки не поддерживаем
    scratchpadS.config &= ~2; // второй бит убираем по-любому

}
//----------------------------------------------------------------------------------------------------------------
void WriteROM()
{
    scratchpadS.rf_id = RF_MODULE_ID;
 
    eeprom_write_block( (void*)scratchpad,ROM_ADDRESS,29);

}
//----------------------------------------------------------------------------------------------------------------
#ifdef USE_RS485_GATE // сказали работать ещё и через RS-485
//----------------------------------------------------------------------------------------------------------------
byte calcCrc8 (const byte *addr, byte len)
{
  byte crc = 0;
  while (len--) 
    {
    byte inbyte = *addr++;
    for (byte i = 8; i; i--)
      {
      byte mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) 
        crc ^= 0x8C;
      inbyte >>= 1;
      }  // end of for
    }  // end of while
  return crc;
}
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
enum {RS485FromMaster = 1, RS485FromSlave = 2};
enum {RS485ControllerStatePacket = 1, RS485SensorDataPacket = 2};
//----------------------------------------------------------------------------------------------------------------
typedef struct
{
  unsigned long WindowsState; // состояние каналов окон, 4 байта = 32 бита = 16 окон)
  byte WaterChannelsState; // состояние каналов полива, 1 байт, (8 каналов)
  byte LightChannelsState; // состояние каналов досветки, 1 байт (8 каналов)
  byte PinsState[8]; // состояние пинов, 8 байт, 64 пина
  
} ControllerState; // состояние контроллера
//----------------------------------------------------------------------------------------------------------------
typedef struct
{
  byte header1;
  byte header2;

  byte direction; // направление: 1 - от меги, 2 - от слейва
  byte type; // тип: 1 - пакет исполнительного модуля, 2 - пакет модуля с датчиками

  byte data[sizeof(ControllerState)]; // N байт данных, для исполнительного модуля в этих данных содержится состояние контроллера
  // для модуля с датчиками: первый байт - тип датчика, 2 байт - его индекс в системе. В обратку модуль с датчиками должен заполнить показания (4 байта следом за индексом 
  // датчика в системе и отправить пакет назад, выставив direction и type.

  byte tail1;
  byte tail2;
  byte crc8;
  
} RS485Packet; // пакет, гоняющийся по RS-485 туда/сюда (20 байт)
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
    byte crc = calcCrc8((const byte*) rsPacketPtr,sizeof(RS485Packet) - 1);
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
              if(wateringChannel< 8)
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
     
              if(byteNum < 8)
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
        
        if(SLOTS[i].Pin)
        {
            digitalWrite(SLOTS[i].Pin, slotStatus);
        }
      }
      
    } // if
    
  } // for
}
//----------------------------------------------------------------------------------------------------------------
void setup()
{
  #ifdef USE_RS485_GATE // если сказано работать через RS-485 - работаем 
    Serial.begin(RS485_SPEED);
    InitRS485(); // настраиваем RS-485 на приём
 #endif
   
    ReadROM(); // читаем наши настройки

    // настраиваем слоты, назначая им конкретные уровни при старте
    for(byte i=0;i<8;i++)
    {
      byte pin = SLOTS[i].Pin;
      if(pin)
      {
        pinMode(pin,OUTPUT);
        digitalWrite(pin,SLOTS[i].State);
      }
    } // for

    pinMode(sensePin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(sensePin), OW_Process, CHANGE);


    machineState = stateSleep;
    workMode = OWW_NO_WRITE;
    OW_DDR &= ~OW_PINN;

    OneWireInterruptAtFallingEdge();

    // Select clock source: internal I/O clock 
    ASSR &= ~(1<<AS2);

    PreInit();
    TimerEnable();

  
}
//----------------------------------------------------------------------------------------------------------------
// обработчик прерывания на пине
//----------------------------------------------------------------------------------------------------------------
void OW_Process()
{
    // копируем переменные в регистры
    uint8_t thisWorkMode = workMode;  
    MachineStates thisMachineState = machineState;

    // попросили отправить нолик
    if ((thisWorkMode == OWW_WRITE_0))
    {
        OneWireSetLow();    // если попросили отправить нолик - жмём линию к земле
        thisWorkMode = OWW_NO_WRITE;
    }
    // если надо отправить единицу - ничего специально не делаем.

    // выключаем прерывание на пине, оно должно быть активно только
    // тогда, когда состояние автомата - stateSleep.
    OneWireDisableInterrupt();

    // чего делаем?
    switch (thisMachineState)
    {

     case stateMeasure:
     case statePresence:
     case stateReset:
     break;

    // ничего не делаем
    case stateSleep:

        // просим таймер проснуться через некоторое время,
        // чтобы проверить, есть ли импульс RESET
        TimerSetTimeout(OWT_MIN_RESET);
        
        // включаем прерывание на пине, ждём других фронтов
        OneWireEnableInterrupt();
        
        break;
        
    // начинаем читать на спадающем фронте от мастера,
    // чтение закрывается в обработчике таймера.
    case stateWriteScratchpad: // ждём приёма
    case stateReadCommand:

        // взводим таймер на чтение из линии
        TimerSetTimeout(OWT_READLINE);
        
        break;
        
    case stateReadScratchpad:  // нам послали бит

        // взводим таймер, удерживая линию в LOW
        TimerSetTimeout(OWT_LOWTIME);
        
        break;
        
    case stateCheckReset:  // нарастающий фронт или импульс RESET

        // включаем прерывание по спадающему фронту
        OneWireInterruptAtFallingEdge();
        
        // проверяем по таймеру - это импульс RESET?
        TimerSetTimeout(OWT_RESET_PRESENCE);

        // говорим конечному автомату, что мы ждём импульса RESET
        thisMachineState = stateReset;
        
        break;
    } // switch

    // включаем таймер
    TimerEnable();

    // сохраняем состояние работы
    machineState = thisMachineState;
    workMode = thisWorkMode;

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


}
//----------------------------------------------------------------------------------------------------------------
// проверяет - в высоком ли уровне линия 1-Wire
//----------------------------------------------------------------------------------------------------------------
inline bool OneWireIsLineHigh() 
{
  return ((OW_PIN&OW_PINN) == OW_PINN);
}
//----------------------------------------------------------------------------------------------------------------
OnTimer
{
    // копируем все переменные в регистры
    uint8_t thisWorkMode = workMode;
    MachineStates thisMachineState = machineState;
    uint8_t thisBytePointer = bytePointer;
    uint8_t thisBitPointer = bitPointer;
    uint8_t thisActualBit = actualBit;
    uint8_t thisCrcHolder = crcHolder;


    // смотрим, в высоком ли уровне линия 1-Wire?
    bool isLineHigh = OneWireIsLineHigh();

    // прерывание активно?
    if (OneWireIsInterruptEnabled())
    {
        // это может быть импульс RESET
        if (!isLineHigh)   // линия всё ещё прижата к земле
        {
            // будем ждать нарастающий фронт импульса
            thisMachineState = stateCheckReset;
            OneWireInterruptAtRisingEdge();
        }
        
        // выключаем таймер
        TimerDisable();
        
    } // if
    else // прерывание неактивно
    {

        // чего делаем?
        switch (thisMachineState)
        {
         case stateCheckReset:
         case stateSleep:
         break;

         case stateMeasure: // измеряем
        
          break;

        case stateReset:  //импульс RESET закончился, надо послать Presence

            // будем ждать окончания отработки Presence
            thisMachineState = statePresence;

            // кидаем линию на землю
            OneWireSetLow();

            // и взводим таймер, чтобы удержать её нужное время
            TimerSetTimeout(OWT_PRESENCE);

            // никаких прерываний на линии, пока не отработаем Presence
            OneWireDisableInterrupt();
            
            break;

        case statePresence: // посылали импульс Presence

            OneWireSendAck();  // импульс Presence послан, теперь надо ждать команды

            // всё сделали, переходим в ожидание команды
             thisMachineState = stateReadCommand;
            commandBuffer = 0;
            thisBitPointer = 1;
            break;

        case stateReadCommand: // ждём команду
        
            if (isLineHigh)    // если линия в высоком уровне - нам передают единичку
            {
                // запоминаем её в текущей позиции
                commandBuffer |= thisBitPointer;
            }

            // сдвигаем позицию записи
            thisBitPointer = (thisBitPointer<<1);
            
            if (!thisBitPointer)   // прочитали 8 бит
            {
                thisBitPointer = 1; // переходим опять на первый бит

                // чего нам послали за команду?
                switch (commandBuffer)
                {
                
                case 0x4E: // попросили записать скратчпад, следом пойдёт скратчпад
                
                    thisMachineState = stateWriteScratchpad;
                    thisBytePointer = 0; //сбрасываем указатель записи на начало данных
                    scratchpad[0] = 0; // обнуляем данные
                    break;
                    
                case 0x25: // попросили сохранить скратчпад в EEPROM
                
                    WriteROM(); // сохранили

                    // и засыпаем
                    thisMachineState = stateSleep;

                    // сбросили данные в исходные значения
                    commandBuffer = 0;
                    thisBitPointer = 1; 
                    
                    break;
                    
                case 0x44:  // попросили запустить конвертацию
                case 0x64:  // и такая команда приходит для конвертации
                      thisMachineState = stateSleep; // спим
                    break;
                    
                case 0xBE: // попросили отдать скратчпад мастеру
                
                    // запоминаем, чего мы будем делать дальше

                    thisMachineState = stateReadScratchpad;
                    thisBytePointer = 0;
                    thisCrcHolder = 0;

                    // запоминаем первый бит, который надо послать
                    thisActualBit = (thisBitPointer & scratchpad[0]) == thisBitPointer;
                    thisWorkMode = thisActualBit; // запоминаем, какой бит послать
                    
                    break;

               case 0xCC:
                    // ждём команды, она пойдёт следом
                    thisMachineState = stateReadCommand;
                    commandBuffer = 0;
                    thisBitPointer = 1;  //Command buffer have to set zero, only set bits will write in

               break;
                    
                default:

                    // по умолчанию - спим
                    thisMachineState = stateSleep;
                } // switch
            }
            break;

        case stateWriteScratchpad: // пишем в скратчпад данные, принятые от мастера

            if (isLineHigh) // если линия поднята - послали единичку
            {
                // запоминаем в текущей позиции
                scratchpad[thisBytePointer] |= thisBitPointer;

            }

            // передвигаем позицию записи
            thisBitPointer = (thisBitPointer << 1);
            
            if (!thisBitPointer) // прочитали байт
            {
                // сдвигаем указатель записи байтов
                thisBytePointer++;
                thisBitPointer = 1;

                
                if (thisBytePointer>=30) // если прочитали 30 байт - переходим на сон
                {
                   thisMachineState = stateSleep;
                   commandBuffer = 0;
                   thisBitPointer = 1;  //Command buffer have to set zero, only set bits will write in 

                   // говорим, что мы прочитали скратчпад c мастера
                   scratchpadReceivedFromMaster = true;       
                  break;
                }
                else // сбрасываем следующий байт в 0, в последующем мы будем туда писать.
                  scratchpad[thisBytePointer]=0;
            }
            break;
            
        case stateReadScratchpad: // посылаем данные скратчпада мастеру
        
            OneWireSendAck(); // подтверждаем, что готовы передавать

            // по ходу считаем CRC
            if ((thisCrcHolder & 1)!= thisActualBit) 
              thisCrcHolder = (thisCrcHolder>>1)^0x8c;
            else 
              thisCrcHolder >>=1;

            // передвигаем позицию чтения
            thisBitPointer = (thisBitPointer<<1);
            
            if (!thisBitPointer) // прочитали байт
            {
                // переходим на следующий байт
                thisBytePointer++;
                thisBitPointer = 1;
                
                if (thisBytePointer>=30) // если послали весь скратчпад, то вываливаемся в сон
                {
                    thisMachineState = stateSleep;
                    break;
                }
                else 
                  if (thisBytePointer==29) // если следующий байт - последний, то пишем туба подсчитанную контрольную сумму
                    scratchpad[29] = thisCrcHolder;
            }

            // вычисляем, какой бит послать
            thisActualBit = (thisBitPointer & scratchpad[thisBytePointer])== thisBitPointer;
            thisWorkMode = thisActualBit; // запоминаем, чего надо послать
            
            break;
        } // switch
    } // else прерывание выключено
    
    if (thisMachineState == stateSleep) // если спим, то выключаем таймер
    {
        TimerDisable();
    }

    if ( (thisMachineState != statePresence) && (thisMachineState != stateMeasure) )
    {
        TimerSetTimeout((OWT_MIN_RESET-OWT_READLINE));
        OneWireEnableInterrupt();
    }


    machineState = thisMachineState;
    workMode = thisWorkMode;
    bytePointer = thisBytePointer;
    bitPointer = thisBitPointer;
    actualBit = thisActualBit;
    crcHolder = thisCrcHolder;
}
//----------------------------------------------------------------------------------------------------------------


