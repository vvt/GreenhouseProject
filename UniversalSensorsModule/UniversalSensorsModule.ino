#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <OneWire.h>
#include "BH1750.h"
#include "UniGlobals.h"
#include "Si7021Support.h"
#include "DHTSupport.h"
#include "LowLevel.h"
#include "OneWireSlave.h"
//----------------------------------------------------------------------------------------------------------------
/*
Прошивка для универсального модуля, предназначена для подключения
любого типа поддерживаемых датчиков и передачи с них показаний по шине 1-Wire.

Также поддерживается работа по RS-485, для включения этой возможности
надо раскомментировать USE_RS485_GATE.

Также поддерживается работа по радиоканалу, используя модуль nRF24L01+,
для этой возможности раскомментируйте USE_NRF.

ВНИМАНИЕ!

RS-485 работает через аппаратный UART (RX0 и TX0 ардуины)!
Перед прошивкой выдёргивать контакты модуля RS-485 из пинов аппаратного UART!
*/
//----------------------------------------------------------------------------------------------------------------
/*
 значения пинов по умолчанию (для платы универсальных модулей, если у вас её нет - назначайте свои пины):

  D2 - линия регистрации модуля в системе (1-Wire)
  A0, A1, A2 - чтение показаний с DS18B20
  D4  - переключение режима приём/передача для RS485
  A7 - вход датчика влажности почвы, аналоговый
  D8 - управление питанием линий DS18B20, I2C и аналогового входа для датчика влажности почвы  
 */
//----------------------------------------------------------------------------------------------------------------
// НАСТРОЙКИ
//----------------------------------------------------------------------------------------------------------------
// настройки управляющих пинов
#define LINES_POWER_DOWN_PIN 8 // номер пина, на котором будет управление питанием линий I2C, 1-Wire и аналогового входа для датчика влажности почвы
#define LINES_POWER_DOWN_LEVEL HIGH // уровень на пине для выключения линий
#define LINES_POWER_UP_LEVEL LOW // уровень на пине для включения линий 

// настройки nRF
#define USE_NRF // закомментировать, если не надо работать через nRF.
/*
 nRF для своей работы занимает следующие пины: 3,9,10,11,12,13. 
 Следите за тем, чтобы номера пинов не пересекались c номерами пинов датчиков, или с RS-485.
 */
#define NRF_CE_PIN 9 // номер пина CE для модуля nRF
#define NRF_CSN_PIN 10 // номер пина CSN для модуля nRF
#define DEFAULT_RF_CHANNEL 19 // номер канала для nRF по умолчанию

// настройки RS-485
#define USE_RS485_GATE // закомментировать, если не нужна работа через RS-485
#define RS485_SPEED 57600 // скорость работы по RS-485
#define RS485_DE_PIN 5 // номер пина, на котором будем управлять направлением приём/передача по RS-485


