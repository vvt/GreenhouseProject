#include "TimerModule.h"
#include "ModuleController.h"
#include "Memory.h"
//--------------------------------------------------------------------------------------------------------------------------------
// PeriodicTimer
//--------------------------------------------------------------------------------------------------------------------------------
PeriodicTimer::PeriodicTimer()
{
  flags.isHoldOnTimer = true;
  tTimer = 0;
  flags.lastPinState = 3;
  memset(&Settings,0,sizeof(Settings));
}
//--------------------------------------------------------------------------------------------------------------------------------
void PeriodicTimer::On()
{
  if(!Settings.Pin)
    return;

  if(flags.lastPinState != TIMER_ON)
  {
    flags.lastPinState = TIMER_ON;
    WORK_STATUS.PinWrite(Settings.Pin,TIMER_ON);
  }
     
}
//--------------------------------------------------------------------------------------------------------------------------------
void PeriodicTimer::Off()
{
  if(!Settings.Pin)
    return;

  if(flags.lastPinState != TIMER_OFF)
  {
    flags.lastPinState = TIMER_OFF;
    WORK_STATUS.PinWrite(Settings.Pin,TIMER_OFF);
  }
     
}
//--------------------------------------------------------------------------------------------------------------------------------
void PeriodicTimer::Init()
{
  if(Settings.Pin)
  {
    WORK_STATUS.PinMode(Settings.Pin, OUTPUT);
    
    if(IsActive())
      On();
    else
      Off();
  }
}
//--------------------------------------------------------------------------------------------------------------------------------
bool PeriodicTimer::IsActive()
{
  bool en = (Settings.DayMaskAndEnable & 128);

  #ifndef USE_DS3231_REALTIME_CLOCK
    // нет часов реального времени в прошивке, работаем просто по флагу "Таймер включён"
    return en;
  #else
    // модуль часов реального времени есть в прошивке, проверяем маску дней недели
    DS3231Clock watch =  MainController->GetClock();
    DS3231Time t =   watch.getTime();
    return en && (Settings.DayMaskAndEnable & (1 << (t.dayOfWeek-1)));
  #endif
}
//--------------------------------------------------------------------------------------------------------------------------------
void PeriodicTimer::Update(uint16_t dt)
{
  if(!IsActive()) // таймер неактивен, выключаем пин и выходим
  {
    Off();
    tTimer = 0;
    return;
  }

  // прибавляем дельту
  tTimer += dt;
  unsigned long tCompare = 0;

  // теперь смотрим, какой интервал мы обрабатываем
  if(flags.isHoldOnTimer)
  {
    // ждём истекания интервала включения
    tCompare = Settings.HoldOnTime;
    tCompare *= 1000;
    
    if(tTimer > tCompare)
    {
      Off();
      tTimer = 0;
      flags.isHoldOnTimer = false;
    }
  }
  else
  {
    tCompare = Settings.HoldOffTime;
    tCompare *= 1000;
    
      if(tTimer > tCompare)
      {
        On();
        tTimer = 0;
        flags.isHoldOnTimer = true;
      }
  } // else
  
    
}
//--------------------------------------------------------------------------------------------------------------------------------
void TimerModule::LoadTimers()
{
  uint16_t addr = TIMERS_EEPROM_ADDR;
  if(MemRead(addr++) != SETT_HEADER1)
    return;

  if(MemRead(addr++) != SETT_HEADER2)
    return;

  // читаем настройки таймеров  
 for(byte i=0;i<NUM_TIMERS;i++)
   {
       byte* pB = (byte*) &(timers[i].Settings);
       for(size_t k=0;k<sizeof(PeriodicTimerSettings);k++)
       {
        *pB = MemRead(addr++);
        pB++;
       }
       //EEPROM.get(addr,timers[i].Settings);
      //addr += sizeof(PeriodicTimerSettings);   
   } // for  
}
//--------------------------------------------------------------------------------------------------------------------------------
void TimerModule::SaveTimers()
{
  uint16_t addr = TIMERS_EEPROM_ADDR;
  MemWrite(addr++,SETT_HEADER1);
  MemWrite(addr++,SETT_HEADER2);

   for(byte i=0;i<NUM_TIMERS;i++)
   {
       byte* pB = (byte*) &(timers[i].Settings);
       for(size_t k=0;k<sizeof(PeriodicTimerSettings);k++)
       {
        MemWrite(addr++,*pB++);
       }
      // EEPROM.put(addr,timers[i].Settings);
     // addr += sizeof(PeriodicTimerSettings);   
   } // for
}
//--------------------------------------------------------------------------------------------------------------------------------
void TimerModule::Setup()
{
  // настройка модуля тут
  LoadTimers();
  
  for(byte i=0;i<NUM_TIMERS;i++)
    timers[i].Init();
}
//--------------------------------------------------------------------------------------------------------------------------------
void TimerModule::Update(uint16_t dt)
{ 
  for(byte i=0;i<NUM_TIMERS;i++)
    timers[i].Update(dt);
}
//--------------------------------------------------------------------------------------------------------------------------------
bool  TimerModule::ExecCommand(const Command& command, bool wantAnswer)
{
  UNUSED(wantAnswer);
  
  uint8_t argsCount = command.GetArgsCount();

  if(command.GetType() == ctGET)
  {
    PublishSingleton.Flags.Status = true;
    PublishSingleton = "";

    for(byte i=0;i<NUM_TIMERS;i++)
    {
      PublishSingleton << timers[i].Settings.DayMaskAndEnable;
      PublishSingleton << PARAM_DELIMITER;

      PublishSingleton << timers[i].Settings.Pin;
      PublishSingleton << PARAM_DELIMITER;

      PublishSingleton << timers[i].Settings.HoldOnTime;
      PublishSingleton << PARAM_DELIMITER;

      PublishSingleton << timers[i].Settings.HoldOffTime;
      PublishSingleton << PARAM_DELIMITER;
      
    } // for
    
  }
  else // ctSET
  {
      if(argsCount < 16)
      {
        PublishSingleton = PARAMS_MISSED;
      }
      else
      {
        byte tmrNum = 0;
        for(byte i=0;i<16;i+=4)
        {
          timers[tmrNum].Settings.DayMaskAndEnable = (byte) atoi(command.GetArg(i));
          timers[tmrNum].Settings.Pin = (byte) atoi(command.GetArg(i+1));
          timers[tmrNum].Settings.HoldOnTime = (uint16_t) atoi(command.GetArg(i+2));
          timers[tmrNum].Settings.HoldOffTime = (uint16_t) atoi(command.GetArg(i+3));

          timers[tmrNum].Init();

          tmrNum++;
        }
        SaveTimers();
        PublishSingleton = REG_SUCC;
        PublishSingleton.Flags.Status = true;
      }
  } // else ctSET
  

  MainController->Publish(this,command);
  return PublishSingleton.Flags.Status;
}
//--------------------------------------------------------------------------------------------------------------------------------

