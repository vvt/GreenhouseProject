#include "WiFiModule.h"
#include "ModuleController.h"
#include "InteropStream.h"
#include "Memory.h"
//--------------------------------------------------------------------------------------------------------------------------------
#define WIFI_DEBUG_WRITE(s,ca) { Serial.print(String(F("[CA] ")) + String((ca)) + String(F(": ")));  Serial.println((s)); }
#define CHECK_QUEUE_TAIL(v) { if(!actionsQueue.size()) {Serial.println(F("[QUEUE IS EMPTY!]"));} else { if(actionsQueue[actionsQueue.size()-1]!=(v)){Serial.print(F("NOT RIGHT TAIL, WAITING: ")); Serial.print((v)); Serial.print(F(", ACTUAL: "));Serial.println(actionsQueue[actionsQueue.size()-1]); } } }
#define CIPSEND_COMMAND F("AT+CIPSENDBUF=") // F("AT+CIPSEND=")
//--------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
//--------------------------------------------------------------------------------------------------------------------------------
#define MQTT_FILENAME_PATTERN F("MQTT/MQTT.")
#define DEFAULT_MQTT_CLIENT F("greenhouse")
#define REPORT_TOPIC_NAME F("/REPORT")
//--------------------------------------------------------------------------------------------------------------------------------
// MQTTClient
//--------------------------------------------------------------------------------------------------------------------------------
MQTTClient::MQTTClient()
{
  flags.isConnected = false;
  flags.wantToSendConnectPacket = true;
  flags.wantToSendSubscribePacket = true;
  flags.wantToSendReportTopic = false;
  flags.busy = false;

  reportTopicString = NULL;

  reconnectTimer = MQTT_RECONNECT_WAIT;
  updateTopicsTimer = 0;
  currentTopicNumber = 0;
  mqttMessageId = 0;
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::AddTopic(const char* topicIndex, const char* topicName, const char* moduleName, const char* sensorType, const char* sensorIndex, const char* topicType)
{

  #ifdef MQTT_DEBUG
    Serial.print(F("Add topic: "));
    Serial.println(topicName);
    Serial.println(moduleName);
    Serial.println(sensorType);
    Serial.println(sensorIndex);
    Serial.println(topicType);
  #endif
    
  // добавляем новый топик
  String fName = MQTT_FILENAME_PATTERN;
  fName += topicIndex;

  String dirName = F("MQTT");
  SDFat.mkdir(dirName.c_str()); // create directory
  
  SdFile f;
  if(f.open(fName.c_str(),FILE_WRITE | O_TRUNC))
  {
    f.println(topicName); // имя топика
    f.println(moduleName); // имя модуля
    f.println(sensorType); // тип датчика
    f.println(sensorIndex); // индекс датчика
    f.println(topicType); // тип топика
    
    f.close();
  }
  #ifdef MQTT_DEBUG
  else
  {
    Serial.print(F("Unable to create topic file: "));
    Serial.println(fName);
  }
  #endif  
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::DeleteAllTopics()
{
  // удаляем все топики
  FileUtils::RemoveFiles(F("MQTT"));
  currentTopicNumber = 0;
}
//--------------------------------------------------------------------------------------------------------------------------------
byte MQTTClient::GetSavedTopicsCount()
{
      if(!MainController->HasSDCard()) // нет SD-карты, деградируем в жёстко прошитые настройки
        return 0;

        String folderName = F("MQTT");
        return FileUtils::CountFiles(folderName);
        
}
//--------------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::setConnected(bool flag)
{
  #ifdef MQTT_DEBUG
    Serial.print(F("MQTTClient - setConnected: "));
    Serial.println(flag);
  #endif
  
  flags.isConnected = flag;
  flags.wantToSendConnectPacket = flags.isConnected; // если законнекчены - надо послать первым пакет авторизации
  flags.wantToSendSubscribePacket = flags.isConnected; // если законнекчены - надо послать после пакета авторизации пакет с подпиской на топики
  flags.reconnectTimerEnabled = !flags.isConnected;      

  if(flags.reconnectTimerEnabled)
    reconnectTimer = 0;

  if(flags.isConnected)
    updateTopicsTimer = 0;

    flags.busy = false;
    
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::packetWriteError()
{
  #ifdef MQTT_DEBUG
    Serial.println(F("MQTTClient - error write packet!"));
  #endif

  flags.busy = false;
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::packetWriteSuccess()
{
  #ifdef MQTT_DEBUG
    Serial.println(F("MQTTClient - packet written."));
  #endif 

   flags.busy = false;
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::process(MQTTBuffer& packet) // process incoming packet
{
  size_t dataLen = packet.size();

  #ifdef MQTT_DEBUG
    Serial.print(F("MQTTClient - PACKET RECEIVED: "));
    for(size_t i=0;i<dataLen;i++)
    {
      Serial.print(packet[i],HEX);
      Serial.print(' ');
    }
    Serial.println();
  #endif


  if(dataLen > 0)
  {

    uint8_t bCommand = packet[0];
    if((bCommand & MQTT_PUBLISH_COMMAND) == MQTT_PUBLISH_COMMAND)
    {
      // это к нам опубликовали топик
      #ifdef MQTT_DEBUG
        Serial.println(F("PUBLISH topic found!!!"));
      #endif

      bool isQoS1 = (bCommand & 6) == MQTT_QOS1;

      // декодируем длину сообщения
      
        unsigned long multiplier = 1;
        int remainingLength = 0;
        unsigned int curReadPos = 1;
        uint8_t encodedByte;
        
        do
        {
          encodedByte =  packet[curReadPos];
          curReadPos++;
          
          remainingLength += (encodedByte & 127) * multiplier;
          multiplier *= 128;
          
        if (multiplier > 0x200000)
          break; // malformed
          
        } while ((encodedByte & 128) != 0);


      #ifdef MQTT_DEBUG
        Serial.print(F("Remaining length: "));
        Serial.println(remainingLength);
      #endif

      if(curReadPos >= dataLen) // malformed
      {
        #ifdef MQTT_DEBUG
          Serial.println(F("MALFORMED 1"));
        #endif
        return;
      }

      // теперь получаем имя топика
      uint8_t topicLengthMSB = packet[curReadPos];    
      curReadPos++;

      if(curReadPos >= dataLen) // malformed
      {
        #ifdef MQTT_DEBUG
          Serial.println(F("MALFORMED 2"));
        #endif
        return;
      }
            
      uint8_t topicLengthLSB = packet[curReadPos];
      curReadPos++;

      uint16_t topicLength = (topicLengthMSB<<8)+topicLengthLSB;
      
      #ifdef MQTT_DEBUG
        Serial.print(F("Topic length: "));
        Serial.println(topicLength);
      #endif


      // теперь собираем топик
      String topic;
      for(uint16_t j=0;j<topicLength;j++)
      {
        if(curReadPos >= dataLen) // malformed
        {
          #ifdef MQTT_DEBUG
            Serial.println(F("MALFORMED 3"));
          #endif
          return;
        }        
        topic += (char) packet[curReadPos];
        curReadPos++;
      }

      // тут работаем с payload, склеивая его с топиком
      if(isQoS1)
      {
       // игнорируем ID сообщения
       curReadPos += 2; // два байта на ID сообщения
      }


      String* payload = new String();

      for(size_t p=curReadPos;p<dataLen;p++)
      {
        (*payload) += (char) packet[p];
      }

      if(payload->length())
      {
          #ifdef MQTT_DEBUG
            Serial.print(F("Payload are: "));
            Serial.println(*payload);
          #endif

          // теперь склеиваем payload с топиком
          if(topic.length() && topic[topic.length()-1] != '/')
          {
            if((*payload)[0] != '/')
              topic += '/';
          }

          topic += *payload;
      }
      
      delete payload;
      
      if(topic.length())
      {
           #ifdef MQTT_DEBUG
            Serial.print(F("Topic are: "));
            Serial.println(topic);
          #endif

          const char* setCommandPtr = strstr_P(topic.c_str(),(const char*) F("SET/") );
          const char* getCommandPtr = strstr_P(topic.c_str(),(const char*) F("GET/") );
          bool isSetCommand = setCommandPtr != NULL;
          bool isGetCommand = getCommandPtr != NULL;

          if(isSetCommand || isGetCommand)
          {
            const char* normalizedTopic = isSetCommand ? setCommandPtr : getCommandPtr;

            // нашли команду SET или GET, перемещаемся за неё
            normalizedTopic += 4;

            // удаляем ненужные префиксы
            topic.remove(0,normalizedTopic - topic.c_str() );
          
            for(unsigned int k=0;k<topic.length();k++)
            {
              if(topic[k] == '/')
                topic[k] = '|';             
            } // for

             #ifdef MQTT_DEBUG
              Serial.print(F("Normalized topic are: "));
              Serial.println(topic);
            #endif     

            // тут мы имеем команду на выполнение - выполняем её
            ModuleInterop.QueryCommand(isSetCommand ? ctSET : ctGET , topic, false);

            // тут получаем ответ от контроллера, и выставляем флаг, что нам надо опубликовать топик ответа
            flags.wantToSendReportTopic = true;
            delete reportTopicString;
                        
            reportTopicString = new String();

            #ifdef MQTT_REPORT_AS_JSON

              convertAnswerToJSON(PublishSingleton.Text,reportTopicString);
            /*
              // тут мы должны сформировать объект JSON из ответа, для этого надо разбить ответ по разделителям, и для каждого параметра создать именованное поле
              // в анонимном JSON-объекте
              // прикинем, сколько нам памяти надо резервировать, чтобы вместиться
              int neededJsonLen = 3; // {} - под скобки и завершающий ноль
              // считаем кол-во параметров ответа
              int jsonParamsCount=1; // всегда есть один ответ
              int answerLen = PublishSingleton.Text.length();
              
              for(int j=0;j<answerLen;j++)
              {
                if(PublishSingleton.Text[j] == '|') // разделитель
                  jsonParamsCount++;
              }
              // у нас есть количество параметров, под каждый параметр нужно минимум 6 символов ("p":""), плюс длина числа, которое будет как имя
              // параметра, плюс длина самого параметра, плюс запятые между параметрами
              int paramNameCharsCount = jsonParamsCount > 9 ? 2 : 1;

               neededJsonLen += (6 + paramNameCharsCount)*jsonParamsCount + (jsonParamsCount-1) + PublishSingleton.Text.length();

               // теперь можем резервировать память
               reportTopicString->reserve(neededJsonLen);

               // теперь формируем наш JSON-объект
               *reportTopicString = '{'; // начали объект

                if(answerLen > 0)
                {
                   int currentParamNumber = 1;

                   *reportTopicString += F("\"p");
                   *reportTopicString += currentParamNumber;
                   *reportTopicString += F("\":\"");
                   
                   for(int j=0;j<answerLen;j++)
                   {
                     if(PublishSingleton.Text[j] == '|')
                     {
                       // достигли нового параметра, закрываем предыдущий и формируем новый
                       currentParamNumber++;
                       *reportTopicString += F("\",\"p");
                       *reportTopicString += currentParamNumber;
                       *reportTopicString += F("\":\"");
                     }
                     else
                     {
                        char ch = PublishSingleton.Text[j];
                        
                        if(ch == '"' || ch == '\\')
                          *reportTopicString += '\\'; // экранируем двойные кавычки и обратный слеш
                          
                        *reportTopicString += ch;
                     }
                   } // for

                   // закрываем последний параметр
                   *reportTopicString += '"';
                } // answerLen > 0

               *reportTopicString += '}'; // закончили объект

              */
            #else // ответ как есть, в виде RAW
              *reportTopicString = PublishSingleton.Text;
            #endif
            
          } // if(isSetCommand || isGetCommand)
          #ifdef MQTT_DEBUG
          else // unsupported topic
          {
              Serial.print(F("Unsupported topic: "));
              Serial.println(topic);
          } // else
          #endif                
          
      } // if(topic.length())
      #ifdef MQTT_DEBUG
      else
      {
        Serial.println(F("Malformed topic name!!!"));
      }
      #endif      

    } // if((bCommand & MQTT_PUBLISH_COMMAND) == MQTT_PUBLISH_COMMAND)
    
  } // if(dataLen > 0)

  
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::reloadSettings()
{
  intervalBetweenTopics = MemRead(MQTT_INTERVAL_BETWEEN_TOPICS_ADDRESS);
  
  if(!intervalBetweenTopics || intervalBetweenTopics == 0xFF)
    intervalBetweenTopics = 10; // 10 секунд по умолчанию на публикацию между топиками
      
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::init()
{
  
  flags.isConnected = false;
  flags.wantToSendConnectPacket = true;
  flags.wantToSendSubscribePacket = true;
  reconnectTimer = MQTT_RECONNECT_WAIT;
  flags.reconnectTimerEnabled = false;
  flags.wantToSendReportTopic = false;
  updateTopicsTimer = 0;
  flags.busy = false;
  currentTopicNumber = 0;
  mqttMessageId = 0;

  reloadSettings();
  
}
//--------------------------------------------------------------------------------------------------------------------------------
bool MQTTClient::enabled()
{
  if(!MainController->HasSDCard()) // нет SD
    return false;

  // проверяем, выключен ли клиент MQTT в настройках
  byte en = MemRead(MQTT_ENABLED_FLAG_ADDRESS);
  if(en == 0xFF)
    en = 0;

  if(!en)
    return false; // выключены в настройках, не надо ничего делать
    
  return true;
}
//--------------------------------------------------------------------------------------------------------------------------------
bool MQTTClient::connected() // возвращает статус соединения
{
  return flags.isConnected;
}
//--------------------------------------------------------------------------------------------------------------------------------
bool MQTTClient::canConnect() // проверяет - можно ли соединяться с сервером?
{
  if(flags.busy)
    return false;
    
  return reconnectTimer >= MQTT_RECONNECT_WAIT;
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::connecting() // вызывается при начале соединения с сервером
{
  #ifdef MQTT_DEBUG
    Serial.println(F("MQTTClient - connecting..."));
  #endif    
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::convertAnswerToJSON(const String& answer, String* resultBuffer)
{
  // тут мы должны сформировать объект JSON из ответа, для этого надо разбить ответ по разделителям, и для каждого параметра создать именованное поле
  // в анонимном JSON-объекте
  // прикинем, сколько нам памяти надо резервировать, чтобы вместиться
  int neededJsonLen = 3; // {} - под скобки и завершающий ноль
  // считаем кол-во параметров ответа
  int jsonParamsCount=1; // всегда есть один ответ
  int answerLen = answer.length();
  
  for(int j=0;j<answerLen;j++)
  {
    if(answer[j] == '|') // разделитель
      jsonParamsCount++;
  }
  // у нас есть количество параметров, под каждый параметр нужно минимум 6 символов ("p":""), плюс длина числа, которое будет как имя
  // параметра, плюс длина самого параметра, плюс запятые между параметрами
  int paramNameCharsCount = jsonParamsCount > 9 ? 2 : 1;

   neededJsonLen += (6 + paramNameCharsCount)*jsonParamsCount + (jsonParamsCount-1) + answer.length();

   // теперь можем резервировать память
   resultBuffer->reserve(neededJsonLen);

   // теперь формируем наш JSON-объект
   *resultBuffer = '{'; // начали объект

    if(answerLen > 0)
    {
       int currentParamNumber = 1;

       *resultBuffer += F("\"p");
       *resultBuffer += currentParamNumber;
       *resultBuffer += F("\":\"");
       
       for(int j=0;j<answerLen;j++)
       {
         if(answer[j] == '|')
         {
           // достигли нового параметра, закрываем предыдущий и формируем новый
           currentParamNumber++;
           *resultBuffer += F("\",\"p");
           *resultBuffer += currentParamNumber;
           *resultBuffer += F("\":\"");
         }
         else
         {
            char ch = answer[j];
            
            if(ch == '"' || ch == '\\')
              *resultBuffer += '\\'; // экранируем двойные кавычки и обратный слеш
              
            *resultBuffer += ch;
         }
       } // for

       // закрываем последний параметр
       *resultBuffer += '"';
    } // answerLen > 0

   *resultBuffer += '}'; // закончили объект  
}
//--------------------------------------------------------------------------------------------------------------------------------
bool MQTTClient::wantToSay(String& mqttBuffer,int& mqttBufferLength) // проверяет - есть ли что к публикации? Если есть - публикует в переданные параметры
{
  if(!connected() || !enabled())
    return false;

  if(flags.busy) // чем-то заняты, не можем ничего делать
    return false;

    if(flags.wantToSendConnectPacket) // после соединения первым делом мы должны отослать пакет CONNECT - если это так, делаем
    {
      flags.wantToSendConnectPacket = false;
      flags.busy = true;
      
      // Тут читаем настройки с SD
      String mqttSettingsFileName = F("mqtt.ini");

      String mqttClientId = DEFAULT_MQTT_CLIENT; // клиент по умолчанию
      String mqttUser, mqttPass;

      SdFile f;
      if(f.open(mqttSettingsFileName.c_str(),FILE_READ))
      {
        // первые две строки пропускаем, там адрес сервера и порт        
        FileUtils::readLine(f,mqttClientId);
        FileUtils::readLine(f,mqttClientId);

        mqttClientId = "";
        // в третьей строке - ID клиента
        FileUtils::readLine(f,mqttClientId);

        if(!mqttClientId.length())
          mqttClientId = DEFAULT_MQTT_CLIENT;

        // в четвёртой - пользователь
        FileUtils::readLine(f,mqttUser);
        
        // в пятой - пароль
        FileUtils::readLine(f,mqttPass);
        
        f.close();
      } // if(f)
      
      constructConnectPacket(mqttBuffer,mqttBufferLength,
        mqttClientId.c_str() // client id
      , mqttUser.length() ? mqttUser.c_str() : NULL // user
      , mqttPass.length() ? mqttPass.c_str() : NULL // pass
      , NULL // will topic
      , 0 // willQoS
      , 0 // willRetain
      , NULL // will message
      );
      
    #ifdef MQTT_DEBUG
      Serial.println(F("MQTTClient - send connect packet: "));
      for(int i=0;i<mqttBufferLength;i++)
      {
        Serial.print(mqttBuffer[i],HEX);
        Serial.print(' ');
      }
      Serial.println();
    #endif 

      return true; // пакет с авторизацией сформирован, выходим
      
    } // if(flags.wantToSendConnectPacket)
    else if(flags.wantToSendSubscribePacket) // надо послать пакет с подпиской на топики
    {
    
      flags.wantToSendSubscribePacket = false;

       // Тут читаем настройки с SD
      String mqttSettingsFileName = F("mqtt.ini");
      SdFile f;

      if(f.open(mqttSettingsFileName.c_str(),FILE_READ))
      {
          flags.busy = true;

          String mqttClientId;
          // первые две строки пропускаем, там адрес сервера и порт        
          FileUtils::readLine(f,mqttClientId);
          FileUtils::readLine(f,mqttClientId);

          mqttClientId = "";
          // в третьей строке - ID клиента
          FileUtils::readLine(f,mqttClientId);

          if(!mqttClientId.length())
            mqttClientId = DEFAULT_MQTT_CLIENT;

          // мы прочитали ID клиента, теперь мы можем подписаться на все топики для него.
          // для этого делаем топик, прибавляя к ID клиента многоуровневую маску:
          mqttClientId += F("/#"); 
          // теперь все приходящие команды вида clientId/SET/WATER/ON - будут очень просто транслироваться в наши внутренние команды
         
           // конструируем пакет подписки
           constructSubscribePacket(mqttBuffer,mqttBufferLength,mqttClientId.c_str());
    
          #ifdef MQTT_DEBUG
            Serial.println(F("MQTTClient - subscribe packet are: "));
            for(int i=0;i<mqttBufferLength;i++)
            {
              Serial.print(mqttBuffer[i],HEX);
              Serial.print(' ');
            }
            Serial.println();
          #endif         
    
          f.close();
          
          return true;
      } // if(f)
      return false; // no settings file !!!
      
    } // flags.wantToSendSubscribePacket
    else 
    if(flags.wantToSendReportTopic) // надо отослать топик со статусом обработки команды
    {
      flags.wantToSendReportTopic = false;
      
      // Тут читаем настройки с SD
      String mqttSettingsFileName = F("mqtt.ini");
      SdFile f;

      if(f.open(mqttSettingsFileName.c_str(),FILE_READ))
      {
          flags.busy = true;

          String mqttClientId;
          // первые две строки пропускаем, там адрес сервера и порт        
          FileUtils::readLine(f,mqttClientId);
          FileUtils::readLine(f,mqttClientId);

          mqttClientId = "";
          // в третьей строке - ID клиента
          FileUtils::readLine(f,mqttClientId);

          if(!mqttClientId.length())
            mqttClientId = DEFAULT_MQTT_CLIENT;

          mqttClientId += REPORT_TOPIC_NAME;

           // конструируем пакет публикации о статусе отработки команды
           constructPublishPacket(mqttBuffer,mqttBufferLength,mqttClientId.c_str(), reportTopicString->c_str());

           delete reportTopicString;
           reportTopicString = NULL;      
    
          f.close();
          
          return true;
      } // if(f)
      return false; // no settings file !!!
      
    } // flags.wantToSendReportTopic
    
    else // режим с отсылкой топиков
    {
      if(flags.haveTopics) // смотрим, надо ли отправлять топики?
      {

        //Тут читаем данные текущего топика с SD
        
        String topicFileName = MQTT_FILENAME_PATTERN;
        topicFileName += String(currentTopicNumber);

        if(!SDFat.exists(topicFileName.c_str())) // нет топика
        {
          currentTopicNumber = 0; // переключаемся на первый топик
          flags.haveTopics = false;
          return false;
        }

        // тут можем читать из файла настроек топика
        SdFile f;
        
        if(!f.open(topicFileName.c_str(),FILE_READ)) // не получилось открыть файл
        {
          switchToNextTopic();
          return false;          
        }

        // теперь читаем настройки топика
        // первой строкой идёт имя топика
        String topicName;
        FileUtils::readLine(f,topicName);

        // второй строкой - идёт имя модуля, в котором взять нужные показания
        String moduleName;
        FileUtils::readLine(f,moduleName);

        // в третьей строке - тип датчика, числовое значение соответствует перечислению ModuleStates
        String sensorTypeString;
        FileUtils::readLine(f,sensorTypeString);
        ModuleStates sensorType = (ModuleStates) sensorTypeString.toInt();

        // в четвёртой строке - индекс датчика в модуле
        String sensorIndexString;
        FileUtils::readLine(f,sensorIndexString);
        int sensorIndex = sensorIndexString.toInt();

        // в пятой строке - тип топика: показания с датчиков (0), или статус контроллера (1).
        // в случае статуса контроллера во второй строке - команда, которую надо запросить у контроллера
        String topicType;
        FileUtils::readLine(f,topicType);
        
        
        // не забываем закрыть файл
        f.close();

        if(topicType == F("1")) // топик со статусом контроллера
        {

          
          #ifdef MQTT_DEBUG
            Serial.println(F("Status topic found - process command..."));
          #endif
          
          
           //Тут работаем с топиком статуса контроллера
           String data; 

            // тут тонкость - команда у нас с изменёнными параметрами, где все разделители заменены на символ @
            // поэтому перед выполнением - меняем назад
            moduleName.replace('@','|');

            ModuleInterop.QueryCommand(ctGET, moduleName, true);

            #ifdef MQTT_REPORT_AS_JSON

              convertAnswerToJSON(PublishSingleton.Text,&data);
               //
            #else // ответ как есть, в виде RAW
              data = PublishSingleton.Text;
            #endif

            flags.busy = true; // ставим флаг занятости
    
             // конструируем пакет публикации
             constructPublishPacket(mqttBuffer,mqttBufferLength,topicName.c_str(), data.c_str()); 
    
             switchToNextTopic();
            
            return true; // нашли и отослали показания












           


           
           /*
           
           switchToNextTopic();
           return false;
           */
          
        } // if
        else // топик с показаниями датчика
        {
            // теперь получаем модуль у контроллера
            AbstractModule* mod = MainController->GetModuleByID(moduleName.c_str());
    
            if(!mod) // не нашли такой модуль
            {
              switchToNextTopic();
              return false;
            }
    
            // получаем состояние
            OneState* os = mod->State.GetState(sensorType,sensorIndex);
    
            if(!os) // нет такого состояния
            {
              switchToNextTopic();
              return false;          
            }
    
            // теперь получаем данные состояния
            String data;
            if(os->HasData()) // данные с датчика есть, можем читать
              data = *os;
            else
              data = "-"; // нет данных с датчика
    
            flags.busy = true; // ставим флаг занятости
    
             // конструируем пакет публикации
             constructPublishPacket(mqttBuffer,mqttBufferLength,topicName.c_str(), data.c_str());
    
            #ifdef MQTT_DEBUG
              Serial.println(F("MQTTClient - post next topic: "));
              for(int i=0;i<mqttBufferLength;i++)
              {
                Serial.print(mqttBuffer[i],HEX);
                Serial.print(' ');
              }
              Serial.println();
            #endif         
    
             switchToNextTopic();
            
            return true; // нашли и отослали показания
             
        } // sensor data topic
        
      } // haveTopics
      
    } // else dont want to send connect packet

    return false;
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::switchToNextTopic()
{
    flags.haveTopics = false; // говорим, что топиков больше нет, до следующего захода           

    // переключаемся на следующий топик
    currentTopicNumber++;
    
    // проверим - не надо ли завернуть на старт?
    String topicFileName = MQTT_FILENAME_PATTERN;
    topicFileName += String(currentTopicNumber);
    if(!SDFat.exists(topicFileName.c_str()))
    {
      currentTopicNumber = 0; // следующего файла нет, начинаем сначала
    }
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::encode(MQTTBuffer& buff,const char* str)
{
  if(!str)
    return;

    size_t sz = buff.size(); // запоминаем текущий размер

    // записываем нули, как длину строки, потом мы это поправим
    buff.push_back(0);
    buff.push_back(0);

    const char* ptr = str;
    int strLen = 0;
    while(*ptr)
    {
      buff.push_back(*ptr++);
      strLen++;
    }

    // теперь записываем актуальную длину
    buff[sz] = (strLen >> 8);
    buff[sz+1] = (strLen & 0xFF);
    
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::constructSubscribePacket(String& mqttBuffer,int& mqttBufferLength, const char* topic)
{
 MQTTBuffer byteBuffer; // наш буфер из байт, в котором будет содержаться пакет

  // тут формируем пакет подписки

  // сначала записываем ID сообщения
  mqttMessageId++;
  
  if(!mqttMessageId)
    mqttMessageId = 1;
    
  byteBuffer.push_back((mqttMessageId >> 8));
  byteBuffer.push_back((mqttMessageId & 0xFF));

  // кодируем топик, на который подписываемся
  encode(byteBuffer,topic);

  // теперь пишем байт QoS
  byteBuffer.push_back(1);

  size_t payloadSize = byteBuffer.size();

  MQTTBuffer fixedHeader;
  
  constructFixedHeader(MQTT_SUBSCRIBE_COMMAND | MQTT_QOS1, fixedHeader, payloadSize);

  writePacket(fixedHeader,byteBuffer,mqttBuffer,mqttBufferLength);  
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::constructPublishPacket(String& mqttBuffer,int& mqttBufferLength, const char* topic, const char* payload)
{

  MQTTBuffer byteBuffer; // наш буфер из байт, в котором будет содержаться пакет

  // тут формируем пакет

  // кодируем топик
  encode(byteBuffer,topic);

  // теперь пишем данные топика
  int sz = strlen(payload);
  const char* readPtr = payload;
  for(int i=0;i<sz;i++)
  {
    byteBuffer.push_back(*readPtr++);
  }   

  size_t payloadSize = byteBuffer.size();

  MQTTBuffer fixedHeader;
  
  constructFixedHeader(MQTT_PUBLISH_COMMAND,fixedHeader,payloadSize);

  writePacket(fixedHeader,byteBuffer,mqttBuffer,mqttBufferLength);
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::constructFixedHeader(byte command, MQTTBuffer& fixedHeader, size_t payloadSize)
{
    fixedHeader.push_back(command); // пишем тип команды
  
    uint8_t remainingLength[4];
    uint8_t digit;
    uint8_t written = 0;
    uint16_t len = payloadSize;
    
    do 
    {
        digit = len % 128;
        len = len / 128;
        if (len > 0) 
        {
            digit |= 0x80;
        }
        
        remainingLength[written++] = digit;
        
    } while(len > 0);

    // мы записали written символов, как длину переменного заголовка - теперь пишем эти байты в фиксированный
    
    for(uint8_t i=0;i<written;i++)
    {
      fixedHeader.push_back(remainingLength[i]);
    }

}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::constructConnectPacket(String& mqttBuffer,int& mqttBufferLength,const char* id, const char* user, const char* pass
,const char* willTopic,uint8_t willQoS, uint8_t willRetain, const char* willMessage)
{
  mqttBuffer = "";

  MQTTBuffer byteBuffer; // наш буфер из байт, в котором будет содержаться пакет

  // теперь формируем переменный заголовок

  // переменный заголовок, для команды CONNECT
  byteBuffer.push_back(0);
  byteBuffer.push_back(6); // длина версии протокола MQTT
  byteBuffer.push_back('M');
  byteBuffer.push_back('Q');
  byteBuffer.push_back('I');
  byteBuffer.push_back('s');
  byteBuffer.push_back('d');
  byteBuffer.push_back('p');

  byteBuffer.push_back(3); // версия протокола - 3

  // теперь рассчитываем флаги
  byte flags = 0;

  if(willTopic)
    flags = 0x06 | (willQoS << 3) | (willRetain << 5);
  else
    flags = 0x02;

  if(user) // есть имя пользователя
    flags |= (1 << 7);

  if(pass) // есть пароль
    flags |= (1 << 6);
  
   byteBuffer.push_back(flags);

   // теперь смотрим настройки keep-alive
   int keepAlive = 60; // 60 секунд
   byteBuffer.push_back((keepAlive >> 8));
   byteBuffer.push_back((keepAlive & 0xFF));

   // теперь записываем payload, для этого каждую строку надо закодировать
   encode(byteBuffer,id);
   encode(byteBuffer,willTopic);
   encode(byteBuffer,willMessage);
   encode(byteBuffer,user);
   encode(byteBuffer,pass);

   // теперь мы имеем буфер переменной длины, нам надо подсчитать его длину, сворфировать фиксированный заголовок,
   // и сохранить всё в буфере
   size_t payloadSize = byteBuffer.size();
   MQTTBuffer fixedHeader;
   constructFixedHeader(MQTT_CONNECT_COMMAND,fixedHeader,payloadSize);

   writePacket(fixedHeader,byteBuffer,mqttBuffer,mqttBufferLength);


   // всё, пакет сформирован
    
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::writePacket(MQTTBuffer& fixedHeader, MQTTBuffer& payload, String& mqttBuffer,int& mqttBufferLength)
{
  mqttBuffer = "";
  
// запомнили, сколько байт надо послать в ESP
   mqttBufferLength = fixedHeader.size() + payload.size();

   // теперь записываем это в строку, перед этим зарезервировав память, и заполнив строку пробелами
   mqttBuffer.reserve(mqttBufferLength);
   for(int i=0;i<mqttBufferLength;i++)
    mqttBuffer += ' ';

  // теперь можем копировать данные в строку побайтово
  int writePos = 0;

  // пишем фиксированный заголовок
  for(size_t i=0;i<fixedHeader.size();i++)
  {
    mqttBuffer[writePos++] = fixedHeader[i];
  }
  
  // и переменный
  for(size_t i=0;i<payload.size();i++)
  {
    mqttBuffer[writePos++] = payload[i];
  }  
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::getMQTTServer(String& host,int& port)
{
  // Тут читаем настройки MQTT-сервера с SD
  
  String mqttSettingsFileName = F("mqtt.ini");
  SdFile f;

  if(!f.open(mqttSettingsFileName.c_str(),FILE_READ))
  {
    host = F("127.0.0.1");
    port = 1883;
    return;
  }

  FileUtils::readLine(f,host);

  if(!host.length())
    host = F("127.0.0.1");

  String portStr;
  FileUtils::readLine(f,portStr);

  port = portStr.toInt();
  if(!port)
    port = 1883;
  

  f.close();
  
  //host = "192.168.43.50";
  //port = 1883;
}
//--------------------------------------------------------------------------------------------------------------------------------
void MQTTClient::update(uint16_t dt)
{
  if(!enabled()) // неактивны, нечего тут делать
    return;
    
  if(flags.reconnectTimerEnabled)
    reconnectTimer +=  dt;

    if(reconnectTimer >= MQTT_RECONNECT_WAIT)
    {
      flags.reconnectTimerEnabled = false;
    }

    if(connected())
    {
      updateTopicsTimer += dt;
      unsigned long neededInterval = intervalBetweenTopics;
      neededInterval *= 1000; // переводим секунды в миллисекунды
      
      if(updateTopicsTimer > neededInterval)
      {
        updateTopicsTimer = 0;
        
        #ifdef MQTT_DEBUG
          Serial.println(F("MQTTClient - prepare next topic!"));
        #endif

        //Тут проверяем - доступен ли следующий топик ?
        
        String topicFileName = MQTT_FILENAME_PATTERN;
        topicFileName += String(currentTopicNumber);

        flags.haveTopics = SDFat.exists(topicFileName.c_str()); // и выставляем флаг, что топики получены
        if(!flags.haveTopics)
        {
          // нет файла настроек топика, переходим на начало
          currentTopicNumber = 0;
          
        } // if
       
        
      }
      else
        flags.haveTopics = false;
        
    } // connected
}
//--------------------------------------------------------------------------------------------------------------------------------
#endif // USE_WIFI_MODULE_AS_MQTT_CLIENT
//--------------------------------------------------------------------------------------------------------------------------------
#if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
void WiFiModule::SendData(IoTService service,uint16_t dataLength, IOT_OnWriteToStream writer, IOT_OnSendDataDone onDone)
{
    // тут смотрим, можем ли мы обработать запрос на отсыл данных в IoT
    IoTSettings iotSettings = MainController->GetSettings()->GetIoTSettings();

    //#if defined(THINGSPEAK_ENABLED) 
    if(iotSettings.Flags.ThingSpeakEnabled && strlen(iotSettings.ThingSpeakChannelID)) // включен один сервис хотя бы
    {

     // сохраняем указатели на функции обратного вызова
      iotWriter = writer;
      iotDone = onDone;
      iotService = service;

      #ifdef WIFI_DEBUG
        Serial.println(F("Requested to write data to IoT using ESP..."));
      #endif

      // Тут формируем данные для запроса
      switch(service)
      {
         case iotThingSpeak:
         {
          // попросили отослать данные через ThingSpeak
          delete iotDataHeader;
          delete iotDataFooter;
          iotDataHeader = new String();
          iotDataFooter = new String();

          // формируем запрос
          *iotDataHeader = F("GET /update?api_key=");
          *iotDataHeader += iotSettings.ThingSpeakChannelID;
          *iotDataHeader += F("&");

          *iotDataFooter = F(" HTTP/1.1\r\nAccept: */*\r\nUser-Agent: ");
          *iotDataFooter += IOT_USER_AGENT;
          *iotDataFooter += F("\r\nHost: ");
          *iotDataFooter += THINGSPEAK_HOST;
          *iotDataFooter += F("\r\n\r\n");

          // теперь вычисляем, сколько всего данных будет
          iotDataLength = iotDataHeader->length() + iotDataFooter->length() + dataLength;

          // теперь можно добавлять в очередь запрос на обработку. Но ситуация с очередью следующая:
          // мы не знаем, чем сейчас занят ESP, и что у нас в очереди. Мы знаем только, что нельзя разбивать
          // команды wfaCIPSEND и wfaACTUALSEND, поскольку после отработки первой в очередь следом ОБЯЗАНА
          // быть помещена вторая, иначе - аллес, т.к. эти команды используются для отсыла ответа на полученную команду.
          // следовательно, всё, что мы можем сделать - это взвести флаг, что как только очередь освободится - мы должны
          // поместить в неё команду на отсыл данных в IoT.
          flags.wantIoTToProcess = true;
         }
         break;
        
      } // switch
    } // enabled
    else
    {
      // ни одного сервиса не включено
      // тут ничего не можем отсылать, сразу дёргаем onDone, говоря, что у нас не получилось отослать
      onDone({false,service});

    }
}
#endif   
//--------------------------------------------------------------------------------------------------------------------------------
bool WiFiModule::IsKnownAnswer(const String& line)
{
  return ( line == F("OK") || line == F("ERROR") || line == F("FAIL") || line.endsWith(F("SEND OK")) || line.endsWith(F("SEND FAIL")));
}
//--------------------------------------------------------------------------------------------------------------------------------
bool isESPBootFound(const String& line)
{
  return (line == F("ready")) || line.startsWith(F("Ai-Thinker Technology"));
}
//--------------------------------------------------------------------------------------------------------------------------------
void WiFiModule::ProcessAnswerLine(String& line)
{

   flags.isAnyAnswerReceived = true; 
    
  #ifdef WIFI_DEBUG
     WIFI_DEBUG_WRITE(line,currentAction);
  #endif

   // проверяем, не перезагрузился ли модем
  if(isESPBootFound(line) && currentAction != wfaWantReady) // мы проверяем на ребут только тогда, когда сами его не вызвали
  {
    #ifdef WIFI_DEBUG
      WIFI_DEBUG_WRITE(F("ESP boot found, init queue.."),currentAction);
    #endif


     // убеждаемся, что мы вызвали коллбэк для отсыла данных в IoT
     #if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
      EnsureIoTProcessed();
     #endif

     // убеждаемся, что мы обработали HTTP-запрос, пусть и неудачно
     EnsureHTTPProcessed(ERROR_MODEM_NOT_ANSWERING);

     #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
      mqtt.setConnected(false);
     #endif
     
    InitQueue(false); // инициализировали очередь по новой, т.к. модем либо только загрузился, либо - перезагрузился. При этом мы не добавляем команду перезагрузки в очередь
    needToWaitTimer = WIFI_WAIT_BOOT_TIME; // дадим модему ещё 2 секунды на раздупливание

    return;
  } 

  // тут проверяем, законнекчены ли мы к роутеру или нет
  if(line == F("WIFI DISCONNECT"))
  {
    #ifdef WIFI_DEBUG
      WIFI_DEBUG_WRITE(F("Disconnected from router :("),currentAction);
    #endif

    #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
      mqtt.setConnected(false);
    #endif
         
    flags.isConnected = false;
  }
  else
  if(line == F("WIFI CONNECTED"))
  {
    #ifdef WIFI_DEBUG
      WIFI_DEBUG_WRITE(F("Connected to router :)"),currentAction);
    #endif
    flags.isConnected = true;
  }
  

  // здесь может придти ответ от сервера, или - запрос от клиента
  if(
 #if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
    currentAction != wfaActualSendIoTData &&  // если мы не в процессе отсыла данных в IoT
#endif
   
    line.startsWith(F("+IPD")))
  {
    ProcessQuery(line); // разбираем пришедшую команду
  } // if

  
  switch(currentAction)
  {

  //////////////////////////// ЦИКЛ MQTT ////////////////////////////////////////
  #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
    case wfaConnectToMQTT:
    {
      if(IsKnownAnswer(line))
      {
        actionsQueue.pop(); // убираем последнюю обработанную команду
        currentAction = wfaIdle;
        
        if(line == F("OK"))
        {
          mqtt.setConnected(true); // законнектились к серверу
        }
        else
        {
          mqtt.setConnected(false); // ошибка соединения
        }
      }
    }
    break;

    case wfaWriteToMQTT: // запросили отсыл данных к MQTT-брокеру, надо прочекать, что да как
    {
      // ждём > для отсыла данных
        #ifdef MQTT_DEBUG
          WIFI_DEBUG_WRITE(F("MQTT, waiting for \">\"..."),currentAction);
        #endif 

      if(line == F(">")) // дождались приглашения
      {
        #ifdef MQTT_DEBUG
          WIFI_DEBUG_WRITE(F("\">\" FOUND, sending MQTT-packet..."),currentAction);
          CHECK_QUEUE_TAIL(wfaWriteToMQTT);
        #endif 

        actionsQueue.pop(); // убираем последнюю обработанную команду
        actionsQueue.push_back(wfaActualWriteToMQTT); // добавляем команду на актуальный отсыл данных в очередь     
        currentAction = wfaIdle;
        flags.inSendData = true; // выставляем флаг, что мы отсылаем данные, и тогда очередь обработки клиентов не будет чухаться
               
      }
      else
      if(line.indexOf(F("FAIL")) != -1 || line.indexOf(F("ERROR")) != -1)
      {
         // всё плохо 
        #ifdef MQTT_DEBUG
          WIFI_DEBUG_WRITE(F("Error sending MQTT-packet!"),currentAction);
          CHECK_QUEUE_TAIL(wfaWriteToMQTT);
        #endif 
        
        actionsQueue.pop(); // убираем последнюю обработанную команду
        currentAction = wfaIdle; // переходим в ждущий режим

        mqtt.packetWriteError(); // говорим MQTT-клиенту, что не удалось послать пакет

      }
    }
    break;

    case wfaActualWriteToMQTT:
    {
      // мы тут, понимаешь ли, ждём ответа на отсыл данных  в MQTT-брокер.

      // сначала проверяем, не закрыто ли соединение?
       int idx = line.indexOf(F(",CLOSED"));
       
       if(idx != -1) // соединение закрыто сервером по какой-то причине
        {
          // клиент отсоединился
          String s = line.substring(0,idx);
          int clientID = s.toInt();
          if(clientID >= 0 && clientID < MAX_WIFI_CLIENTS)
          {
            // проверяем - не наш ли клиент?
            if(clientID == (MAX_WIFI_CLIENTS - 2))
            {

               #ifdef MQTT_DEBUG
                  WIFI_DEBUG_WRITE(F("MQTT client disconnected!"),currentAction);
               #endif 
                            
                // наш - значит, мы уже весь ответ вычитали, и можем рапортовать о завершении
               mqtt.setConnected(false);
                
              actionsQueue.pop(); // убираем последнюю обработанную команду
              currentAction = wfaIdle;
              flags.inSendData = false; // разрешаем обработку других клиентов         

            } // if
            
          }
        } // if
       else
       {

         #ifdef MQTT_DEBUG
            WIFI_DEBUG_WRITE(F("MQTT - packet sent."),currentAction);
         #endif
                     
         // отослали пакет
         actionsQueue.pop(); // убираем последнюю обработанную команду
         currentAction = wfaIdle;
         flags.inSendData = false; // разрешаем обработку других клиентов

         mqtt.packetWriteSuccess(); // сообщаем, что мы отослали пакет
          
      } // else not closed
    }
    break;
  
    
  #endif
  //////////////////////////// ЦИКЛ MQTT КОНЧИЛСЯ ////////////////////////////////////////
    
    case wfaWantReady:
    {
      // ждём ответа "ready" от модуля
      if(isESPBootFound(line)) // получили
      {
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("[OK] => ESP restarted."),currentAction);
          CHECK_QUEUE_TAIL(wfaWantReady);
       #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду
       currentAction = wfaIdle;
      }
    }
    break;

    case wfaCheckModemHang:
    {
      // проверяли, отвечает ли модем
      if(IsKnownAnswer(line)) 
      {
         #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("[OK] => ESP answered and available."),currentAction);
          CHECK_QUEUE_TAIL(wfaCheckModemHang);
         #endif
         
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = wfaIdle;

         if(flags.wantReconnect)
         {
            flags.wantReconnect = false;
    

            #if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
              EnsureIoTProcessed();
            #endif

            EnsureHTTPProcessed(ERROR_CANT_ESTABLISH_CONNECTION);

            #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
              mqtt.setConnected(false);
            #endif

            GlobalSettings* Settings = MainController->GetSettings();
  
            if(Settings->GetWiFiState() & 0x01) // коннектимся к роутеру
            {
             #ifdef WIFI_DEBUG
              WIFI_DEBUG_WRITE(F("No connection, try to reconnect..."),currentAction);
             #endif
                               
              InitQueue();
              needToWaitTimer = 5000; // попробуем через 5 секунд подконнеститься
            }
         }

      }

      if(line == F("No AP"))
      {
        // никуда не подсоединены, пытаемся переподключиться, как только придёт OK
        flags.wantReconnect = true;
      }
      
    }
    break;    

    case wfaEchoOff: // выключили эхо
    {
      if(IsKnownAnswer(line))
      {
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("[OK] => ECHO OFF processed."),currentAction);
          CHECK_QUEUE_TAIL(wfaEchoOff);
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = wfaIdle;
      }
    }
    break;

    case wfaCWMODE: // перешли в смешанный режим
    {
      if(IsKnownAnswer(line))
      {
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("[OK] => SoftAP mode is ON."),currentAction);
          CHECK_QUEUE_TAIL(wfaCWMODE);
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = wfaIdle;
      }
      
    }
    break;

    case wfaCWSAP: // создали точку доступа
    {
      if(IsKnownAnswer(line))
      {
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("[OK] => access point created."),currentAction);
          CHECK_QUEUE_TAIL(wfaCWSAP);
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = wfaIdle;
      }
      
    }
    break;

    case wfaCIPMODE: // установили режим работы сервера
    {
      if(IsKnownAnswer(line))
      {
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("[OK] => TCP-server mode now set to 0."),currentAction);
          CHECK_QUEUE_TAIL(wfaCIPMODE);
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = wfaIdle;
      }
      
    }
    break;

    case wfaCIPMUX: // разрешили множественные подключения
    {
      if(IsKnownAnswer(line))
      {
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("[OK] => Multiple connections allowed."),currentAction);
          CHECK_QUEUE_TAIL(wfaCIPMUX);
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = wfaIdle;
      }
      
    }
    break;

    case wfaCIPSERVER: // запустили сервер
    {
       if(IsKnownAnswer(line))
      {
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("[OK] => TCP-server started."),currentAction);
          CHECK_QUEUE_TAIL(wfaCIPSERVER);
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = wfaIdle;
      }
     
    }
    break;

    case wfaCWJAP: // законнектились к роутеру
    {
       if(IsKnownAnswer(line))
      {
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("[OK] => connected to the router."),currentAction);
          CHECK_QUEUE_TAIL(wfaCWJAP);
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = wfaIdle;
      }
      
    }
    break;

    case wfaCWQAP: // отсоединились от роутера
    {
      if(IsKnownAnswer(line))
      {
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("[OK] => disconnected from router."),currentAction);
          CHECK_QUEUE_TAIL(wfaCWQAP);
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = wfaIdle;
      }
     
    }
    break;

    //////////////////////////// ЦИКЛ HTTP ////////////////////////////////////////
    case wfaStartHTTPSend: // коннектились к хосту по HTTP
    {
      if(IsKnownAnswer(line))
      {

         #ifdef HTTP_DEBUG
          WIFI_DEBUG_WRITE(F("HTTP connection command done, parse..."),currentAction);
          CHECK_QUEUE_TAIL(wfaStartHTTPSend);
         #endif

        actionsQueue.pop(); // убираем последнюю обработанную команду
        currentAction = wfaIdle;
        
        // один из известных нам ответов?
        if(line == F("OK"))
        {
         #ifdef HTTP_DEBUG
          WIFI_DEBUG_WRITE(F("HTTP connection OK, continue..."),currentAction);
         #endif
          // законнектились, можем посылать данные
          actionsQueue.push_back(wfaStartSendHTTPData); // добавляем команду на актуальный отсыл данных в очередь     
        }
        else
        {
         #ifdef HTTP_DEBUG
          WIFI_DEBUG_WRITE(F("HTTP connection ERROR!"),currentAction);
         #endif

          // всё плохо, вызываем коллбэк
            EnsureHTTPProcessed(ERROR_CANT_ESTABLISH_CONNECTION);
        }
      } // if(IsKnownAnswer(line))      
    }
    break;


    case wfaStartSendHTTPData: // запросили отсыл данных по HTTP, надо прочекать, что да как
    {
      // ждём > для отсыла данных
        #ifdef HTTP_DEBUG
          WIFI_DEBUG_WRITE(F("HTTP, waiting for \">\"..."),currentAction);
        #endif 

      if(line == F(">")) // дождались приглашения
      {
        #ifdef HTTP_DEBUG
          WIFI_DEBUG_WRITE(F("\">\" FOUND, sending HTTP-query data..."),currentAction);
          CHECK_QUEUE_TAIL(wfaStartSendHTTPData);
        #endif 

        actionsQueue.pop(); // убираем последнюю обработанную команду
        actionsQueue.push_back(wfaActualSendHTTPData); // добавляем команду на актуальный отсыл данных в очередь     
        currentAction = wfaIdle;
        flags.inSendData = true; // выставляем флаг, что мы отсылаем данные, и тогда очередь обработки клиентов не будет чухаться
               
      }
      else
      if(line.indexOf(F("FAIL")) != -1 || line.indexOf(F("ERROR")) != -1)
      {
         // всё плохо 
        #ifdef HTTP_DEBUG
          WIFI_DEBUG_WRITE(F("Error sending HTTP-query data!"),currentAction);
          CHECK_QUEUE_TAIL(wfaStartSendHTTPData);
        #endif 
        actionsQueue.pop(); // убираем последнюю обработанную команду
        currentAction = wfaIdle; // переходим в ждущий режим
        // поскольку мы законнекчены - надо закрыть соединение
        actionsQueue.push_back(wfaCloseHTTPConnection);

        // убеждаемся, что мы уведомили вызвавшую сторону о результатах запроса
        EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
      }
    }
    break;  

    case wfaCloseHTTPConnection: // закрывали HTTP-соединение
    {
      if(IsKnownAnswer(line)) // дождались закрытия соединения
      {
        #ifdef HTTP_DEBUG
        WIFI_DEBUG_WRITE(F("HTTP connection closed."),currentAction);
        CHECK_QUEUE_TAIL(wfaCloseHTTPConnection);
        #endif
        actionsQueue.pop(); // убираем последнюю обработанную команду     
        currentAction = wfaIdle;
        flags.inSendData = false; // разрешаем обработку других клиентов
      }
    }
    break;

    case wfaActualSendHTTPData:
    {
      // мы тут, понимаешь ли, ждём ответа на отсыл данных  HTTP-запроса.
      // в порт нам сыпется всё подряд, мы пересылаем это вызвавшей стороне до тех пор, пока она скажет "хватит",
      // или пока ESP нам не скажет, что наш клиент отвалился.

      // сначала проверяем, не закрыто ли соединение?
       int idx = line.indexOf(F(",CLOSED"));
       
       if(idx != -1) // соединение закрыто сервером, всё отослали
        {
          // клиент отсоединился
          String s = line.substring(0,idx);
          int clientID = s.toInt();
          if(clientID >= 0 && clientID < MAX_WIFI_CLIENTS)
          {
            // проверяем - не наш ли клиент?
            if(clientID == (MAX_WIFI_CLIENTS - 1))
            {

               #ifdef HTTP_DEBUG
                  WIFI_DEBUG_WRITE(F("HTTP client disconnected, DONE."),currentAction);
               #endif 
                            
                // наш - значит, мы уже весь ответ вычитали, и можем рапортовать о завершении
                EnsureHTTPProcessed(HTTP_REQUEST_COMPLETED);
                
                actionsQueue.pop(); // убираем последнюю обработанную команду
                currentAction = wfaIdle;
                flags.inSendData = false; // разрешаем обработку других клиентов         

            } // if
            
          }
        } // if
       else
       {
          bool enough = false;
          httpHandler->OnAnswerLineReceived(line,enough);
    
          // смотрим, может, хватит?
          if(enough)
          {
             // точно, хватит
             #ifdef HTTP_DEBUG
                WIFI_DEBUG_WRITE(F("HTTP request done."),currentAction);
             #endif
    
             // говорим, что всё на мази
             EnsureHTTPProcessed(HTTP_REQUEST_COMPLETED);
    
             // и закрываем соединение
              actionsQueue.pop(); // убираем последнюю обработанную команду
              currentAction = wfaIdle;         
              
              // поскольку мы законнекчены - надо закрыть соединение
              actionsQueue.push_back(wfaCloseHTTPConnection);              
          } // if(enough)
      } // else not closed
    }
    break;

    //////////////////////////// ЦИКЛ HTTP КОНЧИЛСЯ ////////////////////////////////////////
    
 
 #if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)

    case wfaActualSendIoTData:
    {
      // мы тут, понимаешь ли, ждём ответа на отсыл данных в IoT.
      // Ждём до тех пор, пока не получен известный нам ответ или строка не начинается с +IPD
      bool isIpd = line.startsWith(F("+IPD"));
      if(isIpd)
      {
        // дождались, следовательно, можем вызывать коллбэк, сообщая, что мы успешно отработали
          #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("IoT data processed, parse answer"),currentAction);
          CHECK_QUEUE_TAIL(wfaActualSendIoTData);
         #endif

            #ifdef WIFI_DEBUG
              WIFI_DEBUG_WRITE(F("IoT SUCCESS!"),currentAction);
            #endif
            // хорошо
            EnsureIoTProcessed(true);
         
          // в любом случае - завершаем обработку
          actionsQueue.pop(); // убираем последнюю обработанную команду
          currentAction = wfaIdle;         
          
          // поскольку мы законнекчены - надо закрыть соединение
          actionsQueue.push_back(wfaCloseIoTConnection);          
       }
       else
       {
          if(line.endsWith(F("SEND FAIL"))) // не удалось послать данные
          {
              actionsQueue.pop(); // убираем последнюю обработанную команду
              currentAction = wfaIdle;         
              EnsureIoTProcessed();
          }
       } // else
    }
    break;

    case wfaStartIoTSend:
    {
      // ждём коннекта к серверу
      if(IsKnownAnswer(line))
      {

         #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("IoT connection command done, parse..."),currentAction);
          CHECK_QUEUE_TAIL(wfaStartIoTSend);
         #endif

        actionsQueue.pop(); // убираем последнюю обработанную команду
        currentAction = wfaIdle;
        
        // один из известных нам ответов
        if(line == F("OK"))
        {
         #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("IoT connection OK, continue..."),currentAction);
         #endif
          // законнектились, можем посылать данные
          actionsQueue.push_back(wfaStartSendIoTData); // добавляем команду на актуальный отсыл данных в очередь     
        }
        else
        {
         #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("IoT connection ERROR!"),currentAction);
         #endif

            flags.inSendData = false;
          // всё плохо, вызываем коллбэк
            EnsureIoTProcessed();
        }
      } // if(IsKnownAnswer(line))
    }
    break;

    case wfaCloseIoTConnection:
    {
      if(IsKnownAnswer(line)) // дождались закрытия соединения
      {
        #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(F("IoT connection closed."),currentAction);
        CHECK_QUEUE_TAIL(wfaCloseIoTConnection);
        #endif
        actionsQueue.pop(); // убираем последнюю обработанную команду     
        currentAction = wfaIdle;
        flags.inSendData = false; // разрешаем обработку других клиентов
      }
    }
    break;

    case wfaStartSendIoTData:
    {
      // ждём > для отсыла данных
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("IoT, waiting for \">\"..."),currentAction);
        #endif 

      if(line == F(">")) // дождались приглашения
      {
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("\">\" FOUND, sending data to IoT..."),currentAction);
          CHECK_QUEUE_TAIL(wfaStartSendIoTData);
        #endif 

        actionsQueue.pop(); // убираем последнюю обработанную команду
        actionsQueue.push_back(wfaActualSendIoTData); // добавляем команду на актуальный отсыл данных в очередь     
        currentAction = wfaIdle;
        flags.inSendData = true; // выставляем флаг, что мы отсылаем данные, и тогда очередь обработки клиентов не будет чухаться
               
      }
      else
      if(line.indexOf(F("FAIL")) != -1 || line.indexOf(F("ERROR")) != -1)
      {
         // всё плохо 
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("Error sending data to IoT!"),currentAction);
          CHECK_QUEUE_TAIL(wfaStartSendIoTData);
        #endif 
        actionsQueue.pop(); // убираем последнюю обработанную команду
        currentAction = wfaIdle; // переходим в ждущий режим
        // поскольку мы законнекчены - надо закрыть соединение
        actionsQueue.push_back(wfaCloseIoTConnection);
        EnsureIoTProcessed();
      }
    }
    break;
#endif // IOT

    case wfaCIPSEND: // надо отослать данные клиенту
    {
      // wfaCIPSEND плюёт в очередь функция UpdateClients, перед отсылкой команды модулю.
      // значит, мы сами должны разрулить ситуацию, как быть с обработкой этой команды. 
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("Waiting for \">\"..."),currentAction);
        #endif        
            
      if(line == F(">")) // дождались приглашения
      {
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("\">\" FOUND, sending the data..."),currentAction);
          CHECK_QUEUE_TAIL(wfaCIPSEND);
        #endif        
        actionsQueue.pop(); // убираем последнюю обработанную команду (wfaCIPSEND, которую плюнула в очередь функция UpdateClients)
        actionsQueue.push_back(wfaACTUALSEND); // добавляем команду на актуальный отсыл данных в очередь     
        currentAction = wfaIdle;
        flags.inSendData = true; // выставляем флаг, что мы отсылаем данные, и тогда очередь обработки клиентов не будет чухаться
      }
      else
      if(line.indexOf(F("FAIL")) != -1 || line.indexOf(F("ERROR")) != -1)
      {
        // передача данных клиенту неудачна, отсоединяем его принудительно
         #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("Closing client connection unexpectedly!"),currentAction);
          CHECK_QUEUE_TAIL(wfaCIPSEND);
        #endif 
                
        clients[currentClientIDX].SetConnected(false); // выставляем текущему клиенту статус "отсоединён"
        actionsQueue.pop(); // убираем последнюю обработанную команду (wfaCIPSEND, которую плюнула в очередь функция UpdateClients)
        currentAction = wfaIdle; // переходим в ждущий режим
        flags.inSendData = false;
      }
    }
    break;

    case wfaACTUALSEND: // отослали ли данные?
    {
      // может ли произойти ситуация, когда в очереди есть wfaACTUALSEND, помещенная туда обработчиком wfaCIPSEND,
      // но до Update дело ещё не дошло? Считаем, что нет. Мы попали сюда после функции Update, которая в обработчике wfaACTUALSEND
      // отослала нам пакет данных. Надо проверить результат отсылки.
      if(IsKnownAnswer(line)) // получен результат отсылки пакета
      {
        #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(F("DATA SENT, go to IDLE mode..."),currentAction);
        // проверяем валидность того, что в очереди
        CHECK_QUEUE_TAIL(wfaACTUALSEND);
        #endif
        actionsQueue.pop(); // убираем последнюю обработанную команду (wfaACTUALSEND, которая в очереди)    
        currentAction = wfaIdle; // разрешаем обработку следующего клиента
        flags.inSendData = false; // выставляем флаг, что мы отправили пакет, и можем обрабатывать следующего клиента
        if(!clients[currentClientIDX].HasPacket())
        {
           // данные у клиента закончились
        #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(String(F("No packets in client #")) + String(currentClientIDX),currentAction);
        #endif

         #ifndef WIFI_TCP_KEEP_ALIVE  // если надо разрывать соединение после отсыла результатов - разрываем его
          if(clients[currentClientIDX].IsConnected())
          {
            #ifdef WIFI_DEBUG
            WIFI_DEBUG_WRITE(String(F("Client #")) + String(currentClientIDX) + String(F(" has no packets, closing connection...")),currentAction);
            #endif
            actionsQueue.push_back(wfaCIPCLOSE); // добавляем команду на закрытие соединения
            flags.inSendData = true; // пока не обработаем отсоединение клиента - не разрешаем посылать пакеты другим клиентам
          } // if
        #endif  
        
        } // if
      
        if(line.indexOf(F("FAIL")) != -1 || line.indexOf(F("ERROR")) != -1)
        {
          // передача данных клиенту неудачна, отсоединяем его принудительно
           #ifdef WIFI_DEBUG
            WIFI_DEBUG_WRITE(F("Closing client connection unexpectedly!"),currentAction);
          #endif 
                  
          clients[currentClientIDX].SetConnected(false);
        }      

      } // if known answer

    }
    break;

    case wfaCIPCLOSE: // закрыли соединение
    {
      if(IsKnownAnswer(line)) // дождались приглашения
      {
        #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(F("Client connection closed."),currentAction);
        CHECK_QUEUE_TAIL(wfaCIPCLOSE);
        #endif
        clients[currentClientIDX].SetConnected(false);
        actionsQueue.pop(); // убираем последнюю обработанную команду     
        currentAction = wfaIdle;
        flags.inSendData = false; // разрешаем обработку других клиентов
      }
    }
    break;

    case wfaIdle:
    {
    }
    break;
  } // switch

  // смотрим, может - есть статус клиента
  int idx = line.indexOf(F(",CONNECT"));
  if(idx != -1)
  {
    // клиент подсоединился
    String s = line.substring(0,idx);
    int clientID = s.toInt();

    #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
    if(clientID == MAX_WIFI_CLIENTS - 2)
    {
      #ifdef MQTT_DEBUG
        Serial.println(F("Connected to MQTT!"));
      #endif
      // законнектились нашим клиентом
      mqtt.setConnected(true);
    }
    #endif
    
    if(clientID >= 0 && clientID < MAX_WIFI_CLIENTS -
    #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
    2
    #else
    1
    #endif
    ) // последнему клиенту не даём статус законнекченного
    {
   #ifdef WIFI_DEBUG
    WIFI_DEBUG_WRITE(String(F("[CLIENT CONNECTED] - ")) + s,currentAction);
   #endif     
      clients[clientID].SetConnected(true);
    }
  } // if
  idx = line.indexOf(F(",CLOSED"));
 if(idx != -1)
  {
    // клиент отсоединился
    String s = line.substring(0,idx);
    int clientID = s.toInt();
    if(clientID >= 0 && clientID < MAX_WIFI_CLIENTS)
    {
   #ifdef WIFI_DEBUG
   WIFI_DEBUG_WRITE(String(F("[CLIENT DISCONNECTED] - ")) + s,currentAction);
   #endif     
      clients[clientID].SetConnected(false);

      if(clientID == MAX_WIFI_CLIENTS-1)
      {
        #if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
          EnsureIoTProcessed();
        #endif
      }

      #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
      if(clientID == MAX_WIFI_CLIENTS-2)
      {
        // соединение разорвано
        mqtt.setConnected(false);
        #ifdef MQTT_DEBUG
          Serial.println(F("MQTT disconnected!"));
        #endif        
      }
      #endif
      
    }
  } // if
  
  
}
//--------------------------------------------------------------------------------------------------------------------------------
bool WiFiModule::CanMakeQuery() // тестирует, может ли модуль сейчас сделать запрос
{
 
  if(flags.inSendData || 
    flags.inRebootMode || 
    flags.wantIoTToProcess || 
    flags.wantHTTPRequest || 
    flags.inHTTPRequestMode || 
    !flags.isAnyAnswerReceived ||
    actionsQueue.size())
  {
    // не можем обработать запрос

    return false;
  }

  return flags.isConnected;
}
//--------------------------------------------------------------------------------------------------------------------------------
void WiFiModule::MakeQuery(HTTPRequestHandler* handler) // начинаем запрос по HTTP
{
    // сперва завершаем обработку предыдущего вызова, если он вдруг нечаянно был
    EnsureHTTPProcessed(ERROR_HTTP_REQUEST_CANCELLED);

    // сохраняем обработчик запроса у себя
    httpHandler = handler;

    // и говорим, что мы готовы работать по HTTP-запросу
    flags.wantHTTPRequest = true;
}
//--------------------------------------------------------------------------------------------------------------------------------  
void WiFiModule::ProcessQuery(const String& command)
{
  
  int idx = command.indexOf(F(",")); // ищем первую запятую после +IPD
  const char* ptr = command.c_str();
  ptr += idx+1;
  // перешли за запятую, парсим ID клиента
  String connectedClientID = F("");
  while(*ptr != ',')
  {
    connectedClientID += (char) *ptr;
    ptr++;
  }
  ptr++; // за запятую
  String dataLen;
  while(*ptr != ':')
  {
    dataLen += (char) *ptr;
    ptr++; // перешли на начало данных
  }
  
  ptr++; // за двоеточие

  // тут пришла команда, разбираем её
  ProcessCommand(connectedClientID.toInt(),dataLen.toInt(),ptr);
   
}
//--------------------------------------------------------------------------------------------------------------------------------
void WiFiModule::ProcessCommand(int clientID, int dataLen, const char* command)
{
  // обрабатываем команду, пришедшую по TCP/IP
  
 #ifdef WIFI_DEBUG
  WIFI_DEBUG_WRITE(String(F("Client ID = ")) + String(clientID) + String(F("; len= ")) + String(dataLen),currentAction);
  WIFI_DEBUG_WRITE(String(F("Requested command: ")) + String(command),currentAction);
#endif
  
  // работаем с клиентом
  if(clientID >=0 && clientID < MAX_WIFI_CLIENTS-
  #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
  2
  #else
  1
  #endif
  ) // последний клиент - наш, для IoT и HTTP - в него не пишем
  {


      if(!*command) // пустой пакет, с переводом строки
        dataLen = 0;

        // теперь нам надо сложить все данные в клиента - как только он получит полный пакет - он подготовит
        // все данные к отправке. Признаком конца команды к контроллеру у нас служит перевод строки \r\n.
        // следовательно, пока мы не получим в любом виде перевод строки - считается, что команда не получена.
        // перевод строки может быть либо получен прямо в данных, либо - в следующем пакете.

        // как только клиент накопит всю команду - он получает данные с контроллера в следующем вызове Update.
        clients[clientID].CommandRequested(dataLen,command); // говорим клиенту, чтобы сложил во внутренний буфер
  } // if

  #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
  if(clientID == MAX_WIFI_CLIENTS-2)
  {
    #ifdef MQTT_DEBUG
      Serial.println(F("RECEIVE MQTT PACKET"));
    #endif
    
    // пришли данные в клиента MQTT
    MQTTBuffer mqttReceivedPacket;
    int written = 0;
    // приходится дублировать пакет, т.к. у нас входящий параметр - const
    while(written < dataLen)
    {
      mqttReceivedPacket.push_back(*command); 
      written++;
      command++;
    }
    mqtt.process(mqttReceivedPacket); // просим клиента обработать ответ от сервера
  }
  #endif
}
//--------------------------------------------------------------------------------------------------------------------------------
void WiFiModule::Setup()
{

  
  // настройка модуля тут
  #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
    mqttBuffer = new String();
    mqtt.init(); // инициализируем клиент MQTT
  #endif

 // сообщаем, что мы провайдер HTTP-запросов
 #ifdef USE_WIFI_MODULE_AS_HTTP_PROVIDER
  MainController->SetHTTPProvider(0,this); 
 #endif
  
  for(uint8_t i=0;i<MAX_WIFI_CLIENTS;i++)
    clients[i].Setup(i, WIFI_PACKET_LENGTH);



  #ifdef USE_WIFI_REBOOT_PIN
    WORK_STATUS.PinMode(WIFI_REBOOT_PIN,OUTPUT);
    WORK_STATUS.PinWrite(WIFI_REBOOT_PIN,WIFI_POWER_OFF);
    delay(200);
    WORK_STATUS.PinWrite(WIFI_REBOOT_PIN,WIFI_POWER_ON);
  #endif

  // поднимаем сериал
  WIFI_SERIAL.begin(WIFI_BAUDRATE);

  if(&(WIFI_SERIAL) == &Serial) {
       WORK_STATUS.PinMode(0,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(1,OUTPUT,false);
  } else if(&(WIFI_SERIAL) == &Serial1) {
       WORK_STATUS.PinMode(19,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(18,OUTPUT,false);
  } else if(&(WIFI_SERIAL) == &Serial2) {
       WORK_STATUS.PinMode(17,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(16,OUTPUT,false);
  } else if(&(WIFI_SERIAL) == &Serial3) {
       WORK_STATUS.PinMode(15,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(14,OUTPUT,false);
  } 

  flags.isAnyAnswerReceived = false;
  flags.inRebootMode = false;
  flags.wantIoTToProcess = false;
  flags.wantHTTPRequest = false;
  flags.inHTTPRequestMode = false;
  httpHandler = NULL;
  httpData = NULL;
  
  rebootStartTime = 0;

  InitQueue(); // инициализируем очередь


#if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)

     iotWriter = NULL;
     iotDone = NULL;
     iotDataHeader = NULL;
     iotDataFooter = NULL;
     iotDataLength = 0;     
     IoTList.RegisterGate(this); // регистрируем себя как отсылателя данных в IoT
#endif  

}
//--------------------------------------------------------------------------------------------------------------------------------
void WiFiModule::InitQueue(bool addRebootCommand)
{

  while(actionsQueue.size() > 0) // чистим очередь 
    actionsQueue.pop();

  WaitForDataWelcome = false; // не ждём приглашения
  
  nextClientIDX = 0;
  currentClientIDX = 0;
  flags.inSendData = false;
  flags.isConnected = false;

  // инициализируем время отсылки команды и получения ответа
  sendCommandTime = millis();
  answerWaitTimer = 0;

  needToWaitTimer = 0; // сбрасываем таймер

  // настраиваем то, что мы должны сделать
  currentAction = wfaIdle; // свободны, ничего не делаем

GlobalSettings* Settings = MainController->GetSettings();
  
  if(Settings->GetWiFiState() & 0x01) // коннектимся к роутеру
    actionsQueue.push_back(wfaCWJAP); // коннектимся к роутеру совсем в конце
  else  
    actionsQueue.push_back(wfaCWQAP); // отсоединяемся от роутера
    
  actionsQueue.push_back(wfaCIPSERVER); // сервер поднимаем в последнюю очередь
  actionsQueue.push_back(wfaCIPMUX); // разрешаем множественные подключения
  actionsQueue.push_back(wfaCIPMODE); // устанавливаем режим работы
  actionsQueue.push_back(wfaCWSAP); // создаём точку доступа
  actionsQueue.push_back(wfaCWMODE); // // переводим в смешанный режим
  actionsQueue.push_back(wfaEchoOff); // выключаем эхо
  
  if(addRebootCommand)
    actionsQueue.push_back(wfaWantReady); // надо получить ready от модуля путём его перезагрузки    
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void WiFiModule::SendCommand(const String& command, bool addNewLine)
{
  #ifdef WIFI_DEBUG
    WIFI_DEBUG_WRITE(String(F("==> Send the \"")) + command + String(F("\" command to ESP...")),currentAction);
  #endif

  // запоминаем время отсылки последней команды
  sendCommandTime = millis();
  answerWaitTimer = 0;

  WIFI_SERIAL.write(command.c_str(),command.length());
  
  if(addNewLine)
  {
    WIFI_SERIAL.write(String(NEWLINE).c_str());
  }
      
}
//--------------------------------------------------------------------------------------------------------------------------------
void WiFiModule::ProcessQueue()
{
  if(currentAction != wfaIdle) // чем-то заняты, не можем ничего делать
    return;

    size_t sz = actionsQueue.size();
    if(!sz) 
    {
      // в очереди ничего нет, можем проверять, что мы можем сделать
      if(flags.wantHTTPRequest)
      {
        // от нас ждут запроса по HTTP
        flags.wantHTTPRequest = false;
        flags.inHTTPRequestMode = true;
        actionsQueue.push_back(wfaStartHTTPSend);

        return; // возвращаемся, здесь делать нефик
      }

      #if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)

      if(!flags.inHTTPRequestMode) // только если мы не в режиме отсыла HTTP-запроса
      {
        if(flags.wantIoTToProcess && iotWriter && iotDone)
        {
          // надо поместить в очередь команду на обработку запроса к IoT
          flags.wantIoTToProcess = false;
          flags.inSendData = true; // чтобы не дёргать очередь клиентов
          actionsQueue.push_back(wfaStartIoTSend);
          return;
        }
      }
      #endif
  
      // тут проверяем - можем ли мы протестировать доступность модема?
      if(millis() - sendCommandTime > WIFI_AVAILABLE_CHECK_TIME) 
      {
          // раз в минуту можно проверить доступность модема,
          // и делаем мы это ТОЛЬКО тогда, когда очередь пуста как минимум WIFI_AVAILABLE_CHECK_TIME мс, т.е. все текущие команды отработаны.
          actionsQueue.push_back(wfaCheckModemHang);

          return;
      }


      #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT

        // тут проверяем, надо ли коннектится к MQTT-серверу?
        if(CanMakeQuery() && mqtt.enabled())
        {
          if(!mqtt.connected())
          {
            if(mqtt.canConnect()) // можем коннектится (например, прошло 10 секунд с момента последнего неудачного коннекта
            {
              // MQTT включено, но не законнекчено - коннектимся
               actionsQueue.push_back(wfaConnectToMQTT);
               mqtt.connecting(); // сообщаем, что мы начинаем коннектится
             
              return;
            } // mqtt.canConnect()
          } // !mqtt.connected()
          else
          {
             // присоединены к серверу, проверяем - есть что сказать?

             *mqttBuffer = "";
             mqttBufferLength = 0;
             
             if(mqtt.wantToSay(*mqttBuffer,mqttBufferLength))
             {
              #ifdef MQTT_DEBUG
                Serial.println(F("Push MQTT packet to queue..."));
              #endif
               // есть что сказать MQTT-серверу, говорим
               actionsQueue.push_back(wfaWriteToMQTT);
               return;
             }
          }
        } // if
      
      #endif

            
      return;
  } // if(!sz)
      
    currentAction = actionsQueue[sz-1]; // получаем очередную команду

    // смотрим, что за команда
    switch(currentAction)
    {
      //////////////////////////// ЦИКЛ MQTT ////////////////////////////////////////
      #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT

      case wfaActualWriteToMQTT: // начинаем отсылать данные MQTT-пакета
      {
         #ifdef MQTT_DEBUG
          WIFI_DEBUG_WRITE(F("Send MQTT-packet..."),currentAction);
        #endif  

          // посылаем пакет
          SendCommand(*mqttBuffer,false);
          delete mqttBuffer;
          mqttBuffer = new String();
          
      }
      break;      

        case wfaWriteToMQTT:
        {
           // надо послать серверу пакет
           #ifdef MQTT_DEBUG
            Serial.println(F("Start sending MQTT packet..."));
          #endif    

          String comm = CIPSEND_COMMAND;
          comm += String(MAX_WIFI_CLIENTS-2); // коннектимся предпоследним клиентом
          comm += F(",");
          comm += mqttBufferLength;
          flags.inSendData = false;
          WaitForDataWelcome = true; // выставляем флаг, что мы ждём >
          SendCommand(comm);
                
        }
        break;
      
        case wfaConnectToMQTT:
        {

          
          String host;
          int port;
          mqtt.getMQTTServer(host,port);

            #ifdef MQTT_DEBUG
            Serial.print(F("Connect to MQTT-server "));
            Serial.print(host);
            Serial.print(':');
            Serial.println(port);
          #endif
          // теперь формируем команду
          String comm = F("AT+CIPSTART=");
          comm += String(MAX_WIFI_CLIENTS - 2); // коннектимся предпоследним клиентом
          comm += F(",\"TCP\",\"");
          comm += host;
          comm += F("\",");
          comm += String(port);
  
          // и отсылаем её
          SendCommand(comm);
          
        }
        break;
      #endif
      //////////////////////////// ЦИКЛ MQTT ////////////////////////////////////////

      //////////////////////////// ЦИКЛ HTTP ////////////////////////////////////////
      
      case wfaStartHTTPSend: // начинаем запрос по HTTP
      {
        #ifdef HTTP_DEBUG
          Serial.println(F("Start HTTP connection..."));
        #endif

        // получаем адрес хоста
        String host;
        int port;
        httpHandler->OnAskForHost(host,port);

        // теперь формируем команду
        String comm = F("AT+CIPSTART=");
        comm += String(MAX_WIFI_CLIENTS - 1); // коннектимся последним клиентом
        comm += F(",\"TCP\",\"");
        comm += host;
        comm += F("\",");//80");
        comm += String(port);

        // и отсылаем её
        SendCommand(comm);
      }
      break;

      case wfaStartSendHTTPData: // можем отсылать данные по HTTP
      {
        #ifdef HTTP_DEBUG
          WIFI_DEBUG_WRITE(F("Make HTTP query AND send data..."),currentAction);
        #endif        
        // тут посылаем команду на отсыл данных по HTTP
        // но сначала - запросим-ка мы данные у вызвавшей всю эту движуху стороны
        delete httpData;
        httpData = new String;

        httpHandler->OnAskForData(httpData); // получили данные, которые надо отослать
        
        String comm = CIPSEND_COMMAND;
        comm += String(MAX_WIFI_CLIENTS-1); // коннектимся последним клиентом
        comm += F(",");
        comm += httpData->length();
        WaitForDataWelcome = true; // выставляем флаг, что мы ждём >
        SendCommand(comm);
                
      }
      break;

      case wfaCloseHTTPConnection: // закрываем HTTP-соединение
      {
        // надо закрыть соединение
        #ifdef HTTP_DEBUG
          WIFI_DEBUG_WRITE(F("Closing HTTP connection..."),currentAction);
        #endif

        flags.inSendData = true;
        String comm = F("AT+CIPCLOSE=");
        comm += String(MAX_WIFI_CLIENTS-1);
        SendCommand(comm);
                          
      }
      break;

      case wfaActualSendHTTPData: // начинаем отсылать данные HTTP-запроса
      {
         #ifdef HTTP_DEBUG
          WIFI_DEBUG_WRITE(F("Send HTTP-query data..."),currentAction);
        #endif  
          if(httpData)
          {      
            // тут посылаем данные
            SendCommand(*httpData,false);
            delete httpData;
            httpData = NULL;
          }
          else
          {
         #ifdef HTTP_DEBUG
          WIFI_DEBUG_WRITE(F("HTTP-query data is INVALID!"),currentAction);
        #endif  
        // чего-то в процессе не задалось, вызываем коллбэк, и говорим, что всё плохо
            EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
            actionsQueue.pop();
            currentAction = wfaIdle;
            flags.inSendData = false; 
          }
      }
      break;
      
      //////////////////////////// ЦИКЛ HTTP КОНЧИЛСЯ ////////////////////////////////////////      

      
      #if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)

      case wfaActualSendIoTData:
      {
        // отсылаем данные в IoT
         #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("Send data to IoT using ESP..."),currentAction);
        #endif  
          if(iotDataHeader && iotDataFooter && iotWriter && iotDone)
          {      
            // тут посылаем данные в IoT
            SendCommand(*iotDataHeader,false);
            iotWriter(&(WIFI_SERIAL));
            SendCommand(*iotDataFooter,false);       
          }
          else
          {
         #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("IoT data is INVALID!"),currentAction);
        #endif  
        // чего-то в процессе не задалось, вызываем коллбэк, и говорим, что всё плохо
            EnsureIoTProcessed();
            actionsQueue.pop();
            currentAction = wfaIdle;
            flags.inSendData = false; 
          }
      }
      break;

      case wfaStartSendIoTData:
      {

        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("Start sending data to IoT using ESP..."),currentAction);
        #endif        
        // тут посылаем команду на отсыл данных в IoT
        String comm = CIPSEND_COMMAND;
        comm += String(MAX_WIFI_CLIENTS-1); // коннектимся последним клиентом
        comm += F(",");
        comm += iotDataLength;
        WaitForDataWelcome = true; // выставляем флаг, что мы ждём >
        SendCommand(comm);
      }
      break;

      case wfaStartIoTSend:
      {
          // надо отослать данные в IOT
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("Connect to IoT using ESP..."),currentAction);
        #endif
    
        String comm = F("AT+CIPSTART=");
        comm += String(MAX_WIFI_CLIENTS - 1); // коннектимся последним клиентом
        comm += F(",\"TCP\",\"");

          // смотрим, в какой сервис запросили отсыл данных
          switch(iotService)
          {
            case iotThingSpeak:
              comm += THINGSPEAK_IP;
            break;
          }
        comm += F("\",80");
        SendCommand(comm);   
      }     
      break;

      case wfaCloseIoTConnection:
      {
        // надо закрыть соединение
        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("Closing IoT connection..."),currentAction);
        #endif

        flags.inSendData = true;
        String comm = F("AT+CIPCLOSE=");
        comm += String(MAX_WIFI_CLIENTS-1);
        SendCommand(comm);
                          
      }
      break;

     #endif
      
      case wfaWantReady:
      {
        // надо рестартовать модуль
      #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(F("Restart the ESP..."),currentAction);
      #endif
      SendCommand(F("AT+RST"));
      }
      break;

      case wfaCheckModemHang:
      {
          // проверяем, не завис ли модем?
        #ifdef WIFI_DEBUG
          Serial.println(F("Check if modem available..."));
        #endif

        flags.wantReconnect = false;
        SendCommand(F("AT+CWJAP?")); // проверяем, подконнекчены ли к роутеру
      }
      break;

      case wfaEchoOff:
      {
        // выключаем эхо
      #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(F("Disable echo..."),currentAction);
      #endif
      SendCommand(F("ATE0"));
      //SendCommand(F("AT+CIOBAUD=230400")); // переводим на другую скорость
      }
      break;

      case wfaCWMODE:
      {
        // переходим в смешанный режим
      #ifdef WIFI_DEBUG
       WIFI_DEBUG_WRITE(F("Go to SoftAP mode..."),currentAction);
      #endif
      SendCommand(F("AT+CWMODE_DEF=3"));
      }
      break;

      case wfaCWSAP: // создаём точку доступа
      {

      #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(F("Creating the access point..."),currentAction);
      #endif

        GlobalSettings* Settings = MainController->GetSettings();
      
        String com = F("AT+CWSAP_DEF=\"");
        com += Settings->GetStationID();
        com += F("\",\"");
        com += Settings->GetStationPassword();
        com += F("\",8,4");
        
        SendCommand(com);
        
      }
      break;

      case wfaCIPMODE: // устанавливаем режим работы сервера
      {
      #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(F("Set the TCP server mode to 0..."),currentAction);
      #endif
      SendCommand(F("AT+CIPMODE=0"));
      
      }
      break;

      case wfaCIPMUX: // разрешаем множественные подключения
      {
      #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(F("Allow the multiple connections..."),currentAction);
      #endif
      SendCommand(F("AT+CIPMUX=1"));
        
      }
      break;

      case wfaCIPSERVER: // запускаем сервер
      {  
      #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(F("Starting TCP-server..."),currentAction);
      #endif
      SendCommand(F("AT+CIPSERVER=1,1975"));
      
      }
      break;

      case wfaCWQAP: // отсоединяемся от точки доступа
      {  
      #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(F("Disconnect from router..."),currentAction);
      #endif
      SendCommand(F("AT+CWQAP"));
      
      }
      break;

      case wfaCWJAP: // коннектимся к роутеру
      {
      #ifdef WIFI_DEBUG
        WIFI_DEBUG_WRITE(F("Connecting to the router..."),currentAction);
      #endif

        GlobalSettings* Settings = MainController->GetSettings();
        
        String com = F("AT+CWJAP_DEF=\"");
        com += Settings->GetRouterID();
        com += F("\",\"");
        com += Settings->GetRouterPassword();
        com += F("\"");
        SendCommand(com);

      }
      break;

      case wfaCIPSEND: // надо отослать данные клиенту
      {
        #ifdef WIFI_DEBUG
       //  WIFI_DEBUG_WRITE(F("ASSERT: wfaCIPSEND in ProcessQueue!"),currentAction);
        #endif
        
      }
      break;

      case wfaACTUALSEND: // дождались приглашения в функции ProcessAnswerLine, она поместила команду wfaACTUALSEND в очередь - отсылаем данные клиенту
      {
            #ifdef WIFI_DEBUG
              WIFI_DEBUG_WRITE(String(F("Sending data to the client #")) + String(currentClientIDX),currentAction);
            #endif
      
            if(clients[currentClientIDX].IsConnected()) // не отвалился ли клиент?
            {
              // клиент по-прежнему законнекчен, посылаем данные
              if(!clients[currentClientIDX].SendPacket(&(WIFI_SERIAL)))
              {
                // если мы здесь - то пакетов у клиента больше не осталось. Надо дождаться подтверждения отсылки последнего пакета
                // в функции ProcessAnswerLine (обработчик wfaACTUALSEND), и послать команду на закрытие соединения с клиентом.
              #ifdef WIFI_DEBUG
              WIFI_DEBUG_WRITE(String(F("All data to the client #")) + String(currentClientIDX) + String(F(" has sent, need to wait for last packet sent..")),currentAction);
              #endif
 
              }
              else
              {
                // ещё есть пакеты, продолжаем отправлять в следующих вызовах Update
              #ifdef WIFI_DEBUG
              WIFI_DEBUG_WRITE(String(F("Client #")) + String(currentClientIDX) + String(F(" has ")) + String(clients[currentClientIDX].GetPacketsLeft()) + String(F(" packets left...")),currentAction);
              #endif
              } // else
            } // is connected
            else
            {
              // клиент отвалится, чистим...
            #ifdef WIFI_DEBUG
              WIFI_DEBUG_WRITE(F("Client disconnected, clear the client data..."),currentAction);
            #endif
              clients[currentClientIDX].SetConnected(false);
            }

      }
      break;

      case wfaCIPCLOSE: // закрываем соединение с клиентом
      {
        if(clients[currentClientIDX].IsConnected()) // только если клиент законнекчен 
        {
          #ifdef WIFI_DEBUG
            WIFI_DEBUG_WRITE(String(F("Closing client #")) + String(currentClientIDX) + String(F(" connection...")),currentAction);
          #endif
          clients[currentClientIDX].SetConnected(false);
          String command = F("AT+CIPCLOSE=");
          command += currentClientIDX; // закрываем соединение
          SendCommand(command);
        }
        
        else
        {
          #ifdef WIFI_DEBUG
            WIFI_DEBUG_WRITE(String(F("Client #")) + String(currentClientIDX) + String(F(" already broken!")),currentAction);
            CHECK_QUEUE_TAIL(wfaCIPCLOSE);
          #endif
          // просто убираем команду из очереди
           actionsQueue.pop();
           currentAction = wfaIdle; // разрешаем обработку следующей команды
           flags.inSendData = false; // разрешаем обработку следующего клиента
        } // else
        
      }
      break;

      case wfaIdle:
      {
        // ничего не делаем

      }
      break;
      
    } // switch
}
//--------------------------------------------------------------------------------------------------------------------------------
void WiFiModule::RebootModem()
{
  // перезагружаем модем тут
  #ifdef WIFI_DEBUG
    Serial.println(F("[ERR] - ESP not answering, reboot it..."));
  #endif

  // мы в процессе перезагрузки
  flags.inRebootMode = true;

  // запоминаем время выключения питания
  rebootStartTime = millis();

  //Тут выключение питания модема
  #ifdef USE_WIFI_REBOOT_PIN
    WORK_STATUS.PinWrite(WIFI_REBOOT_PIN,WIFI_POWER_OFF);
  #endif

    
}
//--------------------------------------------------------------------------------------------------------------------------------
void WiFiModule::UpdateClients()
{
  if(currentAction != wfaIdle || flags.inSendData || flags.inHTTPRequestMode) // чем-то заняты, не можем ничего делать
    return;
    
  // тут ищем, какой клиент сейчас хочет отослать данные

  for(uint8_t idx = nextClientIDX;idx < MAX_WIFI_CLIENTS; idx++)
  { 
    ++nextClientIDX; // переходим на следующего клиента, как только текущему будет послан один пакет

    clients[idx].Update(); // обновляем внутреннее состояние клиента - здесь он может подготовить данные к отправке, например
    
    if(clients[idx].IsConnected() && clients[idx].HasPacket())
    {
      currentAction = wfaCIPSEND; // говорим однозначно, что нам надо дождаться >
      actionsQueue.push_back(wfaCIPSEND); // добавляем команду отсылки данных в очередь
      
    #ifdef WIFI_DEBUG
      WIFI_DEBUG_WRITE(F("Sending data command to the ESP..."),currentAction);
    #endif
  
      // клиент подсоединён и ждёт данных от нас - отсылаем ему следующий пакет
      currentClientIDX = idx; // сохраняем номер клиента, которому будем посылать данные
      String command = CIPSEND_COMMAND;
      command += String(idx);
      command += F(",");
      command += String(clients[idx].GetPacketLength());
      WaitForDataWelcome = true; // выставляем флаг, что мы ждём >

      SendCommand(command);
  
      break; // выходим из цикла
    } // if
    
  } // for
  
  if(nextClientIDX >= MAX_WIFI_CLIENTS) // начинаем обработку клиентов сначала
    nextClientIDX = 0;  
}
//--------------------------------------------------------------------------------------------------------------------------------
void WiFiModule::EnsureHTTPProcessed(uint16_t statusCode)
{
  if(!httpHandler) // не было флага запроса HTTP-адреса
    return;

    #ifdef HTTP_DEBUG
      Serial.print(F("EnsureHTTPProcessed: "));
      Serial.println(statusCode);
    #endif
  
    
   httpHandler->OnHTTPResult(statusCode); // сообщаем, что мы закончили обработку

  flags.wantHTTPRequest = false;
  flags.inHTTPRequestMode = false;
  flags.inSendData = false;
  httpHandler = NULL;
  delete httpData;
  httpData = NULL;
}
//--------------------------------------------------------------------------------------------------------------------------------
#if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
void WiFiModule::EnsureIoTProcessed(bool success)
{
     if(iotDone) 
     {
        // да, нас вызывали для отсыла данных в IoT, но что-то пошло не так
        iotDone({success,iotService});
        iotDone = NULL;
        iotWriter = NULL;
     }

     delete iotDataHeader;
     iotDataHeader = NULL;

     delete iotDataFooter;
     iotDataFooter = NULL;
     
     iotDataLength = 0;  
     flags.wantIoTToProcess = false;         
  
}
#endif
//--------------------------------------------------------------------------------------------------------------------------------
void WiFiModule::Update(uint16_t dt)
{ 
  #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
  mqtt.update(dt);
  #endif
  
  if(flags.inRebootMode) {
    // мы в процессе перезагрузки модема, надо проверить, пора ли включать питание?
    if(millis() - rebootStartTime > WIFI_REBOOT_TIME) {
      // две секунды держали питание выключенным, можно включать
      flags.inRebootMode = false;
      flags.isAnyAnswerReceived = false; // говорим, что мы ничего от модема не получали

      InitQueue(); // инициализируем очередь

      // ТУТ включение питания модема 
      #ifdef USE_WIFI_REBOOT_PIN
        WORK_STATUS.PinWrite(WIFI_REBOOT_PIN,WIFI_POWER_ON);
      #endif
      needToWaitTimer = WIFI_WAIT_AFTER_REBOOT_TIME; // дадим модему WIFI_WAIT_AFTER_REBOOT_TIME мс на раздупление, прежде чем начнём что-либо делать

      #ifdef WIFI_DEBUG
        Serial.println(F("[REBOOT] - ESP rebooted, wait for ready..."));
      #endif
    }
    
    return;
  }  

  if(needToWaitTimer > 0) // надо ждать следующей команды запрошенное время
  {
    needToWaitTimer -= dt;
    return;
  }

  needToWaitTimer = 0; // сбрасываем таймер ожидания  

   if(currentAction != wfaIdle) // только если мы в процессе обработки команды, то
    answerWaitTimer += dt; // увеличиваем время ожидания ответа на последнюю команду 


   // сначала проверяем - а не слишком ли долго мы ждём ответа от модема?
  if(answerWaitTimer > WIFI_MAX_ANSWER_TIME) {

     // тут смотрим - возможно, нам надо вызвать функцию обратного вызова для IoT
     #if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
      EnsureIoTProcessed();
     #endif

     // тут убеждаемся, что мы сообщили вызывающей стороне о неуспешном запросе по HTTP
     EnsureHTTPProcessed(ERROR_MODEM_NOT_ANSWERING);
     
      // очень долго, надо перезапустить последнюю команду.
     // причём лучше всего перезапустить всё сначала
     InitQueue();
     needToWaitTimer = WIFI_WAIT_AFTER_REBOOT_TIME; // ещё через 5 секунд попробуем
     sendCommandTime = millis(); // сбросили таймера
     answerWaitTimer = 0;

     if(flags.isAnyAnswerReceived) {
        // получали хоть один ответ от модема - возможно, он завис?
        RebootModem();
        
     } else {
        // ничего не получали, модема не подсоединено?
        #ifdef WIFI_DEBUG
          Serial.println(F("[ERR] - ESP not found, check for presence after short time..."));
        #endif
     }


  }     

  if(!flags.inRebootMode) { // если мы не в процессе перезагрузки - то можем отрабатывать очередь  
    UpdateClients();
    ProcessQueue();
  }

}
//--------------------------------------------------------------------------------------------------------------------------------
bool  WiFiModule::ExecCommand(const Command& command, bool wantAnswer)
{
  UNUSED(wantAnswer);
  
  PublishSingleton = NOT_SUPPORTED;

  if(command.GetType() == ctSET) // установка свойств
  {
    uint8_t argsCnt = command.GetArgsCount();
    if(argsCnt > 0)
    {
      String t = command.GetArg(0);
      if(t == WIFI_SETTINGS_COMMAND) // установить настройки вай-фай
      {
        if(argsCnt > 5)
        {
          GlobalSettings* Settings = MainController->GetSettings();
          
          int shouldConnectToRouter = atoi(command.GetArg(1));
          String routerID = command.GetArg(2);
          String routerPassword = command.GetArg(3);
          String stationID = command.GetArg(4);
          String stationPassword = command.GetArg(5);

          bool shouldReastartAP = Settings->GetStationID() != stationID ||
          Settings->GetStationPassword() != stationPassword;


          Settings->SetWiFiState(shouldConnectToRouter);
          Settings->SetRouterID(routerID);
          Settings->SetRouterPassword(routerPassword);
          Settings->SetStationID(stationID);
          Settings->SetStationPassword(stationPassword);
          
          if(!routerID.length())
            Settings->SetWiFiState(0); // не коннектимся к роутеру

//          Settings->Save(); // сохраняем настройки

          if(Settings->GetWiFiState() & 0x01) // коннектимся к роутеру
            actionsQueue.push_back(wfaCWJAP); // коннектимся к роутеру совсем в конце
          else
            actionsQueue.push_back(wfaCWQAP); // отсоединяемся от роутера

          if(shouldReastartAP) // надо пересоздать точку доступа
          {
            actionsQueue.push_back(wfaCIPSERVER); // сервер поднимаем в последнюю очередь
            actionsQueue.push_back(wfaCIPMUX); // разрешаем множественные подключения
            actionsQueue.push_back(wfaCIPMODE); // устанавливаем режим работы
            actionsQueue.push_back(wfaCWSAP); // создаём точку доступа
          }
           
          
          PublishSingleton.Flags.Status = true;
          PublishSingleton = t; 
          PublishSingleton << PARAM_DELIMITER << REG_SUCC;
        }
        else
          PublishSingleton = PARAMS_MISSED; // мало параметров
        
      } // WIFI_SETTINGS_COMMAND
      #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
      else
      if(t == F("MQTT_DEL")) // удалить все топики
      {
            mqtt.DeleteAllTopics();
            PublishSingleton.Flags.Status = true;
            PublishSingleton = t; 
            PublishSingleton << PARAM_DELIMITER << REG_SUCC;        
      } // MQTT_DEL
      else
      if(t == F("MQTT_ADD")) // добавить топик, CTSET=WIFI|MQTT_ADD|Index|Topic name|Module name|Sensor type|Sensor index|Topic type
      {
        if(argsCnt > 6)
        {
          const char* topicIndex = command.GetArg(1);
          const char* topicName = command.GetArg(2);
          const char* moduleName = command.GetArg(3);
          const char* sensorType = command.GetArg(4);
          const char* sensorIndex = command.GetArg(5);
          const char* topicType = command.GetArg(6);

          mqtt.AddTopic(topicIndex,topicName,moduleName,sensorType,sensorIndex,topicType);
          
          PublishSingleton.Flags.Status = true;
          PublishSingleton = t; 
          PublishSingleton << PARAM_DELIMITER << REG_SUCC;
        }
        else
        {
          PublishSingleton = PARAMS_MISSED; // мало параметров
        }
        
      } // MQTT_ADD
      else
      if(t == F("MQTT")) // установить настройки MQTT: CTSET=WIFI|MQTT|Enabled flag|Server address|Server port|Interval between topics|Client id|User|Pass
      {        
        if(argsCnt > 7)
        {
            byte mqttEnabled = atoi(command.GetArg(1));
            String mqttServer = command.GetArg(2);
            String mqttPort = command.GetArg(3);
            byte mqttInterval = atoi(command.GetArg(4));
            String mqttClient = command.GetArg(5);
            String mqttUser = command.GetArg(6);
            String mqttPass = command.GetArg(7);

            if(mqttServer == "_")
              mqttServer = "";

            if(mqttPort == "_")
              mqttPort = "";

            if(mqttClient == "_")
              mqttClient = "";

            if(mqttUser == "_")
              mqttUser = "";

            if(mqttPass == "_")
              mqttPass = "";

            MemWrite(MQTT_ENABLED_FLAG_ADDRESS,mqttEnabled);
            MemWrite(MQTT_INTERVAL_BETWEEN_TOPICS_ADDRESS,mqttInterval);

            if(MainController->HasSDCard())
            {
              String mqttSettingsFileName = F("mqtt.ini");
              String newline = "\n";
              
              SdFile f;
              
              #define MQTT_WRITE_TO_FILE(f,str) f.write((const uint8_t*) str.c_str(),str.length())
              
              if(f.open(mqttSettingsFileName.c_str(), FILE_WRITE | O_TRUNC))
              {
                // адрес сервера
                MQTT_WRITE_TO_FILE(f,mqttServer);
                MQTT_WRITE_TO_FILE(f,newline);
                
                // порт
                MQTT_WRITE_TO_FILE(f,mqttPort);
                MQTT_WRITE_TO_FILE(f,newline);

                // ID клиента
                MQTT_WRITE_TO_FILE(f,mqttClient);
                MQTT_WRITE_TO_FILE(f,newline);

                // user
                MQTT_WRITE_TO_FILE(f,mqttUser);
                MQTT_WRITE_TO_FILE(f,newline);

                // pass
                MQTT_WRITE_TO_FILE(f,mqttPass);
                MQTT_WRITE_TO_FILE(f,newline);
                
                f.close();
              } // 
              
            } // if

            mqtt.reloadSettings(); // говорим клиенту, что настройки изменились
            
          
            PublishSingleton.Flags.Status = true;
            PublishSingleton = t; 
            PublishSingleton << PARAM_DELIMITER << REG_SUCC;
        }
        else
          PublishSingleton = PARAMS_MISSED;
        
      }
      #endif // USE_WIFI_MODULE_AS_MQTT_CLIENT
    }
    else
      PublishSingleton = PARAMS_MISSED; // мало параметров
  } // SET
  else
  if(command.GetType() == ctGET) // чтение свойств
  {
    uint8_t argsCnt = command.GetArgsCount();
    if(argsCnt > 0)
    {
      String t = command.GetArg(0);
      
      if(t == IP_COMMAND) // получить данные об IP
      {
        if(currentAction != wfaIdle) // не можем ответить на запрос немедленно
          PublishSingleton = BUSY;
        else
        {
        #ifdef WIFI_DEBUG
         WIFI_DEBUG_WRITE(F("Request for IP info..."),currentAction);
        #endif
        
        
        SendCommand(F("AT+CIFSR"));
        // поскольку у нас serialEvent не основан на прерываниях, на самом-то деле (!),
        // то мы должны получить ответ вот прямо вот здесь, и разобрать его.


        String line; // тут принимаем данные до конца строки
        String apCurrentIP;
        String stationCurrentIP;
        bool  apIpDone = false;
        bool staIpDone = false;
        

        char ch;
        while(1)
        { 
          if(apIpDone && staIpDone) // получили оба IP
            break;
            
          while(WIFI_SERIAL.available())
          {
            ch = WIFI_SERIAL.read();
        
            if(ch == '\r')
              continue;
            
            if(ch == '\n')
            {
              // получили строку, разбираем её
                if(line.startsWith(F("+CIFSR:APIP"))) // IP нашей точки доступа
                 {
                    #ifdef WIFI_DEBUG
                      WIFI_DEBUG_WRITE(F("AP IP found, parse..."),currentAction);
                    #endif
            
                   int idx = line.indexOf("\"");
                   if(idx != -1)
                   {
                      apCurrentIP = line.substring(idx+1);
                      idx = apCurrentIP.indexOf("\"");
                      if(idx != -1)
                        apCurrentIP = apCurrentIP.substring(0,idx);
                      
                   }
                   else
                    apCurrentIP = F("0.0.0.0");

                    apIpDone = true;
                 } // if(line.startsWith(F("+CIFSR:APIP")))
                 else
                  if(line.startsWith(F("+CIFSR:STAIP"))) // IP нашей точки доступа, назначенный роутером
                 {
                    #ifdef WIFI_DEBUG
                      WIFI_DEBUG_WRITE(F("STA IP found, parse..."),currentAction);
                    #endif
            
                   int idx = line.indexOf("\"");
                   if(idx != -1)
                   {
                      stationCurrentIP = line.substring(idx+1);
                      idx = stationCurrentIP.indexOf("\"");
                      if(idx != -1)
                        stationCurrentIP = stationCurrentIP.substring(0,idx);
                      
                   }
                   else
                    stationCurrentIP = F("0.0.0.0");

                    staIpDone = true;
                 } // if(line.startsWith(F("+CIFSR:STAIP")))
             
              line = F("");
            } // ch == '\n'
            else
            {
                  line += ch;
            }
        
         if(apIpDone && staIpDone) // получили оба IP
            break;
 
          } // while
          
        } // while(1)
        


        #ifdef WIFI_DEBUG
          WIFI_DEBUG_WRITE(F("IP info requested."),currentAction);
        #endif

        PublishSingleton.Flags.Status = true;
        PublishSingleton = t; 
        PublishSingleton << PARAM_DELIMITER << apCurrentIP << PARAM_DELIMITER << stationCurrentIP;
        } // else not busy
      } // IP_COMMAND
      else
      if(t == WIFI_SETTINGS_COMMAND)
      {
         // получить настройки Wi-Fi

        GlobalSettings* Settings = MainController->GetSettings();  

        PublishSingleton.Flags.Status = true;
        PublishSingleton = t; 
        PublishSingleton << PARAM_DELIMITER << Settings->GetWiFiState() << PARAM_DELIMITER << Settings->GetRouterID()
        << PARAM_DELIMITER << Settings->GetRouterPassword()
        << PARAM_DELIMITER << Settings->GetStationID()
        << PARAM_DELIMITER << Settings->GetStationPassword();
        
      } // WIFI_SETTINGS_COMMAND
      #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
      else
      if(t == F("MQTT_CNT"))
      {
        // получить кол-во топиков, сохранённых на карточке
        PublishSingleton.Flags.Status = true;
        PublishSingleton = t;
        PublishSingleton << PARAM_DELIMITER << mqtt.GetSavedTopicsCount();        
        
      } // MQTT_CNT
      else
      if(t == F("MQTT_VIEW")) // посмотреть топик по индексу, CTGET=WIFI|MQTT_VIEW|Index
      {
        if(argsCnt > 1)
        {
          String topicFileName = MQTT_FILENAME_PATTERN;
          topicFileName += command.GetArg(1);
        
          // тут можем читать из файла настроек топика
                SdFile f;
                
                if(f.open(topicFileName.c_str(),FILE_READ))
                {   
                  // теперь читаем настройки топика
                  // первой строкой идёт имя топика
                  String topicName;
                  FileUtils::readLine(f,topicName);
          
                  // второй строкой - идёт имя модуля, в котором взять нужные показания
                  String moduleName;
                  FileUtils::readLine(f,moduleName);
          
                  // в третьей строке - тип датчика, числовое значение соответствует перечислению ModuleStates
                  String sensorTypeString;
                  FileUtils::readLine(f,sensorTypeString);
                            
                  // в четвёртой строке - индекс датчика в модуле
                  String sensorIndexString;
                  FileUtils::readLine(f,sensorIndexString);
          
                  // в пятой строке - тип топика: показания с датчиков (0), или статус контроллера (1).
                  // в случае статуса контроллера во второй строке - тип статуса, третья и четвёртая - зависят от типа статуса
                  String topicType;
                  FileUtils::readLine(f,topicType);
                  
                  // не забываем закрыть файл
                  f.close(); 

                  PublishSingleton.Flags.Status = true;
                  PublishSingleton = t;
                  PublishSingleton << PARAM_DELIMITER << command.GetArg(1); // index
                  PublishSingleton << PARAM_DELIMITER << topicName; // topic name
                  PublishSingleton << PARAM_DELIMITER << moduleName; // module name
                  PublishSingleton << PARAM_DELIMITER << sensorTypeString; // sensor type
                  PublishSingleton << PARAM_DELIMITER << sensorIndexString; // sensor index
                  PublishSingleton << PARAM_DELIMITER << topicType; // topic type
                  
                }
                else
                {
                  PublishSingleton = PARAMS_MISSED; // мало параметров
                }
                        
         
        }
        else
        {
          PublishSingleton = PARAMS_MISSED; // мало параметров
        }
        
      } // MQTT_VIEW
      else
      if(t == F("MQTT")) // получить настройки MQTT, CTGET=WIFI|MQTT, возвращает OK=WIFI|MQTT|Enabled flag|Server address|Server port|Interval between topics|Client id|User|Pass
      {
        if(MainController->HasSDCard())
        {
           // есть карта, читаем настройки
           byte mqttEnabled = MemRead(MQTT_ENABLED_FLAG_ADDRESS);
           if(mqttEnabled == 0xFF)
            mqttEnabled = 0;
            
           byte mqttInterval = MemRead(MQTT_INTERVAL_BETWEEN_TOPICS_ADDRESS);
           if(mqttInterval == 0xFF)
            mqttInterval = 10;

           String mqttServer, mqttPort, mqttClientId, mqttUser, mqttPass;

           String mqttSettingsFileName = F("mqtt.ini");
           SdFile f;

           if(f.open(mqttSettingsFileName.c_str(),FILE_READ))
           {
            FileUtils::readLine(f,mqttServer); // адрес сервера
            FileUtils::readLine(f,mqttPort); // порт сервера
            FileUtils::readLine(f,mqttClientId); // ID клиента
            FileUtils::readLine(f,mqttUser); // user
            FileUtils::readLine(f,mqttPass); // pass
            
            f.close();
           }
           
            // Всё прочитали, можно постить
            PublishSingleton.Flags.Status = true;
            PublishSingleton = t;
            PublishSingleton << PARAM_DELIMITER << mqttEnabled; 
            PublishSingleton << PARAM_DELIMITER << mqttServer; 
            PublishSingleton << PARAM_DELIMITER << mqttPort; 
            PublishSingleton << PARAM_DELIMITER << mqttInterval; 
            PublishSingleton << PARAM_DELIMITER << mqttClientId; 
            PublishSingleton << PARAM_DELIMITER << mqttUser; 
            PublishSingleton << PARAM_DELIMITER << mqttPass; 
           
        }
      }
      #endif // USE_WIFI_MODULE_AS_MQTT_CLIENT
    }
    else
      PublishSingleton = PARAMS_MISSED; // мало параметров
  } // GET

  MainController->Publish(this,command);

  return PublishSingleton.Flags.Status;
}
//--------------------------------------------------------------------------------------------------------------------------------

