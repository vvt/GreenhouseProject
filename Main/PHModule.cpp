#include "PHModule.h"
#include "ModuleController.h"
#include <EEPROM.h>
//-------------------------------------------------------------------------------------------------------------------------------------------------------
//#define PH_DEBUG
#define PH_DEBUG_OUT(which, value) {Serial.print((which)); Serial.println((value));}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
PHCalculator PHCalculation;
PhModule* _thisPHModule = NULL;
//-------------------------------------------------------------------------------------------------------------------------------------------------------
PHCalculator::PHCalculator()
{
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void PHCalculator::ApplyCalculation(Temperature* temp)
{ 
  if(!_thisPHModule)
    return;

  _thisPHModule->ApplyCalculation(temp);  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void PhModule::ApplyCalculation(Temperature* temp)
{
  // Эта функция вызывается при любом обновлении значения с датчика pH, откуда бы это значение
  // ни пришло. Здесь мы можем применить к значению поправочные факторы калибровки, в том числе
  // по температуре, плюс применяем к показаниям поправочное число.
  
  if(!temp)
    return;

  if(!temp->HasData())
    return;

  // теперь проверяем, можем ли мы применить калибровочные поправки?
  // для этого все вольтажи, наличие показаний с датчика температуры
  // и выставленная настройки температуры калибровочных растворов
  // должны быть актуальными.

  if(ph4Voltage < 1 || ph7Voltage < 1 || ph10Voltage < 1 || phTemperatureSensorIndex < 0 
  || !phSamplesTemperature.HasData() || phSamplesTemperature.Value > 100 || phSamplesTemperature.Value < 0)
    return;

  AbstractModule* tempModule = MainController->GetModuleByID("STATE");
  if(!tempModule)
    return;

  OneState* os = tempModule->State.GetState(StateTemperature,phTemperatureSensorIndex);
  if(!os)
    return;

  TemperaturePair tempPair = *os;
  Temperature tempPH = tempPair.Current;

  if(!tempPH.HasData() || tempPH.Value > 100 || tempPH.Value < 0)
    return;

  Temperature tDiff = tempPH - phSamplesTemperature;

  #ifdef PH_DEBUG
    PH_DEBUG_OUT(F("T diff: "), tDiff);
  #endif

  long ulDiff = tDiff.Value;
  ulDiff *= 100;
  ulDiff += tDiff.Fract;

  #ifdef PH_DEBUG
    PH_DEBUG_OUT(F("ulDiff: "), ulDiff);
  #endif
  
  float fTempDiff = ulDiff/100.0;

  #ifdef PH_DEBUG
    PH_DEBUG_OUT(F("fTempDiff: "), fTempDiff);
    PH_DEBUG_OUT(F("source PH: "), *temp);
  #endif 

  // теперь можем применять факторы калибровки.
  // сначала переводим текущие показания в вольтаж, приходится так делать, поскольку
  // они приходят уже нормализованными.
  long curPHVoltage = temp->Value;

  curPHVoltage *= 100;
  curPHVoltage += temp->Fract + calibration; // прибавляем сотые доли показаний, плюс сотые доли поправочного числа
  curPHVoltage *= 100;
  curPHVoltage /= 35; // например, 7,00 pH  сконвертируется в 2000 милливольт

  #ifdef PH_DEBUG
    PH_DEBUG_OUT(F("curPHVoltage: "), curPHVoltage);
    PH_DEBUG_OUT(F("ph4Voltage: "), ph4Voltage);
    PH_DEBUG_OUT(F("ph7Voltage: "), ph7Voltage);
    PH_DEBUG_OUT(F("ph10Voltage: "), ph10Voltage);
  #endif

  long phDiff = ph4Voltage;
  phDiff -= ph10Voltage;

  float sensitivity = phDiff/6.0;
  sensitivity = sensitivity + fTempDiff*0.0001984;

  #ifdef PH_DEBUG
    PH_DEBUG_OUT(F("sensitivity: "), sensitivity);
  #endif

  phDiff = ph7Voltage;
  phDiff -= curPHVoltage;
  
  float calibratedPH = 7.0 + phDiff/sensitivity;

  #ifdef PH_DEBUG
    PH_DEBUG_OUT(F("calibratedPH: "), calibratedPH);
  #endif

  // теперь переводим всё это обратно в понятный всем вид
  uint16_t phVal = calibratedPH*100;

  // и сохраняем это дело в показания датчика
  temp->Value = phVal/100;
  temp->Fract = phVal%100;

  #ifdef PH_DEBUG
    PH_DEBUG_OUT(F("pH result: "), *temp);
  #endif
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void PhModule::Setup()
{
  _thisPHModule = this;
  
  // настройка модуля тут
  phSensorPin = PH_SENSOR_PIN;
  measureTimer = 0;
  inMeasure = false;
  samplesDone = 0;
  samplesTimer = 0;
  calibration = 0;
  ph4Voltage = 0;
  ph7Voltage = 0;
  ph10Voltage = 0;
  phTemperatureSensorIndex = -1;
  phSamplesTemperature.Value = 25; // 25 градусов температура калибровочных растворов по умолчанию

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

  // пишем вольтаж раствора 4 pH
  EEPROM.put(addr,ph4Voltage);
  addr += sizeof(int16_t);   

  // пишем вольтаж раствора 7 pH
  EEPROM.put(addr,ph7Voltage);
  addr += sizeof(int16_t);   

  // пишем вольтаж раствора 10 pH
  EEPROM.put(addr,ph10Voltage);
  addr += sizeof(int16_t);

  // пишем индекс датчика температуры
  EEPROM.write(addr++,phTemperatureSensorIndex);

  // пишем показания температуры при калибровке
  cal[0] = phSamplesTemperature.Value;
  cal[1] = phSamplesTemperature.Fract;

  EEPROM.put(addr,cal);
  addr += 2;
  
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
    memcpy(&calibration,cal,2); // иначе копируем сохранённую калибровку

 // читаем вольтаж раствора 4 pH
  EEPROM.get(addr,ph4Voltage);
  addr += sizeof(int16_t);   

 // читаем вольтаж раствора 7 pH
  EEPROM.get(addr,ph7Voltage);
  addr += sizeof(int16_t);   

 // читаем вольтаж раствора 10 pH
  EEPROM.get(addr,ph10Voltage);
  addr += sizeof(int16_t);

  // читаем индекс датчика температуры
  phTemperatureSensorIndex = EEPROM.read(addr++);

  // читаем значение температуры калибровки
  cal[0] = EEPROM.read(addr++);
  cal[1] = EEPROM.read(addr++);

  // теперь проверяем корректность всех настроек
  if(0xFFFF == (uint16_t) ph4Voltage)
    ph4Voltage = 0;

  if(0xFFFF == (uint16_t) ph7Voltage)
    ph7Voltage = 0;

  if(0xFFFF == (uint16_t) ph10Voltage)
    ph10Voltage = 0;

  if(0xFF == (byte) phTemperatureSensorIndex)
    phTemperatureSensorIndex = -1;

  if(cal[0] == 0xFF)
    phSamplesTemperature.Value = 25; // 25 градусов дефолтная температура
  else
    phSamplesTemperature.Value = cal[0];

  if(cal[1] == 0xFF)
    phSamplesTemperature.Fract = 0;
  else
    phSamplesTemperature.Fract = cal[1];
  
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
         unsigned long phValue = voltage*350;
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
       if(param == PH_SETTINGS_COMMAND) // установить настройки: CTSET=PH|T_SETT|calibration_factor|ph4Voltage|ph7Voltage|ph10Voltage|temp_sensor_index|samples_temp
       {
          if(argsCnt < 7)
          {
            if(wantAnswer)
              PublishSingleton = PARAMS_MISSED;
          }
          else
          {
             // аргументов хватает
             calibration = atoi(command.GetArg(1));
             ph4Voltage = atoi(command.GetArg(2));
             ph7Voltage = atoi(command.GetArg(3));
             ph10Voltage = atoi(command.GetArg(4));
             phTemperatureSensorIndex = atoi(command.GetArg(5));
             
             int samplesTemp = atoi(command.GetArg(6));
             phSamplesTemperature.Value = samplesTemp/100;
             phSamplesTemperature.Fract = samplesTemp%100;
             
             
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
        if(param == PH_SETTINGS_COMMAND) // получить/установить настройки: CTGET=PH|T_SETT, CTSET=PH|T_SETT|calibration_factor|ph4Voltage|ph7Voltage|ph10Voltage|temp_sensor_index|samples_temp
        {
          PublishSingleton.Status = true;
          if(wantAnswer)
          {
            PublishSingleton = PH_SETTINGS_COMMAND;
            PublishSingleton << PARAM_DELIMITER << calibration;
            PublishSingleton << PARAM_DELIMITER << ph4Voltage;
            PublishSingleton << PARAM_DELIMITER << ph7Voltage;
            PublishSingleton << PARAM_DELIMITER << ph10Voltage;
            PublishSingleton << PARAM_DELIMITER << phTemperatureSensorIndex;
            PublishSingleton << PARAM_DELIMITER << phSamplesTemperature;             
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

