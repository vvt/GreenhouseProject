#include "WateringModule.h"
#include "ModuleController.h"
#include "Memory.h"
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_LOG_MODULE
#include <SD.h> // пробуем записать статус полива не только в EEPROM, но и на SD-карту, если LOG-модуль есть в прошивке
#endif
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef WATER_DEBUG
  #define WTR_LOG(s) { Serial.print((s)); }
#else
  #define WTR_LOG(s) (void) 0
#endif
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#if WATER_RELAYS_COUNT > 0
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t WATER_RELAYS[] = { WATER_RELAYS_PINS }; // объявляем массив пинов реле
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringChannel::SignalToHardware()
{
    byte state = flags.isON ? WATER_RELAY_ON : WATER_RELAY_OFF;
  
    WTR_LOG(F("[WTR] - channel "));
    WTR_LOG(flags.index);
    WTR_LOG(F(" write to pin #"));
    WTR_LOG( WATER_RELAYS[flags.index] );
    WTR_LOG(F(", state = "));
    WTR_LOG(state);
    WTR_LOG(F("\r\n"));

   #if WATER_DRIVE_MODE == DRIVE_DIRECT
    
      WORK_STATUS.PinWrite(WATER_RELAYS[flags.index],state);
      
    #elif WATER_DRIVE_MODE == DRIVE_MCP23S17
        #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
        
          WORK_STATUS.MCP_SPI_PinWrite(WATER_MCP23S17_ADDRESS,WATER_RELAYS[flags.index],state);
          
        #endif
    #elif WATER_DRIVE_MODE == DRIVE_MCP23017
        #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
        
          WORK_STATUS.MCP_I2C_PinWrite(WATER_MCP23017_ADDRESS,WATER_RELAYS[flags.index],state);
          
        #endif
    #endif

  WORK_STATUS.SaveWaterChannelState(flags.index,state); // сохраняем статус канала полива
    
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
WateringChannel::WateringChannel()
{
  flags.isON = flags.lastIsON = false;
  flags.index = 0;
  flags.wateringTimer = flags.wateringDelta = 0;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringChannel::Setup(byte index)
{
    flags.index = index;
    flags.isON = flags.lastIsON = false;
    flags.wateringTimer = flags.wateringDelta = 0;
  
    WTR_LOG(F("[WTR] - setup channel "));
    WTR_LOG(flags.index);
    WTR_LOG(F("; OFF relay...\r\n"));

    #if WATER_DRIVE_MODE == DRIVE_DIRECT
      WORK_STATUS.PinMode(WATER_RELAYS[flags.index],OUTPUT);
    #elif WATER_DRIVE_MODE == DRIVE_MCP23S17
        #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
          WORK_STATUS.MCP_SPI_PinMode(WATER_MCP23S17_ADDRESS,WATER_RELAYS[flags.index],OUTPUT);
        #endif
    #elif WATER_DRIVE_MODE == DRIVE_MCP23017
        #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
          WORK_STATUS.MCP_I2C_PinMode(WATER_MCP23017_ADDRESS,WATER_RELAYS[flags.index],OUTPUT);
        #endif
    #endif    

    SignalToHardware(); // записываем в пины изначально выключенный уровень

    LoadState();
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringChannel::LoadState()
{

    WTR_LOG(F("Load state: channel - "));
    WTR_LOG(flags.index);
    WTR_LOG(F("\r\n"));
  
    GlobalSettings* settings = MainController->GetSettings();
    uint8_t currentWateringOption = settings->GetWateringOption();

    byte offset = flags.index + 1; // единичку прибавляем потому, что у нас под нулевым индексом - настройки для всех каналов одновременно

    // смотрим - чего там в опции полива
    switch(currentWateringOption)
    {
      case wateringOFF: // автоматическое управление поливом выключено
      break;

      case wateringWeekDays: // управление поливом по дням недели, все каналы одновременно
        offset = 0; // читаем сохранённое время полива для всех каналов одновременно
      break;

      case wateringSeparateChannels: // раздельное управление каналами по дням недели
      break;
    }  

    DoLoadState(offset);
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringChannel::SaveState()
{
    WTR_LOG(F("Save state: channel - "));
    WTR_LOG(flags.index);
    WTR_LOG(F("\r\n"));
  
    GlobalSettings* settings = MainController->GetSettings();
    uint8_t currentWateringOption = settings->GetWateringOption();

    byte offset = flags.index + 1; // единичку прибавляем потому, что у нас под нулевым индексом - настройки для всех каналов одновременно

    // смотрим - чего там в опции полива
    switch(currentWateringOption)
    {
      case wateringOFF: // автоматическое управление поливом выключено
      break;

      case wateringWeekDays: // управление поливом по дням недели, все каналы одновременно
        offset = 0; // пишем сохранённое время полива для всех каналов одновременно
      break;

      case wateringSeparateChannels: // раздельное управление каналами по дням недели
      break;
    }  

    DoSaveState(offset);
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringChannel::On()
{
  flags.lastIsON = flags.isON;
  flags.isON = true;
  
  if(IsChanged()) // состояние изменилось
  {
    WTR_LOG(F("[WTR] - state for channel "));
    WTR_LOG(flags.index);
    WTR_LOG(F(" changed, relay ON...\r\n"));
        
    SignalToHardware(); // записываем новое состояние в пин
  }
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringChannel::Off()
{
  flags.lastIsON = flags.isON;
  flags.isON = false;
  
  if(IsChanged()) // состояние изменилось
  {
    WTR_LOG(F("[WTR] - state for channel "));
    WTR_LOG(flags.index);
    WTR_LOG(F(" changed, relay OFF...\r\n"));
        
    SignalToHardware(); // записываем новое состояние в пин
  }
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool WateringChannel::IsChanged()
{
  return (flags.lastIsON != flags.isON);
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool WateringChannel::IsActive()
{
  return flags.isON;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringChannel::Update(uint16_t _dt,WateringWorkMode currentWorkMode, const DS3231Time& currentTime, int8_t savedDayOfWeek)
{
  #ifdef USE_DS3231_REALTIME_CLOCK

    // только если модуль часов есть в прошивке - тогда обновляем состояние

    if(currentWorkMode == wwmManual) // в ручном режиме управления, ничего не делаем
      return;

    GlobalSettings* settings = MainController->GetSettings();
    uint8_t currentWateringOption = settings->GetWateringOption();

    if(currentWateringOption == wateringOFF) // автоматическое управление каналами выключено, не надо обновлять состояние канала
      return;

     unsigned long dt = _dt;
  
     // теперь получаем настойки полива на канале. Они зависят от currentWateringOption: если там wateringWeekDays - рулим всеми каналами одновременно,
     // если там wateringSeparateChannels - рулим каналами по отдельности, если там wateringOFF - мы не попадём в эту ветку кода.
     
     uint8_t weekDays = currentWateringOption == wateringWeekDays ? settings->GetWateringWeekDays() : settings->GetChannelWateringWeekDays(flags.index);
     // получаем время начала полива, в минутах от начала суток
     uint16_t startWateringTime = currentWateringOption == wateringWeekDays ? settings->GetStartWateringTime() : settings->GetChannelStartWateringTime(flags.index);
     unsigned long timeToWatering = currentWateringOption == wateringWeekDays ? settings->GetWateringTime() : settings->GetChannelWateringTime(flags.index); // время полива (в минутах!)

      // переход через день недели мы фиксируем однократно, поэтому нам важно его не пропустить.
      // можем ли мы работать или нет - неважно, главное - правильно обработать переход через день недели.
      
      if(savedDayOfWeek != currentTime.dayOfWeek)  // сначала проверяем, не другой ли день недели уже?
      {
        // начался другой день недели. Для одного дня недели у нас установлена
        // продолжительность полива, поэтому, если мы поливали 28 минут вместо 30, например, во вторник, и перешли на среду,
        // то в среду надо полить ещё 2 мин. Поэтому таймер полива переводим в нужный режим:
        // оставляем в нём недополитое время, чтобы учесть, что поливать надо, например, ещё 2 минуты.

         flags.wateringDelta = 0; // обнуляем дельту дополива, т.к. мы в этот день можем и не работать

        if(bitRead(weekDays,currentTime.dayOfWeek-1)) // можем работать в этот день недели, значит, надо скорректировать значение таймера
        {
          // вычисляем разницу между полным и отработанным временем
            unsigned long wateringDelta = ((timeToWatering*60000) - flags.wateringTimer);
            // запоминаем для канала дополнительную дельту для работы
            flags.wateringDelta = wateringDelta;
        }

        flags.wateringTimer = 0; // сбрасываем таймер полива, т.к. начался новый день недели
        
      } // if(savedDayOfWeek != currentTime.dayOfWeek)      


    // проверяем, установлен ли у нас день недели для полива, и настала ли минута, с которого можно поливать
    uint16_t currentTimeInMinutes = currentTime.hour*60 + currentTime.minute;
    bool canWork = bitRead(weekDays,currentTime.dayOfWeek-1) && (currentTimeInMinutes >= startWateringTime);
  
    if(!canWork)
     { 
       Off(); // выключаем реле
     }
     else
     {
      // можем работать, смотрим, не вышли ли мы за пределы установленного интервала

      flags.wateringTimer += dt; // прибавляем время работы
  
      // проверяем, можем ли мы ещё работать
      // если полив уже отработал, и юзер прибавит минуту - мы должны поливать ещё минуту,
      // вне зависимости от показания таймера. Поэтому мы при срабатывании условия окончания полива
      // просто отнимаем дельту времени из таймера, таким образом оставляя его застывшим по времени
      // окончания полива
  
      if(flags.wateringTimer > ((timeToWatering*60000) + flags.wateringDelta + dt)) // приплыли, надо выключать полив
      {
        flags.wateringTimer -= (dt + flags.wateringDelta);// оставляем таймер застывшим на окончании полива, плюс маленькая дельта
        flags.wateringDelta = 0; // сбросили дельту дополива

        if(IsActive()) // если канал был включён, значит, он будет выключен, и мы однократно запишем в EEPROM нужное значение
        {
         SaveState();
        } // if(IsActive())

        Off(); // выключаем реле
      
      }
      else
        On(); // ещё можем работать, продолжаем поливать
        
     } // else can work
     

  #endif // USE_DS3231_REALTIME_CLOCK
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringChannel::DoLoadState(byte addressOffset)
{
   // сперва сбрасываем настройки времени полива и дополива
   flags.wateringTimer = flags.wateringDelta = 0;
  
#ifdef USE_DS3231_REALTIME_CLOCK

  // читать время полива на канале имеет смысл только, когда модуль часов реального времени есть в прошивке
    DS3231Clock watch =  MainController->GetClock();
    DS3231Time t =   watch.getTime();
    uint8_t today = t.dayOfWeek; // текущий день недели
  
    WTR_LOG(F("[WTR] - load state for channel "));
    WTR_LOG(flags.index);
    WTR_LOG(F(" from EEPROM...\r\n"));

    unsigned long savedWorkTime = 0xFFFFFFFF;
    volatile byte* writeAddr = (byte*) &savedWorkTime;
    uint8_t savedDOW = 0xFF;

    // мы читаем 5 байт на канал, поэтому вычисляем адрес очень просто - по смещению addressOffset, в котором находится индекс канала
    volatile uint16_t curReadAddr = WATERING_STATUS_EEPROM_ADDR + addressOffset*5;

    savedDOW = MemRead(curReadAddr++);

    *writeAddr++ = MemRead(curReadAddr++);
    *writeAddr++ = MemRead(curReadAddr++);
    *writeAddr++ = MemRead(curReadAddr++);
    *writeAddr = MemRead(curReadAddr++);

    if(savedDOW != 0xFF && savedWorkTime != 0xFFFFFFFF) // есть сохранённое время работы канала на сегодня
    {
      WTR_LOG(F("[WTR] - data is OK...\r\n"));
      
      if(savedDOW == today) // поливали на этом канале сегодня, выставляем таймер канала так, как будто он уже поливался сколько-то времени
      {
        flags.wateringTimer = savedWorkTime + 1;
      }
      
    } // if    
 #else
    WTR_LOG(F("[WTR] - NO state for channel - no realtime clock!\r\n"));
 #endif // USE_DS3231_REALTIME_CLOCK

}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringChannel::DoSaveState(byte addressOffset)
{  
#ifdef USE_DS3231_REALTIME_CLOCK

  // писать время полива на канале имеет смысл только, когда модуль часов реального времени есть в прошивке
    DS3231Clock watch =  MainController->GetClock();
    DS3231Time t =   watch.getTime();
    uint8_t today = t.dayOfWeek; // текущий день недели 
    
    WTR_LOG(F("[WTR] - save state for channel "));
    WTR_LOG(flags.index);
    WTR_LOG(F(" to EEPROM...\r\n"));

     GlobalSettings* settings = MainController->GetSettings();

    // получаем время полива на канале. Логика простая: если addressOffset == 0 - у нас опция полива по дням недели, все каналы одновременно,
    // иначе - раздельное управление каналами по дням недели. Соответственно, мы либо получаем время полива для всех каналов, либо - для нужного.
    unsigned long timeToWatering = addressOffset == 0 ? settings->GetWateringTime() : settings->GetChannelWateringTime(flags.index); // время полива (в минутах!)

     //Тут сохранение в EEPROM статуса, что мы на сегодня уже полили сколько-то времени на канале
    uint16_t wrAddr = WATERING_STATUS_EEPROM_ADDR + addressOffset*5; // адрес записи
    
    // сохраняем в EEPROM день недели, для которого запомнили значение таймера
    MemWrite(wrAddr++,today);
    
    // сохраняем в EEPROM значение таймера канала
    unsigned long ttw = timeToWatering*60000; // запишем полное время полива на сегодня
    byte* readAddr = (byte*) &ttw;
    for(int i=0;i<4;i++)
      MemWrite(wrAddr++,*readAddr++);
    
 #else
    WTR_LOG(F("[WTR] - NO state for channel - no realtime clock!\r\n"));
 #endif // USE_DS3231_REALTIME_CLOCK

}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // WATER_RELAYS_COUNT > 0
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_PUMP_RELAY
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::GetPumpsState(bool& pump1State, bool& pump2State)
{
  pump1State = false;
  pump2State = false;

  GlobalSettings* settings = MainController->GetSettings();
  if(settings->GetTurnOnPump() != 1)
  {
    // не надо включать насосы при поливе на любом из каналов
    return;
  }
  
  #if WATER_RELAYS_COUNT > 0
    for(byte i=0;i<WATER_RELAYS_COUNT;i++)
    {
        if(wateringChannels[i].IsActive())
        {
          // канал активен, смотрим, к какому насосу он относится
          #ifdef USE_SECOND_PUMP
            // два насоса в прошивке
             if(i < SECOND_PUMP_START_CHANNEL)
             {
                // канал относится к первому насосу
                pump1State = true;
             }
             else
             {
              // канал относится ко второму насосу
              pump2State = true;
             }
          #else
           // один насос в прошивке, включаем по-любому
           pump1State = true;
           return;
          #endif // #ifdef USE_SECOND_PUMP
        }
    } // for
  #endif // WATER_RELAYS_COUNT > 0
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::UpdatePumps()
{
  bool anyChannelActive = IsAnyChannelActive();
  if(!anyChannelActive)
  {
    // нет полива ни на одном из каналов, выключаем насосы
    TurnPump1(false);

    #ifdef USE_SECOND_PUMP // второй насос
      TurnPump2(false);
    #endif
    
    return;
  }

  // здесь проверяем, какой насос включить. Для этого узнаём, есть ли активные каналы для первого и второго насоса
  bool shouldTurnPump1,shouldTurnPump2;

  // проверяем статус, который надо выставить для насосов
  GetPumpsState(shouldTurnPump1,shouldTurnPump2);

  TurnPump1(shouldTurnPump1);
  
  #ifdef USE_SECOND_PUMP
    TurnPump2(shouldTurnPump2);
  #endif
  
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::TurnPump1(bool isOn)
{
  if(flags.isPump1On == isOn) // состояние не изменилось
    return;

  // сохраняем состояние
  flags.isPump1On = isOn;

  WTR_LOG(F("Turn pump #1 to state: "));
  WTR_LOG(isOn ? F("ON\r\n") : F("OFF\r\n"));
  
  byte state = isOn ? WATER_PUMP_RELAY_ON : WATER_PUMP_RELAY_OFF;
  
  #if WATER_PUMP_DRIVE_MODE == DRIVE_DIRECT
    WORK_STATUS.PinWrite(PUMP_RELAY_PIN,state);
  #elif WATER_PUMP_DRIVE_MODE == DRIVE_MCP23S17
    #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
      WORK_STATUS.MCP_SPI_PinWrite(WATER_PUMP_MCP23S17_ADDRESS,PUMP_RELAY_PIN,state);
    #endif
  #elif WATER_PUMP_DRIVE_MODE == DRIVE_MCP23017
    #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
      WORK_STATUS.MCP_I2C_PinWrite(WATER_PUMP_MCP23017_ADDRESS,PUMP_RELAY_PIN,state);
    #endif
  #endif  
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_SECOND_PUMP
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::TurnPump2(bool isOn)
{
  if(flags.isPump2On == isOn) // состояние не изменилось
    return;

  // сохраняем состояние
  flags.isPump2On = isOn;

  WTR_LOG(F("Turn pump #2 to state: "));
  WTR_LOG(isOn ? F("ON\r\n") : F("OFF\r\n"));
  
  byte state = isOn ? WATER_PUMP_RELAY_ON : WATER_PUMP_RELAY_OFF;
  
    #if WATER_PUMP2_DRIVE_MODE == DRIVE_DIRECT
      WORK_STATUS.PinWrite(SECOND_PUMP_RELAY_PIN,state);
    #elif WATER_PUMP2_DRIVE_MODE == DRIVE_MCP23S17
      #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
        WORK_STATUS.MCP_SPI_PinWrite(WATER_PUMP_MCP23S17_ADDRESS,SECOND_PUMP_RELAY_PIN,state);
      #endif
    #elif WATER_PUMP2_DRIVE_MODE == DRIVE_MCP23017
      #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
        WORK_STATUS.MCP_I2C_PinWrite(WATER_PUMP_MCP23017_ADDRESS,SECOND_PUMP_RELAY_PIN,state);
      #endif
    #endif
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_SECOND_PUMP
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::SetupPumps()
{
    WTR_LOG(F("[WTR] = setup pumps\r\n"));

    WTR_LOG(F("[WTR] - Turn OFF pump 1\r\n"));
  
  #if WATER_PUMP_DRIVE_MODE == DRIVE_DIRECT
    WORK_STATUS.PinMode(PUMP_RELAY_PIN,OUTPUT);
    WORK_STATUS.PinWrite(PUMP_RELAY_PIN,WATER_PUMP_RELAY_OFF);
  #elif WATER_PUMP_DRIVE_MODE == DRIVE_MCP23S17
    #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
      WORK_STATUS.MCP_SPI_PinMode(WATER_PUMP_MCP23S17_ADDRESS,PUMP_RELAY_PIN,OUTPUT);
      WORK_STATUS.MCP_SPI_PinWrite(WATER_PUMP_MCP23S17_ADDRESS,PUMP_RELAY_PIN,WATER_PUMP_RELAY_OFF);
    #endif
  #elif WATER_PUMP_DRIVE_MODE == DRIVE_MCP23017
    #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
      WORK_STATUS.MCP_I2C_PinMode(WATER_PUMP_MCP23017_ADDRESS,PUMP_RELAY_PIN,OUTPUT);
      WORK_STATUS.MCP_I2C_PinWrite(WATER_PUMP_MCP23017_ADDRESS,PUMP_RELAY_PIN,WATER_PUMP_RELAY_OFF);
    #endif
  #endif

  #ifdef USE_SECOND_PUMP // второй насос

    WTR_LOG(F("[WTR] - Turn OFF pump 2\r\n"));
  
    #if WATER_PUMP2_DRIVE_MODE == DRIVE_DIRECT
      WORK_STATUS.PinMode(SECOND_PUMP_RELAY_PIN,OUTPUT);
      WORK_STATUS.PinWrite(SECOND_PUMP_RELAY_PIN,WATER_PUMP_RELAY_OFF);
    #elif WATER_PUMP2_DRIVE_MODE == DRIVE_MCP23S17
      #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
        WORK_STATUS.MCP_SPI_PinMode(WATER_PUMP_MCP23S17_ADDRESS,SECOND_PUMP_RELAY_PIN,OUTPUT);
        WORK_STATUS.MCP_SPI_PinWrite(WATER_PUMP_MCP23S17_ADDRESS,SECOND_PUMP_RELAY_PIN,WATER_PUMP_RELAY_OFF);
      #endif
    #elif WATER_PUMP2_DRIVE_MODE == DRIVE_MCP23017
      #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
        WORK_STATUS.MCP_I2C_PinMode(WATER_PUMP_MCP23017_ADDRESS,SECOND_PUMP_RELAY_PIN,OUTPUT);
        WORK_STATUS.MCP_I2C_PinWrite(WATER_PUMP_MCP23017_ADDRESS,SECOND_PUMP_RELAY_PIN,WATER_PUMP_RELAY_OFF);
      #endif
    #endif
    
  #endif // USE_SECOND_PUMP      
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_PUMP_RELAY
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::Setup()
{
  // настройка модуля тут
  WTR_LOG(F("[WTR] - setup...\r\n"));

 // GlobalSettings* settings = MainController->GetSettings();

  flags.workMode = wwmAutomatic; // автоматический режим работы
  flags.isPump1On = false;
  flags.isPump2On = false;

  // настраиваем насосы
  #ifdef USE_PUMP_RELAY
    SetupPumps();
  #endif
  
  #if WATER_RELAYS_COUNT > 0

    // настраиваем каналы
    for(uint8_t i=0;i<WATER_RELAYS_COUNT;i++)
    {
        // просим канал настроиться, она загрузит свои настройки и выключит реле
        wateringChannels[i].Setup(i);
    } // for
  
  #endif // WATER_RELAYS_COUNT > 0

  // если указано - использовать диод индикации ручного режима работы - настраиваем его
  #ifdef USE_WATERING_MANUAL_MODE_DIODE
    blinker.begin(DIODE_WATERING_MANUAL_MODE_PIN);  // настраиваем блинкер на нужный пин
    blinker.blink(); // и гасим его по умолчанию
  #endif  


  #ifdef USE_DS3231_REALTIME_CLOCK

    DS3231Clock watch =  MainController->GetClock();
    DS3231Time t =   watch.getTime();

    lastDOW = t.dayOfWeek; // запоминаем прошлый день недели
    currentDOW = t.dayOfWeek; // запоминаем текущий день недели
  
  #else
  
    lastDOW = 0; // день недели с момента предыдущего опроса
    currentDOW = 0; // текущий день недели
    
  #endif // USE_DS3231_REALTIME_CLOCK

  // тут всё настроили, перешли в автоматический режим работы, выключили реле на всех каналах, запомнили текущий час и день недели.
  // можно начинать работать

 
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::SwitchToAutomaticMode()
{
  if(flags.workMode == wwmAutomatic) // уже в автоматическом режиме
    return;

  WTR_LOG(F("[WTR] - switch to automatic mode\r\n"));

    flags.workMode = wwmAutomatic;

  // гасим блинкер, если он используется в прошивке
  #ifdef USE_WATERING_MANUAL_MODE_DIODE
     blinker.blink();
  #endif    
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::SwitchToManualMode()
{
  if(flags.workMode == wwmManual) // уже в ручном режиме
    return;

  WTR_LOG(F("[WTR] - switch to manual mode\r\n"));

    flags.workMode = wwmManual;

  // зажигаем блинкер, если он используется в прошивке
  #ifdef USE_WATERING_MANUAL_MODE_DIODE
     blinker.blink(WORK_MODE_BLINK_INTERVAL);
  #endif    
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::ResetChannelsState()
{
  WTR_LOG(F("[WTR] - reset channels state\r\n"));
  //Тут затирание в EEPROM предыдущего сохранённого значения о статусе полива на всех каналах
  uint16_t wrAddr = WATERING_STATUS_EEPROM_ADDR;
  uint8_t bytes_to_write = 5 + WATER_RELAYS_COUNT*5;
  
  for(uint8_t i=0;i<bytes_to_write;i++)
    MemWrite(wrAddr++,0); // для каждого канала по отдельности  
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::TurnChannelsOff() // выключает все каналы
{
  WTR_LOG(F("[WTR] - turn all channels OFF\r\n"));
  
  #if WATER_RELAYS_COUNT > 0
    for(byte i=0;i<WATER_RELAYS_COUNT;i++)
    {
      wateringChannels[i].Off();
    }
  #endif // WATER_RELAYS_COUNT > 0
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::TurnChannelsOn() // включает все каналы
{
  WTR_LOG(F("[WTR] - turn all channels ON\r\n"));
  
  #if WATER_RELAYS_COUNT > 0
    for(byte i=0;i<WATER_RELAYS_COUNT;i++)
    {
      wateringChannels[i].On();
    }
  #endif // WATER_RELAYS_COUNT > 0  
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::TurnChannelOff(byte channelIndex) // выключает канал
{
  WTR_LOG(F("[WTR] - turn channel "));
  WTR_LOG(channelIndex);
  WTR_LOG(F(" OFF\r\n"));
  
  #if WATER_RELAYS_COUNT > 0
    if(channelIndex < WATER_RELAYS_COUNT)
    {
      wateringChannels[channelIndex].Off();
    }
  #endif // WATER_RELAYS_COUNT > 0  
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::TurnChannelOn(byte channelIndex) // включает канал
{
  WTR_LOG(F("[WTR] - turn channel "));
  WTR_LOG(channelIndex);
  WTR_LOG(F(" ON\r\n"));
    
  #if WATER_RELAYS_COUNT > 0
    if(channelIndex < WATER_RELAYS_COUNT)
    {
      wateringChannels[channelIndex].On();
    }
  #endif // WATER_RELAYS_COUNT > 0
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool WateringModule::IsAnyChannelActive() // проверяет, активен ли хоть один канал полива
{
  #if WATER_RELAYS_COUNT > 0
    for(byte i=0;i<WATER_RELAYS_COUNT;i++)
    {
      if(wateringChannels[i].IsActive())
        return true;
    }
  #endif // WATER_RELAYS_COUNT > 0    

  return false;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void WateringModule::Update(uint16_t dt)
{ 
  
  // обновляем блинкер, если он используется в прошивке
  #ifdef USE_WATERING_MANUAL_MODE_DIODE
     blinker.update(dt);
  #endif

  #if WATER_RELAYS_COUNT > 0

    bool anyChannelActive = IsAnyChannelActive(); // проверяем, включен ли хотя бы один канал
     // теперь сохраняем статус полива
    SAVE_STATUS(WATER_STATUS_BIT, anyChannelActive ? 1 : 0); // сохраняем состояние полива
    SAVE_STATUS(WATER_MODE_BIT,flags.workMode == wwmAutomatic ? 1 : 0); // сохраняем режим работы полива

    DS3231Time t;
    
    #ifdef USE_DS3231_REALTIME_CLOCK

      // обновлять каналы имеет смысл только при наличии часов реального времени
      DS3231Clock watch =  MainController->GetClock();
      t =  watch.getTime(); // получаем текущее время

    if(currentDOW != t.dayOfWeek)
    {
      // начался новый день недели, принудительно переходим в автоматический режим работы
      // даже если до этого был включен полив командой от пользователя
      SwitchToAutomaticMode();

      // здесь надо принудительно гасить полив на всех каналах, поскольку у нас может быть выключено автоуправление каналами.
      // в этом случае, если полив был включен пользователем и настали новые сутки - полив не выключится сам,
      // т.к. канал не обновляет своё состояние при выключенном автоуправлении каналами.
      TurnChannelsOff();

      //Тут затирание в EEPROM предыдущего сохранённого значения о статусе полива на всех каналах
      ResetChannelsState();
      
    }

    currentDOW = t.dayOfWeek; // сохраняем текущий день недели

    #endif // USE_DS3231_REALTIME_CLOCK

    // теперь обновляем все каналы
    for(uint8_t i=0;i<WATER_RELAYS_COUNT;i++)
    {
        wateringChannels[i].Update(dt,(WateringWorkMode) flags.workMode,t,lastDOW);
    } // for

    #ifdef USE_PUMP_RELAY
      UpdatePumps();
    #endif

   // обновили все каналы, теперь можно сбросить флаг перехода через день недели
    lastDOW = currentDOW; // сделаем вид, что мы ничего не знаем о переходе на новый день недели.
    // таким образом, код перехода на новый день недели выполнится всего один раз при каждом переходе
    // через день недели.    
  
  #else
    UNUSED(dt);
  #endif // WATER_RELAYS_COUNT > 0

  
}
bool  WateringModule::ExecCommand(const Command& command, bool wantAnswer)
{
  UNUSED(wantAnswer);
  PublishSingleton = UNKNOWN_COMMAND;

  size_t argsCount = command.GetArgsCount();
    
  if(command.GetType() == ctSET) 
  {   
      if(argsCount < 1) // не хватает параметров
      {
        PublishSingleton = PARAMS_MISSED;
      }
      else
      {
        String which = command.GetArg(0);
        //which.toUpperCase();

        if(which == WATER_SETTINGS_COMMAND)
        {
          if(argsCount > 5)
          {
              // парсим параметры
              uint8_t wateringOption = (uint8_t) atoi(command.GetArg(1)); 
              uint8_t wateringWeekDays = (uint8_t) atoi(command.GetArg(2)); 
              uint16_t wateringTime = (uint16_t) atoi(command.GetArg(3)); 
              uint16_t startWateringTime = (uint16_t) atoi(command.GetArg(4)); 
              uint8_t turnOnPump = (uint8_t) atoi(command.GetArg(5));

              GlobalSettings* settings = MainController->GetSettings();

              byte oldWateringOption = settings->GetWateringOption();
              
              // пишем в настройки
              settings->SetWateringOption(wateringOption);
              settings->SetWateringWeekDays(wateringWeekDays);
              settings->SetWateringTime(wateringTime);
              settings->SetStartWateringTime(startWateringTime);
              settings->SetTurnOnPump(turnOnPump);
      
              // сохраняем настройки
              settings->Save();

              if(oldWateringOption != wateringOption)
              {
                 // состояние управления поливом изменилось, мы должны перезагрузить для всех каналов настройки из EEPROM
                 #if WATER_RELAYS_COUNT > 0

                  for(byte i=0;i<WATER_RELAYS_COUNT;i++)
                  {
                      wateringChannels[i].LoadState();
                  } // for
                 
                 #endif // WATER_RELAYS_COUNT > 0
              }

              // Поскольку пришла команда от юзера, и там среди параметров присутствует опция
              // управления поливом, то мы ВРОДЕ КАК должны переключиться в автоматический режим работы.
              // Если придёт команда из правил - мы выключим автоуправление поливом, и всё.
              // Если автоуправление поливом выключено - то мы в этой точке не вправе гасить
              // полив на всех каналах, т.к. юзер может вручную до этого включить канал полива.
              // Исходя из вышеизложенного - при изменении опции управления поливом делать ничего
              // не надо, кроме как вычитать состояние каналов из EEPROM, т.к. режим работы
              // может быть как ручным, так и автоматическим.              
              
              PublishSingleton.Status = true;
              PublishSingleton = WATER_SETTINGS_COMMAND; 
              PublishSingleton << PARAM_DELIMITER << REG_SUCC;
          } // argsCount > 3
          else
          {
            // не хватает команд
            PublishSingleton = PARAMS_MISSED;
          }
          
        } // WATER_SETTINGS_COMMAND
        else
        if(which == WATER_CHANNEL_SETTINGS) // настройки канала CTSET=WATER|CH_SETT|IDX|WateringDays|WateringTime|StartTime
        {
           if(argsCount > 4)
           {
              #if WATER_RELAYS_COUNT > 0
                uint8_t channelIdx = (uint8_t) atoi(command.GetArg(1));
                if(channelIdx < WATER_RELAYS_COUNT)
                {
                  // нормальный индекс
                  uint8_t wDays = (uint8_t) atoi(command.GetArg(2));
                  uint16_t wTime =(uint16_t) atoi(command.GetArg(3));
                  uint16_t sTime = (uint16_t) atoi(command.GetArg(4));

                  GlobalSettings* settings = MainController->GetSettings();
                  
                  settings->SetChannelWateringWeekDays(channelIdx,wDays);
                  settings->SetChannelWateringTime(channelIdx,wTime);
                  settings->SetChannelStartWateringTime(channelIdx,sTime);
                  
                  PublishSingleton.Status = true;
                  PublishSingleton = WATER_CHANNEL_SETTINGS; 
                  PublishSingleton << PARAM_DELIMITER << (command.GetArg(1)) << PARAM_DELIMITER << REG_SUCC;
                 
                }
                else
                {
                  // плохой индекс
                  PublishSingleton = UNKNOWN_COMMAND;
                }
             #else
              PublishSingleton = UNKNOWN_COMMAND;
             #endif // WATER_RELAYS_COUNT > 0
           }
           else
           {
            // не хватает команд
            PublishSingleton = PARAMS_MISSED;            
           }
        }
        else
        if(which == WORK_MODE) // CTSET=WATER|MODE|AUTO, CTSET=WATER|MODE|MANUAL
        {
           // попросили установить режим работы
           String param = command.GetArg(1);
           
           if(param == WM_AUTOMATIC)
           {
            // переходим в автоматический режим работы
            SwitchToAutomaticMode();
           }
           else
           {
            // переходим на ручной режим работы
             SwitchToManualMode();
           }

              PublishSingleton.Status = true;
              PublishSingleton = WORK_MODE; 
              PublishSingleton << PARAM_DELIMITER << param;

              
        
        } // WORK_MODE
        else 
        if(which == STATE_ON) // попросили включить полив на всех каналах, CTSET=WATER|ON, или на одном из каналов: CTSET=WATER|ON|2
        {
           // Здесь ситуация интересная: мы можем быть в автоматическом режиме работы или в ручном.
           // если мы в ручном режиме работы и команда пришла не от юзера (а, например, из правил) - то нам надо выключить
           // автоуправление поливом и перейти в автоматический режим работы.
           // если же мы в автоматическом режиме и команда пришла не от юзера - также выключаем автоуправление поливом.
           // если команда пришла от юзера - переходим в ручной режим работы

           if(argsCount < 2) // для всех каналов запросили
              TurnChannelsOn(); // включаем все каналы
           else
           {
             // запросили для одного канала
             byte channelIndex = (byte) atoi(command.GetArg(1));
             #if WATER_RELAYS_COUNT > 0
              if(channelIndex < WATER_RELAYS_COUNT)
                TurnChannelOn(channelIndex); // включаем полив на канале
             #endif // WATER_RELAYS_COUNT > 0
           }

           // потом смотрим - откуда команда
           if(command.IsInternal())
           {
             // внутренняя команда
             GlobalSettings* settings = MainController->GetSettings();
             // выключаем автоуправление поливом
             settings->SetWateringOption(wateringOFF);

             if(flags.workMode == wwmManual)
             {
               // мы в ручном режиме работы, пришла внутренняя команда - надо переключиться в автоматический режим работы
               SwitchToAutomaticMode();
             }
            
           } // internal command
           else
           {
            // команда от пользователя
              SwitchToManualMode(); // переключаемся в ручной режим работы
           } // command from user
        
          PublishSingleton.Status = true;
          PublishSingleton = STATE_ON;
          if(argsCount > 1)
          {
            PublishSingleton << PARAM_DELIMITER;
            PublishSingleton << command.GetArg(1);
          }
        } // STATE_ON
        else 
        if(which == STATE_OFF) // попросили выключить полив на всех каналах, CTSET=WATER|OFF, или для одного канала: CTSET=WATER|OFF|3
        { 
           if(argsCount < 2)
            TurnChannelsOff(); // выключаем все каналы
           else
           {
             // запросили для одного канала
             byte channelIndex = (byte) atoi(command.GetArg(1));
             #if WATER_RELAYS_COUNT > 0
              if(channelIndex < WATER_RELAYS_COUNT)
                TurnChannelOff(channelIndex); // выключаем полив на канале
             #endif // WATER_RELAYS_COUNT > 0
           }            

           // потом смотрим - откуда команда
           if(command.IsInternal())
           {
             // внутренняя команда
             GlobalSettings* settings = MainController->GetSettings();
             // выключаем автоуправление поливом
             settings->SetWateringOption(wateringOFF);

             if(flags.workMode == wwmManual)
             {
               // мы в ручном режиме работы, пришла внутренняя команда - надо переключиться в автоматический режим работы
               SwitchToAutomaticMode();
             }
            
           } // internal command
           else
           {
            // команда от пользователя
              SwitchToManualMode(); // переключаемся в ручной режим работы
           } // command from user 

          PublishSingleton.Status = true;
          PublishSingleton = STATE_OFF;
          if(argsCount > 1)
          {
            PublishSingleton << PARAM_DELIMITER;
            PublishSingleton << command.GetArg(1);
          }         
        } // STATE_OFF        

      } // else
  }
  else
  if(command.GetType() == ctGET) //получить данные
  {    
    if(!argsCount) // нет аргументов, попросили вернуть статус полива
    {
      PublishSingleton.Status = true;
      #if WATER_RELAYS_COUNT > 0
      
        PublishSingleton = (IsAnyChannelActive() ? STATE_ON : STATE_OFF);
        
      #else
        PublishSingleton = STATE_OFF;
      #endif //  WATER_RELAYS_COUNT > 0
      
      PublishSingleton << PARAM_DELIMITER << (flags.workMode == wwmAutomatic ? WM_AUTOMATIC : WM_MANUAL);
    }
    else
    {
      String t = command.GetArg(0);
      
        if(t == WATER_SETTINGS_COMMAND) // запросили данные о настройках полива
        {
          GlobalSettings* settings = MainController->GetSettings();
          
          PublishSingleton.Status = true;
          PublishSingleton = WATER_SETTINGS_COMMAND; 
          PublishSingleton << PARAM_DELIMITER; 
          PublishSingleton << (settings->GetWateringOption()) << PARAM_DELIMITER;
          PublishSingleton << (settings->GetWateringWeekDays()) << PARAM_DELIMITER;
          PublishSingleton << (settings->GetWateringTime()) << PARAM_DELIMITER;
          PublishSingleton << (settings->GetStartWateringTime()) << PARAM_DELIMITER;
          PublishSingleton << (settings->GetTurnOnPump());
        }
        else
        if(t == WATER_CHANNELS_COUNT_COMMAND)
        {
          PublishSingleton.Status = true;
          PublishSingleton = WATER_CHANNELS_COUNT_COMMAND; 
          PublishSingleton << PARAM_DELIMITER << WATER_RELAYS_COUNT;
          
        }
        else
        if(t == WORK_MODE) // получить режим работы
        {
          PublishSingleton.Status = true;
          PublishSingleton = WORK_MODE; 
          PublishSingleton << PARAM_DELIMITER << (flags.workMode == wwmAutomatic ? WM_AUTOMATIC : WM_MANUAL);
        }
        else
        if(t == F("STATEMASK")) // запросили маску состояния каналов
        {
          PublishSingleton.Status = true;
          PublishSingleton = F("STATEMASK");
          PublishSingleton << PARAM_DELIMITER << WATER_RELAYS_COUNT;
          
          #if WATER_RELAYS_COUNT > 0
          
              PublishSingleton << PARAM_DELIMITER;
              ControllerState state = WORK_STATUS.GetState();

              for(byte i=0;i<WATER_RELAYS_COUNT;i++)
              {
                if(state.WaterChannelsState & (1 << i)) // канал включен
                {
                  PublishSingleton << F("1");
                }
                else // канал выключен
                {
                  PublishSingleton << F("0");
                }
              } // for
              
          #endif // WATER_RELAYS_COUNT > 0
        } // STATEMASK
        else
        {
           // команда с аргументами
           if(argsCount > 1)
           {
                t = command.GetArg(0);
    
                if(t == WATER_CHANNEL_SETTINGS)
                {
                  #if WATER_RELAYS_COUNT > 0
                  // запросили настройки канала
                  uint8_t idx = (uint8_t) atoi(command.GetArg(1));
                  
                  if(idx < WATER_RELAYS_COUNT)
                  {
                    PublishSingleton.Status = true;

                    GlobalSettings* settings = MainController->GetSettings();
                 
                    PublishSingleton = WATER_CHANNEL_SETTINGS; 
                    PublishSingleton << PARAM_DELIMITER << (command.GetArg(1)) << PARAM_DELIMITER 
                    << (settings->GetChannelWateringWeekDays(idx)) << PARAM_DELIMITER
                    << (settings->GetChannelWateringTime(idx)) << PARAM_DELIMITER
                    << (settings->GetChannelStartWateringTime(idx));
                  }
                  else
                  {
                    // плохой индекс
                    PublishSingleton = UNKNOWN_COMMAND;
                  }
                  #else
                    PublishSingleton = UNKNOWN_COMMAND;
                  #endif // WATER_RELAYS_COUNT > 0
                          
                } // if
           } // if
        } // else
    } // else have arguments
  } // if ctGET
 
 // отвечаем на команду
    MainController->Publish(this,command);
    
  return PublishSingleton.Status;
}

