#include "NextionModule.h"
#include "ModuleController.h"
#include "InteropStream.h"
//--------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_NEXTION_MODULE

NextionWaitScreenInfo _waitScreenInfos[] = 
{
   NEXTION_WAIT_SCREEN_SENSORS
  ,{0,0,""} // последний элемент пустой, заглушка для признака окончания списка
};
//--------------------------------------------------------------------------------------------------------------------------------------
void nSleep(NextionAbstractController* Sender)
{
  NextionModule* m = (NextionModule*) Sender->getUserData();
  m->SetSleep(true);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void nWake(NextionAbstractController* Sender)
{
  NextionModule* m = (NextionModule*) Sender->getUserData();
  m->SetSleep(false);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void nString(NextionAbstractController* Sender, const char* str)
{
 NextionModule* m = (NextionModule*) Sender->getUserData();
 m->StringReceived(str);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void NextionModule::StringReceived(const char* str)
{

  // Обрабатываем пришедшие команды здесь
  
  if(!strcmp_P(str,(const char*)F("w_open")))
  {
    // попросили открыть окна
    ModuleInterop.QueryCommand(ctSET,F("STATE|WINDOW|ALL|OPEN"),false);
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("w_close")))
  {
    // попросили закрыть окна
    ModuleInterop.QueryCommand(ctSET,F("STATE|WINDOW|ALL|CLOSE"),false);
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("w_auto")))
  {
    // попросили перевести в автоматический режим окон
    ModuleInterop.QueryCommand(ctSET,F("STATE|MODE|AUTO"),false);
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("w_manual")))
  {
    // попросили перевести в ручной режим работы окон
    ModuleInterop.QueryCommand(ctSET,F("STATE|MODE|MANUAL"),false);
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("wtr_on")))
  {
    // попросили включить полив
    ModuleInterop.QueryCommand(ctSET,F("WATER|ON"),false);
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("wtr_off")))
  {
    // попросили выключить полив
    ModuleInterop.QueryCommand(ctSET,F("WATER|OFF"),false);
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("wtr_auto")))
  {
    // попросили перевести в автоматический режим работы полива
    ModuleInterop.QueryCommand(ctSET,F("WATER|MODE|AUTO"),false);
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("wtr_manual")))
  {
    // попросили перевести в ручной режим работы полива
    ModuleInterop.QueryCommand(ctSET,F("WATER|MODE|MANUAL"),false);
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("lht_on")))
  {
    // попросили включить досветку
    ModuleInterop.QueryCommand(ctSET,F("LIGHT|ON"),false);
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("lht_off")))
  {
    // попросили выключить досветку
    ModuleInterop.QueryCommand(ctSET,F("LIGHT|OFF"),false);
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("lht_auto")))
  {
    // попросили перевести досветку в автоматический режим
    ModuleInterop.QueryCommand(ctSET,F("LIGHT|MODE|AUTO"),false);
    return;
  }
  
  if(!strcmp_P(str,(const char*)F("lht_manual")))
  {
    // попросили перевести досветку в ручной режим
    ModuleInterop.QueryCommand(ctSET,F("LIGHT|MODE|MANUAL"),false);
    return;
  }
  
 if(!strcmp_P(str,(const char*)F("topen_down")))
 {
    // листают температуру открытия вниз
    uint8_t tmp = sett->GetOpenTemp();
    
    if(tmp > 0)
      --tmp;
      
      sett->SetOpenTemp(tmp); // запоминаем температуру открытия
    
    return;
  }  

 if(!strcmp_P(str,(const char*)F("topen_up")))
  {
    // листают температуру открытия вверх
    uint8_t tmp = sett->GetOpenTemp();
    
    if(tmp < 50)
      ++tmp;
      
      sett->SetOpenTemp(tmp); // запоминаем температуру открытия
    
    return;
  }

 if(!strcmp_P(str,(const char*)F("tclose_down")))
  {
    // листают температуру закрытия вниз
    uint8_t tmp = sett->GetCloseTemp();
    
    if(tmp > 0)
      --tmp;
      
      sett->SetCloseTemp(tmp); // запоминаем температуру закрытия
    
    return;
  }  

 if(!strcmp_P(str,(const char*)F("tclose_up")))
  {
    // листают температуру закрытия вверх
    uint8_t tmp = sett->GetCloseTemp();
    
    if(tmp < 50)
      ++tmp;
      
      sett->SetCloseTemp(tmp); // запоминаем температуру закрытия
    
    return;
  }

 if(!strcmp_P(str,(const char*)F("prev")))
  {
    rotationTimer = 0;
    displayNextSensorData(-1);
    return;
  }

 if(!strcmp_P(str,(const char*)F("next")))
  {
    rotationTimer = 0;
    displayNextSensorData(1);
    return;
  }

  // тут отрабатываем остальные команды

   
}
//--------------------------------------------------------------------------------------------------------------------------------------
void NextionModule::SetSleep(bool bSleep)
{
  flags.isDisplaySleep = bSleep;
  updateDisplayData(); // обновляем основные данные для дисплея

  // говорим, что надо бы показать данные с датчиков
  rotationTimer = NEXTION_ROTATION_INTERVAL;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void NextionModule::Setup()
{
  // настройка модуля тут
  sett = MainController->GetSettings();

  rotationTimer = NEXTION_ROTATION_INTERVAL;
  currentSensorIndex = -1;
  
  flags.isDisplaySleep = false;
  flags.bInited = false;
  
  flags.windowChanged = true;
  flags.windowModeChanged = true;
  flags.waterChanged = true;
  flags.waterModeChanged = true;
  flags.lightChanged = true;
  flags.lightModeChanged = true;
  flags.openTempChanged = true;
  flags.closeTempChanged = true;
  
  
  NEXTION_SERIAL.begin(NEXTION_BAUD_RATE);

  if(&(NEXTION_SERIAL) == &Serial) {
       WORK_STATUS.PinMode(0,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(1,OUTPUT,false);
  } else if(&(NEXTION_SERIAL) == &Serial1) {
       WORK_STATUS.PinMode(19,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(18,OUTPUT,false);
  } else if(&(NEXTION_SERIAL) == &Serial2) {
       WORK_STATUS.PinMode(17,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(16,OUTPUT,false);
  } else if(&(NEXTION_SERIAL) == &Serial3) {
       WORK_STATUS.PinMode(15,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(14,OUTPUT,false);
  }  
  
  // подписываемся на события
  NextionSubscribeStruct ss;
  memset(&ss,0,sizeof(ss));
  ss.OnStringReceived = nString;
  ss.OnSleep = nSleep;
  ss.OnWakeUp = nWake;
  nextion.subscribe(ss);
   
  nextion.begin(&NEXTION_SERIAL,this);
 }
//--------------------------------------------------------------------------------------------------------------------------------------
void NextionModule::updateDisplayData()
{
  if(flags.isDisplaySleep) // для спящего дисплея нечего обновлять
    return;
    
    if(flags.windowChanged)
    {
      flags.windowChanged = false;
      nextion.notifyWindowState(flags.isWindowsOpen);
    }
    
    if(flags.windowModeChanged)
    {
      flags.windowModeChanged = false;
      nextion.notifyWindowMode(flags.isWindowAutoMode);
    }
    
    if(flags.waterChanged)
    {
      flags.waterChanged = false;
      nextion.notifyWaterState(flags.isWaterOn);
    }
    
    if(flags.waterModeChanged)
    {
      flags.waterModeChanged = false;
      nextion.notifyWaterMode(flags.isWaterAutoMode);
    }
    
    if(flags.lightChanged)
    {
      flags.lightChanged = false;
      nextion.notifyLightState(flags.isLightOn);
    }
    
    if(flags.lightModeChanged)
    {
      flags.lightModeChanged = false;
      nextion.notifyLightMode(flags.isLightAutoMode);
    }
    
    if(flags.openTempChanged)
    {
      flags.openTempChanged = false;
      nextion.showOpenTemp(openTemp);
    }
    
    if(flags.closeTempChanged)
    {
      flags.closeTempChanged = false;
      nextion.showCloseTemp(closeTemp);
    }
       
}
//--------------------------------------------------------------------------------------------------------------------------------------
void NextionModule::Update(uint16_t dt)
{ 
  rotationTimer += dt;
  // обновление модуля тут
  
  if(!flags.bInited) // ещё не инициализировались, начинаем
  {
    nextion.setWaitTimerInterval();
    nextion.setSleepDelay();
    nextion.setWakeOnTouch();
    nextion.setEchoMode();
    
    flags.isWindowsOpen = WORK_STATUS.GetStatus(WINDOWS_STATUS_BIT);
    flags.isWindowAutoMode = WORK_STATUS.GetStatus(WINDOWS_MODE_BIT);
    
    flags.isWaterOn = WORK_STATUS.GetStatus(WATER_STATUS_BIT);
    flags.isWaterAutoMode = WORK_STATUS.GetStatus(WATER_MODE_BIT);

    flags.isLightOn = WORK_STATUS.GetStatus(LIGHT_STATUS_BIT);
    flags.isLightAutoMode = WORK_STATUS.GetStatus(LIGHT_MODE_BIT);

    openTemp = sett->GetOpenTemp();
    closeTemp = sett->GetCloseTemp();
    
    updateDisplayData();
        
    flags.bInited = true;
    
    return;
  }
  
  nextion.update(); // обновляем работу с дисплеем
  
  // теперь получаем все настройки и смотрим, изменилось ли чего?
  bool curVal = WORK_STATUS.GetStatus(WINDOWS_STATUS_BIT);
  if(curVal != flags.isWindowsOpen)
  {
    // состояние окон изменилось
    flags.isWindowsOpen = curVal;
    flags.windowChanged = true;
  }
  
  curVal = WORK_STATUS.GetStatus(WINDOWS_MODE_BIT);
  if(curVal != flags.isWindowAutoMode)
  {
    // состояние режима окон изменилось
    flags.isWindowAutoMode = curVal;
    flags.windowModeChanged = true;
  }
  
  curVal = WORK_STATUS.GetStatus(WATER_STATUS_BIT);
  if(curVal != flags.isWaterOn)
  {
    // состояние полива изменилось
    flags.isWaterOn = curVal;
    flags.waterChanged = true;
  }
  
  curVal = WORK_STATUS.GetStatus(WATER_MODE_BIT);
  if(curVal != flags.isWaterAutoMode)
  {
    // состояние режима полива изменилось
    flags.isWaterAutoMode = curVal;
    flags.waterModeChanged = true;
  }
  
  
  curVal = WORK_STATUS.GetStatus(LIGHT_STATUS_BIT);
  if(curVal != flags.isLightOn)
  {
    // состояние досветки изменилось
    flags.isLightOn = curVal;
    flags.lightChanged = true;
  }
  
  curVal = WORK_STATUS.GetStatus(LIGHT_MODE_BIT);
  if(curVal != flags.isLightAutoMode)
  {
    // состояние режима досветки изменилось
    flags.isLightAutoMode = curVal;
    flags.lightModeChanged = true;
  }
  
  uint8_t cTemp = sett->GetOpenTemp();
  if(cTemp != openTemp)
  {
    // температура открытия изменилась
    openTemp = cTemp;
    flags.openTempChanged = true;
  }
  
  cTemp = sett->GetCloseTemp();
  if(cTemp != closeTemp)
  {
    // температура закрытия изменилась
    closeTemp = cTemp;
    flags.closeTempChanged = true;
  }

  updateDisplayData(); // обновляем дисплей
  
  // обновили дисплей, теперь на нём актуальные данные, можем работать с датчиками
  if(rotationTimer > NEXTION_ROTATION_INTERVAL)
  {
    rotationTimer = 0;
    displayNextSensorData(1);
  }
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void NextionModule::displayNextSensorData(int8_t dir)
{
  if(flags.isDisplaySleep)
    return;

  currentSensorIndex += dir; // прибавляем направление
  // при старте currentSensorIndex у нас равен -1, следовательно,
  // мы должны обработать эту ситуацию
  if(currentSensorIndex < 0)
  {
     // перемещаемся на последний элемент
     currentSensorIndex = 0;
     int8_t i = 0;
     while(_waitScreenInfos[i].sensorType) // идём до конца массива, как только встретим пустой элемент - выходим
     {
      i++;
     }
     currentSensorIndex = i; // запомнили последний валидный элемент в массиве
     if(currentSensorIndex > 0)
      currentSensorIndex--;
  } // if(currentSensorIndex < 0)
  

  NextionWaitScreenInfo wsi = _waitScreenInfos[currentSensorIndex];
  if(!wsi.sensorType)
  {
    // ничего нет в текущем элементе списка.
    // перемещаемся в начало
    currentSensorIndex = 0;
    wsi = _waitScreenInfos[currentSensorIndex];
  }

  if(!wsi.sensorType)
  {
    return; // так ничего и не нашли
  }


 // теперь получаем показания от модулей
  AbstractModule* mod = MainController->GetModuleByID(wsi.moduleName);

  if(!mod) // не нашли такой модуль
  {
    rotationTimer = NEXTION_ROTATION_INTERVAL; // просим выбрать следующий модуль
    return;
  }
  OneState* os = mod->State.GetState((ModuleStates)wsi.sensorType,wsi.sensorIndex);
  if(!os)
  {
    // нет такого датчика, просим показать следующие данные
    rotationTimer = NEXTION_ROTATION_INTERVAL;
    return;
  }


   //Тут получаем актуальные данные от датчиков
   switch(wsi.sensorType)
   {
      case StateTemperature:
      {
        if(os->HasData())
        {
          TemperaturePair tp = *os;
          nextion.showTemperature(tp.Current);
        }
        else
          rotationTimer = NEXTION_ROTATION_INTERVAL;
      } 
      break;

      case StateHumidity:
      case StateSoilMoisture:
      {
        if(os->HasData())
        {
          HumidityPair hp = *os;
          nextion.showHumidity(hp.Current);
        }
         else
          rotationTimer = NEXTION_ROTATION_INTERVAL;
       
      }
      break;

      case StateLuminosity:
      {
        if(os->HasData())
        {
          LuminosityPair lp = *os;
          nextion.showLuminosity(lp.Current);
        }
         else
          rotationTimer = NEXTION_ROTATION_INTERVAL;
     }
      break;
    
   } // switch    

}
//--------------------------------------------------------------------------------------------------------------------------------------
bool  NextionModule::ExecCommand(const Command& command, bool wantAnswer)
{
  UNUSED(wantAnswer);
  UNUSED(command);
 
  return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_NEXTION_MODULE

