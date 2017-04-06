#include "SoilMoistureModule.h"
#include "ModuleController.h"


#define PULSE_TIMEOUT 50000 // 50 миллисекунд на чтение фронта максимум

typedef struct
{
  int8_t pin;
  int8_t type;
  
} SoilMoistureSensorSettings;

#if SUPPORTED_SOIL_MOISTURE_SENSORS > 0
static SoilMoistureSensorSettings SOIL_MOISTURE_SENSORS_ARRAY[] = { SOIL_MOISTURE_SENSORS };
#endif

void SoilMoistureModule::Setup()
{
  // настройка модуля тут
  
  #if SUPPORTED_SOIL_MOISTURE_SENSORS > 0
    for(uint8_t i=0;i<SUPPORTED_SOIL_MOISTURE_SENSORS;i++)
    {
      WORK_STATUS.PinMode(SOIL_MOISTURE_SENSORS_ARRAY[i].pin, INPUT, false);
      if(SOIL_MOISTURE_SENSORS_ARRAY[i].type == FREQUENCY_SOIL_MOISTURE)
      {
        pinMode(SOIL_MOISTURE_SENSORS_ARRAY[i].pin,INPUT);
        digitalWrite(SOIL_MOISTURE_SENSORS_ARRAY[i].pin,HIGH);
      }
      State.AddState(StateSoilMoisture,i); // добавляем датчики влажности почвы
    } // for
  #endif
 }

void SoilMoistureModule::Update(uint16_t dt)
{ 

  // обновление модуля тут
  
  lastUpdateCall += dt;
  if(lastUpdateCall < SOIL_MOISTURE_UPDATE_INTERVAL) // обновляем согласно настроенному интервалу
    return;
  else
    lastUpdateCall = 0; 
    
    
    #if SUPPORTED_SOIL_MOISTURE_SENSORS > 0
      for(uint8_t i=0;i<SUPPORTED_SOIL_MOISTURE_SENSORS;i++)
      {
        switch(SOIL_MOISTURE_SENSORS_ARRAY[i].type)
        {
          case ANALOG_SOIL_MOISTURE: // аналоговый датчик влажности почвы
          {
              int val = analogRead(SOIL_MOISTURE_SENSORS_ARRAY[i].pin);
             // Serial.println(val);
      
              // теперь нам надо отразить показания между SOIL_MOISTURE_100_PERCENT и SOIL_MOISTURE_0_PERCENT
      
              int percentsInterval = map(val,min(SOIL_MOISTURE_0_PERCENT,SOIL_MOISTURE_100_PERCENT),max(SOIL_MOISTURE_0_PERCENT,SOIL_MOISTURE_100_PERCENT),0,10000);
      
              // теперь, если у нас значение 0% влажности больше, чем значение 100% влажности - надо от 10000 отнять полученное значение
              if(SOIL_MOISTURE_0_PERCENT > SOIL_MOISTURE_100_PERCENT)
                percentsInterval = 10000 - percentsInterval;
           
              Humidity h;
              h.Value = percentsInterval/100;
              h.Fract  = percentsInterval%100;
              if(h.Value > 99)
              {
                h.Value = 100;
                h.Fract = 0;
              }
      
              if(h.Value < 0)
              {
                h.Value = NO_TEMPERATURE_DATA;
                h.Fract = 0;
              }
      
              
              // обновляем состояние  
              State.UpdateState(StateSoilMoisture,i,(void*)&h);
          } 
          break;

          case FREQUENCY_SOIL_MOISTURE: // частотный датчик влажности почвы
          {
           // Serial.println("Update frequency sensors...");
            
            int8_t pin = SOIL_MOISTURE_SENSORS_ARRAY[i].pin;
            Humidity h;

            int highTime = pulseIn(pin,HIGH, PULSE_TIMEOUT);

            if(!highTime) // ALWAYS HIGH,  BUS ERROR
            {
             // Serial.println("BUS ERROR, NO HIGH TIME");
              State.UpdateState(StateSoilMoisture,i,(void*)&h);
            }
            else
            {
              // normal
              highTime = pulseIn(pin,HIGH, PULSE_TIMEOUT);
              int lowTime = pulseIn(pin,LOW, PULSE_TIMEOUT);

              if(!lowTime || !highTime)
              {
               // Serial.println("BUS ERROR, NO LOW OR HIGH TIME");
                // BUS ERROR
                State.UpdateState(StateSoilMoisture,i,(void*)&h);
              }
              else
              {
                // normal
                int totalTime = lowTime + highTime;
                float moisture = (highTime*100.0)/totalTime;
                int moistureInt = moisture*100;

                h.Value = moistureInt/100;
                h.Fract = moistureInt%100;

                State.UpdateState(StateSoilMoisture,i,(void*)&h);

                //Serial.print("Moisture is: ");
               // Serial.println(h);

              }

            } // else
            
            
          }
          break;

        } // switch
      } // for
    #endif
  

}

bool  SoilMoistureModule::ExecCommand(const Command& command, bool wantAnswer)
{
  if(wantAnswer) 
    PublishSingleton = NOT_SUPPORTED;
    
  if(command.GetType() == ctSET) // установка свойств
  {
    
  } // ctSET    
  else
  if(command.GetType() == ctGET) // запрос свойств
  {
      uint8_t argsCnt = command.GetArgsCount();
      if(argsCnt < 1)
      {
        if(wantAnswer) 
          PublishSingleton = PARAMS_MISSED; // не хватает параметров
        
      } // argsCnt < 1 
      else
      {     
        String param = command.GetArg(0);
        
        if(param == ALL) // запросили показания со всех датчиков: CTGET=SOIL|ALL
        {
          PublishSingleton.Status = true;
          uint8_t _cnt = State.GetStateCount(StateSoilMoisture);
          if(wantAnswer) 
            PublishSingleton = _cnt;
          
          for(uint8_t i=0;i<_cnt;i++)
          {

             OneState* stateHumidity = State.GetStateByOrder(StateSoilMoisture,i);
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
        if(param == PROP_CNT) // запросили данные о кол-ве датчиков: CTGET=SOIL|CNT
        {
          PublishSingleton.Status = true;
          if(wantAnswer) 
          {
            PublishSingleton = PROP_CNT; 
            uint8_t _cnt = State.GetStateCount(StateSoilMoisture);
            PublishSingleton << PARAM_DELIMITER << _cnt;
          }
        } // PROP_CNT
        else
        if(param != GetID()) // если только не запросили без параметров
        {
 // запросили показания с датчика по индексу
          uint8_t idx = param.toInt();
          uint8_t _cnt = State.GetStateCount(StateSoilMoisture);
          
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
              
             OneState* stateHumidity = State.GetStateByOrder(StateSoilMoisture,idx);
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

