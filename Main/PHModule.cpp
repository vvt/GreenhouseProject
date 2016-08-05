#include "PHModule.h"
#include "ModuleController.h"
#include <EEPROM.h>
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void PhModule::Setup()
{
  // настройка модуля тут
  phSensorPin = PH_SENSOR_PIN;
  measureTimer = 0;
  inMeasure = false;
  samplesDone = 0;
  samplesTimer = 0;
  calibration = 0;

  // читаем настройки
  ReadSettings();

  // теперь смотрим - если у нас пин pH не 0 - значит, надо добавить состояние
  if(phSensorPin > 0)
  {
    State.AddState(StatePH,0); // добавляем датчик pH, прикреплённый к меге
    pinMode(phSensorPin,INPUT);
    digitalWrite(phSensorPin,HIGH);
  }
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void PhModule::SaveSettings()
{
  uint16_t addr = PH_SETTINGS_EEPROM_ADDR;

  EEPROM.write(addr++,SETT_HEADER1);
  EEPROM.write(addr++,SETT_HEADER2);

  EEPROM.write(addr++,phSensorPin);

  // пишем калибровку
  byte cal[2];
  memcpy(cal,&calibration,2);
  EEPROM.write(addr++,cal[0]);
  EEPROM.write(addr++,cal[1]);
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void PhModule::ReadSettings()
{
  uint16_t addr = PH_SETTINGS_EEPROM_ADDR;
  if(EEPROM.read(addr++) != SETT_HEADER1)
    return;

  if(EEPROM.read(addr++) != SETT_HEADER2)
    return;

  phSensorPin =  EEPROM.read(addr++); 
  if(phSensorPin == 0xFF)
    phSensorPin = PH_SENSOR_PIN;

  byte cal[2];
  cal[0] = EEPROM.read(addr++);
  cal[1] = EEPROM.read(addr++);

  if(cal[0] == 0xFF && cal[1] == 0xFF) // нет калибровки
    calibration = PH_DEFAULT_CALIBRATION;
  else
  {
    memcpy(&calibration,cal,2); // иначе копируем сохранённую калибровку
  }
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void PhModule::Update(uint16_t dt)
{ 
  // обновление модуля тут
  if(phSensorPin > 0)
  {
    // у нас есть датчик, жёстко прикреплённый к меге, можно читать с него данные.
    // если вы сейчас измеряем, то надо проверять, не истёк ли интервал между семплированиями.
    // если истёк - начинаем замерять. если нет - ничего не делаем.

    // как только достигнем нужного кол-ва семплов - подсчитываем значение pH и обновляем его

    // если же мы не измеряем, то проверяем - не истёк ли интервал между замерами.
    // если истёк - начинаем измерения.

    if(inMeasure)
    {
      // в процессе замера
      if(samplesDone >= PH_SAMPLES_PER_MEASURE)
      {
         // набрали нужное кол-во семплов
         samplesTimer = 0;
         inMeasure = false;
         measureTimer = 0;

         // теперь преобразуем полученное значение в среднее
         float avgSample = (dataArray*1.0)/samplesDone;

         // теперь считаем вольтаж
         float voltage = avgSample*5.0/1024;
  
         // теперь получаем значение pH
         unsigned long phValue = voltage*350 + calibration;
         Humidity h;         

         if(avgSample > 1000)
         {
           // не прочитали ничего из порта
         }
         else
         {
           h.Value = phValue/100;
           h.Fract = phValue%100;          
         }

         // сохраняем состояние с датчика
         State.UpdateState(StatePH,0,(void*)&h);     

         samplesDone = 0;
        
      } // if(samplesDone >= PH_SAMPLES_PER_MEASURE)
      else
      {
        // ещё набираем семплы
        samplesTimer += dt; // обновляем таймер
        if(samplesTimer > PH_SAMPLES_INTERVAL)
        {
          // настало время очередного замера
          samplesTimer = 0; // сбрасываем таймер
          samplesDone++; // увеличиваем кол-во семплов

          // читаем из порта и запоминаем прочитанное
          dataArray += analogRead(phSensorPin);
          
        } // PH_SAMPLES_INTERVAL
        
      } // else
      
    } // inMeasure
    else
    {
      // ничего не измеряем, сохраняем дельту в таймер между замерами
      measureTimer += dt;

      if(measureTimer > PH_UPDATE_INTERVAL)
      {
        // настала пора переключиться на замер
        measureTimer = 0;
        inMeasure = true;
        samplesDone = 0;
        samplesTimer = 0;

        // очищаем массив данных
        dataArray = 0;

        // читаем первый раз, игнорируем значение, чтобы выровнять датчик
        analogRead(phSensorPin);
      }
    } // else
    
  } // if(phSensorPin > 0)

}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool  PhModule::ExecCommand(const Command& command, bool wantAnswer)
{
  if(wantAnswer) 
    PublishSingleton = NOT_SUPPORTED;


  uint8_t argsCnt = command.GetArgsCount();
    
  if(command.GetType() == ctSET) // установка свойств
  {
    if(argsCnt < 1)
     {
        if(wantAnswer) 
          PublishSingleton = PARAMS_MISSED;
     }
     else
     {
       String param = command.GetArg(0);
       if(param == PH_SETTINGS_COMMAND) // установить настройки: CTSET=PH|T_SETT|calibration_factor
       {
          if(argsCnt < 2)
          {
            if(wantAnswer)
              PublishSingleton = PARAMS_MISSED;
          }
          else
          {
             // аргументов хватает
             calibration = atoi(command.GetArg(1));
             SaveSettings();

             PublishSingleton.Status = true;
             PublishSingleton = REG_SUCC;
          }
       } // PH_SETTINGS_COMMAND
       
     } // else argsCount >= 1
     
    
  } // ctSET    
  else
  if(command.GetType() == ctGET) // запрос свойств
  {
      
      if(argsCnt < 1)
      {
        if(wantAnswer) 
          PublishSingleton = PARAMS_MISSED; // не хватает параметров
        
      } // argsCnt < 1 
      else
      {     
        String param = command.GetArg(0);
        
        if(param == ALL) // запросили показания со всех датчиков: CTGET=PH|ALL
        {
          PublishSingleton.Status = true;
          uint8_t _cnt = State.GetStateCount(StatePH);
          if(wantAnswer) 
            PublishSingleton = _cnt;
          
          for(uint8_t i=0;i<_cnt;i++)
          {

             OneState* stateHumidity = State.GetStateByOrder(StatePH,i);
             if(stateHumidity)
             {
                HumidityPair hp = *stateHumidity;
              
                if(wantAnswer) 
                {
                  PublishSingleton << PARAM_DELIMITER << (hp.Current);
                }
             } // if
          } // for        
        } // param == ALL
        else
        if(param == PH_SETTINGS_COMMAND) // получить/установить настройки: CTGET=PH|T_SETT, CTSET=PH|T_SETT|calibration_factor
        {
          PublishSingleton.Status = true;
          if(wantAnswer)
          {
            PublishSingleton = PH_SETTINGS_COMMAND;
            PublishSingleton << PARAM_DELIMITER << calibration; 
          }
          
        } // PH_SETTINGS_COMMAND
        else
        if(param == PROP_CNT) // запросили данные о кол-ве датчиков: CTGET=PH|CNT
        {
          PublishSingleton.Status = true;
          if(wantAnswer) 
          {
            PublishSingleton = PROP_CNT; 
            uint8_t _cnt = State.GetStateCount(StatePH);
            PublishSingleton << PARAM_DELIMITER << _cnt;
          }
        } // PROP_CNT
        else
        if(param != GetID()) // если только не запросили без параметров
        {
          // запросили показания с датчика по индексу
          uint8_t idx = param.toInt();
          uint8_t _cnt = State.GetStateCount(StatePH);
          
          if(idx >= _cnt)
          {
            // плохой индекс
            if(wantAnswer) 
              PublishSingleton = NOT_SUPPORTED;
          } // плохой индекс
          else
          {
             if(wantAnswer) 
              PublishSingleton = param;
              
             OneState* stateHumidity = State.GetStateByOrder(StatePH,idx);
             if(stateHumidity)
             {
                PublishSingleton.Status = true;
                HumidityPair hp = *stateHumidity;
                
                if(wantAnswer)
                {
                  PublishSingleton << PARAM_DELIMITER << (hp.Current);
                }
             } // if
            
          } // else нормальный индекс        
        } // if param != GetID()
        
      } // else
  }
  
  MainController->Publish(this,command); 
  
  return true;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------

