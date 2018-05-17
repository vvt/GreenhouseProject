#include "LuminosityModule.h"
#include "ModuleController.h"
#include "Max44009.h"
//--------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_LUMINOSITY_MODULE

#if LAMP_RELAYS_COUNT > 0
static uint8_t LAMP_RELAYS[] = { LAMP_RELAYS_PINS }; // объявляем массив пинов реле
#endif

#if LIGHT_SENSORS_COUNT > 0
static uint8_t LIGHT_SENSORS_MAPPING[] = { LIGHT_SENSORS };
#endif
//--------------------------------------------------------------------------------------------------------------------------------------
BH1750Support::BH1750Support()
{

}
//--------------------------------------------------------------------------------------------------------------------------------------
void BH1750Support::begin(BH1750Address addr, BH1750Mode mode)
{
  deviceAddress = addr;
  //////////////Wire.begin();    
  writeByte(BH1750PowerOn); // включаем датчик
  ChangeMode(mode); 
}
//--------------------------------------------------------------------------------------------------------------------------------------
void BH1750Support::ChangeMode(BH1750Mode mode) // смена режима работы
{
   currentMode = mode; // сохраняем текущий режим опроса
   writeByte((uint8_t)currentMode);
  //_delay_ms(10);
  delay(10);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void BH1750Support::ChangeAddress(BH1750Address newAddr)
{
  if(newAddr != deviceAddress) // только при смене адреса включаем датчик
  { 
    deviceAddress = newAddr;
    
    writeByte(BH1750PowerOn); // включаем датчик  
    ChangeMode(currentMode); // меняем режим опроса на текущий
  } // if
}
//--------------------------------------------------------------------------------------------------------------------------------------
void BH1750Support::writeByte(uint8_t toWrite) 
{
  Wire.beginTransmission(deviceAddress);
  BH1750_WIRE_WRITE(toWrite);
  Wire.endTransmission();
}
//--------------------------------------------------------------------------------------------------------------------------------------
long BH1750Support::GetCurrentLuminosity() 
{
  long curLuminosity = NO_LUMINOSITY_DATA;

 if(Wire.requestFrom(deviceAddress, 2) == 2)// ждём два байта
 {
  // читаем два байта
  curLuminosity = BH1750_WIRE_READ();
  curLuminosity <<= 8;
  curLuminosity |= BH1750_WIRE_READ();
  curLuminosity = curLuminosity/1.2; // конвертируем в люксы
 }


  return curLuminosity;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void LuminosityModule::Setup()
{

 #if LIGHT_SENSORS_COUNT > 0

   bool bh1750NotFirst = false;
   bool max44009NotFirst = false;
 
  for(uint8_t i=0;i<LIGHT_SENSORS_COUNT;i++)
  {
    State.AddState(StateLuminosity,i); // добавляем в состояние нужные индексы датчиков

    switch(LIGHT_SENSORS_MAPPING[i])
    {
       case BH1750_SENSOR:
       {
          BH1750Support* bh = new BH1750Support;
          if(bh1750NotFirst)
            bh->begin(BH1750Address2);
          else
            bh->begin();

          bh1750NotFirst = true;

          lightSensors[i] = bh;
       }
       break;

       case MAX44009_SENSOR:
       {
          Max44009* bh = new Max44009;
          if(max44009NotFirst)
            bh->begin(MAX44009_ADDRESS2);
          else
            bh->begin(MAX44009_ADDRESS1);

          max44009NotFirst = true;

          lightSensors[i] = bh;
       }
       break;
      
    } // switch
    
  } // for

  
  #endif

  
  // настройка модуля тут

  flags.workMode = lightAutomatic; // автоматический режим работы
  flags.bRelaysIsOn = false; // все реле выключены
  flags.bLastRelaysIsOn = false; // состояние не изменилось
  
  SAVE_STATUS(LIGHT_STATUS_BIT,0); // сохраняем, что досветка выключена
  SAVE_STATUS(LIGHT_MODE_BIT,1); // сохраняем, что мы в автоматическом режиме работы
  
#ifdef USE_LIGHT_MANUAL_MODE_DIODE
  blinker.begin(DIODE_LIGHT_MANUAL_MODE_PIN); // настраиваем блинкер на нужный пин
#endif

  #if LAMP_RELAYS_COUNT > 0
   // выключаем все реле
   
    for(uint8_t i=0;i<LAMP_RELAYS_COUNT;i++)
    {
      #if LIGHT_DRIVE_MODE == DRIVE_DIRECT
        WORK_STATUS.PinMode(LAMP_RELAYS[i],OUTPUT);
        WORK_STATUS.PinWrite(LAMP_RELAYS[i],LIGHT_RELAY_OFF);
      #elif LIGHT_DRIVE_MODE == DRIVE_MCP23S17
        #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
          WORK_STATUS.MCP_SPI_PinMode(LIGHT_MCP23S17_ADDRESS,LAMP_RELAYS[i],OUTPUT);
          WORK_STATUS.MCP_SPI_PinWrite(LIGHT_MCP23S17_ADDRESS,LAMP_RELAYS[i],LIGHT_RELAY_OFF);
        #endif
      #elif LIGHT_DRIVE_MODE == DRIVE_MCP23017
        #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
          WORK_STATUS.MCP_I2C_PinMode(LIGHT_MCP23017_ADDRESS,LAMP_RELAYS[i],OUTPUT);
          WORK_STATUS.MCP_I2C_PinWrite(LIGHT_MCP23017_ADDRESS,LAMP_RELAYS[i],LIGHT_RELAY_OFF);
        #endif
      #endif
     
      WORK_STATUS.SaveLightChannelState(i,LIGHT_RELAY_OFF);
    } // for
  #endif
    
       
 }
//--------------------------------------------------------------------------------------------------------------------------------------
void LuminosityModule::Update(uint16_t dt)
{ 
  // обновление модуля тут
#ifdef USE_LIGHT_MANUAL_MODE_DIODE
    blinker.update(dt);
#endif

 // обновляем состояние всех реле управления досветкой
 if(flags.bLastRelaysIsOn != flags.bRelaysIsOn) // только если состояние с момента последнего опроса изменилось
 {
    flags.bLastRelaysIsOn = flags.bRelaysIsOn; // сохраняем текущее

    #if LAMP_RELAYS_COUNT > 0
      for(uint8_t i=0;i<LAMP_RELAYS_COUNT;i++)
      {
        #if LIGHT_DRIVE_MODE == DRIVE_DIRECT
          WORK_STATUS.PinWrite(LAMP_RELAYS[i],flags.bRelaysIsOn ? LIGHT_RELAY_ON : LIGHT_RELAY_OFF); // пишем в пин нужное состояние
        #elif LIGHT_DRIVE_MODE == DRIVE_MCP23S17
          #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
            WORK_STATUS.MCP_SPI_PinWrite(LIGHT_MCP23S17_ADDRESS,LAMP_RELAYS[i],flags.bRelaysIsOn ? LIGHT_RELAY_ON : LIGHT_RELAY_OFF); // пишем в пин нужное состояние
          #endif
        #elif LIGHT_DRIVE_MODE == DRIVE_MCP23017
          #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
            WORK_STATUS.MCP_I2C_PinWrite(LIGHT_MCP23017_ADDRESS,LAMP_RELAYS[i],flags.bRelaysIsOn ? LIGHT_RELAY_ON : LIGHT_RELAY_OFF); // пишем в пин нужное состояние
          #endif
        #endif

          WORK_STATUS.SaveLightChannelState(i,flags.bRelaysIsOn ? LIGHT_RELAY_ON : LIGHT_RELAY_OFF);            
      } // for
    #endif 
 } // if


  lastUpdateCall += dt;
  if(lastUpdateCall < LUMINOSITY_UPDATE_INTERVAL) // обновляем согласно настроенному интервалу
    return;
  else
    lastUpdateCall = 0;


  #if LIGHT_SENSORS_COUNT > 0

    for(int i=0;i<LIGHT_SENSORS_COUNT;i++)
    {
        long lum = NO_LUMINOSITY_DATA;
        
        switch(LIGHT_SENSORS_MAPPING[i])
        {
           case BH1750_SENSOR:
           {
              BH1750Support* bh = (BH1750Support*) lightSensors[i];
              lum = bh->GetCurrentLuminosity();
              
           }
           break;
    
           case MAX44009_SENSOR:
           {
              Max44009* bh = (Max44009*) lightSensors[i];
              float curLum = bh->readLuminosity();
              
             if(curLum < 0)
              lum = NO_LUMINOSITY_DATA;
            else
            {
              unsigned long ulLum = (unsigned long) curLum;
              if(ulLum > 65535)
                ulLum = 65535;

              lum = ulLum;
            }
        
           }
           break;
                  
      
          
        } // switch 

       State.UpdateState(StateLuminosity,i,(void*)&lum);
    } // for
  
    
  #endif   

}
//--------------------------------------------------------------------------------------------------------------------------------------
bool  LuminosityModule::ExecCommand(const Command& command, bool wantAnswer)
{
  if(wantAnswer) 
    PublishSingleton = UNKNOWN_COMMAND;
  
  uint8_t argsCnt = command.GetArgsCount();
  
  if(command.GetType() == ctSET) 
  {
      if(wantAnswer) 
        PublishSingleton = PARAMS_MISSED;

      if(argsCnt > 0)
      {
         String s = command.GetArg(0);
         if(s == STATE_ON) // CTSET=LIGHT|ON
         {
          
          // попросили включить досветку
          if(command.IsInternal() // если команда пришла от другого модуля
          && flags.workMode == lightManual)  // и мы в ручном режиме, то
          {
            // просто игнорируем команду, потому что нами управляют в ручном режиме
          } // if
           else
           {
              if(!command.IsInternal()) // пришла команда от пользователя,
              {
                flags.workMode = lightManual; // переходим на ручной режим работы
                #ifdef USE_LIGHT_MANUAL_MODE_DIODE
                // мигаем светодиодом на 8 пине
                blinker.blink(WORK_MODE_BLINK_INTERVAL);
                #endif
              }

            if(!flags.bRelaysIsOn)
            {
              // значит - досветка была выключена и будет включена, надо записать в лог событие
              MainController->Log(this,s); 
            }

            flags.bRelaysIsOn = true; // включаем реле досветки
            
            PublishSingleton.Flags.Status = true;
            if(wantAnswer) 
              PublishSingleton = STATE_ON;


            
           } // else
 
            SAVE_STATUS(LIGHT_STATUS_BIT,flags.bRelaysIsOn ? 1 : 0); // сохраняем состояние досветки
            SAVE_STATUS(LIGHT_MODE_BIT,flags.workMode == lightAutomatic ? 1 : 0); // сохраняем режим работы досветки
          
         } // STATE_ON
         else
         if(s == STATE_OFF) // CTSET=LIGHT|OFF
         {
          // попросили выключить досветку
          if(command.IsInternal() // если команда пришла от другого модуля
          && flags.workMode == lightManual)  // и мы в ручном режиме, то
          {
            // просто игнорируем команду, потому что нами управляют в ручном режиме
           } // if
           else
           {
              if(!command.IsInternal()) // пришла команда от пользователя,
              {
                flags.workMode = lightManual; // переходим на ручной режим работы
                #ifdef USE_LIGHT_MANUAL_MODE_DIODE
                // мигаем светодиодом на 8 пине
                blinker.blink(WORK_MODE_BLINK_INTERVAL);
                #endif
              }

            if(flags.bRelaysIsOn)
            {
              // значит - досветка была включена и будет выключена, надо записать в лог событие
              MainController->Log(this,s); 
            }

            flags.bRelaysIsOn = false; // выключаем реле досветки
            
            PublishSingleton.Flags.Status = true;
            if(wantAnswer) 
              PublishSingleton = STATE_OFF;

            
           } // else

            SAVE_STATUS(LIGHT_STATUS_BIT,flags.bRelaysIsOn ? 1 : 0); // сохраняем состояние досветки
            SAVE_STATUS(LIGHT_MODE_BIT,flags.workMode == lightAutomatic ? 1 : 0); // сохраняем режим работы досветки

         } // STATE_OFF
         else
         if(s == WORK_MODE) // CTSET=LIGHT|MODE|AUTO, CTSET=LIGHT|MODE|MANUAL
         {
           // попросили установить режим работы
           if(argsCnt > 1)
           {
              s = command.GetArg(1);
              if(s == WM_MANUAL)
              {
                // попросили перейти в ручной режим работы
                flags.workMode = lightManual; // переходим на ручной режим работы
                #ifdef USE_LIGHT_MANUAL_MODE_DIODE
                 // мигаем светодиодом на 8 пине
               blinker.blink(WORK_MODE_BLINK_INTERVAL);
               #endif
              }
              else
              if(s == WM_AUTOMATIC)
              {
                // попросили перейти в автоматический режим работы
                flags.workMode = lightAutomatic; // переходим на автоматический режим работы
                #ifdef USE_LIGHT_MANUAL_MODE_DIODE
                 // гасим диод на 8 пине
                blinker.blink();
                #endif
              }

              PublishSingleton.Flags.Status = true;
              if(wantAnswer)
              {
                PublishSingleton = WORK_MODE; 
                PublishSingleton << PARAM_DELIMITER << (flags.workMode == lightAutomatic ? WM_AUTOMATIC : WM_MANUAL);
              }

            SAVE_STATUS(LIGHT_STATUS_BIT,flags.bRelaysIsOn ? 1 : 0); // сохраняем состояние досветки
            SAVE_STATUS(LIGHT_MODE_BIT,flags.workMode == lightAutomatic ? 1 : 0); // сохраняем режим работы досветки
              
           } // if (argsCnt > 1)
         } // WORK_MODE
         
      } // if(argsCnt > 0)
  }
  else
  if(command.GetType() == ctGET) //получить данные
  {

    if(!argsCnt) // нет аргументов, попросили дать показания с датчика
    {
      
      PublishSingleton.Flags.Status = true;
      if(wantAnswer) 
      {
         PublishSingleton = "";
        // запросили показания с датчиков. У нас должно выводиться минимум 2 показания,
        // для обеспечения нормальной работы конфигуратора. Поэтому мы добавляем недостающие показания
        // как показания NO_LUMINOSITY_DATA для тех датчиков, которых нет в прошивке.

         uint8_t _cnt = State.GetStateCount(StateLuminosity);
         uint8_t _written = 0;
         
         for(uint8_t i=0;i<_cnt;i++)
         {
            OneState* os = State.GetStateByOrder(StateLuminosity,i);
            if(os)
            {
              LuminosityPair lp = *os;

              if(_written > 0)
                PublishSingleton << PARAM_DELIMITER;

              PublishSingleton << lp.Current;
                
              _written++;
              
            } // if(os)
         } // for

         // добиваем до двух датчиков минимум
         for(uint8_t i=_written; i<2;i++)
         {
           PublishSingleton << PARAM_DELIMITER;
           PublishSingleton << NO_LUMINOSITY_DATA;
         } // for
        
      }
    }
    else // есть аргументы
    {
       String s = command.GetArg(0);
       if(s == WORK_MODE) // запросили режим работы
       {
          PublishSingleton.Flags.Status = true;
          if(wantAnswer) 
          {
            PublishSingleton = WORK_MODE;
            PublishSingleton << PARAM_DELIMITER << (flags.workMode == lightAutomatic ? WM_AUTOMATIC : WM_MANUAL);
          }
          
       } // if(s == WORK_MODE)
       else
       if(s == LIGHT_STATE_COMMAND) // CTGET=LIGHT|STATE
       {
          if(wantAnswer)
          {
            PublishSingleton = LIGHT_STATE_COMMAND;
            PublishSingleton << PARAM_DELIMITER << (flags.bRelaysIsOn ? STATE_ON : STATE_OFF) << PARAM_DELIMITER << (flags.workMode == lightAutomatic ? WM_AUTOMATIC : WM_MANUAL);
          }
          
          PublishSingleton.Flags.Status = true;
             
       } // LIGHT_STATE_COMMAND
        
      // разбор других команд

      
    } // if(argsCnt > 0)
    
  } // if
 
 // отвечаем на команду
    MainController->Publish(this,command);
    
  return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_LUMINOSITY_MODULE
