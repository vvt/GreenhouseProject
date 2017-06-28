#include "Settings.h"
#include "Globals.h"
#include "Memory.h" 
//--------------------------------------------------------------------------------------------------------------------------------------
//  ГЛОБАЛЬНЫЕ НАСТРОЙКИ
//--------------------------------------------------------------------------------------------------------------------------------------
GlobalSettings::GlobalSettings()
{
 // ResetToDefault();
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::WriteDeltaSettings(DeltaCountFunction OnDeltaGetCount, DeltaReadWriteFunction OnDeltaWrite)
{
  if(!(OnDeltaGetCount && OnDeltaWrite)) // обработчики не заданы
    return;

  uint16_t writeAddr = DELTA_SETTINGS_EEPROM_ADDR;

  // записываем заголовок
  MemWrite(writeAddr++,SETT_HEADER1);
  MemWrite(writeAddr++,SETT_HEADER2);
  

  uint8_t deltaCount = 0;

  // получаем кол-во дельт
  OnDeltaGetCount(deltaCount);

  // записываем кол-во дельт
  MemWrite(writeAddr++,deltaCount);

  //теперь пишем дельты
  for(uint8_t i=0;i<deltaCount;i++)
  {
    String name1,name2;
    uint8_t sensorType = 0,sensorIdx1 = 0,sensorIdx2 = 0;

    // получаем настройки дельт
    OnDeltaWrite(sensorType,name1,sensorIdx1,name2,sensorIdx2);

    // получили, можем сохранять. Каждая запись дельт идёт так:
  
  // 1 байт - тип датчика (температура, влажность, освещенность)
  
  // 1 байт - длина имени модуля 1
  // N байт - имя модуля 1
  // 1 байт - индекс датчика модуля 1
  
  // 1 байт - длина имени модуля 2
  // N байт - имя модуля 2
  // 1 байт - индекс датчика модуля 1

    // пишем тип датчика
     MemWrite(writeAddr++,sensorType);

     // пишем длину имени модуля 1
     uint8_t nameLen = name1.length();
     MemWrite(writeAddr++,nameLen);

     // пишем имя модуля 1
     const char* namePtr = name1.c_str();
     for(uint8_t idx=0;idx<nameLen; idx++)
      MemWrite(writeAddr++,*namePtr++);

     // пишем индекс датчика 1
     MemWrite(writeAddr++,sensorIdx1);


     // пишем длину имени модуля 2
     nameLen = name2.length();
     MemWrite(writeAddr++,nameLen);

     // пишем имя модуля 2
     namePtr = name2.c_str();
     for(uint8_t idx=0;idx<nameLen; idx++)
      MemWrite(writeAddr++,*namePtr++);

     // пишем индекс датчика 2
     MemWrite(writeAddr++,sensorIdx2);
     
    
  } // for

  // записали, отдыхаем
    
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::ReadDeltaSettings(DeltaCountFunction OnDeltaSetCount, DeltaReadWriteFunction OnDeltaRead)
{
  if(!(OnDeltaSetCount && OnDeltaRead)) // обработчики не заданы
    return;

  uint16_t readAddr = DELTA_SETTINGS_EEPROM_ADDR;
  uint8_t h1,h2;
  
  h1 = MemRead(readAddr++);
  h2 = MemRead(readAddr++);

  uint8_t deltaCount = 0;

  if(!(h1 == SETT_HEADER1 && h2 == SETT_HEADER2)) // в памяти нет данных о сохранённых настройках дельт
  {
    
    OnDeltaSetCount(deltaCount); // сообщаем, что мы прочитали 0 настроек
    return; // и выходим
  }

  // читаем кол-во настроек
  deltaCount = MemRead(readAddr++);
  if(deltaCount == 0xFF) // ничего нет
    deltaCount = 0; // сбрасываем в ноль
    
  OnDeltaSetCount(deltaCount); // сообщаем, что мы прочитали N настроек

  // читаем настройки дельт. В памяти каждая запись дельт идёт так:
  
  // 1 байт - тип датчика (температура, влажность, освещенность)
  
  // 1 байт - длина имени модуля 1
  // N байт - имя модуля 1
  // 1 байт - индекс датчика модуля 1
  
  // 1 байт - длина имени модуля 2
  // N байт - имя модуля 2
  // 1 байт - индекс датчика модуля 1
  
  // теперь читаем настройки
  for(uint8_t i=0;i<deltaCount;i++)
  {
    // читаем тип датчика
    uint8_t sensorType = MemRead(readAddr++);

    // читаем длину имени модуля 1
    uint8_t nameLen = MemRead(readAddr++);
    
    // резервируем память
    String name1; name1.reserve(nameLen + 1);
    
    // читаем имя модуля 1
    for(uint8_t idx = 0; idx < nameLen; idx++)
      name1 += (char) MemRead(readAddr++);

    // читаем индекс датчика модуля 1
    uint8_t sensorIdx1 = MemRead(readAddr++);

    // читаем длину имени модуля 2 
    nameLen = MemRead(readAddr++);
    
    // резервируем память
    String name2; name2.reserve(nameLen + 1);

    // читаем имя модуля 2
    for(uint8_t idx = 0; idx < nameLen; idx++)
      name2 += (char) MemRead(readAddr++);

    // читаем индекс датчика модуля 2
    uint8_t sensorIdx2 = MemRead(readAddr++);

    // всё прочитали - можем вызывать функцию, нам переданную
    OnDeltaRead(sensorType,name1,sensorIdx1,name2,sensorIdx2);
    
  } // for

  // всё прочитали, отдыхаем
  
    
}
//--------------------------------------------------------------------------------------------------------------------------------------
/*
void GlobalSettings::ResetToDefault()
{
 // tempOpen = DEF_OPEN_TEMP;
  //tempClose = DEF_CLOSE_TEMP;
  //openInterval = DEF_OPEN_INTERVAL;
 // wateringOption = wateringOFF;
 // wateringWeekDays = 0;
  //wateringTime = 0;
  //startWateringTime = 0;
  //wifiState = 0x01; // первый бит устанавливаем, говорим, что мы коннектимся к роутеру
 // controllerID = 0; // по умолчанию 0 как ID контроллера
 // gsmProvider = MTS;

//  memset(&iotSettings,0,sizeof(iotSettings));
}
*/
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t GlobalSettings::read8(uint16_t address, uint8_t defaultVal)
{
    uint8_t curVal = MemRead(address);
    if(curVal == 0xFF)
      curVal = defaultVal;

   return curVal;
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint16_t GlobalSettings::read16(uint16_t address, uint16_t defaultVal)
{
    uint16_t val = 0;
    byte* b = (byte*) &val;
    
    for(byte i=0;i<2;i++)
      *b++ = MemRead(address + i);

   if(val == 0xFFFF)
    val = defaultVal;

    return val;  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::write16(uint16_t address, uint16_t val)
{
  byte* b = (byte*) &val;

  for(byte i=0;i<2;i++)
    MemWrite(address + i, *b++);
      
}
//--------------------------------------------------------------------------------------------------------------------------------------
unsigned long GlobalSettings::read32(uint16_t address, unsigned long defaultVal)
{
   unsigned long val = 0;
    byte* b = (byte*) &val;
    
    for(byte i=0;i<4;i++)
      *b++ = MemRead(address + i);

   if(val == 0xFFFFFFFF)
    val = defaultVal;

    return val;    
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::write32(uint16_t address, unsigned long val)
{
  byte* b = (byte*) &val;

  for(byte i=0;i<4;i++)
    MemWrite(address + i, *b++);  
}
//--------------------------------------------------------------------------------------------------------------------------------------
String GlobalSettings::readString(uint16_t address, byte maxlength)
{
  String result;
  for(byte i=0;i<maxlength;i++)
  {
    byte b = read8(address++,0);
    if(b == 0)
      break;

    result += (char) b;
  }

  return result;  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::writeString(uint16_t address, const String& v, byte maxlength)
{
  for(byte i=0;i<maxlength;i++)
    MemWrite(address++,v[i]);
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t GlobalSettings::GetChannelWateringWeekDays(uint8_t idx)
{
    // вычисляем начало адреса
    uint16_t readAddr = WATERING_CHANNELS_SETTINGS_EEPROM_ADDR + idx*sizeof(WateringChannelOptions);
    return read8(readAddr,0);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetChannelWateringWeekDays(uint8_t idx, uint8_t val)
{
    // вычисляем начало адреса
    uint16_t writeAddr = WATERING_CHANNELS_SETTINGS_EEPROM_ADDR + idx*sizeof(WateringChannelOptions);
    MemWrite(writeAddr, val);
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint16_t GlobalSettings::GetChannelWateringTime(uint8_t idx)
{
    uint16_t readAddr = WATERING_CHANNELS_SETTINGS_EEPROM_ADDR + idx*sizeof(WateringChannelOptions) + 1; // со второго байта в структуре идёт время продолжительностиала полива
    return read16(readAddr,0);  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetChannelWateringTime(uint8_t idx,uint16_t val)
{
    uint16_t writeAddr = WATERING_CHANNELS_SETTINGS_EEPROM_ADDR + idx*sizeof(WateringChannelOptions) + 1; // со второго байта в структуре идёт время продолжительности полива
    write16(writeAddr,val);  
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint16_t GlobalSettings::GetChannelStartWateringTime(uint8_t idx)
{
    uint16_t readAddr = WATERING_CHANNELS_SETTINGS_EEPROM_ADDR + idx*sizeof(WateringChannelOptions) + 3; // с четвёртого байта в структуре идёт время начала полива
    return read16(readAddr,0);    
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetChannelStartWateringTime(uint8_t idx,uint16_t val)
{
    uint16_t writeAddr = WATERING_CHANNELS_SETTINGS_EEPROM_ADDR + idx*sizeof(WateringChannelOptions) + 3; // с четвёртого байта в структуре идёт время начала полива
    write16(writeAddr,val);    
}
//--------------------------------------------------------------------------------------------------------------------------------------
int8_t GlobalSettings::GetChannelWateringSensorIndex(uint8_t idx)
{
   uint16_t readAddr = WATERING_CHANNELS_SETTINGS_EEPROM_ADDR + idx*sizeof(WateringChannelOptions) + 5; // с шестого байта в структуре идёт индекс датчика
  return (int8_t) read8(readAddr,-1);   
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetChannelWateringSensorIndex(uint8_t idx,int8_t val)
{
   uint16_t writeAddr = WATERING_CHANNELS_SETTINGS_EEPROM_ADDR + idx*sizeof(WateringChannelOptions) + 5; // с шестого байта в структуре идёт индекс датчика
  MemWrite(writeAddr,val);   
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t GlobalSettings::GetChannelWateringStopBorder(uint8_t idx)
{
  uint16_t readAddr = WATERING_CHANNELS_SETTINGS_EEPROM_ADDR + idx*sizeof(WateringChannelOptions) + 6; // с седьмого байта в структуре идёт значение показаний датчика
  return read8(readAddr,0);     
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetChannelWateringStopBorder(uint8_t idx,uint8_t val)
{
  uint16_t writeAddr = WATERING_CHANNELS_SETTINGS_EEPROM_ADDR + idx*sizeof(WateringChannelOptions) + 6; // с седьмого байта в структуре идёт значение показаний датчика
  MemWrite(writeAddr,val);     
}
//--------------------------------------------------------------------------------------------------------------------------------------
String GlobalSettings::GetStationPassword()
{
  return readString(STATION_PASSWORD_EEPROM_ADDR,20);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetStationPassword(const String& v)
{
  writeString( STATION_PASSWORD_EEPROM_ADDR, v,  min(20,v.length()) ); 
}
//--------------------------------------------------------------------------------------------------------------------------------------
String GlobalSettings::GetStationID()
{
  return readString(STATION_ID_EEPROM_ADDR,20);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetStationID(const String& v)
{
  writeString( STATION_ID_EEPROM_ADDR, v,  min(20,v.length()) ); 
}
//--------------------------------------------------------------------------------------------------------------------------------------
String GlobalSettings::GetRouterPassword()
{
  return readString(ROUTER_PASSWORD_EEPROM_ADDR,20);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetRouterPassword(const String& v)
{
  writeString( ROUTER_PASSWORD_EEPROM_ADDR, v,  min(20,v.length()) ); 
}
//--------------------------------------------------------------------------------------------------------------------------------------
String GlobalSettings::GetRouterID()
{
  return readString(ROUTER_ID_EEPROM_ADDR,20);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetRouterID(const String& v)
{
  writeString( ROUTER_ID_EEPROM_ADDR, v,  min(20,v.length()) ); 
}
//--------------------------------------------------------------------------------------------------------------------------------------
String GlobalSettings::GetSmsPhoneNumber()
{
  return readString(SMS_NUMBER_EEPROM_ADDR,15);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetSmsPhoneNumber(const String& v)
{
    writeString( SMS_NUMBER_EEPROM_ADDR, v,  min(15,v.length()) ); 
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetIoTSettings(IoTSettings& sett)
{
    byte writePtr = IOT_SETTINGS_EEPROM_ADDR;
    byte* readPtr = (byte*) &sett;

     for(size_t i=0;i<sizeof(IoTSettings);i++)
        MemWrite(writePtr++, *readPtr++);
}
//--------------------------------------------------------------------------------------------------------------------------------------
IoTSettings GlobalSettings::GetIoTSettings()
{
    uint16_t readPtr = IOT_SETTINGS_EEPROM_ADDR;
    IoTSettings result;
    memset(&result,0,sizeof(IoTSettings));

    byte* writePtr = (byte*) &result;
    for(size_t i=0;i<sizeof(IoTSettings);i++)
      *writePtr++ = read8(readPtr++,0);

   return result;
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t GlobalSettings::GetWiFiState()
{
  return read8(WIFI_STATE_EEPROM_ADDR,0x01);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetWiFiState(uint8_t st)
{
  MemWrite(WIFI_STATE_EEPROM_ADDR,st);
}
//--------------------------------------------------------------------------------------------------------------------------------------
unsigned long GlobalSettings::GetOpenInterval()
{
  return read32(OPEN_INTERVAL_EEPROM_ADDR,DEF_OPEN_INTERVAL);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetOpenInterval(unsigned long val)
{
  write32(OPEN_INTERVAL_EEPROM_ADDR,val);
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t GlobalSettings::GetCloseTemp()
{
  return read8(CLOSE_TEMP_EEPROM_ADDR,DEF_CLOSE_TEMP);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetCloseTemp(uint8_t val)
{
  MemWrite(CLOSE_TEMP_EEPROM_ADDR,val);
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t GlobalSettings::GetOpenTemp()
{
    return read8(OPEN_TEMP_EEPROM_ADDR,DEF_OPEN_TEMP);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetOpenTemp(uint8_t val)
{
  MemWrite(OPEN_TEMP_EEPROM_ADDR,val);
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t GlobalSettings::GetTurnOnPump()
{
  return read8(TURN_PUMP_EEPROM_ADDR,0);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetTurnOnPump(uint8_t val)
{
  MemWrite(TURN_PUMP_EEPROM_ADDR,val);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetStartWateringTime(uint16_t val)
{
  write16(START_WATERING_TIME_EEPROM_ADDR,val);
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint16_t GlobalSettings::GetStartWateringTime()
{
  return read16(START_WATERING_TIME_EEPROM_ADDR,0);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetWateringTime(uint16_t val)
{
  write16(WATERING_TIME_EEPROM_ADDR,val);    
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint16_t GlobalSettings::GetWateringTime()
{
  return read16(WATERING_TIME_EEPROM_ADDR,0);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetWateringWeekDays(uint8_t val)
{
  MemWrite(WATERING_WEEKDAYS_EEPROM_ADDR, val);
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t GlobalSettings::GetWateringWeekDays()
{
  return read8(WATERING_WEEKDAYS_EEPROM_ADDR,0);
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t GlobalSettings::GetWateringStopBorder()
{
  return read8(WATERING_STOP_BORDER_EEPROM_ADDR, 0);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetWateringStopBorder(uint8_t val)
{
  MemWrite(WATERING_STOP_BORDER_EEPROM_ADDR,val);
}
//--------------------------------------------------------------------------------------------------------------------------------------
int8_t GlobalSettings::GetWateringSensorIndex()
{
  return (int8_t) read8(WATERING_SENSOR_EEPROM_ADDR, -1);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetWateringSensorIndex(int8_t val)
{
  MemWrite(WATERING_SENSOR_EEPROM_ADDR,val);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetWateringOption(uint8_t val)
{
  MemWrite(WATERING_OPTION_EEPROM_ADDR,val);
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t GlobalSettings::GetWateringOption()
{
  return read8(WATERING_OPTION_EEPROM_ADDR, wateringOFF);
}
//--------------------------------------------------------------------------------------------------------------------------------------
byte GlobalSettings::GetGSMProvider()
{
  return read8(GSM_PROVIDER_EEPROM_ADDR,MTS); 
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool GlobalSettings::SetGSMProvider(byte p)
{
  if(p < Dummy_Last_Op) 
  {
    MemWrite(GSM_PROVIDER_EEPROM_ADDR,p);
    return true;
  }
  return false;
}
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t GlobalSettings::GetControllerID()
{
  return read8(CONTROLLER_ID_EEPROM_ADDR,0);  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetControllerID(uint8_t val)
{
  //controllerID = val;
  MemWrite(CONTROLLER_ID_EEPROM_ADDR,val);
}
//--------------------------------------------------------------------------------------------------------------------------------------
/*
void GlobalSettings::Load()
{  
  uint16_t readPtr = 0; // сбрасываем указатель чтения на начало памяти

  // читаем ID контроллера
  uint8_t cid = MemRead(CONTROLLER_ID_EEPROM_ADDR);
  if(cid != 0xFF)
    controllerID = cid;

  // читаем заголовок
  uint8_t h1,h2;
  h1 = MemRead(readPtr++);
  h2 = MemRead(readPtr++);

  if(!(h1 == SETT_HEADER1 && h2 == SETT_HEADER2)) // ничего нет в памяти
  {
    ResetToDefault(); // применяем настройки по умолчанию
    Save(); // сохраняем их
    return; // и выходим
  }
  
  // читаем температуру открытия
  tempOpen = MemRead(readPtr++);

  // читаем температуру закрытия
  tempClose = MemRead(readPtr++);

  // читаем интервал работы окон
   byte* wrAddr = (byte*) &openInterval;
  
  *wrAddr++ = MemRead(readPtr++);
  *wrAddr++ = MemRead(readPtr++);
  *wrAddr++ = MemRead(readPtr++);
  *wrAddr = MemRead(readPtr++);

  // читаем номер телефона для управления по СМС
  uint8_t smsnumlen = MemRead(readPtr++);
  if(smsnumlen != 0xFF) // есть номер телефона
  {
    for(uint8_t i=0;i<smsnumlen;i++)
      smsPhoneNumber += (char) MemRead(readPtr++);
  }

  // читаем установку контроля за поливом
  uint8_t bOpt = MemRead(readPtr++);
  if(bOpt != 0xFF) // есть настройка контроля за поливом
  {
    wateringOption = bOpt;
  } // if
  
 // читаем установку дней недели полива
  bOpt = MemRead(readPtr++);
  if(bOpt != 0xFF) // есть настройка дней недели
  {
    wateringWeekDays = bOpt;
  } // if

  // читаем время полива
  bOpt = MemRead(readPtr);
  if(bOpt != 0xFF) // есть настройка длительности полива
  {
    // читаем длительность полива
    wrAddr = (byte*) &wateringTime;
    *wrAddr++ = MemRead(readPtr++);
    *wrAddr = MemRead(readPtr++);
  }

  // читаем время начала полива
  //bOpt = EEPROM.read(readPtr++);
  uint16_t stwtime = 0;
  wrAddr = (byte*) &stwtime;

  *wrAddr++ = MemRead(readPtr++);
  *wrAddr = MemRead(readPtr++);

  if(stwtime != 0xFFFF) // есть время начала полива
  {
    startWateringTime = stwtime;
  }

  
 // читаем , включать ли насос во время полива?
  bOpt = MemRead(readPtr++);
  if(bOpt != 0xFF) // есть настройка включение насоса
  {
    turnOnPump = bOpt;
  } // if

  // читаем сохранённое кол-во настроек каналов полива
  bOpt = MemRead(readPtr++);
  uint8_t addToAddr = 0; // сколько пропустить при чтении каналов, чтобы нормально прочитать следующую настройку.
  // нужно, если сначала скомпилировали с 8 каналами, сохранили настройки из конфигуратора, а потом - перекомпилировали
  // в 2 канала. Нам надо вычитать первые два, а остальные 6 - пропустить, чтобы не покалечить настройки.
  
  if(bOpt != 0xFF)
  {
    // есть сохранённое кол-во каналов, читаем каналы
    if(bOpt > WATER_RELAYS_COUNT) // только сначала убедимся, что мы не вылезем за границы массива
    {
      addToAddr = (bOpt - WATER_RELAYS_COUNT)*sizeof(WateringChannelOptions); // 5 байт в каждой структуре настроек
      bOpt = WATER_RELAYS_COUNT;
    }

    // теперь мы можем читать настройки каналов
    uint16_t wTimeHelper = 0;
    
    for(uint8_t i=0;i<bOpt;i++)
    {
      wateringChannelsOptions[i].wateringWeekDays = MemRead(readPtr++);
      
      wrAddr = (byte*) &wTimeHelper;
      *wrAddr++ = MemRead(readPtr++);
      *wrAddr = MemRead(readPtr++);
      wateringChannelsOptions[i].wateringTime = wTimeHelper;

      // читаем время начала полива на канале
      wrAddr = (byte*) &wTimeHelper;
      *wrAddr++ = MemRead(readPtr++);
      *wrAddr = MemRead(readPtr++);
      if(wTimeHelper != 0xFFFF)
        wateringChannelsOptions[i].startWateringTime = wTimeHelper;    
      else
        wateringChannelsOptions[i].startWateringTime = 0;
        
     // wateringChannelsOptions[i].startWateringTime = EEPROM.read(readPtr++);
    } // for
    
  } // if(bOpt != 0xFF)

    // переходим на следующую настройку
     readPtr += addToAddr;

   wifiState = MemRead(readPtr++);
   if(wifiState != 0xFF) // есть сохраненные настройки Wi-Fi
   {
        // читаем ID точки доступа
        routerID = F("");
         uint8_t str_len = MemRead(readPtr++);
          for(uint8_t i=0;i<str_len;i++)
            routerID += (char) MemRead(readPtr++);

        // читаем пароль к точке доступа
        routerPassword = F("");
         str_len = MemRead(readPtr++);
          for(uint8_t i=0;i<str_len;i++)
            routerPassword += (char) MemRead(readPtr++);

        // читаем название нашей точки доступа
        stationID = F("");
         str_len = MemRead(readPtr++);
          for(uint8_t i=0;i<str_len;i++)
            stationID += (char) MemRead(readPtr++);

        // читаем пароль к нашей точке доступа
        stationPassword = F("");
         str_len = MemRead(readPtr++);
          for(uint8_t i=0;i<str_len;i++)
            stationPassword += (char) MemRead(readPtr++);
  }
   else
   {
      wifiState = 0x01;
      // применяем настройки по умолчанию
      routerID = ROUTER_ID;
      routerPassword = ROUTER_PASSWORD;
      stationID = STATION_ID;
      stationPassword = STATION_PASSWORD;

      if(!routerID.length()) // если нет названия точки доступа роутера - не коннектимся к нему
        wifiState = 0; 
   }

   byte* pB = (byte*) &iotSettings;
   for(size_t k=0;k<sizeof(iotSettings);k++)
   {
      *pB = MemRead(readPtr++);
      pB++;
   }

 //  EEPROM.get(readPtr,iotSettings);
 //  readPtr += sizeof(iotSettings);

   if(!(iotSettings.Header1 == SETT_HEADER1 && iotSettings.Header2 == SETT_HEADER2))
  {
    memset(&iotSettings,0,sizeof(iotSettings));
  }

  gsmProvider = MemRead(readPtr++);
  if(gsmProvider >= Dummy_Last_Op)
    gsmProvider = MTS;

   
  // читаем другие настройки!

  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::Save()
{
  uint16_t addr = 0;

  // пишем наш заголовок, который будет сигнализировать о наличии сохранённых настроек
  MemWrite(addr++,SETT_HEADER1);
  MemWrite(addr++,SETT_HEADER2);

  // сохраняем температуру открытия
  MemWrite(addr++,tempOpen);

  // сохраняем температуру закрытия
  MemWrite(addr++,tempClose);

  // сохраняем интервал работы
  const byte* readAddr = (const byte*) &openInterval;
  MemWrite(addr++,*readAddr++);
  MemWrite(addr++,*readAddr++);
  MemWrite(addr++,*readAddr++);
  MemWrite(addr++,*readAddr);

  // сохраняем номер телефона для управления по смс
  uint8_t smsnumlen = smsPhoneNumber.length();
  MemWrite(addr++,smsnumlen);
  
  const char* sms_c = smsPhoneNumber.c_str();
  for(uint8_t i=0;i<smsnumlen;i++)
  {
    MemWrite(addr++, *sms_c++);
  }

  // сохраняем опцию контроля за поливом
  MemWrite(addr++,wateringOption);
  
  // сохраняем дни недели для полива
  MemWrite(addr++,wateringWeekDays);

  // сохраняем продолжительность полива
  readAddr = (const byte*) &wateringTime;
  MemWrite(addr++,*readAddr++);
  MemWrite(addr++,*readAddr);

  // сохраняем время начала полива
  // EEPROM.write(addr++,startWateringTime);
  readAddr = (const byte*) &startWateringTime;
  MemWrite(addr++,*readAddr++);
  MemWrite(addr++,*readAddr);
 
  // сохраняем опцию включения насоса при поливе
   MemWrite(addr++,turnOnPump);

   // сохраняем кол-во каналов полива
   MemWrite(addr++,WATER_RELAYS_COUNT);

   // пишем настройки каналов полива
   #if WATER_RELAYS_COUNT > 0
   for(uint8_t i=0;i<WATER_RELAYS_COUNT;i++)
   {
      MemWrite(addr++,wateringChannelsOptions[i].wateringWeekDays);
      readAddr = (const byte*) &(wateringChannelsOptions[i].wateringTime);
      MemWrite(addr++,*readAddr++);
      MemWrite(addr++,*readAddr);

      // пишем время начала полива
//      EEPROM.write(addr++,wateringChannelsOptions[i].startWateringTime);
      readAddr = (const byte*) &(wateringChannelsOptions[i].startWateringTime);
      MemWrite(addr++,*readAddr++);
      MemWrite(addr++,*readAddr);
   } // for
   #endif

 // сохраняем настройки Wi-Fi
 MemWrite(addr++,wifiState);

// сохраняем ID роутера
  uint8_t str_len = routerID.length();
  MemWrite(addr++,str_len);
  
  const char* str_p = routerID.c_str();
  for(uint8_t i=0;i<str_len;i++)
    MemWrite(addr++, *str_p++);


 // сохраняем пароль к роутеру
  str_len = routerPassword.length();
  MemWrite(addr++,str_len);
  
  str_p = routerPassword.c_str();
  for(uint8_t i=0;i<str_len;i++)
    MemWrite(addr++, *str_p++);

 // сохраняем название нашей точки доступа
  str_len = stationID.length();
  MemWrite(addr++,str_len);
  
  str_p = stationID.c_str();
  for(uint8_t i=0;i<str_len;i++)
    MemWrite(addr++, *str_p++);

 // сохраняем пароль к нашей точке доступа
  str_len = stationPassword.length();
  MemWrite(addr++,str_len);
  
  str_p = stationPassword.c_str();
  for(uint8_t i=0;i<str_len;i++)
    MemWrite(addr++, *str_p++);

   iotSettings.Header1 = SETT_HEADER1;
   iotSettings.Header2 = SETT_HEADER2;

    byte* pB = (byte*) &iotSettings;
    for(size_t k=0;k<sizeof(iotSettings);k++)
    {
      MemWrite(addr++,*pB++);
    }
  
   //EEPROM.put(addr,iotSettings);
   //addr += sizeof(iotSettings);


  MemWrite(addr++,gsmProvider);
  
  // сохраняем другие настройки!
  
}
*/
//--------------------------------------------------------------------------------------------------------------------------------------
bool GlobalSettings::IsHttpApiEnabled()
{
  uint16_t addr = HTTP_API_KEY_ADDRESS + 34;
  byte en = MemRead(addr);
  if(en == 0xFF)
    en = 0; // если ничего не записано - считаем, что API выключено

  return en ? true : false;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetHttpApiEnabled(bool val)
{
  uint16_t addr = HTTP_API_KEY_ADDRESS + 34;
  MemWrite(addr,val ? 1 : 0);
}
//--------------------------------------------------------------------------------------------------------------------------------------
int16_t GlobalSettings::GetTimezone()
{
  int16_t result = 0;
  uint16_t addr = TIMEZONE_ADDRESS;
  
  byte header1 = MemRead(addr++);
  byte header2 = MemRead(addr++);

  if(header1 == SETT_HEADER1 && header2 == SETT_HEADER2)
  {
      byte* b = (byte*) &result;
      *b++ = MemRead(addr++);
      *b++ = MemRead(addr++);

      if(0xFFFF == (uint16_t)result)
        result = 0;
  }

  return result;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetTimezone(int16_t val)
{
  uint16_t addr = TIMEZONE_ADDRESS;
  
  MemWrite(addr++,SETT_HEADER1);
  MemWrite(addr++,SETT_HEADER2);

  byte* b = (byte*) &val;

  MemWrite(addr++,*b++);
  MemWrite(addr++,*b++);
  
    
}
//--------------------------------------------------------------------------------------------------------------------------------------        
bool GlobalSettings::CanSendSensorsDataToHTTP()
{
  byte en = MemRead(HTTP_SEND_SENSORS_DATA_ADDRESS);
  if(en == 0xFF)
    en = 1; // если ничего не записано - считаем, что можем отсылать данные

  return en ? true : false;  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetSendSensorsDataFlag(bool val)
{
   MemWrite(HTTP_SEND_SENSORS_DATA_ADDRESS, val ? 1 : 0); 
}
//--------------------------------------------------------------------------------------------------------------------------------------        
bool GlobalSettings::CanSendControllerStatusToHTTP()
{
  byte en = MemRead(HTTP_SEND_STATUS_ADDRESS);
  if(en == 0xFF)
    en = 1; // если ничего не записано - считаем, что можем отсылать данные

  return en ? true : false;  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetSendControllerStatusFlag(bool val)
{
   MemWrite(HTTP_SEND_STATUS_ADDRESS, val ? 1 : 0); 
}
//--------------------------------------------------------------------------------------------------------------------------------------        
String GlobalSettings::GetHttpApiKey()
{
  String result;
  uint16_t addr = HTTP_API_KEY_ADDRESS;
  
  byte header1 = MemRead(addr++);
  byte header2 = MemRead(addr++);

  if(header1 == SETT_HEADER1 && header2 == SETT_HEADER2)
  {
      for(byte i=0;i<32;i++)
      {
        char ch = (char) MemRead(addr++);
        if(ch != '\0')
          result += ch;
        else
          break;
      }
  } // if

  return result;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void GlobalSettings::SetHttpApiKey(const char* val)
{
  if(!*val)
    return;

  uint16_t addr = HTTP_API_KEY_ADDRESS;
  
  MemWrite(addr++,SETT_HEADER1);
  MemWrite(addr++,SETT_HEADER2);

  for(byte i=0;i<32;i++)
  {
      if(!*val)
      {
          MemWrite(addr++,'\0');
          break;  
      }

      MemWrite(addr++,*val);
      val++;
  } // for
    
}
//--------------------------------------------------------------------------------------------------------------------------------------