// настройки датчиков для модуля, МЕНЯТЬ ЗДЕСЬ!
const SensorSettings Sensors[3] = {

{mstFrequencySoilMoistureMeter,A1},//{mstBH1750,BH1750Address1}, // датчик освещённости BH1750 на шине I2C
{mstNone,0},//{mstPHMeter,A0}, // датчик pH на пине A0
{mstDS18B20,A2}//{mstSi7021,0} // датчик температуры и влажности Si7021 на шине I2C
/* 
 поддерживаемые типы датчиков: 
 
  {mstSi7021,0} - датчик температуры и влажности Si7021 на шине I2C  
  {mstBH1750,BH1750Address1} - датчик освещённости BH1750 на шине I2C, его первый адрес I2C
  {mstBH1750,BH1750Address2} - датчик освещённости BH1750 на шине I2C, его второй адрес I2C
  {mstDS18B20,A0} - датчик DS18B20 на пине A0
  {mstChinaSoilMoistureMeter,A7} - китайский датчик влажности почвы на пине A7
  {mstDHT22, 6} - датчик DHT2x на пине 6
  {mstDHT11, 5} - датчик DHT11 на пине 5
  {mstPHMeter,A0} // датчик pH на пине A0

  // Частотные датчики влажности почвы должны на выходе выдавать ШИМ, по заполнению которого рассчитывается влажность почвы !!! Максимальный коэффициент заполнения - 254, минимальный - 1.
  {mstFrequencySoilMoistureMeter,A5} - частотный датчик влажности почвы на аналоговом пине A5
  {mstFrequencySoilMoistureMeter,A4} - частотный датчик влажности почвы на аналоговом пине A4
  {mstFrequencySoilMoistureMeter,A3} - частотный датчик влажности почвы на аналоговом пине A3
  

  если в слоте записано
    {mstNone,0}
  то это значит, что датчика на этом слоте нет   

 */

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
//-------------------------------------------------------------------------------------------------ы---------------
Pin linesPowerDown(LINES_POWER_DOWN_PIN);
//----------------------------------------------------------------------------------------------------------------
#define ROM_ADDRESS (void*) 0 // по какому адресу у нас настройки?
//----------------------------------------------------------------------------------------------------------------
t_scratchpad scratchpadS, scratchpadToSend;
volatile char* scratchpad = (char *)&scratchpadS; //что бы обратиться к scratchpad как к линейному массиву

volatile bool scratchpadReceivedFromMaster = false; // флаг, что мы получили данные с мастера
volatile bool needToMeasure = false; // флаг, что мы должны запустить конвертацию
volatile unsigned long sensorsUpdateTimer = 0; // таймер получения информации с датчиков и обновления данных в скратчпаде
volatile bool measureTimerEnabled = false; // флаг, что мы должны прочитать данные с датчиков после старта измерений
unsigned long query_interval = MEASURE_MIN_TIME; // тут будет интервал опроса
unsigned long last_measure_at = 0; // когда в последний раз запускали конвертацию

volatile bool connectedViaOneWire = false; // флаг, что мы присоединены к линии 1-Wire, при этом мы не сорим в эфир по nRF и не обновляем состояние по RS-485
volatile bool needResetOneWireLastCommandTimer = false;
volatile unsigned long oneWireLastCommandTimer = 0;
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
   CRC - контрольная сумма пакета

 
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

    if(rs485Packet.type != RS485SensorDataPacket) // пакет не c запросом показаний датчика
      return;

     // теперь приводим пакет к нужному виду
     byte* readPtr = rs485Packet.data;
     // в первом байте у нас идёт тип датчика для опроса
     byte sensorType =  *readPtr++;
     // во втором - индекс датчика в системе
     byte sensorIndex = *readPtr++;

     // теперь нам надо найти, есть ли у нас этот датчик
     sensor* sMatch = NULL;
     if(scratchpadS.sensor1.type == sensorType && scratchpadS.sensor1.index == sensorIndex)
        sMatch = &(scratchpadS.sensor1);

     if(!sMatch)
     {
     if(scratchpadS.sensor2.type == sensorType && scratchpadS.sensor2.index == sensorIndex)
        sMatch = &(scratchpadS.sensor2);        
     }

     if(!sMatch)
     {
     if(scratchpadS.sensor3.type == sensorType && scratchpadS.sensor3.index == sensorIndex)
        sMatch = &(scratchpadS.sensor3);        
     }

     if(!sMatch) // не нашли у нас такого датчика
      return;

     memcpy(readPtr,sMatch->data,4); // у нас 4 байта на показания, копируем их все

     // выставляем нужное направление пакета
     rs485Packet.direction = RS485FromSlave;
     rs485Packet.type = RS485SensorDataPacket;

     // подсчитываем CRC
     rs485Packet.crc8 = OneWireSlave::crc8((const byte*) &rs485Packet,sizeof(RS485Packet)-1 );

     // теперь переключаемся на передачу
     RS485Send();

     // пишем в порт данные
     Serial.write((const uint8_t *)&rs485Packet,sizeof(RS485Packet));

     // ждём окончания передачи
     RS485waitTransmitComplete();
     
    // переключаемся на приём
    RS485Receive();


    
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
void RS485Receive()
{
  digitalWrite(RS485_DE_PIN,LOW); // переводим контроллер RS-485 на приём
}
//----------------------------------------------------------------------------------------------------------------
void RS485Send()
{
  digitalWrite(RS485_DE_PIN,HIGH); // переводим контроллер RS-485 на передачу
}
//----------------------------------------------------------------------------------------------------------------
void InitRS485()
{
  memset(&rs485Packet,0,sizeof(RS485Packet));
  // тут настраиваем RS-485 на приём
  pinMode(RS485_DE_PIN,OUTPUT);
  RS485Receive();
}
//----------------------------------------------------------------------------------------------------------------
void RS485waitTransmitComplete()
{
  // ждём завершения передачи по UART
  while(!(UCSR0A & _BV(TXC0) ));
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_RS485_GATE
//-------------------------------------------------------------------------------------------------------------------------------------------------------
byte GetSensorType(const SensorSettings& sett)
{
  switch(sett.Type)
  {
    case mstNone:
      return uniNone;
    
    case mstDS18B20:
      return uniTemp;
      
    case mstBH1750:
      return uniLuminosity;

    case mstSi7021:
    case mstDHT11:
    case mstDHT22:
      return uniHumidity;

    case mstChinaSoilMoistureMeter:
    case mstFrequencySoilMoistureMeter:
      return uniSoilMoisture;

    case mstPHMeter:
      return uniPH;
    
  }

  return uniNone;
}
//----------------------------------------------------------------------------------------------------------------
void SetDefaultValue(const SensorSettings& sett, byte* data)
{
  switch(sett.Type)
  {
    case mstNone:
      *data = 0xFF;
    break;
    
    case mstDS18B20:
    case mstChinaSoilMoistureMeter:
    case mstPHMeter:
    case mstFrequencySoilMoistureMeter:
    {
      *data = NO_TEMPERATURE_DATA;
    }
    break;
      
    case mstBH1750:
    {
    long lum = NO_LUMINOSITY_DATA;
    memcpy(data,&lum,sizeof(lum));
    }
    break;

    case mstSi7021:
    case mstDHT11:
    case mstDHT22:
    {
    *data = NO_TEMPERATURE_DATA;
    data++; data++;
    *data = NO_TEMPERATURE_DATA;
    }
    break;

  }
}
//----------------------------------------------------------------------------------------------------------------
void* SensorDefinedData[3] = {NULL}; // данные, определённые датчиками при инициализации
//----------------------------------------------------------------------------------------------------------------
void* InitSensor(const SensorSettings& sett)
{
  switch(sett.Type)
  {
    case mstNone:
      return NULL;
    
    case mstDS18B20:
      return InitDS18B20(sett);

    case mstFrequencySoilMoistureMeter:
        return InitFrequencySoilMoistureMeter(sett);
    break;
      
    case mstBH1750:
      return InitBH1750(sett);

    case mstSi7021:
      return InitSi7021(sett);

    case mstDHT11:
      return InitDHT(sett,DHT_11);

    case mstDHT22:
      return InitDHT(sett,DHT_2x);

    case mstChinaSoilMoistureMeter:
      return NULL;

    case mstPHMeter: // инициализируем структуру для опроса pH
    {      
      PHMeasure* m = new PHMeasure;
      m->samplesDone = 0;
      m->samplesTimer = 0;
      m->data = 0;
      m->inMeasure = false; // ничего не измеряем
      return m;
    }
    break;
  }

  return NULL;  
}
//----------------------------------------------------------------------------------------------------------------
void ReadROM()
{
    memset((void*)&scratchpadS,0,sizeof(scratchpadS));
    eeprom_read_block((void*)&scratchpadS, ROM_ADDRESS, 29);

    // пишем номер канала по умолчанию
    if(scratchpadS.rf_id == 0xFF || scratchpadS.rf_id == 0)
      scratchpadS.rf_id = DEFAULT_RF_CHANNEL; 

    scratchpadS.packet_type = ptSensorsData; // говорим, что это тип пакета - данные с датчиками
    scratchpadS.packet_subtype = 0;


    // если интервала опроса не сохранено - выставляем по умолчанию
    if(scratchpadS.query_interval_min == 0xFF)
      scratchpadS.query_interval_min = 0;
      
    if(scratchpadS.query_interval_sec == 0xFF)
      scratchpadS.query_interval_sec =  MEASURE_MIN_TIME/1000;

   if(scratchpadS.query_interval_min == 0 && scratchpadS.query_interval_sec < 5) // минимум 5 секунд между обновлениями датчиков
    scratchpadS.query_interval_sec = 5;

    // вычисляем интервал опроса
    query_interval = (scratchpadS.query_interval_min*60 + scratchpadS.query_interval_sec)*1000;
    
    scratchpadS.sensor1.type = GetSensorType(Sensors[0]);
    scratchpadS.sensor2.type = GetSensorType(Sensors[1]);
    scratchpadS.sensor3.type = GetSensorType(Sensors[2]);

    SetDefaultValue(Sensors[0],scratchpadS.sensor1.data);
    SetDefaultValue(Sensors[1],scratchpadS.sensor2.data);
    SetDefaultValue(Sensors[2],scratchpadS.sensor3.data);

    // смотрим, есть ли у нас калибровка?
    byte calibration_enabled = false;
    for(byte i=0;i<3;i++)
    {
        switch(Sensors[i].Type)
        {
            case mstChinaSoilMoistureMeter:
            {
              calibration_enabled = true;
              // устанавливаем значения по умолчанию
              if(scratchpadS.calibration_factor1 == 0xFF || scratchpadS.calibration_factor1 == 0)
              {
                scratchpadS.calibration_factor1 = map(450,0,1023,0,255);
              }
              if(scratchpadS.calibration_factor2 == 0xFF || scratchpadS.calibration_factor2 == 0)
              {
                scratchpadS.calibration_factor2 = map(1023,0,1023,0,255);
              }

              // мы поддерживаем два фактора калибровки
              scratchpadS.config |= (4 | 8);
            }
            break; // mstChinaSoilMoistureMeter

            case mstPHMeter:
            {
              calibration_enabled = true;
              // устанавливаем значения по умолчанию
              if(scratchpadS.calibration_factor1 == 0xFF)
              {
                // поскольку у нас беззнаковый байт - значение 127 соответствует калибровке 0,
                // всё, что меньше - отрицательная калибровка, что больше - положительная калибровка
                scratchpadS.calibration_factor1 = 127; 
              }

              scratchpadS.config |= 4; // поддерживаем всего один фактор калибровки
            }
            break; // mstPHMeter
          
        } // switch

        if(calibration_enabled)
          break;
    
    } // for

    if(calibration_enabled)
    {
      // включён фактор калибровки
      scratchpadS.config |= 2; // устанавливаем второй бит, говоря, что мы поддерживаем калибровку
    } // if
    else
    {
      scratchpadS.config &= ~2; // второй бит убираем по-любому
    }

}
//----------------------------------------------------------------------------------------------------------------
void WakeUpSensor(const SensorSettings& sett, void* sensorDefinedData)
{
  // просыпаем сенсоры
  switch(sett.Type)
  {
    case mstNone:
      break;
    
    case mstDS18B20:
    break;

    case mstBH1750:
    {
      BH1750Support* bh = (BH1750Support*) sensorDefinedData;
      bh->begin((BH1750Address)sett.Pin);
    }
    break;

    case mstSi7021:
    {
      Si7021* si = (Si7021*) sensorDefinedData;
      si->begin();
    }
    break;

    case mstChinaSoilMoistureMeter:
    case mstDHT11:
    case mstDHT22:
    break;

    case mstFrequencySoilMoistureMeter:
    {
      Pin pin(sett.Pin);
      pin.inputMode();
      pin.writeHigh();      
    }
    break;
    
    case mstPHMeter:
    {
      // надо подтянуть пин к питанию
      Pin pin(sett.Pin);
      pin.inputMode();
      pin.writeHigh();
      analogRead(sett.Pin);
    }
    break;
  }    
}
//----------------------------------------------------------------------------------------------------------------
void WakeUpSensors() // будим все датчики
{
  // включаем все линии
  linesPowerDown.write(LINES_POWER_UP_LEVEL);
  
 if(HasI2CSensors())
  PowerUpI2C(); // поднимаем I2C
 
   // будим датчики
   for(byte i=0;i<3;i++)
    WakeUpSensor(Sensors[i],SensorDefinedData[i]);
   
}
//----------------------------------------------------------------------------------------------------------------
void PowerDownSensors()
{
  // выключаем все линии
  linesPowerDown.write(LINES_POWER_DOWN_LEVEL);
  
  PowerDownI2C(); // глушим шину I2C
      
}
//----------------------------------------------------------------------------------------------------------------
void* InitDHT(const SensorSettings& sett, DHTType dhtType) // инициализируем датчик влажности DHT*
{
  UNUSED(sett);
  
  DHTSupport* dht = new DHTSupport(dhtType);
  
  return dht;
}
//----------------------------------------------------------------------------------------------------------------
void* InitSi7021(const SensorSettings& sett) // инициализируем датчик влажности Si7021
{
  UNUSED(sett);
  
  Si7021* si = new Si7021();
  si->begin();

  return si;
}
//----------------------------------------------------------------------------------------------------------------
void* InitFrequencySoilMoistureMeter(const SensorSettings& sett)
{
    UNUSED(sett);
    return NULL;  
}
//----------------------------------------------------------------------------------------------------------------
void* InitBH1750(const SensorSettings& sett) // инициализируем датчик освещённости
{
  BH1750Support* bh = new BH1750Support();
  
  bh->begin((BH1750Address)sett.Pin);
  
  return bh;
}
//----------------------------------------------------------------------------------------------------------------
void* InitDS18B20(const SensorSettings& sett) // инициализируем датчик температуры
{
  if(!sett.Pin)
    return NULL;   

   OneWire ow(sett.Pin);

  if(!ow.reset()) // нет датчика
    return NULL;  

   ow.write(0xCC); // пофиг на адреса (SKIP ROM)
   ow.write(0x4E); // запускаем запись в scratchpad

   ow.write(0); // верхний температурный порог 
   ow.write(0); // нижний температурный порог
   ow.write(0x7F); // разрешение датчика 12 бит

   ow.reset();
   ow.write(0xCC); // пофиг на адреса (SKIP ROM)
   ow.write(0x48); // COPY SCRATCHPAD
   delay(10);
   ow.reset();

   return NULL;
    
}
//----------------------------------------------------------------------------------------------------------------
void InitSensors()
{
  // инициализируем датчики
  for(byte i=0;i<3;i++)
    SensorDefinedData[i] = InitSensor(Sensors[i]);
         
}
//----------------------------------------------------------------------------------------------------------------
 void ReadDS18B20(const SensorSettings& sett, struct sensor* s) // читаем данные с датчика температуры
{ 
  s->data[0] = NO_TEMPERATURE_DATA;
  s->data[1] = 0;
  
  if(!sett.Pin)
    return;

   OneWire ow(sett.Pin);
    
    if(!ow.reset()) // нет датчика на линии
      return; 

  byte data[9] = {0};
  
  ow.write(0xCC); // пофиг на адреса (SKIP ROM)
  ow.write(0xBE); // читаем scratchpad датчика на пине

  for(uint8_t i=0;i<9;i++)
    data[i] = ow.read();


 if (OneWire::crc8(data, 8) != data[8]) // проверяем контрольную сумму
      return;
  
  int loByte = data[0];
  int hiByte = data[1];

  int temp = (hiByte << 8) + loByte;
  
  bool isNegative = (temp & 0x8000);
  
  if(isNegative)
    temp = (temp ^ 0xFFFF) + 1;

  int tc_100 = (6 * temp) + temp/4;
   
  s->data[0] = tc_100/100;
  s->data[1] = tc_100 % 100;
    
}
//----------------------------------------------------------------------------------------------------------------
void ReadFrequencySoilMoistureMeter(const SensorSettings& sett, void* sensorDefinedData, struct sensor* s)
{
    UNUSED(sensorDefinedData);

 int highTime = pulseIn(sett.Pin,HIGH);
 
 if(!highTime) // always HIGH ?
 {
   s->data[0] = NO_TEMPERATURE_DATA;

  // Serial.println("ALWAYS HIGH,  BUS ERROR!");

   return;
 }
 highTime = pulseIn(sett.Pin,HIGH);
 int lowTime = pulseIn(sett.Pin,LOW);

 //Serial.print("HIGH pulse: ");
// Serial.println(highTime);

// Serial.print("LOW pulse: ");
// Serial.println(lowTime);

 if(!lowTime)
 {
//  Serial.println("NO LOW PULSE!");
  return;
 }
  int totalTime = lowTime + highTime;
  // теперь смотрим отношение highTime к общей длине импульсов - это и будет влажность почвы
  // totalTime = 100%
  // highTime = x%
  // x = (highTime*100)/totalTime;

  float moisture = (highTime*100.0)/totalTime;

  int moistureInt = moisture*100;

   s->data[0] = moistureInt/100;
   s->data[1] = moistureInt%100;

 // Serial.print("Moisture are: ");
//  Serial.print(s->data[0]);
//  Serial.print(",");
//  Serial.print(s->data[1]);
//  Serial.println();   
}
//----------------------------------------------------------------------------------------------------------------
void ReadBH1750(const SensorSettings& sett, void* sensorDefinedData, struct sensor* s) // читаем данные с датчика освещённости
{
  UNUSED(sett);
  BH1750Support* bh = (BH1750Support*) sensorDefinedData;
  long lum = bh->GetCurrentLuminosity();
  memcpy(s->data,&lum,sizeof(lum));

}
//----------------------------------------------------------------------------------------------------------------
void ReadDHT(const SensorSettings& sett, void* sensorDefinedData, struct sensor* s) // читаем данные с датчика влажности Si7021
{
  DHTSupport* dht = (DHTSupport*) sensorDefinedData;
    
  HumidityAnswer ha;
  dht->read(sett.Pin,ha);

  memcpy(s->data,&ha,sizeof(ha));

}
//----------------------------------------------------------------------------------------------------------------
void ReadSi7021(const SensorSettings& sett, void* sensorDefinedData, struct sensor* s) // читаем данные с датчика влажности Si7021
{
  UNUSED(sett);
  Si7021* si = (Si7021*) sensorDefinedData;
  HumidityAnswer ha;
  si->read(ha);

  memcpy(s->data,&ha,sizeof(ha));

}
//----------------------------------------------------------------------------------------------------------------
void ReadChinaSoilMoistureMeter(const SensorSettings& sett, void* sensorDefinedData, struct sensor* s)
{
   UNUSED(sensorDefinedData);
   
   int val = analogRead(sett.Pin);
   
   int soilMoisture0Percent = map(scratchpadS.calibration_factor1,0,255,0,1023);
   int soilMoisture100Percent = map(scratchpadS.calibration_factor2,0,255,0,1023);

   int percentsInterval = map(val,min(soilMoisture0Percent,soilMoisture100Percent),max(soilMoisture0Percent,soilMoisture100Percent),0,10000);
   
  // теперь, если у нас значение 0% влажности больше, чем значение 100% влажности - надо от 10000 отнять полученное значение
  if(soilMoisture0Percent > soilMoisture100Percent)
    percentsInterval = 10000 - percentsInterval;

   int8_t sensorValue;
   byte sensorFract;

   sensorValue = percentsInterval/100;
   sensorFract = percentsInterval%100;

   if(sensorValue > 99)
   {
      sensorValue = 100;
      sensorFract = 0;
   }

   if(sensorValue < 0)
   {
      sensorValue = NO_TEMPERATURE_DATA;
      sensorFract = 0;
   }

   s->data[0] = sensorValue;
   s->data[1] = sensorFract;
   
}
//----------------------------------------------------------------------------------------------------------------
void ReadPHValue(const SensorSettings& sett, void* sensorDefinedData, struct sensor* s)
{
  
  UNUSED(sett);
  
  // подсчитываем актуальное значение pH
 PHMeasure* pm = (PHMeasure*) sensorDefinedData;

 pm->inMeasure = false; // говорим, что уже ничего не измеряем

 s->data[0] = NO_TEMPERATURE_DATA;
 
 if(pm->samplesDone > 0)
 {
  // сначала получаем значение калибровки, преобразовывая его в знаковое число
  int8_t calibration = map(scratchpadS.calibration_factor1,0,255,-128,127);
  
  // преобразуем полученное значение в среднее
  float avgSample = (pm->data*1.0)/pm->samplesDone;
  
  // теперь считаем вольтаж
  float voltage = avgSample*5.0/1024;
        
  // теперь получаем значение pH
  unsigned long phValue = voltage*350 + calibration;
    
    if(avgSample > 1000)
    {
      // не прочитали из порта ничего, потому что у нас включена подтяжка к питанию
    }
    else
    {
      s->data[0] = phValue/100;
      s->data[1] = phValue%100;
    } // else
  
 } // pm->samplesDone > 0

 // сбрасываем данные в 0
 pm->samplesDone = 0;
 pm->samplesTimer = 0;
 pm->data = 0;  
  
}
//----------------------------------------------------------------------------------------------------------------
void ReadSensor(const SensorSettings& sett, void* sensorDefinedData, struct sensor* s)
{
  switch(sett.Type)
  {
    case mstNone:
      
    break;

    case mstDS18B20:
    ReadDS18B20(sett,s);
    break;

    case mstBH1750:
    ReadBH1750(sett,sensorDefinedData,s);
    break;

    case mstSi7021:
    ReadSi7021(sett,sensorDefinedData,s);
    break;

    case mstChinaSoilMoistureMeter:
      ReadChinaSoilMoistureMeter(sett,sensorDefinedData,s);
    break;

    case mstPHMeter:
      ReadPHValue(sett,sensorDefinedData,s);
    break;

    case mstDHT11:
    case mstDHT22:
      ReadDHT(sett,sensorDefinedData,s);
    break;

    case mstFrequencySoilMoistureMeter:
      ReadFrequencySoilMoistureMeter(sett,sensorDefinedData,s);
    break;
  }
}
//----------------------------------------------------------------------------------------------------------------
void ReadSensors()
{
  // читаем информацию с датчиков
    
  ReadSensor(Sensors[0],SensorDefinedData[0],&scratchpadS.sensor1);
  ReadSensor(Sensors[1],SensorDefinedData[1],&scratchpadS.sensor2);
  ReadSensor(Sensors[2],SensorDefinedData[2],&scratchpadS.sensor3);

}
//----------------------------------------------------------------------------------------------------------------
void MeasureDS18B20(const SensorSettings& sett)
{
  if(!sett.Pin)
    return;

   OneWire ow(sett.Pin);
    
    if(!ow.reset()) // нет датчика на линии
      return; 

    ow.write(0xCC);
    ow.write(0x44); // посылаем команду на старт измерений
    
    ow.reset();    
  
}
//----------------------------------------------------------------------------------------------------------------
bool HasI2CSensors()
{
  // проверяем, есть ли у нас хоть один датчик на I2C
  for(byte i=0;i<3;i++)
  {
    switch(Sensors[i].Type)
    {
      case mstBH1750:
      case mstSi7021:
        return true;
    }
    
  } // for
  return false;
}
//----------------------------------------------------------------------------------------------------------------
inline void PowerUpI2C()
{
  power_twi_enable();
}
//----------------------------------------------------------------------------------------------------------------
inline void PowerDownI2C()
{
  power_twi_disable();
}
//----------------------------------------------------------------------------------------------------------------
void MeasurePH(const SensorSettings& sett,void* sensorDefinedData)
{
 // начинаем измерения pH здесь 
 PHMeasure* pm = (PHMeasure*) sensorDefinedData;
 
 if(pm->inMeasure) // уже измеряем
  return;
  
 pm->samplesDone = 0;
 pm->samplesTimer = millis();
 pm->data = 0;
 // читаем из пина и игнорируем это значение
 analogRead(sett.Pin);
 pm->inMeasure = true; // говорим, что готовы измерять
}
//----------------------------------------------------------------------------------------------------------------
void MeasureSensor(const SensorSettings& sett,void* sensorDefinedData) // запускаем конвертацию с датчика, если надо
{
  switch(sett.Type)
  {
    case mstNone:    
    break;

    case mstDS18B20:
    MeasureDS18B20(sett);
    break;

    case mstPHMeter:
      MeasurePH(sett,sensorDefinedData);
    break;

    case mstBH1750:
    case mstSi7021:
    case mstChinaSoilMoistureMeter:
    case mstDHT11:
    case mstDHT22:
    case mstFrequencySoilMoistureMeter:
    break;
  }  
}
//----------------------------------------------------------------------------------------------------------------
void UpdatePH(const SensorSettings& sett,void* sensorDefinedData, unsigned long curMillis)
{
  PHMeasure* pm = (PHMeasure*) sensorDefinedData;
  
  if(!pm->inMeasure) // ничего не меряем
    return;
    
  if(pm->samplesDone >= PH_NUM_SAMPLES) // закончили измерения
  {    
    pm->inMeasure = false;
    return;
  }
    
  if((curMillis - pm->samplesTimer) > PH_SAMPLES_INTERVAL)
  {
    
    pm->samplesTimer = curMillis; // запоминаем, когда замерили
    // пора прочитать из порта
    pm->samplesDone++;
    pm->data += analogRead(sett.Pin);
  }
}
//----------------------------------------------------------------------------------------------------------------
void UpdateSensor(const SensorSettings& sett,void* sensorDefinedData, unsigned long curMillis)
{
  // обновляем датчики здесь
  switch(sett.Type)
  {

    case mstPHMeter:
      UpdatePH(sett,sensorDefinedData,curMillis);
    break;

    case mstNone:    
    case mstDS18B20:
    case mstBH1750:
    case mstSi7021:
    case mstChinaSoilMoistureMeter:
    case mstDHT11:
    case mstDHT22:
    case mstFrequencySoilMoistureMeter:
    break;
  }  
}
//----------------------------------------------------------------------------------------------------------------
void UpdateSensors()
{
  unsigned long thisMillis = millis();
  for(byte i=0;i<3;i++)
    UpdateSensor(Sensors[i],SensorDefinedData[i],thisMillis);  
}
//----------------------------------------------------------------------------------------------------------------
void StartMeasure()
{  
 WakeUpSensors(); // будим все датчики
  
  // запускаем конвертацию
  for(byte i=0;i<3;i++)
    MeasureSensor(Sensors[i],SensorDefinedData[i]);

  last_measure_at = millis();
}
//----------------------------------------------------------------------------------------------------------------
#ifdef USE_NRF
//----------------------------------------------------------------------------------------------------------------
//uint64_t controllerStatePipe = 0xF0F0F0F0E0LL; // труба, с которой мы слушаем состояние контроллера
// трубы, в которые мы можем писать
const uint64_t writingPipes[5] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0E2LL, 0xF0F0F0F0E3LL, 0xF0F0F0F0E4LL, 0xF0F0F0F0E5LL };
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

  if(nRFInited) {
  delay(200); // чуть-чуть подождём

  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(scratchpadS.rf_id);
  radio.setRetries(15,15);
  radio.setPayloadSize(sizeof(t_scratchpad)); // у нас 30 байт на пакет
  radio.setCRCLength(RF24_CRC_16);
  radio.setAutoAck(true);

  radio.powerDown(); // входим в режим энергосбережения
  
  } // nRFInited
  
}
//----------------------------------------------------------------------------------------------------------------
void sendDataViaNRF()
{
  if(!nRFInited)
    return;
    
  if(!((scratchpadS.config & 1) == 1))
  {
  //  Serial.println(F("Transiever disabled."));
    return;
  }
  
  radio.powerUp(); // просыпаемся
  
 // Serial.println(F("Send sensors data via nRF..."));
  // посылаем данные через nRF
    uint8_t writePipeNum = random(0,5);

    // подсчитываем контрольную сумму
    scratchpadS.crc8 = OneWireSlave::crc8((const byte*)&scratchpadS,sizeof(scratchpadS)-1);
  //  radio.stopListening(); // останавливаем прослушку
    radio.openWritingPipe(writingPipes[writePipeNum]); // открываем канал для записи
    radio.write(&scratchpadS,sizeof(scratchpadS)); // пишем в него
  //  radio.startListening(); // начинаем прослушку эфира опять  

 // Serial.println(F("Sensors data sent."));

 radio.powerDown(); // входим в режим энергосбережения

}
//----------------------------------------------------------------------------------------------------------------
#endif // USE_NRF
//----------------------------------------------------------------------------------------------------------------
void WriteROM()
{

    scratchpadS.sensor1.type = GetSensorType(Sensors[0]);
    scratchpadS.sensor2.type = GetSensorType(Sensors[1]);
    scratchpadS.sensor3.type = GetSensorType(Sensors[2]);
  
    eeprom_write_block( (void*)scratchpad,ROM_ADDRESS,29);
    memcpy(&scratchpadToSend,&scratchpadS,sizeof(scratchpadS));

    #ifdef USE_NRF
      // переназначаем канал радио
      if(nRFInited)
        radio.setChannel(scratchpadS.rf_id);
        
    #endif
    

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
  
  // настраиваем пин управления линиями на выход
  linesPowerDown.outputMode();
  
  // включаем все линии на период настройки
 linesPowerDown.write(LINES_POWER_UP_LEVEL);
  
    ReadROM();

   InitSensors(); // инициализируем датчики   
   PowerDownSensors(); // и выключаем их нафик при старте
    
    #ifdef USE_NRF
      initNRF();
    #endif

  scratchpadS.crc8 = OneWireSlave::crc8((const byte*) scratchpad,sizeof(scratchpadS)-1);
  memcpy(&scratchpadToSend,&scratchpadS,sizeof(scratchpadS));

  oneWireLastCommandTimer = millis();
  
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
  connectedViaOneWire = true; // говорим, что мы подключены через 1-Wire
  needResetOneWireLastCommandTimer = true; // просим, чтобы сбросили таймер момента получения последней команды
  
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
        if(!measureTimerEnabled) // только если она уже не запущена
          needToMeasure = true;
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
//return;

  if(scratchpadReceivedFromMaster) {
    // скратч был получен от мастера, тут можно что-то делать
    // вычисляем новый интервал опроса
    query_interval = (scratchpadS.query_interval_min*60 + scratchpadS.query_interval_sec)*1000;
    scratchpadReceivedFromMaster = false;
      
  } // scratchpadReceivedFromMaster

  
  unsigned long curMillis = millis();

  // если попросили сбросить таймер получения последней команды по линии 1-Wire - делаем это
  if(needResetOneWireLastCommandTimer) {
    needResetOneWireLastCommandTimer = false;
    oneWireLastCommandTimer = curMillis;
  }

  // проверяем - когда приходила последняя команда по 1-Wire: если её не было больше 15 секунд - активируем nRF и RS-485
  if(connectedViaOneWire) {
      if(oneWireLastCommandTimer - curMillis > 15000) {
          connectedViaOneWire = false; // соединение через 1-Wire разорвано
      }
  }
  


  if(((curMillis - last_measure_at) > query_interval) && !measureTimerEnabled && !needToMeasure) {
    // чего-то долго не запускали конвертацию, запустим, пожалуй
      if(!connectedViaOneWire) // и запустим только тогда, когда мы не подключены к 1-Wire, иначе - мастер сам запросит конвертацию.
        needToMeasure = true;
  }

  // только если ничего не делаем на линии 1-Wire и запросили конвертацию
  if(needToMeasure) {
    needToMeasure = false;
    StartMeasure();
    sensorsUpdateTimer = curMillis; // сбрасываем таймер обновления
    measureTimerEnabled = true; // включаем флаг, что мы должны прочитать данные с датчиков
  }

  if(measureTimerEnabled) {
    UpdateSensors(); // обновляем датчики, если кому-то из них нужно периодическое обновление
  }
  
  if(measureTimerEnabled && ((curMillis - sensorsUpdateTimer) > query_interval)) {

     sensorsUpdateTimer = curMillis;
     measureTimerEnabled = false;
     // можно читать информацию с датчиков
     ReadSensors();

     // прочитали, всё в скратчпаде, вычисляем CRC
     scratchpadS.crc8 = OneWireSlave::crc8((const byte*) scratchpad,sizeof(scratchpadS)-1);
     // и копируем скратчпад в скратчпад для отсылки, чтобы данные оставались валидными до тех пор, пока мастер их не примет.
     memcpy(&scratchpadToSend,&scratchpadS,sizeof(scratchpadS));

     // теперь усыпляем все датчики
     PowerDownSensors();

     // прочитали, отправляем
     #ifdef USE_NRF
      if(!connectedViaOneWire)
        sendDataViaNRF();
     #endif
  }

  #ifdef USE_RS485_GATE
    if(!connectedViaOneWire)
      ProcessIncomingRS485Packets(); // обрабатываем входящие пакеты по RS-485
  #endif  

}
//----------------------------------------------------------------------------------------------------------------

