#include "UniversalSensors.h"
#include <OneWire.h>
#include "Memory.h"
#include "InteropStream.h"
//-------------------------------------------------------------------------------------------------------------------------------------------------------
UniRegDispatcher UniDispatcher;
UniScratchpadClass UniScratchpad; // наш пишичитай скратчпада
UniClientsFactory UniFactory; // наша фабрика клиентов
UniRawScratchpad SHARED_SCRATCHPAD; // общий скратчпад для классов опроса модулей, висящих на линиях
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_UNI_NEXTION_MODULE
  UniNextionWaitScreenData UNI_NX_SENSORS_DATA[] = { UNI_NEXTION_WAIT_SCREEN_SENSORS, {0,0,""} };
#endif
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_RS485_GATE
//-------------------------------------------------------------------------------------------------------------------------------------------------------
UniRS485Gate::UniRS485Gate()
{
#ifdef USE_UNI_EXECUTION_MODULE  
  updateTimer = 0;
#endif  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_UNIVERSAL_SENSORS
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniRS485Gate::isInOnlineQueue(const RS485QueueItem& item)
{
  for(size_t i=0;i<sensorsOnlineQueue.size();i++)
    if(sensorsOnlineQueue[i].sensorType == item.sensorType && sensorsOnlineQueue[i].sensorIndex == item.sensorIndex)
      return true;
  return false;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_UNIVERSAL_SENSORS
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRS485Gate::enableSend()
{
  digitalWrite(RS_485_DE_PIN,HIGH); // переводим контроллер RS-485 на передачу
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRS485Gate::enableReceive()
{
  digitalWrite(RS_485_DE_PIN,LOW); // переводим контроллер RS-485 на приём
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRS485Gate::Setup()
{
  WORK_STATUS.PinMode(RS_485_DE_PIN,OUTPUT);
  
  enableSend();
  
  RS_485_SERIAL.begin(RS485_SPEED);

  if(&(RS_485_SERIAL) == &Serial) {
       WORK_STATUS.PinMode(0,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(1,OUTPUT,false);
  } else if(&(RS_485_SERIAL) == &Serial1) {
       WORK_STATUS.PinMode(19,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(18,OUTPUT,false);
  } else if(&(RS_485_SERIAL) == &Serial2) {
       WORK_STATUS.PinMode(17,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(16,OUTPUT,false);
  } else if(&(RS_485_SERIAL) == &Serial3) {
       WORK_STATUS.PinMode(15,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(14,OUTPUT,false);
  }

  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
byte UniRS485Gate::crc8(const byte *addr, byte len)
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
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRS485Gate::waitTransmitComplete()
{
  // ждём завершения передачи по UART
  while(!(RS_485_UCSR & _BV(RS_485_TXC) ));
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRS485Gate::Update(uint16_t dt)
{

  static RS485Packet packet;
  
  #ifdef USE_UNI_EXECUTION_MODULE

  // посылаем в шину данные для исполнительных модулей
  
    updateTimer += dt;
    if(updateTimer > RS495_STATE_PUSH_FREQUENCY)
    {
      updateTimer = 0;

      // тут посылаем слепок состояния контроллера
        memset(&packet,0,sizeof(RS485Packet));
        
        packet.header1 = 0xAB;
        packet.header2 = 0xBA;
        packet.tail1 = 0xDE;
        packet.tail2 = 0xAD;

        packet.direction = RS485FromMaster;
        packet.type = RS485ControllerStatePacket;

        void* dest = packet.data;
        ControllerState curState = WORK_STATUS.GetState();
        void* src = &curState;
        memcpy(dest,src,sizeof(ControllerState));

        const byte* b = (const byte*) &packet;
        packet.crc8 = crc8(b,sizeof(RS485Packet)-1);

        // пишем в шину RS-495 слепок состояния контроллера
        RS_485_SERIAL.write((const uint8_t *)&packet,sizeof(RS485Packet));

        // теперь ждём завершения передачи
        waitTransmitComplete();
        
    }
  #endif // USE_UNI_EXECUTION_MODULE

  #ifdef USE_UNIVERSAL_SENSORS


   static byte _is_inited = false;
   if(!_is_inited)
   {
      _is_inited = true;
      // инициализируем очередь
       for(byte sensorType=uniTemp;sensorType<=uniPH;sensorType++)
       {
         byte cnt = UniDispatcher.GetUniSensorsCount((UniSensorType) sensorType);
    
          for(byte k=0;k<cnt;k++)
          {
            RS485QueueItem qi;
            qi.sensorType = sensorType;
            qi.sensorIndex = k;
            queue.push_back(qi);
          } // for
          
       } // for
    
       currentQueuePos = 0;
       sensorsTimer = 0;      
    
   } // if
  

    sensorsTimer += dt;
    if(sensorsTimer > RS485_ONE_SENSOR_UPDATE_INTERVAL)
    {
      sensorsTimer = 0;

      // настало время опроса датчиков на шине
      if(queue.size())
      {
        // есть очередь для опроса
        RS485QueueItem* qi = &(queue[currentQueuePos]);
        currentQueuePos++;

        // мы не можем обновлять состояние датчика в дефолтные значения здесь, поскольку
        // мы не знаем, откуда с него могут придти данные. В случае с работой через 1-Wire
        // состояние автоматически обновляется, поскольку считается, что если модуль есть
        // на линии - с него будут данные. У нас же ситуация обстоит по-другому:
        // мы проходим все зарегистрированные универсальные датчики, и не можем
        // делать вывод - висит ли модуль с датчиком на линии RS-485, или работает по радиоканалу,
        // или - работает по 1-Wire. Поэтому мы не вправе делать никаких предположений и менять
        // показания датчика на вид <нет данных>, поскольку очерёдность вызовов опроса
        // универсальных модулей по разным шлюзам не определена. 
        // поэтому мы сбрасываем состояния только тех датчиков, которые хотя бы однажды
        // откликнулись по шине RS-495.

        if(isInOnlineQueue(*qi))
        {
          byte sType = qi->sensorType;
          byte sIndex = qi->sensorIndex;
          // датчик был онлайн, сбрасываем его показания в "нет данных" перед опросом
          UniDispatcher.AddUniSensor((UniSensorType)sType,sIndex);

                    // проверяем тип датчика, которому надо выставить "нет данных"
                    switch(qi->sensorType)
                    {
                      case uniTemp:
                      {
                        // температура
                        Temperature t;
                        // получаем состояния
                        UniSensorState states;
                        if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                        {
                          if(states.State1)
                            states.State1->Update(&t);
                        } // if
                      }
                      break;

                      case uniHumidity:
                      {
                        // влажность
                        Humidity h;
                        // получаем состояния
                        UniSensorState states;
                        if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                        {
                          if(states.State1)
                            states.State1->Update(&h);
                                              
                          if(states.State2)
                            states.State2->Update(&h);
                        } // if                        
                      }
                      break;

                      case uniLuminosity:
                      {
                        // освещённость
                        long lum = NO_LUMINOSITY_DATA;
                        // получаем состояния
                        UniSensorState states;
                        if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                        {
                          if(states.State1)
                            states.State1->Update(&lum);
                        } // if                        
                        
                        
                      }
                      break;

                      case uniSoilMoisture: // влажность почвы
                      case uniPH: // показания pH
                      {
                        
                        Humidity h;
                        // получаем состояния
                        UniSensorState states;
                        if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                        {
                          if(states.State1)
                            states.State1->Update(&h);
                        } // if                        
                        
                      }
                      break;
                      
                    } // switch
          
        } // if in online queue
        
        
        if(currentQueuePos >= queue.size()) // достигли конца очереди, начинаем сначала
          currentQueuePos = 0;


        memset(&packet,0,sizeof(RS485Packet)); 
        packet.header1 = 0xAB;
        packet.header2 = 0xBA;
        packet.tail1 = 0xDE;
        packet.tail2 = 0xAD;

        packet.direction = RS485FromMaster; // направление - от нас ведомым
        packet.type = RS485SensorDataPacket; // это пакет - запрос на показания с датчиков

        byte* dest = packet.data;
        // в первом байте - тип датчика для опроса
        *dest = qi->sensorType;
        dest++;
        // во втором байте - индекс датчика, зарегистрированный в системе
        *dest = qi->sensorIndex;

        // считаем контрольную сумму
        const byte* b = (const byte*) &packet;
        packet.crc8 = crc8(b,sizeof(RS485Packet)-1);


        #ifdef RS485_DEBUG

        // отладочная информация
        Serial.print(F("Request data for sensor type="));
        Serial.print(qi->sensorType);
        Serial.print(F(" and index="));
        Serial.println(qi->sensorIndex);

        #endif

        // пакет готов к отправке, отправляем его
        RS_485_SERIAL.write((const uint8_t *)&packet,sizeof(RS485Packet));
        waitTransmitComplete(); // ждём окончания посыла
        // теперь переключаемся на приём
        enableReceive();

        // поскольку мы сразу же переключились на приём - можем дать поработать критичному ко времени коду
        yield();

        // и получаем наши байты
        memset(&packet,0,sizeof(RS485Packet));
        byte* writePtr = (byte*) &packet;
        byte bytesReaded = 0; // кол-во прочитанных байт
        // запоминаем время начала чтения
        unsigned long startReadingTime = micros();
        // вычисляем таймаут как время для чтения десяти байт.
        // в RS485_SPEED - у нас скорость в битах в секунду. Для чтения десяти байт надо вычитать 100 бит.
        const unsigned long readTimeout  = (10000000ul/RS485_SPEED)*RS485_BYTES_TIMEOUT; // кол-во микросекунд, необходимое для вычитки десяти байт

        // начинаем читать данные
        while(1)
        {
          if( micros() - startReadingTime > readTimeout)
          {
            #ifdef RS485_DEBUG
              Serial.println(F("TIMEOUT REACHED!!!"));
            #endif
            
            break;
          } // if

          if(RS_485_SERIAL.available())
          {
            startReadingTime = micros(); // сбрасываем таймаут
            *writePtr++ = (byte) RS_485_SERIAL.read();
            bytesReaded++;
          } // if available

          if(bytesReaded == sizeof(RS485Packet)) // прочитали весь пакет
          {
            #ifdef RS485_DEBUG
              Serial.println(F("Packet received from slave!"));
            #endif
            
            break;
          }
          
        } // while

        // затем опять переключаемся на передачу
        enableSend();

        // теперь парсим пакет
        if(bytesReaded == sizeof(RS485Packet))
        {
          // пакет получен полностью, парсим его
          #ifdef RS485_DEBUG
            Serial.println(F("Packet from slave received, parse it..."));
          #endif
          
          bool headOk = packet.header1 == 0xAB && packet.header2 == 0xBA;
          bool tailOk = packet.tail1 == 0xDE && packet.tail2 == 0xAD;
          if(headOk && tailOk)
          {
            #ifdef RS485_DEBUG
              Serial.println(F("Header and tail ok."));
            #endif
            
            // вычисляем crc
            byte crc = crc8((const byte*)&packet,sizeof(RS485Packet)-1);
            if(crc == packet.crc8)
            {
              #ifdef RS485_DEBUG
                Serial.println(F("Checksum ok."));
              #endif
              
              // теперь проверяем, нам ли пакет
              if(packet.direction == RS485FromSlave && packet.type == RS485SensorDataPacket)
              {
                #ifdef RS485_DEBUG
                  Serial.println(F("Packet type ok"));
                #endif

                byte* readDataPtr = packet.data;
                // проверяем - байт типа и байт индекса должны совпадать с посланными в шину
                byte sType = *readDataPtr++;
                byte sIndex = *readDataPtr++;
                
                if(sType == qi->sensorType && sIndex == qi->sensorIndex)
                {
                  #ifdef RS485_DEBUG
                    Serial.println(F("Reading sensor data..."));
                  #endif

                  // добавляем наш тип сенсора в систему, если этого ещё не сделано
                  UniDispatcher.AddUniSensor((UniSensorType)sType,sIndex);

                  // добавляем датчик в список онлайн-датчиков
                  if(!isInOnlineQueue(*qi))
                    sensorsOnlineQueue.push_back(*qi);

                    // проверяем тип датчика, с которого читали показания
                    switch(sType)
                    {
                      case uniTemp:
                      {
                        // температура
                        // получаем данные температуры
                        Temperature t;
                        t.Value = (int8_t) *readDataPtr++;
                        t.Fract = *readDataPtr;

                        #ifdef RS485_DEBUG
                          Serial.print(F("Temperature: "));
                          Serial.println(t);
                        #endif

                        // получаем состояния
                        UniSensorState states;
                        if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                        {
                          if(states.State1)
                          {
                            #ifdef RS485_DEBUG
                              Serial.println(F("Update data in controller..."));
                            #endif
                            
                            states.State1->Update(&t);
                          }
                        } // if
                      }
                      break;

                      case uniHumidity:
                      {
                        // влажность
                        Humidity h;
                        h.Value = (int8_t) *readDataPtr++;
                        h.Fract = *readDataPtr++;

                        // температура
                        Temperature t;
                        t.Value = (int8_t) *readDataPtr++;
                        t.Fract = *readDataPtr++;

                        #ifdef RS485_DEBUG
                          Serial.print(F("Humidity: "));
                          Serial.println(h);
                        #endif

                        // получаем состояния
                        UniSensorState states;
                        if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                        {
                            #ifdef RS485_DEBUG
                              Serial.println(F("Update data in controller..."));
                            #endif

                          if(states.State1)
                            states.State1->Update(&h);

                          if(states.State2)
                            states.State2->Update(&t);
                            
                        } // if                        
                      }
                      break;

                      case uniLuminosity:
                      {
                        // освещённость
                        long lum;
                        memcpy(&lum,readDataPtr,sizeof(long));

                        #ifdef RS485_DEBUG
                          Serial.print(F("Luminosity: "));
                          Serial.println(lum);
                        #endif

                        // получаем состояния
                        UniSensorState states;
                        if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                        {
                          if(states.State1)
                          {
                            #ifdef RS485_DEBUG
                              Serial.println(F("Update data in controller..."));
                            #endif
                            
                            states.State1->Update(&lum);
                          }
                        } // if                        
                        
                        
                      }
                      break;

                      case uniSoilMoisture: // влажность почвы
                      case uniPH:  // показания pH
                      {
                        
                        Humidity h;
                        h.Value = (int8_t) *readDataPtr++;
                        h.Fract = *readDataPtr;

                        #ifdef RS485_DEBUG
                          if(sType == uniSoilMoisture)
                            Serial.print(F("Soil moisture: "));
                          else
                            Serial.print(F("pH: "));
                            
                          Serial.println(h);
                        #endif

                        // получаем состояния
                        UniSensorState states;
                        if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                        {
                          if(states.State1)
                          {
                            #ifdef RS485_DEBUG
                              Serial.println(F("Update data in controller..."));
                            #endif
                            
                            states.State1->Update(&h);
                          }
                        } // if                        
                        
                      }
                      break;
                      
                    } // switch
                }
                #ifdef RS485_DEBUG
                else
                {
                  Serial.println(F("Received data from unknown sensor :("));
                }
                #endif
              }
              #ifdef RS485_DEBUG
              else
              {
                Serial.println(F("Wrong packet type :("));
              }
              #endif
            }
            #ifdef RS485_DEBUG
            else
            {
              Serial.println(F("Bad checksum :("));
            }
            #endif
          }
          #ifdef RS485_DEBUG
          else
          {
            Serial.println(F("Head or tail of packet is invalid :("));
          } // else
          #endif
        }
        #ifdef RS485_DEBUG
        else
        {
          Serial.println(F("Received uncompleted packet :("));
        } // else
        #endif
          
        
      } // if(queue.size())
      #ifdef RS485_DEBUG
      else
      {
        Serial.println(F("No queue size :("));
      }
      #endif
        
      
    } // if(sensorsTimer > _upd_interval)
    
  #endif // USE_UNIVERSAL_SENSORS
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
UniRS485Gate RS485;
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_RS485_GATE
//-------------------------------------------------------------------------------------------------------------------------------------------------------
// UniClientsFactory
//-------------------------------------------------------------------------------------------------------------------------------------------------------
UniClientsFactory::UniClientsFactory()
{
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractUniClient* UniClientsFactory::GetClient(UniRawScratchpad* scratchpad)
{
  if(!scratchpad)
    return &dummyClient;

  UniClientType ct = (UniClientType) scratchpad->head.packet_type;
  
  switch(ct)
  {
    case uniSensorsClient:
      return &sensorsClient;

    case uniNextionClient:
      #ifdef USE_UNI_NEXTION_MODULE
        return &nextionClient;
      #else
      break;
      #endif

    case uniExecutionClient:
    #ifdef USE_UNI_EXECUTION_MODULE
      return &executionClient;
    #else
      break;
    #endif
  }

  return &dummyClient;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_UNI_EXECUTION_MODULE
//-------------------------------------------------------------------------------------------------------------------------------------------------------
UniExecutionModuleClient::UniExecutionModuleClient()
{
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniExecutionModuleClient::Register(UniRawScratchpad* scratchpad)
{
  // нам регистрироваться в системе дополнительно не надо
  UNUSED(scratchpad);
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniExecutionModuleClient::Update(UniRawScratchpad* scratchpad, bool isModuleOnline, UniScratchpadSource receivedThrough)
{
  if(!isModuleOnline) // когда модуль офлайн - ничего делать не надо
    return;

   // приводим к типу нашего скратча
   UniExecutionModuleScratchpad* ourScratch = (UniExecutionModuleScratchpad*) &(scratchpad->data);

   // получаем состояние контроллера
   ControllerState state = WORK_STATUS.GetState();

   // теперь проходимся по всем слотам
   for(byte i=0;i<8;i++)
   {
      byte slotStatus = 0; // статус слота - 0 по умолчанию
      
      switch(ourScratch->slots[i].slotType)
      {
        case slotEmpty: // пустой слот, ничего делать не надо
        case 0xFF: // если вычитали из EEPROM, а там ничего не было
        break;

        case slotWindowLeftChannel:
        {
          // состояние левого канала окна, в slotLinkedData - номер окна
          byte windowNumber = ourScratch->slots[i].slotLinkedData;
          if(windowNumber < 16)
          {
            // окна у нас нумеруются от 0 до 15, всего 16 окон.
            // на каждое окно - два бита, для левого и правого канала.
            // следовательно, чтобы получить стартовый бит - надо номер окна
            // умножить на 2.
            byte bitNum = windowNumber*2;           
            if(state.WindowsState & (1 << bitNum))
              slotStatus = 1; // выставляем в слоте значение 1
          }
        }
        break;

        case slotWindowRightChannel:
        {
          // состояние левого канала окна, в slotLinkedData - номер окна
          byte windowNumber = ourScratch->slots[i].slotLinkedData;
          if(windowNumber < 16)
          {
            // окна у нас нумеруются от 0 до 15, всего 16 окон.
            // на каждое окно - два бита, для левого и правого канала.
            // следовательно, чтобы получить стартовый бит - надо номер окна
            // умножить на 2.
            byte bitNum = windowNumber*2;

            // поскольку канал у нас правый - его бит идёт следом за левым.
            bitNum++;
                       
            if(state.WindowsState & (1 << bitNum))
              slotStatus = 1; // выставляем в слоте значение 1
          }
        }
        break;

        case slotWateringChannel:
        {
          // состояние канала полива, в slotLinkedData - номер канала полива
          byte wateringChannel = ourScratch->slots[i].slotLinkedData;
          if(wateringChannel< 16)
          {
            if(state.WaterChannelsState & (1 << wateringChannel))
              slotStatus = 1; // выставляем в слоте значение 1
              
          }
        }        
        break;

        case slotLightChannel:
        {
          // состояние канала досветки, в slotLinkedData - номер канала досветки
          byte lightChannel = ourScratch->slots[i].slotLinkedData;
          if(lightChannel < 8)
          {
            if(state.LightChannelsState & (1 << lightChannel))
              slotStatus = 1; // выставляем в слоте значение 1
              
          }
        }
        break;

        case slotPin:
        {
          // получаем статус пина
          byte pinNumber = ourScratch->slots[i].slotLinkedData;
          byte byteNum = pinNumber/8;
          byte bitNum = pinNumber%8;

          if(byteNum < 16)
          {
            // если нужный бит с номером пина установлен - на пине высокий уровень
            if(state.PinsState[byteNum] & (1 << bitNum))
              slotStatus = 1; // выставляем в слоте значение 1
          }
          
        }
        break;
        
      } // switch

      // мы получили slotStatus, записываем его обратно в слот
      ourScratch->slots[i].slotStatus = slotStatus;
   } // for

  if(receivedThrough == ssOneWire)
  {
    // пишем актуальное состояние слотов клиенту, если скратч был получен по 1-Wire, иначе - вызывающая сторона сама разберётся, что делать с изменениями
    UniScratchpad.begin(pin,scratchpad);
    UniScratchpad.write();
  }
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_UNI_EXECUTION_MODULE
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_UNI_NEXTION_MODULE
//-------------------------------------------------------------------------------------------------------------------------------------------------------
// NextionUniClient
//-------------------------------------------------------------------------------------------------------------------------------------------------------
NextionUniClient::NextionUniClient()
{
  updateTimer = 0;
  //tempChanged = false;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void NextionUniClient::Register(UniRawScratchpad* scratchpad)
{
  UNUSED(scratchpad);
  // нам регистрироваться не надо, ничего не делаем
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void NextionUniClient::Update(UniRawScratchpad* scratchpad, bool isModuleOnline, UniScratchpadSource receivedThrough)
{
  // тут обновляем данные, полученные с Nextion, и записываем ему текущее состояние
  if(!isModuleOnline) // не надо ничего делать
    return;

  // сначала проверяем, чего там у нас нажато в дисплее
  UniNextionScratchpad ourScratch;
  memcpy(&ourScratch,scratchpad->data,sizeof(UniNextionScratchpad));

  byte changesCount = 0; // кол-во изменений, если оно больше нуля - мы запишем скратч обратно, не дожидаясь наступления интервала обновления

  if(WORK_STATUS.IsModeChanged()) // были изменения в режиме работы
    changesCount++;

  if(bitRead(ourScratch.nextionStatus1,0))
  {
   // Serial.println("close windows");
    bitWrite(ourScratch.nextionStatus1,0,0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("STATE|WINDOW|ALL|CLOSE"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus1,1))
  {
  //  Serial.println("open windows");
    bitWrite(ourScratch.nextionStatus1,1, 0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("STATE|WINDOW|ALL|OPEN"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus1,2))
  {
 //   Serial.println("windows auto mode");
    bitWrite(ourScratch.nextionStatus1,2, 0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("STATE|MODE|AUTO"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus1,3))
  {
 //   Serial.println("windows manual mode");
    bitWrite(ourScratch.nextionStatus1,3,0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("STATE|MODE|MANUAL"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus1,4))
  {
  //  Serial.println("water on");
    bitWrite(ourScratch.nextionStatus1,4,0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("WATER|ON"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus1,5))
  {
 //   Serial.println("water off");
    bitWrite(ourScratch.nextionStatus1,5, 0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("WATER|OFF"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus1,6))
  {
  //  Serial.println("water auto mode");
    bitWrite(ourScratch.nextionStatus1,6,0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("WATER|MODE|AUTO"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus1,7))
  {
 //   Serial.println("water manual mode");
    bitWrite(ourScratch.nextionStatus1,7, 0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("WATER|MODE|MANUAL"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus2,0))
  {
  //  Serial.println("light on");
    bitWrite(ourScratch.nextionStatus2,0,0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("LIGHT|ON"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus2,1))
  {
 //   Serial.println("light off");
    bitWrite(ourScratch.nextionStatus2,1, 0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("LIGHT|OFF"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus2,2))
  {
 //   Serial.println("light auto mode");
    bitWrite(ourScratch.nextionStatus2,2, 0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("LIGHT|MODE|AUTO"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus2,3))
  {
 //   Serial.println("light manual mode");
    bitWrite(ourScratch.nextionStatus2,3, 0);
    changesCount++;
    ModuleInterop.QueryCommand(ctSET, F("LIGHT|MODE|MANUAL"),false);//,false);  
  }

  if(bitRead(ourScratch.nextionStatus2,4))
  {
 //   Serial.println("open temp inc");
    bitWrite(ourScratch.nextionStatus2,4, 0);
    changesCount++;
    byte tmp = MainController->GetSettings()->GetOpenTemp();
    if(tmp < 50)
      ++tmp;  

    MainController->GetSettings()->SetOpenTemp(tmp);
    //tempChanged = true;
  }

  if(bitRead(ourScratch.nextionStatus2,5))
  {
 //   Serial.println("open temp dec");
    bitWrite(ourScratch.nextionStatus2,5, 0);
    changesCount++;
    byte tmp = MainController->GetSettings()->GetOpenTemp();
    if(tmp > 0)
      --tmp;  

    MainController->GetSettings()->SetOpenTemp(tmp);
   // tempChanged = true;
  }

  if(bitRead(ourScratch.nextionStatus2,6))
  {
  //  Serial.println("close temp inc");
    bitWrite(ourScratch.nextionStatus2,6, 0);
    changesCount++;
    byte tmp = MainController->GetSettings()->GetCloseTemp();
    if(tmp < 50)
      ++tmp;  

    MainController->GetSettings()->SetCloseTemp(tmp);
    //tempChanged = true;
  }

  if(bitRead(ourScratch.nextionStatus2,7))
  {
 //   Serial.println("close temp dec");
    bitWrite(ourScratch.nextionStatus2,7, 0);
    changesCount++;
    byte tmp = MainController->GetSettings()->GetCloseTemp();
    if(tmp > 0)
      --tmp;  

    MainController->GetSettings()->SetCloseTemp(tmp);
    //tempChanged = true;
  }

  if(bitRead(ourScratch.controllerStatus,6)) // дисплей заснул, можно сохранять настройки
  {
    bitWrite(ourScratch.controllerStatus,6,0); 
  //  Serial.println("enter sleep");
    //if(tempChanged)
  //   MainController->GetSettings()->Save();

  //  tempChanged = false;
  }

  // теперь проверяем, надо ли нам записывать настройки немедленно
   unsigned long curMillis = millis();
   bool needToWrite = (changesCount > 0) || (curMillis - updateTimer > 1000);
   if(needToWrite)
   {
    // надо записать текущее положение дел в Nextion
      updateTimer = curMillis;

      bitWrite(ourScratch.controllerStatus,0, WORK_STATUS.GetStatus(WINDOWS_STATUS_BIT));
      bitWrite(ourScratch.controllerStatus,1, WORK_STATUS.GetStatus(WINDOWS_MODE_BIT));
      bitWrite(ourScratch.controllerStatus,2, WORK_STATUS.GetStatus(WATER_STATUS_BIT));
      bitWrite(ourScratch.controllerStatus,3, WORK_STATUS.GetStatus(WATER_MODE_BIT));
      bitWrite(ourScratch.controllerStatus,4, WORK_STATUS.GetStatus(LIGHT_STATUS_BIT));
      bitWrite(ourScratch.controllerStatus,5, WORK_STATUS.GetStatus(LIGHT_MODE_BIT));

      GlobalSettings* sett = MainController->GetSettings();
      ourScratch.openTemperature = sett->GetOpenTemp();
      ourScratch.closeTemperature = sett->GetCloseTemp();

      // теперь пишем показания с датчиков
      ourScratch.dataCount = 0;
      
      byte cntr = 0;
      while(UNI_NX_SENSORS_DATA[cntr].sensorType > 0)
      {
        AbstractModule* module = MainController->GetModuleByID(UNI_NX_SENSORS_DATA[cntr].moduleName);
        if(module)
        {
          OneState* os = module->State.GetState((ModuleStates)UNI_NX_SENSORS_DATA[cntr].sensorType,UNI_NX_SENSORS_DATA[cntr].sensorIndex);
          if(os)
          {
            // получили состояние, теперь пишем его в скратч
            if(os->HasData())
            {
              byte buff[4] = {0};
              os->GetRawData(buff);
              ourScratch.data[ourScratch.dataCount].sensorType = UNI_NX_SENSORS_DATA[cntr].sensorType;
              memcpy(ourScratch.data[ourScratch.dataCount].sensorData,buff,2);
              ourScratch.dataCount++;
            }
          } // if(os)
        } // if(module)

        cntr++;

        if(ourScratch.dataCount > 4)
          break;
      } // while
      

      // копируем скратчпад обратно
      memcpy(scratchpad->data,&ourScratch,sizeof(UniNextionScratchpad));

      if(receivedThrough == ssOneWire)
      {
        // и пишем его в Nextion, если скратч был получен по 1-Wire, иначе - вызывающая сторона сама разберётся, куда пихать изменённый скратч
        UniScratchpad.begin(pin,scratchpad);
        UniScratchpad.write();
      }
      
   } // needToWrite
   
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_UNI_NEXTION_MODULE
//-------------------------------------------------------------------------------------------------------------------------------------------------------
// SensorsUniClient
//-------------------------------------------------------------------------------------------------------------------------------------------------------
SensorsUniClient::SensorsUniClient() : AbstractUniClient()
{
  measureTimer = 0;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void SensorsUniClient::Register(UniRawScratchpad* scratchpad)
{
  // регистрируем модуль тут, добавляя нужные индексы датчиков в контроллер
  UniSensorsScratchpad* ourScrath = (UniSensorsScratchpad*) &(scratchpad->data);
  byte addedCount = 0;

  for(byte i=0;i<MAX_UNI_SENSORS;i++)
  {
    byte type = ourScrath->sensors[i].type;
    if(type == NO_SENSOR_REGISTERED) // нет типа датчика 
      continue;

    UniSensorType ut = (UniSensorType) type;
    
    if(ut == uniNone) // нет типа датчика
      continue;

    // имеем тип датчика, можем регистрировать
    if(UniDispatcher.AddUniSensor(ut,ourScrath->sensors[i].index))
      addedCount++;
    
  } // for

  if(addedCount > 0) // добавили датчики, надо сохранить состояние контроллера в EEPROM
    UniDispatcher.SaveState();
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void SensorsUniClient::Update(UniRawScratchpad* scratchpad, bool isModuleOnline, UniScratchpadSource receivedThrough)
{
  
    // тут обновляем данные, полученный по проводу с модуля. 
    // нам передали адрес скратчпада, куда можно писать данные, полученные
    // с клиента, при необходимости.

    // нас дёргают после вычитки скратчпада из модуля, всё, что мы должны сделать - 
    // это обновить данные в контроллере.

    UniSensorsScratchpad* ourScratch = (UniSensorsScratchpad*) &(scratchpad->data);
    UniSensorState states;
    
    
    for(byte i=0;i<MAX_UNI_SENSORS;i++)
    {

      byte type = ourScratch->sensors[i].type;
      if(type == NO_SENSOR_REGISTERED) // нет типа датчика 
        continue;
  
      UniSensorType ut = (UniSensorType) type;
      
      if(ut == uniNone) // нет типа датчика
        continue;
      
      if(UniDispatcher.GetRegisteredStates(ut, ourScratch->sensors[i].index, states))
      {
        // получили состояния, можно обновлять
        UpdateStateData(states, &(ourScratch->sensors[i]), isModuleOnline);
      } // if
    } // for

    // тут запускаем конвертацию, чтобы при следующем вызове вычитать актуальные данные.
    // конвертацию не стоит запускать чаще, чем в 5, скажем, секунд.
    if(receivedThrough == ssOneWire && isModuleOnline)
    {
      // работаем таким образом только по шине 1-Wire, в остальном вызывающая сторона разберётся, что делать со скратчпадом
      unsigned long curMillis = millis();
      if(curMillis - measureTimer > 5000)
      {
        #ifdef UNI_DEBUG
          Serial.print(F("Start measure on 1-Wire pin "));
          Serial.println(pin);
         #endif    
        
        measureTimer = curMillis;
        UniScratchpad.begin(pin,scratchpad);
        UniScratchpad.startMeasure();
      }
      
    } // if

}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void SensorsUniClient::UpdateStateData(const UniSensorState& states,const UniSensorData* data,bool IsModuleOnline)
{
  if(!(states.State1 || states.State2))
    return; // не найдено ни одного состояния  

  UpdateOneState(states.State1,data,IsModuleOnline);
  UpdateOneState(states.State2,data,IsModuleOnline);  

}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void SensorsUniClient::UpdateOneState(OneState* os, const UniSensorData* dataPacket, bool IsModuleOnline)
{
    if(!os)
      return;

   uint8_t sensorIndex = dataPacket->index;
   uint8_t sensorType = dataPacket->type;
   uint8_t dataIndex = 0;

   if(sensorIndex == NO_SENSOR_REGISTERED || sensorType == NO_SENSOR_REGISTERED || sensorType == uniNone)
    return; // нет датчика вообще

   switch(os->GetType())
   {
      case StateTemperature:
      {
        if(sensorType == uniHumidity) // если тип датчика - влажность, значит температура у нас идёт после влажности, в 3-м и 4-м байтах
        {
          dataIndex++; dataIndex++;
        }

        int8_t dt = (int8_t) dataPacket->data[dataIndex++];
        uint8_t dt2 =  dataPacket->data[dataIndex];

        
        int8_t b1 = IsModuleOnline ? dt : NO_TEMPERATURE_DATA;             
        uint8_t b2 = IsModuleOnline ? dt2 : 0;

        Temperature t(b1, b2);
        os->Update(&t);
        
      }
      break;

      case StateHumidity:
      case StateSoilMoisture:
      case StatePH:
      {
        int8_t dt = (int8_t)  dataPacket->data[dataIndex++];
        uint8_t dt2 =  dataPacket->data[dataIndex];
        
        int8_t b1 = IsModuleOnline ? dt : NO_TEMPERATURE_DATA;    
        uint8_t b2 = IsModuleOnline ? dt2 : 0;
        
        Humidity h(b1, b2);
        os->Update(&h);        

      }
      break;

      case StateLuminosity:
      {
        unsigned long lum = NO_LUMINOSITY_DATA;
        
        if(IsModuleOnline)
          memcpy(&lum, dataPacket->data, 4);

        os->Update(&lum);
        
      }
      break;

      case StateWaterFlowInstant:
      case StateWaterFlowIncremental:
      case StateUnknown:
      
      break;
      
    
   } // switch
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#if UNI_WIRED_MODULES_COUNT > 0
//-------------------------------------------------------------------------------------------------------------------------------------------------------
UniPermanentLine::UniPermanentLine(uint8_t pinNumber)
{
  pin = pinNumber;
  timer = random(0,UNI_MODULE_UPDATE_INTERVAL); // разнесём опрос датчиков по времени
  lastClient = NULL;

}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniPermanentLine::IsRegistered()
{
  if(SHARED_SCRATCHPAD.head.packet_type == uniNextionClient) // для дисплея Nextion не требуется регистрация 
    return true;
    
  return ( SHARED_SCRATCHPAD.head.controller_id == UniDispatcher.GetControllerID() );
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniPermanentLine::Update(uint16_t dt)
{
  timer += dt;

  if(timer < UNI_MODULE_UPDATE_INTERVAL) // рано обновлять
    return;

  timer -= UNI_MODULE_UPDATE_INTERVAL; // сбрасываем таймер

  // теперь обновляем последнего клиента, если он был.
  // говорим ему, чтобы обновился, как будто модуля нет на линии.
  if(lastClient)
  {
    lastClient->Update(&SHARED_SCRATCHPAD,false, ssOneWire);
    lastClient = NULL; // сбрасываем клиента, поскольку его может больше не быть на линии
  }

  // теперь пытаемся прочитать скратчпад
  UniScratchpad.begin(pin,&SHARED_SCRATCHPAD);
  
  if(UniScratchpad.read())
  {
    // прочитали, значит, датчик есть на линии.
   #ifdef UNI_DEBUG
    Serial.print(F("Module found on 1-Wire pin "));
    Serial.println(pin);
   #endif    
   
    // проверяем, зарегистрирован ли модуль у нас?
    if(!IsRegistered()) // модуль не зарегистрирован у нас
      return;
      
    // получаем клиента для прочитанного скратчпада
    lastClient = UniFactory.GetClient(&SHARED_SCRATCHPAD);
    lastClient->SetPin(pin); // назначаем тот же самый пин, что у нас    
    lastClient->Update(&SHARED_SCRATCHPAD,true, ssOneWire);

    // вот здесь получается следующая ситуация - может отправиться команда на старт измерений, и, одновременно - 
    // команда на чтение скратчпада. команда на старт измерений требует времени, поэтому, если
    // сразу после запуска конвертации пытаться читать - получится бяка.
    
  } // if
  else
  {
    // на линии никого нет
   #ifdef UNI_DEBUG
    Serial.print(F("NO MODULES FOUND ON 1-Wire pin "));
    Serial.println(pin);
   #endif

  }
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#endif
//-------------------------------------------------------------------------------------------------------------------------------------------------------
// UniRegDispatcher
//-------------------------------------------------------------------------------------------------------------------------------------------------------
UniRegDispatcher::UniRegDispatcher()
{
  temperatureModule = NULL;
  humidityModule = NULL;
  luminosityModule = NULL;
  soilMoistureModule = NULL;
  phModule = NULL;  

  currentTemperatureCount = 0;
  currentHumidityCount = 0;
  currentLuminosityCount = 0;
  currentSoilMoistureCount = 0;
  currentPHCount = 0;

  hardCodedTemperatureCount = 0;
  hardCodedHumidityCount = 0;
  hardCodedLuminosityCount = 0;
  hardCodedSoilMoistureCount = 0;
  hardCodedPHCount = 0;

  rfChannel = UNI_DEFAULT_RF_CHANNEL;
    
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniRegDispatcher::AddUniSensor(UniSensorType type, uint8_t sensorIndex)
{
  // добавляем состояние для датчика в систему. Состояние надо добавлять только тогда,
  // когда переданный индекс датчика не укладывается в уже выданный диапазон.
  // например, переданный индекс - 0, и кол-во выданных до этого индексов - 0, следовательно,
  // мы не попадаем в выданный диапазон. Или - переданный индекс - 1, кол-во ранее выданных - 0,
  // значит, мы должны добавить 2 новых состояния.

  // если sensorIndex == 0xFF - ничего делать не надо
  if(sensorIndex == NO_SENSOR_REGISTERED) // попросили зарегистрировать датчик без назначенного ранее индекса, ошибка.
    return false;
  
   switch(type)
  {
    case uniNone:  // нет датчика
      return false;
    
    case uniTemp:  // температурный датчик
      if(temperatureModule)
      {
          if(sensorIndex < currentTemperatureCount) // попадаем в диапазон уже выданных
            return false;

          // здесь sensorIndex больше либо равен currentTemperatureCount, следовательно, мы не попадаем в диапазон
          uint8_t to_add = (sensorIndex - currentTemperatureCount) + 1;

          for(uint8_t cntr = 0; cntr < to_add; cntr++)
          {
            temperatureModule->State.AddState(StateTemperature,hardCodedTemperatureCount + currentTemperatureCount + cntr);
          } // for

          // сохраняем кол-во добавленных
          currentTemperatureCount += to_add;
          
        return true;
      } // if(temperatureModule)
      else
        return false;
    
    case uniHumidity: 
    if(humidityModule)
      {

          if(sensorIndex < currentHumidityCount) // попадаем в диапазон уже выданных
            return false;

          // здесь sensorIndex больше либо равен currentHumidityCount, следовательно, мы не попадаем в диапазон
          uint8_t to_add = (sensorIndex - currentHumidityCount) + 1;

          for(uint8_t cntr = 0; cntr < to_add; cntr++)
          {
            humidityModule->State.AddState(StateTemperature,hardCodedHumidityCount + currentHumidityCount + cntr);
            humidityModule->State.AddState(StateHumidity,hardCodedHumidityCount + currentHumidityCount + cntr);
          } // for

          // сохраняем кол-во добавленных
          currentHumidityCount += to_add;
          
        return true;
        
      }
      else
        return false;
    
    case uniLuminosity: 
    if(luminosityModule)
      {

          if(sensorIndex < currentLuminosityCount) // попадаем в диапазон уже выданных
            return false;

          // здесь sensorIndex больше либо равен currentLuminosityCount, следовательно, мы не попадаем в диапазон
          uint8_t to_add = (sensorIndex - currentLuminosityCount) + 1;

          for(uint8_t cntr = 0; cntr < to_add; cntr++)
          {
            luminosityModule->State.AddState(StateLuminosity,hardCodedLuminosityCount + currentLuminosityCount + cntr);
          } // for

          // сохраняем кол-во добавленных
          currentLuminosityCount += to_add;
          
        return true;
      }    
      else
        return false;
    
    case uniSoilMoisture: 
     if(soilMoistureModule)
      {
     
          if(sensorIndex < currentSoilMoistureCount) // попадаем в диапазон уже выданных
            return false;

          // здесь sensorIndex больше либо равен currentSoilMoistureCount, следовательно, мы не попадаем в диапазон
          uint8_t to_add = (sensorIndex - currentSoilMoistureCount) + 1;

          for(uint8_t cntr = 0; cntr < to_add; cntr++)
          {
            soilMoistureModule->State.AddState(StateSoilMoisture,hardCodedSoilMoistureCount + currentSoilMoistureCount + cntr);
          } // for

          // сохраняем кол-во добавленных
          currentSoilMoistureCount += to_add;
          
        return true;
      } 
      else
        return false;


    case uniPH:
     if(phModule)
      {
     
          if(sensorIndex < currentPHCount) // попадаем в диапазон уже выданных
            return false;

          // здесь sensorIndex больше либо равен currentPHCount, следовательно, мы не попадаем в диапазон
          uint8_t to_add = (sensorIndex - currentPHCount) + 1;

          for(uint8_t cntr = 0; cntr < to_add; cntr++)
          {
            phModule->State.AddState(StatePH,hardCodedPHCount + currentPHCount + cntr);
          } // for

          // сохраняем кол-во добавленных
          currentPHCount += to_add;
          
        return true;
      } 
      else
        return false;
  } 

  return false;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t UniRegDispatcher::GetUniSensorsCount(UniSensorType type)
{
  switch(type)
  {
    case uniNone: return 0;
    case uniTemp: return currentTemperatureCount;
    case uniHumidity: return currentHumidityCount;
    case uniLuminosity: return currentLuminosityCount;
    case uniSoilMoisture: return currentSoilMoistureCount;
    case uniPH: return currentPHCount;
  }

  return 0;  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t UniRegDispatcher::GetHardCodedSensorsCount(UniSensorType type)
{
  switch(type)
  {
    case uniNone: return 0;
    case uniTemp: return hardCodedTemperatureCount;
    case uniHumidity: return hardCodedHumidityCount;
    case uniLuminosity: return hardCodedLuminosityCount;
    case uniSoilMoisture: return hardCodedSoilMoistureCount;
    case uniPH: return hardCodedPHCount;
  }

  return 0;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRegDispatcher::Setup()
{
    temperatureModule = MainController->GetModuleByID(F("STATE"));
    if(temperatureModule)
      hardCodedTemperatureCount = temperatureModule->State.GetStateCount(StateTemperature);
    
    humidityModule = MainController->GetModuleByID(F("HUMIDITY"));
    if(humidityModule)
      hardCodedHumidityCount = humidityModule->State.GetStateCount(StateHumidity);
    
    luminosityModule = MainController->GetModuleByID(F("LIGHT"));
    if(luminosityModule)
      hardCodedLuminosityCount = luminosityModule->State.GetStateCount(StateLuminosity);

    soilMoistureModule = MainController->GetModuleByID(F("SOIL"));
    if(soilMoistureModule)
      hardCodedSoilMoistureCount = soilMoistureModule->State.GetStateCount(StateSoilMoisture);

    phModule = MainController->GetModuleByID(F("PH"));
    if(phModule)
      hardCodedPHCount = phModule->State.GetStateCount(StatePH);


    ReadState(); // читаем последнее запомненное состояние
    RestoreState(); // восстанавливаем последнее запомненное состояние
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t UniRegDispatcher::GetRFChannel() // возвращает текущий канал для nRF
{
  rfChannel = MemRead(UNI_SENSOR_INDICIES_EEPROM_ADDR + 4);
  return rfChannel;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRegDispatcher::SetRFChannel(uint8_t channel) // устанавливает канал для nRF
{
  if(rfChannel != channel)
  {
    rfChannel = channel;
    SaveState();
  }
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRegDispatcher::ReadState()
{
  //Тут читаем последнее запомненное состояние по индексам сенсоров
  uint16_t addr = UNI_SENSOR_INDICIES_EEPROM_ADDR;
  uint8_t val = MemRead(addr++);
  if(val != 0xFF)
    currentTemperatureCount = val;

  val = MemRead(addr++);
  if(val != 0xFF)
    currentHumidityCount = val;

  val = MemRead(addr++);
  if(val != 0xFF)
    currentLuminosityCount = val;

  val = MemRead(addr++);
  if(val != 0xFF)
    currentSoilMoistureCount = val;

  val = MemRead(addr++);
  if(val != 0xFF)
    rfChannel = val;

  val = MemRead(addr++);
  if(val != 0xFF)
    currentPHCount = val;
   
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRegDispatcher::RestoreState()
{
  //Тут восстанавливаем последнее запомненное состояние индексов сенсоров.
  // добавляем новые датчики в нужный модуль до тех пор, пока
  // их кол-во не сравняется с сохранённым последним выданным индексом.
  // индексы универсальным датчикам выдаются, начиная с 0, при этом данный индекс является
  // виртуальным, поэтому нам всегда надо добавить датчик в конец
  // списка, после жёстко указанных в прошивке датчиков. Такой подход
  // обеспечит нормальную работу универсальных датчиков вне зависимости
  // от настроек прошивки.
  
  if(temperatureModule)
  {
    uint8_t cntr = 0;    
    while(cntr < currentTemperatureCount)
    {
      temperatureModule->State.AddState(StateTemperature, hardCodedTemperatureCount + cntr);
      cntr++;
    }
    
  } // if(temperatureModule)

  if(humidityModule)
  {
    uint8_t cntr = 0;
    while(cntr < currentHumidityCount)
    {
      humidityModule->State.AddState(StateTemperature, hardCodedHumidityCount + cntr);
      humidityModule->State.AddState(StateHumidity, hardCodedHumidityCount + cntr);
      cntr++;
    }
    
  } // if(humidityModule)

 if(luminosityModule)
  {
    uint8_t cntr = 0;
    while(cntr < currentLuminosityCount)
    {
      luminosityModule->State.AddState(StateLuminosity, hardCodedLuminosityCount + cntr);
      cntr++;
    }
    
  } // if(luminosityModule)  

if(soilMoistureModule)
  {
    uint8_t cntr = 0;
    while(cntr < currentSoilMoistureCount)
    {
      soilMoistureModule->State.AddState(StateSoilMoisture, hardCodedSoilMoistureCount + cntr);
      cntr++;
    }
    
  } // if(soilMoistureModule) 

 if(phModule)
  {
    uint8_t cntr = 0;
    while(cntr < currentPHCount)
    {
      phModule->State.AddState(StatePH, hardCodedPHCount + cntr);
      cntr++;
    }
    
  } // if(phModule)  

 // Что мы сделали? Мы добавили N виртуальных датчиков в каждый модуль, основываясь на ранее сохранённой информации.
 // в результате в контроллере появились датчики с показаниями <нет данных>, и показания с них обновятся, как только
 // поступит информация от них с универсальных модулей.
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRegDispatcher::SaveState()
{
  //Тут сохранение текущего состояния в EEPROM
  uint16_t addr = UNI_SENSOR_INDICIES_EEPROM_ADDR;  
  MemWrite(addr++,currentTemperatureCount);
  MemWrite(addr++,currentHumidityCount);
  MemWrite(addr++,currentLuminosityCount);
  MemWrite(addr++,currentSoilMoistureCount);
  MemWrite(addr++,rfChannel);
  MemWrite(addr++,currentPHCount);
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniRegDispatcher::GetRegisteredStates(UniSensorType type, uint8_t sensorIndex, UniSensorState& resultStates)
{
  resultStates.State1 = NULL;
  resultStates.State2 = NULL;
  
   // смотрим тип сенсора, получаем состояния
   switch(type)
   {
    case uniNone: return false;
    
    case uniTemp: 
    {
        if(!temperatureModule)
          return false; // нет модуля температур в прошивке

       // получаем состояние. Поскольку индексы виртуальных датчиков у нас относительные, то прибавляем
       // к индексу датчика кол-во жёстко прописанных в прошивке. В результате получаем абсолютный индекс датчика в системе.
       resultStates.State1 = temperatureModule->State.GetState(StateTemperature,hardCodedTemperatureCount + sensorIndex);

       return (resultStates.State1 != NULL);
       
    }
    break;
    
    case uniHumidity: 
    {
        if(!humidityModule)
          return false; // нет модуля влажности в прошивке

       resultStates.State1 = humidityModule->State.GetState(StateHumidity,hardCodedHumidityCount + sensorIndex);
       resultStates.State2 = humidityModule->State.GetState(StateTemperature,hardCodedHumidityCount + sensorIndex);

       return (resultStates.State1 != NULL && resultStates.State2 != NULL);

    }
    break;
    
    case uniLuminosity: 
    {
        if(!luminosityModule)
          return false; // нет модуля освещенности в прошивке

       resultStates.State1 = luminosityModule->State.GetState(StateLuminosity,hardCodedLuminosityCount + sensorIndex);
       return (resultStates.State1 != NULL);      
    }
    break;
    
    case uniSoilMoisture: 
    {
        if(!soilMoistureModule)
          return false; // нет модуля влажности почвы в прошивке

       resultStates.State1 = soilMoistureModule->State.GetState(StateSoilMoisture,hardCodedSoilMoistureCount + sensorIndex);
       return (resultStates.State1 != NULL);
      
    }
    break;

    case uniPH:
    {
        if(!phModule)
          return false; // нет модуля pH в прошивке

       resultStates.State1 = phModule->State.GetState(StatePH,hardCodedPHCount + sensorIndex);
       return (resultStates.State1 != NULL);
      
    }
    break;
   } // switch

  return false;    
 
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t UniRegDispatcher::GetControllerID()
{
  return MainController->GetSettings()->GetControllerID(); 
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
// UniScratchpadClass
//-------------------------------------------------------------------------------------------------------------------------------------------------------
UniScratchpadClass::UniScratchpadClass()
{
  pin = 0;
  scratchpad = NULL;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniScratchpadClass::canWork()
{
  return (pin > 0 && scratchpad != NULL);
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniScratchpadClass::begin(byte _pin,UniRawScratchpad* scratch)
{
  pin = _pin;
  scratchpad = scratch;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniScratchpadClass::read()
{
  if(!canWork())
    return false;
    
    OneWire ow(pin);
    WORK_STATUS.PinMode(pin,INPUT,false);
    
    if(!ow.reset()) { // нет датчика на линии 
      
     #ifdef UNI_DEBUG
      Serial.print(F("NO PRESENCE FOUND ON 1-Wire pin "));
      Serial.println(pin);
     #endif
      return false; 
}

    // теперь читаем скратчпад
    ow.write(0xCC, 1);
    ow.write(UNI_READ_SCRATCHPAD,1); // посылаем команду на чтение скратчпада

    byte* raw = (byte*) scratchpad;
    // читаем скратчпад
    for(uint8_t i=0;i<sizeof(UniRawScratchpad);i++)
      raw[i] = ow.read();
      
    // проверяем контрольную сумму
    bool isCrcGood =  OneWire::crc8(raw, sizeof(UniRawScratchpad)-1) == raw[sizeof(UniRawScratchpad)-1];

   #ifdef UNI_DEBUG
    if(isCrcGood) {
      Serial.print(F("Checksum OK on 1-Wire pin "));
      Serial.println(pin);
    } else {
      Serial.print(F("BAD scratchpad checksum on 1-Wire pin "));
      Serial.println(pin);
      
    }
   #endif

    return isCrcGood;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniScratchpadClass::startMeasure()
{
  if(!canWork())
    return false;
    
    OneWire ow(pin);
    WORK_STATUS.PinMode(pin,INPUT,false);
    
    if(!ow.reset()) // нет датчика на линии
      return false; 

    ow.write(0xCC, 1);
    ow.write(UNI_START_MEASURE,1); // посылаем команду на старт измерений
    
    return ow.reset();
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniScratchpadClass::write()
{
  if(!canWork())
    return false;
    
    OneWire ow(pin);
    WORK_STATUS.PinMode(pin,INPUT,false);
    
  // выставляем ID нашего контроллера
  scratchpad->head.controller_id = UniDispatcher.GetControllerID();
  
  // подсчитываем контрольную сумму и записываем её в последний байт скратчпада
  scratchpad->crc8 = OneWire::crc8((byte*) scratchpad, sizeof(UniRawScratchpad)-1);

  if(!ow.reset()) // нет датчика на линии
    return false; 

  ow.write(0xCC, 1);
  ow.write(UNI_WRITE_SCRATCHPAD,1); // говорим, что хотим записать скратчпад

  byte* raw = (byte*) scratchpad;
  // теперь пишем данные
   for(uint8_t i=0;i<sizeof(UniRawScratchpad);i++)
    ow.write(raw[i]);

   return ow.reset();
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniScratchpadClass::save()
{
  if(!canWork())
    return false;
    
  OneWire ow(pin);
  WORK_STATUS.PinMode(pin,INPUT,false);

  if(!ow.reset())
    return false;
    
  // записываем всё в EEPROM
  ow.write(0xCC, 1);
  ow.write(UNI_SAVE_EEPROM,1);
  delay(100);
   
  return ow.reset();   
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
// UniRegistrationLine
//-------------------------------------------------------------------------------------------------------------------------------------------------------
UniRegistrationLine::UniRegistrationLine(byte _pin)
{
  pin = _pin;

  memset(&scratchpad,0xFF,sizeof(scratchpad));
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniRegistrationLine::IsModulePresent()
{
  // проверяем, есть ли модуль на линии, простой вычиткой скратчпада
  UniScratchpad.begin(pin,&scratchpad);

   return UniScratchpad.read();
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRegistrationLine::CopyScratchpad(UniRawScratchpad* dest)
{
  memcpy(dest,&scratchpad,sizeof(UniRawScratchpad));
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniRegistrationLine::Register()
{
  // регистрируем модуль в системе. Чего там творится в скратчпаде - нас не колышет, это делает конфигуратор: назначает индексы и т.п.

  // однако, в зависимости от типа пакета, нам надо обновить состояние контроллера (например, добавить индексы виртуальных датчиков в систему).
  // это делается всегда, вне зависимости от того, был ранее зарегистрирован модуль или нет - индексы всегда поддерживаются в актуальном
  // состоянии - переназначили мы их или нет. Считаем, что в случаем универсального модуля с датчиками конфигуратор сам правильно расставил
  // все индексы, и нам достаточно только поддержать актуальное состояние индексов у контроллера.

  // подобная настройка при регистрации разных типов модулей может иметь различное поведение, поэтому мы должны работать с разными субъектами
  // такой настройки.

  // получаем клиента
  AbstractUniClient* client = UniFactory.GetClient(&scratchpad);
  
  // просим клиента зарегистрировать модуль в системе, чего он там будет делать - дело десятое.
  client->Register(&scratchpad);

  // теперь мы смело можем писать скратчпад обратно в модуль
  UniScratchpad.begin(pin,&scratchpad);
  
  if(UniScratchpad.write())
    UniScratchpad.save();

}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniRegistrationLine::IsSameScratchpadType(UniRawScratchpad* src)
{
  if(!src)
    return false;

  return (scratchpad.head.packet_type == src->head.packet_type);
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniRegistrationLine::SetScratchpadData(UniRawScratchpad* src)
{
  if(!IsSameScratchpadType(src)) // разные типы пакетов в переданном скратчпаде и вычитанном, нельзя копировать
    return false;

  memcpy(&scratchpad,src,sizeof(UniRawScratchpad));
  return true;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_NRF_GATE
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#include <RF24.h>
//-------------------------------------------------------------------------------------------------------------------------------------------------------
RF24 radio(NRF_CE_PIN,NRF_CSN_PIN);
uint64_t controllerStatePipe = 0xF0F0F0F0E0LL; // труба, в которую  мы пишем состояние контроллера
// трубы, которые мы слушаем на предмет показаний с датчиков
const uint64_t readingPipes[5] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0E2LL, 0xF0F0F0F0E3LL, 0xF0F0F0F0E4LL, 0xF0F0F0F0E5LL };
#define PAYLOAD_SIZE 30 // размер нашего пакета
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef NRF_DEBUG
int serial_putc( char c, FILE * ) {
  Serial.write( c );
  return c;
}

void printf_begin(void) {
  fdevopen( &serial_putc, 0 );
  Serial.println(F("Init nRF..."));
}
#endif // NRF_DEBUG
//-------------------------------------------------------------------------------------------------------------------------------------------------------
UniNRFGate::UniNRFGate()
{
  bFirstCall = true;
  nRFInited = false;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
bool UniNRFGate::isInOnlineQueue(byte sensorType,byte sensorIndex, byte& result_index)
{
  for(size_t i=0;i<sensorsOnlineQueue.size();i++)
    if(sensorsOnlineQueue[i].sensorType == sensorType && sensorsOnlineQueue[i].sensorIndex == sensorIndex)
    {
      result_index = i;
      return true;
    }
  return false;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniNRFGate::Setup()
{
  #ifdef USE_NRF_REBOOT_PIN
    WORK_STATUS.PinMode(NRF_REBOOT_PIN,OUTPUT);
    WORK_STATUS.PinWrite(NRF_REBOOT_PIN,NRF_POWER_ON);
  #endif
  
  initNRF();
  memset(&packet,0,sizeof(packet));
  ControllerState st = WORK_STATUS.GetState();
  // копируем состояние контроллера к нам
  memcpy(&(packet.state),&st,sizeof(ControllerState));
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniNRFGate::readFromPipes()
{
  if(!nRFInited)
    return;
    
  // открываем все пять труб на прослушку
  for(byte i=0;i<5;i++)
    radio.openReadingPipe(i+1,readingPipes[i]);  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniNRFGate::Update(uint16_t dt)
{
  if(!nRFInited)
    return;

  static uint16_t onlineCheckTimer = 0;
  onlineCheckTimer += dt;
  if(onlineCheckTimer > 5000)
  {
    onlineCheckTimer = 0;

    //Тут, раз в пять секунд - мы должны проверять, не истёк ли интервал
    // получения показаний с датчиков, показания с которых были получены ранее.
    // если интервал истёк - мы должны выставить датчику показания "нет данных"
    // и удалить его из очереди.

    byte count_passes = sensorsOnlineQueue.size();
    byte cur_idx = count_passes-1;

    unsigned long nowTime = millis();

    // проходим от хвоста до головы
    while(count_passes)
    {
      NRFQueueItem* qi = &(sensorsOnlineQueue[cur_idx]);

      // вычисляем интервал в миллисекундах
      unsigned long query_interval = qi->queryInterval*1000;
      
      // смотрим, не истёк ли интервал с момента последнего опроса
      if((nowTime - qi->gotLastDataAt) > (query_interval+3000) )
      {
        
        // датчик не откликался дольше, чем интервал между опросами плюс дельта в 3 секунды,
        // надо ему выставить показания "нет данных"
          byte sType = qi->sensorType;
          byte sIndex = qi->sensorIndex;
        
          UniDispatcher.AddUniSensor((UniSensorType)sType,sIndex);

            // проверяем тип датчика, которому надо выставить "нет данных"
            switch(qi->sensorType)
            {
              case uniTemp:
              {
                // температура
                Temperature t;
                // получаем состояния
                UniSensorState states;
                if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                {
                  if(states.State1)
                    states.State1->Update(&t);
                } // if
              }
              break;

              case uniHumidity:
              {
                // влажность
                Humidity h;
                // получаем состояния
                UniSensorState states;
                if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                {
                  if(states.State1)
                    states.State1->Update(&h);

                  if(states.State2)
                    states.State2->Update(&h);
                } // if                        
              }
              break;

              case uniLuminosity:
              {
                // освещённость
                long lum = NO_LUMINOSITY_DATA;
                // получаем состояния
                UniSensorState states;
                if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                {
                  if(states.State1)
                    states.State1->Update(&lum);
                } // if                        
                
                
              }
              break;

              case uniSoilMoisture: // влажность почвы
              case uniPH: // показания pH
              {
                
                Humidity h;
                // получаем состояния
                UniSensorState states;
                if(UniDispatcher.GetRegisteredStates((UniSensorType)sType,sIndex,states))
                {
                  if(states.State1)
                    states.State1->Update(&h);
                } // if                        
                
              }
              break;
              
            } // switch

        // теперь удаляем оффлайн-датчик из очереди
        sensorsOnlineQueue.pop();
        
      } // if((nowTime
      
      count_passes--;
      cur_idx--;
    } // while
    
   
  } // if onlineCheckTimer

  static uint16_t controllerStateTimer = 0;
  controllerStateTimer += dt;

  // чтобы часто не проверять состояние контроллера
  if(controllerStateTimer > NRF_CONTROLLER_STATE_CHECK_FREQUENCY)
  {
    controllerStateTimer = 0;
    
      // получаем текущее состояние контроллера
      ControllerState st = WORK_STATUS.GetState();
      if(bFirstCall || memcmp(&st,&(packet.state),sizeof(ControllerState)))
      {
        bFirstCall = false;
        // состояние контроллера изменилось, посылаем его в эфир
         memcpy(&(packet.state),&st,sizeof(ControllerState));
         packet.controller_id = UniDispatcher.GetControllerID();
         packet.crc8 = OneWire::crc8((const byte*) &packet,sizeof(packet)-1);
    
         #ifdef NRF_DEBUG
         Serial.println(F("Controller state changed, send it..."));
         #endif // NRF_DEBUG
      
        // останавливаем прослушку
        radio.stopListening();
    
        // пишем наш скратч в эфир
        radio.write(&packet,PAYLOAD_SIZE);
    
        // включаем прослушку
        radio.startListening();
    
        #ifdef NRF_DEBUG
        Serial.println(F("Controller state sent."));
        #endif // NRF_DEBUG
            
      } // if
      
  } // if(controllerStateTimer > NRF_CONTROLLER_STATE_CHECK_FREQUENCY

  // тут читаем данные из труб
  uint8_t pipe_num = 0; // из какой трубы пришло
  if(radio.available(&pipe_num))
  {
     static UniRawScratchpad nrfScratch;
     // читаем скратч
     radio.read(&nrfScratch,PAYLOAD_SIZE);

     #ifdef NRF_DEBUG
      Serial.println(F("Received the scratch via radio..."));
     #endif

     byte checksum = OneWire::crc8((const byte*)&nrfScratch,sizeof(UniRawScratchpad)-1);
     if(checksum == nrfScratch.crc8)
     {
      #ifdef NRF_DEBUG
      Serial.println(F("Checksum OK"));
     #endif

      // проверяем, наш ли пакет
      if(nrfScratch.head.controller_id == UniDispatcher.GetControllerID())
      {
      #ifdef NRF_DEBUG
      Serial.println(F("Packet for us :)"));
      #endif  
          // наш пакет, продолжаем
          AbstractUniClient* client = UniFactory.GetClient(&nrfScratch);
          client->Register(&nrfScratch);
          client->Update(&nrfScratch,true,ssRadio);

          //Тут мы должны для всех датчиков модуля добавить в онлайн-очередь
          // время последнего получения значений и интервал между опросами модуля,
          // если таких данных ещё нету у нас.

              UniSensorsScratchpad* ourScrath = (UniSensorsScratchpad*) &(nrfScratch.data);       
                   
              for(byte i=0;i<MAX_UNI_SENSORS;i++)
              {
                byte type = ourScrath->sensors[i].type;
                if(type == NO_SENSOR_REGISTERED) // нет типа датчика 
                  continue;
            
                UniSensorType ut = (UniSensorType) type;
                
                if(ut == uniNone || ourScrath->sensors[i].index == NO_SENSOR_REGISTERED) // нет типа датчика
                  continue;
            
                // имеем тип датчика, можем проверять, есть ли он у нас в онлайновых
                byte result_index = 0;
                if(isInOnlineQueue(type,ourScrath->sensors[i].index,result_index))
                {
                  // он уже был онлайн, надо сбросить таймер опроса
                  NRFQueueItem* qi = &(sensorsOnlineQueue[result_index]);
                  qi->gotLastDataAt = millis();
                }
                else
                {
                  // датчик не был в онлайн очереди, надо его туда добавить
                  NRFQueueItem qi;
                  qi.sensorType = type;
                  qi.sensorIndex = ourScrath->sensors[i].index;
                  qi.queryInterval = ourScrath->query_interval_min*60 + ourScrath->query_interval_sec;
                  qi.gotLastDataAt = millis();

                  sensorsOnlineQueue.push_back(qi);
                } // else
                
              } // for


          

      #ifdef NRF_DEBUG
      Serial.println(F("Controller data updated."));
      #endif  

      }
      #ifdef NRF_DEBUG
      else 
      {
        Serial.print(F("Unknown controller "));
        Serial.println(nrfScratch.head.controller_id);
      }
      #endif       
       
      
     } // checksum
      #ifdef NRF_DEBUG
      else
      Serial.println(F("Checksum FAIL"));
     #endif
    
    
  } // available
  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniNRFGate::SetChannel(byte channel)
{
  if(!nRFInited)
    return;
    
  radio.stopListening();
  radio.setChannel(channel);
  radio.startListening();
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
int UniNRFGate::ScanChannel(byte channel)
{
  
  if(!nRFInited)
    return -1;

    int level = 0;

    radio.stopListening();
    radio.setAutoAck(
      #ifdef NRF_AUTOACK_INVERTED
        true
      #else
      false
      #endif
      );
    radio.setChannel(channel);   
    radio.startListening();

    for(int i=0;i<1000;i++)
    {
        if(radio.testRPD())
          level++;

         delayMicroseconds(50);
    }

    radio.stopListening();
    radio.setAutoAck(
      #ifdef NRF_AUTOACK_INVERTED
        false
      #else
      true
      #endif
      );
    radio.setChannel(UniDispatcher.GetRFChannel());   
    radio.startListening();

    return level;
    
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void UniNRFGate::initNRF()
{
  #ifdef NRF_DEBUG
  printf_begin();
  #endif
  
  // инициализируем nRF
  nRFInited = radio.begin();

  if(nRFInited)
  {

  WORK_STATUS.PinMode(NRF_CSN_PIN,OUTPUT,false);
  WORK_STATUS.PinMode(NRF_CE_PIN,OUTPUT,false);
  WORK_STATUS.PinMode(MOSI,OUTPUT,false);
  WORK_STATUS.PinMode(MISO,INPUT,false);
  WORK_STATUS.PinMode(SCK,OUTPUT,false);


    delay(200); // чуть-чуть подождём
  
    radio.setDataRate(RF24_1MBPS);
    radio.setPALevel(RF24_PA_MAX);
    radio.setChannel(UniDispatcher.GetRFChannel());
    radio.setRetries(15,15);
    radio.setPayloadSize(PAYLOAD_SIZE); // у нас 30 байт на пакет
    radio.setCRCLength(RF24_CRC_16);
    radio.setAutoAck(
      #ifdef NRF_AUTOACK_INVERTED
        false
      #else
        true
      #endif
      );
  
    // открываем трубу, в которую будем писать состояние контроллера
    radio.openWritingPipe(controllerStatePipe);
  
    // открываем все пять труб на прослушку
    readFromPipes();
  
    radio.startListening(); // начинаем слушать
    
    #ifdef NRF_DEBUG
      radio.printDetails();
    #endif

  } // nRFInited

}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_NRF_GATE
//-------------------------------------------------------------------------------------------------------------------------------------------------------

