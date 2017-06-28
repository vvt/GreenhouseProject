#include "SMSModule.h"
#include "ModuleController.h"
#include "PDUClasses.h"
#include "InteropStream.h"
#if defined(USE_ALARM_DISPATCHER) && defined(USE_SMS_MODULE)
#include "AlarmDispatcher.h"
#endif
//--------------------------------------------------------------------------------------------------------------------------------
// функция хэширования строки
//--------------------------------------------------------------------------------------------------------------------------------
#define A_PRIME 54059 /* a prime */
#define B_PRIME 76963 /* another prime */
#define C_PRIME 86969 /* yet another prime */
//--------------------------------------------------------------------------------------------------------------------------------
unsigned int hash_str(const char* s)
{
   unsigned int h = 31 /* also prime */;
   while (*s) {
     h = (h * A_PRIME) ^ (s[0] * B_PRIME);
     s++;
   }
   return h; // or return h % C;
}
//--------------------------------------------------------------------------------------------------------------------------------
bool SMSModule::IsKnownAnswer(const String& line, bool& okFound)
{
  okFound = false;
  
  if(line == F("OK"))
  {
    okFound = true;
    return true;
  }
  return ( line.indexOf(F("ERROR")) != -1 );
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::GetAPNUserPass(String& user, String& pass)
{
    byte provider = MainController->GetSettings()->GetGSMProvider();
    switch(provider)
    {
       case MTS:
        user = MTS_USER;
        pass = MTS_PASS;
       break;

       case Beeline:
        user = BEELINE_USER;
        pass = BEELINE_PASS;
       break;

       case Megafon:
        user = MEGAFON_USER;
        pass = MEGAFON_PASS;
       break;

       case Tele2:
        user = TELE2_USER;
        pass = TELE2_PASS;
       break;

       case Yota:
        user = YOTA_USER;
        pass = YOTA_PASS;
       break;

       case MTS_Bel:
        user = MTS_BEL_USER;
        pass = MTS_BEL_PASS;
       break;

       case Velcom_Bel:
        user = VELCOM_BEL_USER;
        pass = VELCOM_BEL_PASS;
       break;

       case Privet_Bel:
        user = PRIVET_BEL_USER;
        pass = PRIVET_BEL_PASS;
       break;

       case Life_Bel:
        user = LIFE_BEL_USER;
        pass = LIFE_BEL_PASS;
       break;
      
    } // switch   
}
//--------------------------------------------------------------------------------------------------------------------------------
String SMSModule::GetAPN()
{
    byte provider = MainController->GetSettings()->GetGSMProvider();

        switch(provider)
        {
           case MTS:
            return MTS_APN;

           case Beeline:
            return BEELINE_APN;

           case Megafon:
            return MEGAFON_APN;

           case Tele2:
            return TELE2_APN;

           case Yota:
            return YOTA_APN;
          
           case MTS_Bel:
            return MTS_BEL_APN;
          
           case Velcom_Bel:
            return VELCOM_BEL_APN;
          
           case Privet_Bel:
            return PRIVET_BEL_APN;
          
           case Life_Bel:
            return LIFE_BEL_APN;          
        } // switch  

   return F("");
}
//--------------------------------------------------------------------------------------------------------------------------------
#if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::SendData(IoTService service,uint16_t dataLength, IOT_OnWriteToStream writer, IOT_OnSendDataDone onDone)
{

    if(!flags.isModuleRegistered)
    {
      // не зарегистрированы, не можем обрабатывать запросы IoT
      onDone({false,service});
      return;
    }
  
    // тут смотрим, можем ли мы обработать запрос на отсыл данных в IoT
    IoTSettings* iotSettings = MainController->GetSettings()->GetIoTSettings();

    if(iotSettings->Flags.ThingSpeakEnabled && strlen(iotSettings->ThingSpeakChannelID)) // включен один сервис хотя бы
    {

     // сохраняем указатели на функции обратного вызова
      iotWriter = writer;
      iotDone = onDone;
      iotService = service;

      #ifdef WIFI_DEBUG
        Serial.println(F("Requested to write data to IoT using GSM..."));
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
          *iotDataHeader += iotSettings->ThingSpeakChannelID;
          *iotDataHeader += F("&");

          *iotDataFooter = F(" HTTP/1.1\r\nAccept: */*\r\nUser-Agent: ");
          *iotDataFooter += IOT_USER_AGENT;
          *iotDataFooter += F("\r\nHost: ");
          *iotDataFooter += THINGSPEAK_HOST;
          *iotDataFooter += F("\r\n\r\n");

          // теперь вычисляем, сколько всего данных будет
          iotDataLength = iotDataHeader->length() + iotDataFooter->length() + dataLength;

          #ifdef GSM_DEBUG_MODE
            Serial.println(F("IOT HEADER:"));
            Serial.println(*iotDataHeader);
            Serial.println(F("IOT FOOTER:"));
            Serial.println(*iotDataFooter);
          #endif

          // теперь можно добавлять в очередь запрос на обработку. Но ситуация с очередью следующая:
          // мы не знаем, чем сейчас занят GSM, и что у нас в очереди.
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
void SMSModule::Setup()
{
 // сообщаем, что мы провайдер HTTP-запросов
 MainController->SetHTTPProvider(1,this); 

  smsToSend = new String();
  cusdSMS = NULL;

  queuedWindowCommand = new String();
  commandToSend = new String();
  customSMSCommandAnswer = new String();

  #ifdef USE_GSM_REBOOT_PIN
    WORK_STATUS.PinMode(GSM_REBOOT_PIN,OUTPUT);
    WORK_STATUS.PinWrite(GSM_REBOOT_PIN,GSM_POWER_ON);
  #endif
  
  // запускаем наш сериал
  GSM_SERIAL.begin(GSM_BAUDRATE);

  if(&(GSM_SERIAL) == &Serial) {
       WORK_STATUS.PinMode(0,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(1,OUTPUT,false);
  } else if(&(GSM_SERIAL) == &Serial1) {
       WORK_STATUS.PinMode(19,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(18,OUTPUT,false);
  } else if(&(GSM_SERIAL) == &Serial2) {
       WORK_STATUS.PinMode(17,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(16,OUTPUT,false);
  } else if(&(GSM_SERIAL) == &Serial3) {
       WORK_STATUS.PinMode(15,INPUT_PULLUP,true);
       WORK_STATUS.PinMode(14,OUTPUT,false);
  }  


  // говорим, что мы от модема не получали ничего
  flags.isAnyAnswerReceived = false;
  flags.inRebootMode = false;
  flags.wantIoTToProcess = false;
  flags.wantBalanceToProcess = false;
  flags.wantHTTPRequest = false;
  flags.inHTTPRequestMode = false;
  httpHandler = NULL;
  httpData = NULL;
  
  rebootStartTime = 0;

  flags.model = M590; // считаем, что у нас по умолчанию Neoway M590
 
  InitQueue(); // инициализируем очередь

#if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)

     iotWriter = NULL;
     iotDone = NULL;
     iotDataHeader = NULL;
     iotDataFooter = NULL;
     iotDataLength = 0;     
     IoTList.RegisterGate(this); // регистрируем себя как отсылателя данных в IoT
#endif  

    #if defined(USE_ALARM_DISPATCHER) && defined(USE_SMS_MODULE) && defined(CLEAR_ALARM_STATUS)
      processedAlarmsClearTimer = millis();
    #endif

   
  // настройка модуля тут
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::InitQueue()
{
  while(actionsQueue.size() > 0) // чистим очередь 
    actionsQueue.pop();
 
  flags.isModuleRegistered = false;
  flags.waitForSMSInNextLine = false;
  WaitForSMSWelcome = false; // не ждём приглашения
  needToWaitTimer = 0; // сбрасываем таймер

  // инициализируем время отсылки команды и получения ответа
  sendCommandTime = millis();
  answerWaitTimer = 0;
   
  // настраиваем то, что мы должны сделать для начала работы
  currentAction = smaIdle; // свободны, ничего не делаем
  actionsQueue.push_back(smaWaitReg); // ждём регистрации
  actionsQueue.push_back(smaSMSSettings); // настройки вывода SMS
  actionsQueue.push_back(smaUCS2Encoding); // кодировка сообщений
  actionsQueue.push_back(smaPDUEncoding); // формат сообщений
  actionsQueue.push_back(smaAON); // включение АОН
  actionsQueue.push_back(smaDisableCellBroadcastMessages); // выключение броадкастовых SMS
  actionsQueue.push_back(smaEchoOff); // выключение эха
  actionsQueue.push_back(smaCheckReady); // проверка готовности
  actionsQueue.push_back(smaCheckModemHardware); // проверяем, какой модем подключен
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::ProcessAnswerLine(String& line)
{
  // от модема получен какой-то ответ, мы можем утверждать, что на той стороне что-то отвечает
  
  // что-то пришло от модема, значит - откликается
  flags.isAnyAnswerReceived = true;
  
  // получаем ответ на команду, посланную модулю
  if(!line.length()) // пустая строка, нечего её разбирать
    return;

  #ifdef GSM_DEBUG_MODE
    Serial.print(F("<== Receive \"")); Serial.print(line); Serial.println(F("\" answer from modem..."));
  #endif

  // проверяем, не перезагрузился ли модем
  if(line.indexOf(F("PBREADY")) != -1 || line.indexOf(F("SMS Ready")) != -1)
  {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Modem boot found, init queue.."));
    #endif

     // убеждаемся, что мы вызвали коллбэк для отсыла данных в IoT
     #if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)
      EnsureIoTProcessed();
     #endif    

     // убеждаемся, что мы обработали HTTP-запрос, пусть и неудачно
     EnsureHTTPProcessed(ERROR_MODEM_NOT_ANSWERING);
     

    InitQueue(); // инициализировали очередь по новой, т.к. модем либо только загрузился, либо - перезагрузился
    needToWaitTimer = GSM_WAIT_BOOT_TIME; // дадим модему ещё 2 секунды на раздупливание

    return;
  }


  bool okFound = false;

  switch(currentAction)
  {
#if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)


    case smaTCPClose: // закрывали соединение
    {
      if(line.startsWith(F("+TCPCLOSE")))
      {
          // наш ответ
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
               #ifdef GSM_DEBUG_MODE
                Serial.println(F("TCP connection closed."));
             #endif       
      }
    }
    break;

    case smaTCPWaitAnswer:
    {
      if(line.startsWith(F("+TCPRECV:")))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
               #ifdef GSM_DEBUG_MODE
                Serial.println(F("Answer received, closing connection..."));
             #endif  

             // говорим, что мы всё послали
             EnsureIoTProcessed(true);
             
             actionsQueue.push_back(smaTCPClose);                  
      }
    }
    break;



    case smaTCPSendData: // писали данные для IoT в поток, проверяем, чего там
    {
      if(line.startsWith(F("+TCPSEND:")))
      {
          // наш ответ
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         // теперь смотрим, правильно ли отослалось?
         String wantedEnd = F("0,");
         wantedEnd += iotDataLength;

         if(line.endsWith(wantedEnd))
         {
              #ifdef GSM_DEBUG_MODE
                Serial.println(F("TCP data sent, waiting answer..."));
             #endif

             actionsQueue.push_back(smaTCPWaitAnswer);

         }
         else
         {
           // не удалось
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("TCP Send FAIL!"));
             #endif

             EnsureIoTProcessed();
          }
      }
    }
    break;

    case smaStartSendIoTData: // отсылали команду на пересылку данных для SIM800L, ждём приглашения
    {
      if(line == F(">"))
      {
        WaitForSMSWelcome = false;
        // дождались приглашения, посылаем данные
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

             #ifdef GSM_DEBUG_MODE
                Serial.println(F("Welcome received, start sending data..."));
             #endif

         actionsQueue.push_back(smaSendDataToSIM800);
      }        
    }
    break;

    case smaTCPSEND: // отсылали данные, ждём приглашения
    {
      if(line == F(">"))
      {
        WaitForSMSWelcome = false;
        // дождались приглашения, посылаем данные
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

             #ifdef GSM_DEBUG_MODE
                Serial.println(F("Welcome received, start sending data..."));
             #endif

         actionsQueue.push_back(smaTCPSendData);
      }
    }
    break;

    case smaTCPSETUP: // коннектились к сервису IoT
    {
      if(line.startsWith(F("+TCPSETUP:")))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         if(line.endsWith(F(",OK")))
         {
          // законнектились, продолжаем
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("IoT connected, continue..."));
             #endif

            actionsQueue.push_back(smaTCPSEND);
         }
         else
         {
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("Unable to connect to IoT!"));
             #endif
           // не удалось законнектиться
            EnsureIoTProcessed(); 
         }
      }
    }
    break;

    case smaCheckPPPIp: // проверяли выбранный адрес
    {
      if(IsKnownAnswer(line,okFound))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         if(flags.isIPAssigned) 
         {
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("IP assigned, start TCP connection..."));
             #endif

          actionsQueue.push_back(smaTCPSETUP);
                          
         }
         else
         {
            // bad
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("No PPP IP assigned!"));
             #endif      
             // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
            EnsureIoTProcessed();
          
         }
      }
      if(line.startsWith(F("+XIIC:")))
      {
         // пришёл адрес, проверяем
         if(line.endsWith(F("0.0.0.0")))
          flags.isIPAssigned = false;
         else
             flags.isIPAssigned = true;
      }
    }
    break;

    case smaXIIC: // устанавливали соединение PPP
    {
      if(IsKnownAnswer(line,okFound))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         if(okFound)
         {
           // good, можем проверять соединение
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("PPP connection established, check for IP address..."));
             #endif

             // Проверяем выданный IP
             flags.isIPAssigned = false;
             actionsQueue.push_back(smaCheckPPPIp);

             needToWaitTimer = 5000; // дадим модему 5 секунд на раздупливание
           
         }
         else
         {
           // bad
            // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
            EnsureIoTProcessed();
  
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("PPP connection failed!"));
             #endif                
         }         
         
      }
      
    }
    break;

    case smaXGAUTH: // авторизовывались в APN
    {    
      if(IsKnownAnswer(line,okFound))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         if(okFound)
         {
           // good, можем продолжать авторизацию
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("Authorized in APN, continue..."));
             #endif

             // устанавливаем соединение PPP
             actionsQueue.push_back(smaXIIC);
           
         }
         else
         {
           // bad
            // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
            EnsureIoTProcessed();
  
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("APN authorization failed!"));
             #endif                
         }
      }      
    }
    break;


    case smaGDCONT: // устанавливали параметры APN для M590
    {
      if(IsKnownAnswer(line,okFound))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
                 
          if(okFound)
          {
            // good
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("APN setup completed, continue..."));
             #endif  

             actionsQueue.push_back(smaXGAUTH);
          }
          else
          {
            // bad
            // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
            EnsureIoTProcessed();
  
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("APN setup failed!"));
             #endif         
            
          }
        
      } // if
      
      
    }
    break;

    case smaCheckGPRSConnection:
    {

         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle; 

       if(line == F("ERROR"))
       {
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("No IP address found!"));
           #endif
          // ошибка подключения
          EnsureIoTProcessed();
        
       }
       else
       {
         // установили подключение
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("IP address obtained, continue..."));
           #endif

          actionsQueue.push_back(smaConnectToIOT);
       }

    }
    break;

    case smaSendDataToSIM800: // отсылали данные через SIM800L
    {
        bool sendOk = line.endsWith(F("SEND OK"));
        bool sendFail = line.endsWith(F("SEND FAIL"));
        bool anyOtherKnownAnswers = line.startsWith(F("+CME")) || line.startsWith(F("DATA"));

        if(sendOk || sendFail || anyOtherKnownAnswers)
        {
           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;

           if(!sendOk)
           {
              // bad
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("Can't send data!"));
             #endif
               EnsureIoTProcessed();
               actionsQueue.push_back(smaCloseGPRSConnection);           
           }
           else
           {
            // good
               #ifdef GSM_DEBUG_MODE
                Serial.println(F("Data sent, waiting for answer..."));
             #endif

             actionsQueue.push_back(smaWaitForIoTAnswer);
           }
         
        }
    }
    break;  


    case smaWaitForIoTAnswer:
    {
      if(line.startsWith(F("HTTP/")))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
               #ifdef GSM_DEBUG_MODE
                Serial.println(F("Answer received, closing connection..."));
             #endif  

             // говорим, что мы всё послали
             EnsureIoTProcessed(true);
             
             actionsQueue.push_back(smaCloseGPRSConnection);                  
      }
    }
    break;      

    case smaConnectToIOT: // коннектились в IOT
    {
       bool isConnectOk = line.endsWith(F("CONNECT OK"));
       bool isConnectFail = line.endsWith(F("CONNECT FAIL"));

       if(!isConnectFail)
       {
        isConnectFail = line.startsWith(F("+CME ERROR")) || line.startsWith(F("STATE:"));
       }

       if(isConnectOk || isConnectFail)
       {
           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;

           if(isConnectFail)
           {
             // bad
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("Can't connect to IoT!"));
           #endif
             EnsureIoTProcessed();
             actionsQueue.push_back(smaCloseGPRSConnection);
           }
           else
           {
             // good
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("IoT connected, continue..."));
           #endif 

              actionsQueue.push_back(smaStartSendIoTData);
           }
        
       } // if
    }
    break;

    case smaStartGPRSConnection: // поднимали соединение с GPRS
    {
      if(IsKnownAnswer(line,okFound)) 
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
                 
        if(!okFound)
        {
          // bad
          // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
          EnsureIoTProcessed();

           #ifdef GSM_DEBUG_MODE
              Serial.println(F("Can't open GPRS connection!"));
           #endif

        }
        else
        {
          // good
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("GPRS connection opened, continue..."));
           #endif 

          actionsQueue.push_back(smaCheckGPRSConnection);           
        }
      }
    }
    break;

    case smaCloseGPRSConnection:
    {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;        
    }
    break;

    case smaStartIoTSend:

      if(IsKnownAnswer(line,okFound)) 
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         if(!okFound) // не срослось
         {

          // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
          EnsureIoTProcessed();

           #ifdef GSM_DEBUG_MODE
              Serial.println(F("IoT request failed!"));
           #endif   

           switch(flags.model)
           {
            case M590:
            break;

            case SIM800:
              actionsQueue.push_back(smaCloseGPRSConnection);
            break;
            
           } // switch
                            
         }
         else
         {
           // всё норм, продолжаем
           switch(flags.model)
           {
            case M590:
              // пихаем в очередь следующую команду
              actionsQueue.push_back(smaGDCONT);
            break;

            case SIM800:
              actionsQueue.push_back(smaStartGPRSConnection);
            break;
            
           } // switch
         } // else
        
      }
      
    break;
#endif

//// ЦИКЛ HTTP////////////////////////////////////////////////////////
    case smaHttpTCPClose: // закрывали соединение
    {
      if(line.startsWith(F("+TCPCLOSE")))
      {
          // наш ответ
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
               #ifdef GSM_DEBUG_MODE
                Serial.println(F("HTTP connection closed."));
             #endif       
      }
    }
    break;

    case smaHttpTCPWaitAnswer:
    {
      if(line.startsWith(F("+TCPRECV:")))
      {
           
          bool enough = false;
          httpHandler->OnAnswerLineReceived(line,enough);
          if(enough)
          {

             #ifdef GSM_DEBUG_MODE
                Serial.println(F("HTTP answer received, closing connection..."));
             #endif
             
             actionsQueue.pop(); // убираем последнюю обработанную команду     
             currentAction = smaIdle;
             
             // говорим, что мы всё послали
             EnsureHTTPProcessed(HTTP_REQUEST_COMPLETED);
             
             actionsQueue.push_back(smaHttpTCPClose);
          }                  
      }
    }
    break;



    case smaHttpTCPSendData: // писали данные для HTTP в поток, проверяем, чего там
    {
      if(line.startsWith(F("+TCPSEND:")))
      {
          // наш ответ
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         // теперь смотрим, правильно ли отослалось?
         String wantedEnd = F("0,");
         wantedEnd += httpData->length();

         if(line.endsWith(wantedEnd))
         {
              #ifdef GSM_DEBUG_MODE
                Serial.println(F("HTTP data sent, waiting answer..."));
             #endif

             actionsQueue.push_back(smaHttpTCPWaitAnswer);

         }
         else
         {
           // не удалось
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("HTTP Send FAIL!"));
             #endif

             EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
          }
      }
    }
    break;

    case smaHttpStartSendDataToService: // отсылали команду на пересылку данных для SIM800L, ждём приглашения
    {
      if(line == F(">"))
      {
        WaitForSMSWelcome = false;
        // дождались приглашения, посылаем данные
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

             #ifdef GSM_DEBUG_MODE
                Serial.println(F("Welcome received, start sending data..."));
             #endif

         actionsQueue.push_back(smaHttpSendDataToSIM800);
      }        
    }
    break;

    case smaHttpTCPSEND: // отсылали данные, ждём приглашения
    {
      if(line == F(">"))
      {
        WaitForSMSWelcome = false;
        // дождались приглашения, посылаем данные
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

             #ifdef GSM_DEBUG_MODE
                Serial.println(F("Welcome received, start sending data..."));
             #endif

         actionsQueue.push_back(smaHttpTCPSendData);
      }
    }
    break;

    case smaHttpTCPSETUP: // коннектились к сервису HTTP
    {
      if(line.startsWith(F("+TCPSETUP:")))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         if(line.endsWith(F(",OK")))
         {
          // законнектились, продолжаем
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("HTTP connected, continue..."));
             #endif

            actionsQueue.push_back(smaHttpTCPSEND);
         }
         else
         {
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("Unable to connect to IoT!"));
             #endif
           // не удалось законнектиться
            EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
         }
      }
    }
    break;

    case smaHttpCheckPPPIp: // проверяли выбранный адрес
    {
      if(IsKnownAnswer(line,okFound))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         if(flags.isIPAssigned) 
         {
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("IP assigned, start TCP connection..."));
             #endif

          actionsQueue.push_back(smaHttpTCPSETUP);
                          
         }
         else
         {
            // bad
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("No PPP IP assigned!"));
             #endif      
             // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
            EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
          
         }
      }
      if(line.startsWith(F("+XIIC:")))
      {
         // пришёл адрес, проверяем
         if(line.endsWith(F("0.0.0.0")))
          flags.isIPAssigned = false;
         else
             flags.isIPAssigned = true;
      }
    }
    break;

    case smaHttpXIIC: // устанавливали соединение PPP
    {
      if(IsKnownAnswer(line,okFound))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         if(okFound)
         {
           // good, можем проверять соединение
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("PPP connection established, check for IP address..."));
             #endif

             // Проверяем выданный IP
             flags.isIPAssigned = false;
             actionsQueue.push_back(smaHttpCheckPPPIp);

             needToWaitTimer = 5000; // дадим модему 5 секунд на раздупливание
           
         }
         else
         {
           // bad
            // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
            EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
  
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("PPP connection failed!"));
             #endif                
         }         
         
      }
      
    }
    break;

    case smaHttpXGAUTH: // авторизовывались в APN
    {    
      if(IsKnownAnswer(line,okFound))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         if(okFound)
         {
           // good, можем продолжать авторизацию
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("Authorized in APN, continue..."));
             #endif

             // устанавливаем соединение PPP
             actionsQueue.push_back(smaHttpXIIC);
           
         }
         else
         {
           // bad
            // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
            EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
  
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("APN authorization failed!"));
             #endif                
         }
      }      
    }
    break;


    case smaHttpGDCONT: // устанавливали параметры APN для M590
    {
      if(IsKnownAnswer(line,okFound))
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
                 
          if(okFound)
          {
            // good
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("APN setup completed, continue..."));
             #endif  

             actionsQueue.push_back(smaHttpXGAUTH);
          }
          else
          {
            // bad
            // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
            EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
  
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("APN setup failed!"));
             #endif         
            
          }
        
      } // if
      
      
    }
    break;

    case smaHttpCheckGPRSConnection:
    {

         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle; 

       if(line == F("ERROR"))
       {
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("No IP address found!"));
           #endif
          // ошибка подключения
          EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
        
       }
       else
       {
         // установили подключение
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("IP address obtained, continue..."));
           #endif

          actionsQueue.push_back(smaHttpConnectToService);
       }

    }
    break;

    case smaHttpSendDataToSIM800: // отсылали данные через SIM800L
    {
        bool sendOk = line.endsWith(F("SEND OK"));
        bool sendFail = line.endsWith(F("SEND FAIL"));
        bool anyOtherKnownAnswers = line.startsWith(F("+CME")) || line.startsWith(F("DATA"));

        if(sendOk || sendFail || anyOtherKnownAnswers)
        {
           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;

           if(!sendOk)
           {
              // bad
             #ifdef GSM_DEBUG_MODE
                Serial.println(F("Can't send data!"));
             #endif
               EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
               actionsQueue.push_back(smaHttpCloseGPRSConnection);           
           }
           else
           {
            // good
               #ifdef GSM_DEBUG_MODE
                Serial.println(F("Data sent, waiting for answer..."));
             #endif

             actionsQueue.push_back(smaHttpWaitForServiceAnswer);
           }
         
        }
    }
    break;  


    case smaHttpWaitForServiceAnswer:
    {
          bool enough = false;
          httpHandler->OnAnswerLineReceived(line,enough);

          if(enough)
          {
              // точно, хватит
             #ifdef HTTP_DEBUG
                 Serial.println(F("HTTP request done."));
             #endif
    
             // говорим, что всё на мази
             EnsureHTTPProcessed(HTTP_REQUEST_COMPLETED);
    
             // и закрываем соединение
              actionsQueue.pop(); // убираем последнюю обработанную команду
              currentAction = smaIdle;         
              
              // поскольку мы законнекчены - надо закрыть соединение
              actionsQueue.push_back(smaHttpCloseGPRSConnection);                         
          }
    }
    break;      

    case smaHttpConnectToService: // коннектились к сервису
    {
       bool isConnectOk = line.endsWith(F("CONNECT OK"));
       bool isConnectFail = line.endsWith(F("CONNECT FAIL"));

       if(!isConnectFail)
       {
        isConnectFail = line.startsWith(F("+CME ERROR")) || line.startsWith(F("STATE:"));
       }

       if(isConnectOk || isConnectFail)
       {
           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;

           if(isConnectFail)
           {
             // bad
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("Can't connect to HTTP!"));
           #endif
             EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
             actionsQueue.push_back(smaHttpCloseGPRSConnection);
           }
           else
           {
             // good
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("IoT connected, continue..."));
           #endif 

              actionsQueue.push_back(smaHttpStartSendDataToService);
           }
        
       } // if
    }
    break;

    case smaHttpStartGPRSConnection: // поднимали соединение с GPRS
    {
      if(IsKnownAnswer(line,okFound)) 
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
                 
        if(!okFound)
        {
          // bad
          // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
          EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);

           #ifdef GSM_DEBUG_MODE
              Serial.println(F("Can't open GPRS connection!"));
           #endif

        }
        else
        {
          // good
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("GPRS connection opened, continue..."));
           #endif 

          actionsQueue.push_back(smaHttpCheckGPRSConnection);           
        }
      }
    }
    break;

    case smaHttpCloseGPRSConnection:
    {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;        
    }
    break;

    case smaStartHTTPSend: // начало отсыла данных по HTTP, с этой точки всё ветвится для разных модемов

      if(IsKnownAnswer(line,okFound)) 
      {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         if(!okFound) // не срослось
         {

          // вызываем функцию обратного вызова и сообщаем, что не удалось ничего
          EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);

           #ifdef GSM_DEBUG_MODE
              Serial.println(F("HTTP request failed!"));
           #endif   

           switch(flags.model)
           {
            case M590:
            break;

            case SIM800:
              actionsQueue.push_back(smaHttpCloseGPRSConnection);
            break;
            
           } // switch
                            
         }
         else
         {
           // всё норм, продолжаем
           switch(flags.model)
           {
            case M590:
              // пихаем в очередь следующую команду
              actionsQueue.push_back(smaHttpGDCONT);
            break;

            case SIM800:
              actionsQueue.push_back(smaHttpStartGPRSConnection);
            break;
            
           } // switch
         } // else
        
      }
      
    break;
// КОНЕЦ ЦИКЛА HTTP ////////////////////////////////////////////    

    case smaCheckModemHardware:
    {
      
      if(line.indexOf(F("M590")) != -1)
        flags.model = M590;
      else if(line.indexOf(F("SIM800")) != -1)
        flags.model = SIM800;
        
       if(IsKnownAnswer(line,okFound)) {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         #ifdef GSM_DEBUG_MODE
          Serial.print(F("[OK] => Modem hardware detected: "));
          switch(flags.model)
          {
            case M590:
              Serial.println(F("Neoway M590"));
            break;

            case SIM800:
              Serial.println(F("SIM800 series"));
            break;
          }
         #endif
      }     
    }
    break;
    
    case smaCheckReady:
    {
      // ждём ответа "+CPAS: 0" от модуля
      // ситуация следующая: ответ от модема может быть получен вместе с эхом,
      // причём ответ не обязан быть +CPAS: 0
      // нам надо обработать ситуацию, когда модем отвечает
      // другими статусами, и перезапускать команду проверки готовности
      // в этом случае. При этом мы должны игнорировать эхо и ответы OK и ERROR

       if( line.startsWith( F("+CPAS:") ) ) {
          // это ответ на команду AT+CPAS, можем его разбирать
          if(line == F("+CPAS: 0")) {
              // модем готов, можем убирать команду из очереди и переходить к следующей
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => Modem ready."));
           #endif
           actionsQueue.pop(); // убираем последнюю обработанную команду
           currentAction = smaIdle; // и переходим на следующую
          }
          else {
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("[ERR] => Modem NOT ready, try again later..."));
           #endif
             needToWaitTimer = 2000; // повторим через 2 секунды
             currentAction = smaIdle; // и пошлём ещё раз команду проверки готовности           
          }
       }
      
    }
    break;

    case smaCheckModemHang:
    {
      // проверяли, отвечает ли модем
      if(IsKnownAnswer(line,okFound)) {
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;

         #ifdef GSM_DEBUG_MODE
          Serial.println(F("[OK] => Modem answered and available."));
         #endif
      }
    }
    break;

    case smaRequestBalance:
    {
         if(IsKnownAnswer(line,okFound))
         {
            if(!okFound)
            {
               // fail
            // проверяли баланс
             actionsQueue.pop(); // убираем последнюю обработанную команду     
             currentAction = smaIdle;
             
              #ifdef GSM_DEBUG_MODE
                Serial.println(F("CUSD Query FAILED!"));
              #endif                 
           }
         }
         else
         {
                // пришёл ответ на команду CUSD ?
                if(line.startsWith(F("+CUSD:"))) 
                {
                   // дождались ответа, парсим
                    int quotePos = line.indexOf('"');
                    int lastQuotePos = line.lastIndexOf('"');
            
                   if(quotePos != -1 && lastQuotePos != -1 && quotePos != lastQuotePos) 
                   {

                      delete cusdSMS;
                      cusdSMS = new String();
            
                      actionsQueue.pop(); // убираем последнюю обработанную команду     
                      currentAction = smaIdle;
             
                      *cusdSMS = line.substring(quotePos+1,lastQuotePos);
                      
                      #ifdef GSM_DEBUG_MODE
                        Serial.println(F("BALANCE RECEIVED, PARSE..."));
                        Serial.print(F("CUSD IS: ")); Serial.println(*cusdSMS);
                        Serial.println(F("Send balance to master..."));
                      #endif
            
                      SendSMS(*cusdSMS,true);
                      delete cusdSMS;
                      cusdSMS = NULL;              
                    
                   }
                   
                } // if(line.startsWith(F("+CUSD:")))              
         } // else
    }
    break;

    case smaEchoOff: // выключили эхо
    {
      if(IsKnownAnswer(line,okFound))
      {
        #ifdef GSM_DEBUG_MODE
          if(okFound)
            Serial.println(F("[OK] => ECHO OFF processed."));
          else
            Serial.println(F("[ERR] => ECHO OFF FAIL!"));
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = smaIdle;
      }
    }
    break;

    case smaDisableCellBroadcastMessages: // запретили получение броадкастовых SMS
    {
      if(IsKnownAnswer(line,okFound))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("[OK] => Broadcast SMS disabled."));
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = smaIdle;
      }
      
    }
    break;

    case smaAON: // включили АОН
    {
      if(IsKnownAnswer(line,okFound))
      {
        if(okFound)
        {
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("[OK] => AON is ON."));
          #endif
        }
          actionsQueue.pop(); // убираем последнюю обработанную команду     
          currentAction = smaIdle;
        /*
        } // if
        else
        {
          // пробуем ещё раз
          needToWaitTimer = 1500; // через некоторое время
          currentAction = smaIdle;
        }
        */
      } // known answer
      
    }
    break;

    case smaPDUEncoding: // формат PDU
    {
      if(IsKnownAnswer(line,okFound))
      {
        if(okFound)
        {
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("[OK] => PDU format is set."));
          #endif
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
        }
        else
        {
            // пробуем ещё раз
          needToWaitTimer = 1500; // через некоторое время
          currentAction = smaIdle;
        
        }
      }
      
    }
    break;

    case smaUCS2Encoding: // кодировка UCS2
    {
      if(IsKnownAnswer(line,okFound))
      {
        if(okFound)
        {
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("[OK] => UCS2 encoding is set."));
          #endif
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
        }
        else
        {
            // пробуем ещё раз
          needToWaitTimer = 1500; // через некоторое время
          currentAction = smaIdle;
        
        }
      }
      
    }
    break;
    

    case smaSMSSettings: // установили режим отображения входящих SMS сразу в порт
    {
      if(IsKnownAnswer(line,okFound))
      {
        if(okFound)
        {
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => SMS settings is set."));
            #endif
           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;
        }
        else
        {
            // пробуем ещё раз
          needToWaitTimer = 1500; // через некоторое время
          currentAction = smaIdle;           
        }
      }
      
    }
    break;

    case smaWaitReg: // пришёл ответ о регистрации
    {
      if(line.indexOf(F("+CREG: 0,1")) != -1)
      {
        // зарегистрированы в GSM-сети
           flags.isModuleRegistered = true;
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => Modem registered in GSM!"));
            #endif
           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;
      } // if
      else
      {
        // ещё не зарегистрированы
          flags.isModuleRegistered = false;
          needToWaitTimer = GSM_CHECK_REGISTRATION_INTERVAL; // через некоторое время повторим команду
          currentAction = smaIdle;
      } // else
    }
    break;

    case smaHangUp: // положили трубку
    {
      if(IsKnownAnswer(line,okFound))
      {
             #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => Hang up DONE."));
            #endif
       
           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;
      } 
      
    }
    break;

    case smaStartSendSMS: // начинаем посылать SMS
    {

        if(line == F(">")) {
          
          // дождались приглашения, можно посылать    
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => Welcome received, continue sending..."));
            #endif

           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;
           actionsQueue.push_back(smaSmsActualSend); // добавляем команду на обработку
        } 
        else {

          // пришло не то, что ждали - просто игнорируем отсыл СМС
          
           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;
            
            #ifdef GSM_DEBUG_MODE
              Serial.print(F("[ERR] => BAD ANWER TO SMS COMMAND: WANT '>', RECEIVED: "));
              Serial.println(line);
            #endif          
        }
      
    }
    break;

    case smaSmsActualSend: // отослали SMS
    {
      if(IsKnownAnswer(line,okFound))
      {
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => SMS sent."));
            #endif
      
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = smaIdle;
       actionsQueue.push_back(smaClearAllSMS); // добавляем команду на обработку
      }
    }
    break;

    case smaClearAllSMS: // очистили все SMS
    {
      if(IsKnownAnswer(line,okFound))
      {
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => saved SMS cleared."));
            #endif
      
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = smaIdle;
      }
     
    }
    break;


    case smaIdle:
    {
      if(flags.waitForSMSInNextLine) // дождались входящего SMS
      {
        flags.waitForSMSInNextLine = false;
        ProcessIncomingSMS(line);
      }
      
      if(line.startsWith(F("+CLIP:")))
        ProcessIncomingCall(line);
      else
      if(line.startsWith(F("+CMT:")))
        flags.waitForSMSInNextLine = true;
       

   

    }
    break;
  } // switch   

 
  
}
//--------------------------------------------------------------------------------------------------------------------------------
bool SMSModule::CanMakeQuery() // тестирует, может ли модуль сейчас сделать запрос
{
  
  if(flags.wantBalanceToProcess || 
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

  return true;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::MakeQuery(HTTPRequestHandler* handler) // начинаем запрос по HTTP
{
    // сперва завершаем обработку предыдущего вызова, если он вдруг нечаянно был
    EnsureHTTPProcessed(ERROR_HTTP_REQUEST_CANCELLED);

    // сохраняем обработчик запроса у себя
    httpHandler = handler;

    // и говорим, что мы готовы работать по HTTP-запросу
    flags.wantHTTPRequest = true;
}
//--------------------------------------------------------------------------------------------------------------------------------  
void SMSModule::ProcessIncomingSMS(const String& line) // обрабатываем входящее SMS
{
  #ifdef GSM_DEBUG_MODE
  Serial.print(F("SMS RECEIVED: ")); Serial.println(line);
  #endif


  bool shouldSendSMS = false;

  GlobalSettings* Settings = MainController->GetSettings();

  PDUIncomingMessage message = PDU.Decode(line, Settings->GetSmsPhoneNumber());
  if(message.IsDecodingSucceed) // сообщение пришло с нужного номера
  {
  
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Phone number is OK, continue..."));
    #endif

    // ищем команды
    int16_t idx = message.Message.indexOf(SMS_OPEN_COMMAND); // открыть окна
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("WINDOWS->OPEN command found, execute it..."));
    #endif

        // открываем окна
        // сохраняем команду на выполнение тогда, когда окна будут открыты или закрыты - иначе она не отработает
        *queuedWindowCommand = F("STATE|WINDOW|ALL|OPEN");
        shouldSendSMS = true;
    }
    
    idx = message.Message.indexOf(SMS_CLOSE_COMMAND); // закрыть окна
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("WINDOWS->CLOSE command found, execute it..."));
    #endif

      // закрываем окна
      // сохраняем команду на выполнение тогда, когда окна будут открыты или закрыты - иначе она не отработает
      *queuedWindowCommand = F("STATE|WINDOW|ALL|CLOSE");
      shouldSendSMS = true;
    }
    
    idx = message.Message.indexOf(SMS_AUTOMODE_COMMAND); // перейти в автоматический режим работы
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Automatic mode command found, execute it..."));
    #endif

      // переводим управление окнами в автоматический режим работы
      if(ModuleInterop.QueryCommand(ctSET, F("STATE|MODE|AUTO"),false))//,false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTSET=STATE|MODE|AUTO command parsed, process it..."));
        #endif
    
      }

      // переводим управление поливом в автоматический режим работы
      if(ModuleInterop.QueryCommand(ctSET, F("WATER|MODE|AUTO"),false))//,false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTSET=WATER|MODE|AUTO command parsed, process it..."));
        #endif
    
      }
     
      // переводим управление досветкой в актоматический режим работы    
      if(ModuleInterop.QueryCommand(ctSET, F("LIGHT|MODE|AUTO"),false))//,false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTSET=LIGHT|MODE|AUTO command parsed, process it..."));
        #endif
    
      }    

      shouldSendSMS = true;
    }

    idx = message.Message.indexOf(SMS_WATER_ON_COMMAND); // включить полив
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Water ON command found, execute it..."));
    #endif

    // включаем полив
      if(ModuleInterop.QueryCommand(ctSET, F("WATER|ON"),false))//,false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTSET=WATER|ON command parsed, process it..."));
        #endif
    
       shouldSendSMS = true;
      }
    }

    idx = message.Message.indexOf(SMS_WATER_OFF_COMMAND); // выключить полив
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Water OFF command found, execute it..."));
    #endif

    // выключаем полив
      if(ModuleInterop.QueryCommand(ctSET, F("WATER|OFF"),false))//,false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTSET=WATER|OFF command parsed, process it..."));
        #endif
    
        shouldSendSMS = true;
      }

    }

           
    idx = message.Message.indexOf(SMS_STAT_COMMAND); // послать статистику
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("STAT command found, execute it..."));
    #endif

      // посылаем статистику вызвавшему номеру
      SendStatToCaller(message.SenderNumber);

      // возвращаемся, поскольку нет необходимости посылать СМС с ответом ОК - вместо этого придёт статистика
      return;
    }

    idx = message.Message.indexOf(SMS_BALANCE_COMMAND); // послать баланс
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("BALANCE command found, execute it..."));
    #endif

      // посылаем баланс хозяину
      RequestBalance();

      // возвращаемся, поскольку нет необходимости посылать СМС с ответом ОК - вместо этого придёт баланс
      return;
    }
    
    if(!shouldSendSMS)
    {
        // тут пробуем найти файл по хэшу переданной команды
        if(MainController->HasSDCard())
        {
          unsigned int hash = hash_str(message.Message.c_str());
         

          #ifdef GSM_DEBUG_MODE
            Serial.print(F("passed message = "));
            Serial.println(message.Message);
            Serial.print(F("computed hash = "));
            Serial.println(hash);
          #endif
                        
          String filePath = F("sms");
          filePath += F("/");
          filePath += hash;
          filePath += F(".sms");
    
          File smsFile = SD.open(filePath);
          if(smsFile)
          {
      
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("SMS file found, continue..."));
          #endif            
            // нашли такой файл, будем читать с него данные
            String answerMessage, commandToExecute;
            char ch = 0;
    
            // в первой строке у нас лежит сообщение, которое надо послать после выполнения команды.
            while(1)
            {
              ch = (char) smsFile.read();
              if(ch == -1)
                break;
                
              if(ch == '\r')
                continue;
              else if(ch == '\n')
                break;
             else
              answerMessage += ch;
            } // while
    
            ch = 0;
    
            // во второй строке - команда
            while(1)
            {
              ch = (char) smsFile.read();
              if(ch == -1 || ch =='\r' || ch == '\n')
                break;
    
             commandToExecute += ch;    
            } // while
    
            // закрываем файл
            smsFile.close();

          #ifdef GSM_DEBUG_MODE
            Serial.print(F("command to execute = "));
            Serial.println(commandToExecute);
          #endif  
            // парсим команду
            CommandParser* cParser = MainController->GetCommandParser();
            Command cmd;
            if(cParser->ParseCommand(commandToExecute,cmd))
            {
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("Command parsed, execute it..."));
          #endif                
              // команду разобрали, можно исполнять
              //customSMSCommandAnswer = "";
              delete customSMSCommandAnswer;
              customSMSCommandAnswer = new String();
              
              cmd.SetIncomingStream(this);
              MainController->ProcessModuleCommand(cmd);

              // теперь получаем ответ
              if(!answerMessage.length()) {
                SendSMS(*customSMSCommandAnswer);
                delete customSMSCommandAnswer;
                customSMSCommandAnswer = new String();
              }
              else
                SendSMS(answerMessage);
              
            } // if
    
            return; // возвращаемся, т.к. мы сами пошлём СМС с текстом, отличным от ОК
          } // if(smsFile)
          #ifdef GSM_DEBUG_MODE
          else
          {
            Serial.println(F("SMS file NOT FOUND, skip the SMS."));
          }
          #endif            
          
        } // if(MainController->HasSDCard())
        
    } // !shouldSendSMS
    
  }
  else
  {
  #ifdef GSM_DEBUG_MODE
    Serial.println(F("Message decoding error or message received from unknown number!"));
  #endif
  }

  if(shouldSendSMS) // надо послать СМС с ответом "ОК"
    SendSMS(OK_ANSWER);


  
}
//--------------------------------------------------------------------------------------------------------------------------------
size_t SMSModule::write(uint8_t toWr)
{
 *customSMSCommandAnswer += (char) toWr;
 return 1; 
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::ProcessIncomingCall(const String& line) // обрабатываем входящий звонок
{
  // приходит строка вида
  // +CLIP: "79182900063",145,,,"",0
  
   // входящий звонок, проверяем, приняли ли мы конец строки?
    String ring = line.substring(8); // пропускаем команду +CLIP:, пробел и открывающую кавычку "

    int idx = ring.indexOf("\"");
    if(idx != -1)
      ring = ring.substring(0,idx);

    if(ring.length() && ring[0] != '+')
      ring = String(F("+")) + ring;
      
      #ifdef GSM_DEBUG_MODE
          Serial.print(F("RING DETECTED: ")); Serial.println(ring);
      #endif

  GlobalSettings* Settings = MainController->GetSettings();
  if(ring != Settings->GetSmsPhoneNumber()) // не наш номер
  {
    #ifdef GSM_DEBUG_MODE
      Serial.print(F("UNKNOWN NUMBER: ")); Serial.print(ring); Serial.println(F("!"));
    #endif

 // добавляем команду "положить трубку"
  actionsQueue.push_back(smaHangUp);
    
    return;
  }

  // отправляем статистику вызвавшему номеру
   SendStatToCaller(ring); // посылаем статистику вызвавшему
  
 // добавляем команду "положить трубку" - она выполнится первой, а потом уже уйдёт SMS
  actionsQueue.push_back(smaHangUp);
 
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::SendCommand(const String& command, bool addNewLine)
{
  #ifdef GSM_DEBUG_MODE
    Serial.print(F("==> Send the \"")); Serial.print(command); Serial.println(F("\" command to modem..."));
  #endif

  // запоминаем время отсылки последней команды
  sendCommandTime = millis();
  answerWaitTimer = 0;

  GSM_SERIAL.write(command.c_str(),command.length());
  
  if(addNewLine)
  {
    GSM_SERIAL.write(String(NEWLINE).c_str());
  }
      
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::ProcessQueue()
{
  
  if(currentAction != smaIdle) // чем-то заняты, не можем ничего делать
    return;

    size_t sz = actionsQueue.size();
    if(!sz) 
    { // в очереди ничего нет

      #if defined(USE_ALARM_DISPATCHER) && defined(USE_SMS_MODULE)

        if(flags.isModuleRegistered && flags.isAnyAnswerReceived && !flags.inRebootMode)
        {        
          // проверяем, есть ли для нас тревоги
          AlarmDispatcher* alD = MainController->GetAlarmDispatcher();
          if(alD->HasSMSAlarm())
          {
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("HAS ALARM VIA SMS, send it..."));
            #endif

            // имеем тревогу, которую надо послать по СМС
            String dt = alD->GetSMSAlarmData();
            alD->MarkSMSAlarmDone();
            SendSMS(dt,false);

            return; // возвращаемся, ибо мы уже очередь пополнили
          }
        }
      #endif

        if(flags.wantBalanceToProcess) // запросили баланс
        {
          flags.wantBalanceToProcess = false;
          actionsQueue.push_back(smaRequestBalance);
          return;
        }

        if(flags.wantHTTPRequest)
        {
          // от нас ждут запроса по HTTP
          flags.wantHTTPRequest = false;
          flags.inHTTPRequestMode = true;
          actionsQueue.push_back(smaStartHTTPSend);
  
          return; // возвращаемся, здесь делать нефик
        }        


      #if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)
      if(!flags.inHTTPRequestMode && (flags.wantIoTToProcess && iotWriter && iotDone) )
      {
        // надо поместить в очередь команду на обработку запроса к IoT
        flags.wantIoTToProcess = false;
        actionsQueue.push_back(smaStartIoTSend);
        return;
      }
      #endif      

      // тут проверяем - можем ли мы протестировать доступность модема?
      if(millis() - sendCommandTime > GSM_AVAILABLE_CHECK_TIME) {
          // раз в минуту можно проверить доступность модема,
          // и делаем мы это ТОЛЬКО тогда, когда очередь пуста как минимум GSM_AVAILABLE_CHECK_TIME мс, т.е. все текущие команды отработаны.
          actionsQueue.push_back(smaCheckModemHang);
      }
      
      return;
    }
      
    currentAction = actionsQueue[sz-1]; // получаем очередную команду

    // смотрим, что за команда
    switch(currentAction)
    {

      //////////////////////////// ЦИКЛ HTTP ////////////////////////////////////////
      case smaHttpTCPWaitAnswer: // ждём ответа, ничего модему не посылаем
      break;
      
      case smaHttpTCPClose: // закрываем соединение
      {
        SendCommand(F("AT+TCPCLOSE=0"));
      }
      break;

      case smaHttpSendDataToSIM800: // отсылаем данные через SIM800
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Send data to HTTP using SIM800..."));
        #endif  
          if(httpData)
          {      
            // тут посылаем данные пр HTTP
            SendCommand(*httpData,false);
            GSM_SERIAL.write(0x1A);
            delete httpData;
            httpData = NULL;       
          }
          else
          {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("HTTP data is INVALID!"));
        #endif  
        // чего-то в процессе не задалось, вызываем коллбэк, и говорим, что всё плохо
            EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
            actionsQueue.pop();
            currentAction = smaIdle;
          }        
          
      }
      break;

      case smaHttpTCPSendData: // отсылаем данные
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Send data to HTTP using Neoway..."));
        #endif  
          if(httpData)
          {      
            // тут посылаем данные в IoT
            SendCommand(*httpData,false);
            GSM_SERIAL.write(0x0D);       
          }
          else
          {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("HTTP data is INVALID!"));
        #endif  
        // чего-то в процессе не задалось, вызываем коллбэк, и говорим, что всё плохо
            EnsureHTTPProcessed(ERROR_HTTP_REQUEST_FAILED);
            actionsQueue.pop();
            currentAction = smaIdle;
          }        
      }
      break;

      case smaHttpWaitForServiceAnswer: // ничего не делаем, просто ждём ответа
      break;

      case smaHttpStartSendDataToService: // отсылаем данные для SIM800L
      {
           #ifdef GSM_DEBUG_MODE
            Serial.println(F("Start sending data command..."));
          #endif
          
          delete httpData;
          httpData = new String();
          httpHandler->OnAskForData(httpData);
          
          String command = F("AT+CIPSEND=");
          command += httpData->length();
          WaitForSMSWelcome = true; // выставляем флаг, что мы ждём >
          SendCommand(command);        
      }
      break;


      case smaHttpTCPSEND: // начинаем посылать данные
      {
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("Start sending data command..."));
          #endif

          delete httpData;
          httpData = new String();
          httpHandler->OnAskForData(httpData);

          String command = F("AT+TCPSEND=0,");
          command += httpData->length();
          WaitForSMSWelcome = true; // выставляем флаг, что мы ждём >
          SendCommand(command); 
      }
      break;

      case smaHttpTCPSETUP: // устанавливаем TCP-соединение
      {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Connect to HTTP..."));
        #endif

        String command = F("AT+TCPSETUP=0,");
        String host;
        httpHandler->OnAskForHost(host);
        command += host;

        command += F(",80");
        
        SendCommand(command);        
      }
      break;
      

      case smaHttpCheckPPPIp: // проверяем выданный IP
      {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Check PPP IP-address..."));
        #endif

        SendCommand(F("AT+XIIC?"));
        
      }
      break;

      case smaHttpXIIC: // устанавливаем соединение PPP
      {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Establish PPP connection..."));
        #endif

        SendCommand(F("AT+XIIC=1"));
        
      }
      break;

      case smaHttpXGAUTH: // авторизуемся в APN
      {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Authorize in APN..."));
        #endif 
        String command = F("AT+XGAUTH=1,1,\"");

        // тут проверяем оператора, в зависимости от этого формируем нужную команду
        String user,pass;
        GetAPNUserPass(user,pass);
        
        command += user;
        command += F("\",\"");
        command += pass;
        command += F("\"");
        
        SendCommand(command);                         
      }
      break;

      case smaHttpGDCONT: // параметры для Neoway
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Setup APN for HTTP connection..."));
        #endif        
        // устанавливаем параметры PDP-контекста, эта команда актуальна для M590.
        // начиная с этой команды - мы идём по отдельным веткам моделей модема, поэтому проверять модель модема тут необязательно.
        String command = F("AT+CGDCONT=1,\"IP\",\"");

        // тут проверяем оператора, в зависимости от этого формируем нужную команду
        command += GetAPN();
        command += F("\"");
        SendCommand(command);
        
      }
      break;


      case smaHttpCheckGPRSConnection:
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Checking GPRS connection..."));
        #endif
        SendCommand(F("AT+CIFSR"));
      break;

      case smaHttpStartGPRSConnection:
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Open GPRS connection..."));
        #endif
        SendCommand(F("AT+CIICR"));
        
      }
      break;

      case smaHttpConnectToService:
      {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Start connect to HTTP..."));
        #endif

        String command = F("AT+CIPSTART=\"TCP\",\"");

        String host;
        httpHandler->OnAskForHost(host);
        command += host;

        command += F("\",80");

        SendCommand(command);
         
      }
      break;

      case smaHttpCloseGPRSConnection:
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Shutdown GPRS connection..."));
        #endif
        SendCommand(F("AT+CIPSHUT"));
      }
      break;
      
      case smaStartHTTPSend:
      {
          // надо отослать данные по HTTP
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Connect to HTTP using GPRS..."));
        #endif
    
        String comm;

        switch(flags.model)
        {
          case M590:
            comm = F("AT+XISP=0"); // сначала переключаемся на внутренний стек протокола TCP/IP
          break;

          case SIM800:

            comm = F("AT+CSTT=\"");
            comm += GetAPN();
            comm += F("\",\"");
            String user,pass;
            GetAPNUserPass(user,pass);
            comm += user;
            comm += F("\",\"");
            comm += pass;
            comm += F("\"");

          break;
          
        } // switch

        SendCommand(comm);   
      }     
      break;   
      //////////////////////////// ЦИКЛ HTTP КОНЧИЛСЯ////////////////////////////////////////
      
#if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)

      case smaTCPWaitAnswer: // ждём ответа, ничего модему не посылаем
      break;
      
      case smaTCPClose: // закрываем соединение
      {
        SendCommand(F("AT+TCPCLOSE=0"));
      }
      break;

      case smaSendDataToSIM800: // отсылаем данные через SIM800
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Send data to IoT using GSM..."));
        #endif  
          if(iotDataHeader && iotDataFooter && iotWriter && iotDone)
          {      
            // тут посылаем данные в IoT
            SendCommand(*iotDataHeader,false);
            iotWriter(&(GSM_SERIAL));
            SendCommand(*iotDataFooter,false);
            GSM_SERIAL.write(0x1A);       
          }
          else
          {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("IoT data is INVALID!"));
        #endif  
        // чего-то в процессе не задалось, вызываем коллбэк, и говорим, что всё плохо
            EnsureIoTProcessed();
            actionsQueue.pop();
            currentAction = smaIdle;
          }        
          
      }
      break;

      case smaTCPSendData: // отсылаем данные
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Send data to IoT using GSM..."));
        #endif  
          if(iotDataHeader && iotDataFooter && iotWriter && iotDone)
          {      
            // тут посылаем данные в IoT
            SendCommand(*iotDataHeader,false);
            iotWriter(&(GSM_SERIAL));
            SendCommand(*iotDataFooter,false);
            GSM_SERIAL.write(0x0D);       
          }
          else
          {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("IoT data is INVALID!"));
        #endif  
        // чего-то в процессе не задалось, вызываем коллбэк, и говорим, что всё плохо
            EnsureIoTProcessed();
            actionsQueue.pop();
            currentAction = smaIdle;
          }        
      }
      break;

      case smaWaitForIoTAnswer: // ничего не делаем, просто ждём ответа
      break;

      case smaStartSendIoTData: // отсылаем данные для SIM800L
      {
           #ifdef GSM_DEBUG_MODE
            Serial.println(F("Start sending data command..."));
          #endif

          String command = F("AT+CIPSEND=");
          command += iotDataLength;
          WaitForSMSWelcome = true; // выставляем флаг, что мы ждём >
          SendCommand(command);        
      }
      break;


      case smaTCPSEND: // начинаем посылать данные
      {
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("Start sending data command..."));
          #endif

          String command = F("AT+TCPSEND=0,");
          command += iotDataLength;
          WaitForSMSWelcome = true; // выставляем флаг, что мы ждём >
          SendCommand(command); 
      }
      break;

      case smaTCPSETUP: // устанавливаем TCP-соединение
      {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Connect to IoT..."));
        #endif

        String command = F("AT+TCPSETUP=0,");
        switch(iotService)
        {
          case iotThingSpeak:
            command += THINGSPEAK_IP;
          break;

          //TODO: Тут другие сервисы!!!
        }

        command += F(",80");
        
        SendCommand(command);        
      }
      break;
      

      case smaCheckPPPIp: // проверяем выданный IP
      {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Check PPP IP-address..."));
        #endif

        SendCommand(F("AT+XIIC?"));
        
      }
      break;

      case smaXIIC: // устанавливаем соединение PPP
      {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Establish PPP connection..."));
        #endif

        SendCommand(F("AT+XIIC=1"));
        
      }
      break;

      case smaXGAUTH: // авторизуемся в APN
      {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Authorize in APN..."));
        #endif 
        String command = F("AT+XGAUTH=1,1,\"");

        // тут проверяем оператора, в зависимости от этого формируем нужную команду
        String user,pass;
        GetAPNUserPass(user,pass);
        
        command += user;
        command += F("\",\"");
        command += pass;
        command += F("\"");
        
        SendCommand(command);                         
      }
      break;

     // case smaSIM800GDCONT: // параметры для SIM800
      case smaGDCONT: // параметры для Neoway
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Setup APN for IoT connection..."));
        #endif        
        // устанавливаем параметры PDP-контекста, эта команда актуальна для M590.
        // начиная с этой команды - мы идём по отдельным веткам моделей модема, поэтому проверять модель модема тут необязательно.
        String command = F("AT+CGDCONT=1,\"IP\",\"");

        // тут проверяем оператора, в зависимости от этого формируем нужную команду
        command += GetAPN();
        command += F("\"");
        SendCommand(command);
        
      }
      break;


      case smaCheckGPRSConnection:
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Checking GPRS connection..."));
        #endif
        //SendCommand(F("AT+CGATT?")); 
        SendCommand(F("AT+CIFSR"));
      break;

      case smaStartGPRSConnection:
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Open GPRS connection..."));
        #endif
        SendCommand(F("AT+CIICR"));
        
      }
      break;

      case smaConnectToIOT:
      {
         #ifdef GSM_DEBUG_MODE
          Serial.println(F("Start connect to IoT..."));
        #endif

        String command = F("AT+CIPSTART=\"TCP\",\"");

        switch(iotService)
        {
          case iotThingSpeak:
            command += THINGSPEAK_IP;
          break;

          //TODO: Тут другие сервисы!!!
        }

        command += F("\",80");

        SendCommand(command);
         
      }
      break;

      case smaCloseGPRSConnection:
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Shutdown GPRS connection..."));
        #endif
        SendCommand(F("AT+CIPSHUT"));
      }
      break;
      
      case smaStartIoTSend:
      {
          // надо отослать данные в IOT
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Connect to IoT using GPRS..."));
        #endif
    
        String comm;

        switch(flags.model)
        {
          case M590:
            comm = F("AT+XISP=0"); // сначала переключаемся на внутренний стек протокола TCP/IP
          break;

          case SIM800:

            //comm = F("AT+CGATT=1"); // подключаемся к GPRS
            comm = F("AT+CSTT=\"");
            comm += GetAPN();
            comm += F("\",\"");
            String user,pass;
            GetAPNUserPass(user,pass);
            comm += user;
            comm += F("\",\"");
            comm += pass;
            comm += F("\"");

          break;
          
        } // switch

        SendCommand(comm);   
      }     
      break;      
      
#endif

      case smaCheckModemHardware:
      {
        // надо проверить железку модема
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Request for modem hardware..."));
      #endif
      SendCommand(F("AT+CGMM"));
      }
      break;
      
      case smaCheckReady:
      {
        // надо проверить модуль на готовность
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Check for modem READY..."));
      #endif
      SendCommand(F("AT+CPAS"));
      }
      break;

      case smaCheckModemHang:
      {
          // проверяем, не завис ли модем?
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Check if modem available..."));
        #endif
        SendCommand(F("AT"));
      }
      break;

      case smaRequestBalance:
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("Request balance..."));
        #endif

        String balanceCommand = MTS_BALANCE;
        byte op = MainController->GetSettings()->GetGSMProvider();

        switch(op)
        {
          case MTS:
          break;

          case Beeline:
            balanceCommand = BEELINE_BALANCE;
          break;

          case Megafon:
            balanceCommand = MEGAFON_BALANCE;
          break;

          case Tele2:
            balanceCommand = TELE2_BALANCE;
          break;

          case Yota:
            balanceCommand = YOTA_BALANCE;
          break;

          case MTS_Bel:
            balanceCommand = MTS_BEL_BALANCE;
          break;

          case Velcom_Bel:
            balanceCommand = VELCOM_BEL_BALANCE;
          break;

          case Privet_Bel:
            balanceCommand = PRIVET_BEL_BALANCE;
          break;
          
          case Life_Bel:
            balanceCommand = LIFE_BEL_BALANCE;
          break;

        }
        //SendCommand(balanceCommand);

        unsigned int bp = 0;
        String out;
        
        PDU.UTF8ToUCS2(balanceCommand, bp, &out);

        String completeCommand = F("AT+CUSD=1,\"");
        completeCommand += out;
        completeCommand += F("\"");

        SendCommand(completeCommand);
        
        
      }
      break;

      case smaEchoOff:
      {
        // выключаем эхо
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Disable echo..."));
      #endif
      SendCommand(F("ATE0"));
      }
      break;

      case smaDisableCellBroadcastMessages:
      {
        // выключаем эхо
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Disable cell broadcast SMS..."));
      #endif
      SendCommand(F("AT+CSCB=1"));
      }
      break;

      case smaAON:
      {
        // включаем АОН
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Turn AON ON..."));
      #endif
      SendCommand(F("AT+CLIP=1"));
      }
      break;

      case smaPDUEncoding: // устанавливаем формат сообщений
      {

      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Set PDU format..."));
      #endif
      
       SendCommand(F("AT+CMGF=0"));
        
      }
      break;


      case smaUCS2Encoding: // устанавливаем кодировку сообщений
      {

      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Set UCS2 format..."));
      #endif
      
       SendCommand(F("AT+CSCS=\"UCS2\""));
        
      }
      break;

      case smaSMSSettings: // устанавливаем режим отображения SMS
      {
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Set SMS output mode..."));
      #endif
      SendCommand(F("AT+CNMI=2,2"));
      
      }
      break;

      case smaWaitReg: // ждём регистрации модуля в сети
      {
     #ifdef GSM_DEBUG_MODE
        Serial.println(F("Check registration status..."));
      #endif
      SendCommand(F("AT+CREG?"));
        
      }
      break;

      case smaHangUp: // кладём трубку
      {
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Hang up..."));
      #endif
      SendCommand(F("ATH"));
       
      }
      break;

      case smaStartSendSMS: // начало отсылки SMS
      {
        #ifdef GSM_DEBUG_MODE
        Serial.println(F("Start SMS sending..."));
        #endif
        
        SendCommand(*commandToSend);
        //commandToSend = "";
        delete commandToSend;
        commandToSend = new String();
      
       
      }
      break;

      case smaSmsActualSend: // отсылаем данные SMS
      {
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Start sending SMS data..."));
      #endif
      
        SendCommand(*smsToSend,false);
        GSM_SERIAL.write(0x1A); // посылаем символ окончания посыла
        //smsToSend = "";
        delete smsToSend;
        smsToSend = new String();
        
        
      }
      break;

      case smaClearAllSMS: // надо очистить все SMS
      {
       #ifdef GSM_DEBUG_MODE
        Serial.println(F("SMS clearance..."));
      #endif
      SendCommand(F("AT+CMGD=1,4"));
       
      }
      break;


      case smaIdle:
      {
        // ничего не делаем
      }
      break;
      
    } // switch
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::EnsureHTTPProcessed(uint16_t statusCode)
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
  httpHandler = NULL;
  delete httpData;
  httpData = NULL;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::RebootModem()
{
  // перезагружаем модем тут
  #ifdef GSM_DEBUG_MODE
    Serial.println(F("[ERR] - GSM-modem not answering, reboot it..."));
  #endif

  // мы в процессе перезагрузки
  flags.inRebootMode = true;

  // запоминаем время выключения питания
  rebootStartTime = millis();

  //Тут выключение питания модема
  #ifdef USE_GSM_REBOOT_PIN
    WORK_STATUS.PinWrite(GSM_REBOOT_PIN,GSM_POWER_OFF);
  #endif

    
}
//--------------------------------------------------------------------------------------------------------------------------------
#if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)
void SMSModule::EnsureIoTProcessed(bool success)
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
void SMSModule::Update(uint16_t dt)
{ 

    #if defined(USE_ALARM_DISPATCHER) && defined(USE_SMS_MODULE) && defined(CLEAR_ALARM_STATUS)
    
      unsigned long curAlarmsTimer = millis();
      unsigned long wantedAlarmsClearInterval = ALARM_CLEAR_INTERVAL*60000;

      if((curAlarmsTimer - processedAlarmsClearTimer) > wantedAlarmsClearInterval)
      {
        // настало время очистить сработавшие тревоги
        processedAlarmsClearTimer = curAlarmsTimer;
        AlarmDispatcher* alD = MainController->GetAlarmDispatcher();
        if(alD)
          alD->ClearProcessedAlarms();
        
      }

    #endif


  if(flags.inRebootMode) {
    // мы в процессе перезагрузки модема, надо проверить, пора ли включать питание?
    if(millis() - rebootStartTime > GSM_REBOOT_TIME) {
      // две секунды держали питание выключенным, можно включать
      flags.inRebootMode = false;
      flags.isAnyAnswerReceived = false; // говорим, что мы ничего от модема не получали

      // делать что-либо дополнительное не надо, т.к. как только от модема в порт упадёт строка о готовности - очередь проинициализируется сама.

      // ТУТ включение питания модема 
      #ifdef USE_GSM_REBOOT_PIN
        WORK_STATUS.PinWrite(GSM_REBOOT_PIN,GSM_POWER_ON);
      #endif
      needToWaitTimer = GSM_WAIT_AFTER_REBOOT_TIME; // дадим модему GSM_WAIT_AFTER_REBOOT_TIME мс на раздупление, прежде чем начнём что-либо делать

      #ifdef GSM_DEBUG_MODE
        Serial.println(F("[REBOOT] - Modem rebooted, wait for ready..."));
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

  if(currentAction != smaIdle) // только если мы в процессе обработки команды, то
    answerWaitTimer += dt; // увеличиваем время ожидания ответа на последнюю команду

   // сначала проверяем - а не слишком ли долго мы ждём ответа от модема?
  if(answerWaitTimer > GSM_MAX_ANSWER_TIME) {

     // тут смотрим - возможно, нам надо вызвать функцию обратного вызова для IoT
     #if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)
      EnsureIoTProcessed();
     #endif    
     
    // тут убеждаемся, что мы сообщили вызывающей стороне о неуспешном запросе по HTTP
     EnsureHTTPProcessed(ERROR_MODEM_NOT_ANSWERING);
     
     // очень долго, надо перезапустить последнюю команду.
     // причём лучше всего перезапустить всё сначала
     InitQueue();
     needToWaitTimer = GSM_WAIT_AFTER_REBOOT_TIME; // ещё через 5 секунд попробуем
     sendCommandTime = millis(); // сбросили таймера
     answerWaitTimer = 0;

     if(flags.isAnyAnswerReceived) {
        // получали хоть один ответ от модема - возможно, он завис?
        RebootModem();
        
     } else {
        // ничего не получали, модема не подсоединено?
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("[ERR] - GSM-modem not found, check for presence after short time..."));
        #endif
     }
     
  } 
  if(!flags.inRebootMode) { // если мы не в процессе перезагрузки - то можем отрабатывать очередь
    ProcessQueue();
    ProcessQueuedWindowCommand(dt);
  }

}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::ProcessQueuedWindowCommand(uint16_t dt)
{
    if(!queuedWindowCommand->length()) // а нет команды на управление окнами
    {
      queuedTimer = 0; // обнуляем таймер
      return;
    }

    queuedTimer += dt;
    if(queuedTimer < 3000) // не дёргаем чаще, чем раз в три секунды
      return;

    queuedTimer = 0; // обнуляем таймер ожидания

      if(ModuleInterop.QueryCommand(ctGET,F("STATE|WINDOW|ALL"),false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTGET=STATE|WINDOW|ALL command parsed, process it..."));
          Serial.println(PublishSingleton.Text);
        #endif
    

        // теперь проверяем ответ. Если окна не в движении - нам вернётся OPEN или CLOSED последним параметром.
        // только в этом случае мы можем исполнять команду
        const char* strPtr = PublishSingleton.Text.c_str();
        int16_t idx = PublishSingleton.Text.lastIndexOf(PARAM_DELIMITER);
        if(idx != -1)
        {
          strPtr += idx + 1;
          
              if((strstr_P(strPtr,(const char*)STATE_OPEN) && !strstr_P(strPtr,(const char*)STATE_OPENING)) || strstr_P(strPtr,(const char*)STATE_CLOSED))
              {
                // окна не двигаются, можем отправлять команду
                 if(ModuleInterop.QueryCommand(ctSET,*queuedWindowCommand,false))
                 {
           
                  // команда разобрана, можно выполнять
                    //queuedWindowCommand = ""; // очищаем команду, нам она больше не нужна
                    delete queuedWindowCommand;
                    queuedWindowCommand = new String();

                    // всё, команда выполнена, когда окна не находились в движении
                 } // if
                
              } // if(state == STATE_OPEN || state == STATE_CLOSED)
              
         } // if(idx != -1)
        
      } // if(cParser->ParseCommand(F("CTGET=STATE|WINDOW|ALL")
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::SendStatToCaller(const String& phoneNum)
{
  #ifdef GSM_DEBUG_MODE
    Serial.println("Try to send stat SMS to " + phoneNum + "...");
  #endif

  GlobalSettings* Settings = MainController->GetSettings();
  if(phoneNum != Settings->GetSmsPhoneNumber()) // не наш номер
  {
    #ifdef GSM_DEBUG_MODE
      Serial.println("NOT RIGHT NUMBER: " + phoneNum + "!");
    #endif
    
    return;
  }

  AbstractModule* stateModule = MainController->GetModuleByID(F("STATE"));

  if(!stateModule)
  {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Unable to find STATE module registered!"));
    #endif
    
    return;
  }


  // получаем температуры
  OneState* os1 = stateModule->State.GetState(StateTemperature,0);
  OneState* os2 = stateModule->State.GetState(StateTemperature,1);

  String sms;

   if(os1)
  {
    TemperaturePair tp = *os1;
  
    sms += T_INDOOR; // сообщение
    if(tp.Current.Value != NO_TEMPERATURE_DATA)
      sms += tp.Current;
    else
      sms += NO_DATA;
      
    sms += NEWLINE;
    
  } // if 

  if(os2)
  {
    TemperaturePair tp = *os2;
  
    sms += T_OUTDOOR;
    if(tp.Current.Value != NO_TEMPERATURE_DATA)
      sms += tp.Current;
    else
      sms += NO_DATA;
    
    sms += NEWLINE;
  } // if


  // тут получаем состояние окон
  if(ModuleInterop.QueryCommand(ctGET,F("STATE|WINDOW|0"),true))
  {

    sms += W_STATE;

    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Command CTGET=STATE|WINDOW|0 parsed, execute it..."));
    #endif

    const char* strPtr = PublishSingleton.Text.c_str();
     if(strstr_P(strPtr,(const char*) STATE_OPEN))
        sms += W_OPEN;
      else
        sms += W_CLOSED;


     sms += NEWLINE;
 
    #ifdef GSM_DEBUG_MODE
      Serial.print(F("Receive answer from STATE: ")); Serial.println(PublishSingleton.Text);
    #endif
  }
    // получаем состояние полива
  if(ModuleInterop.QueryCommand(ctGET,F("WATER"),true))
  {
    sms += WTR_STATE;

    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Command CTGET=WATER parsed, execute it..."));
    #endif

    const char* strPtr = PublishSingleton.Text.c_str();
    if(strstr_P(strPtr,(const char*) STATE_OFF))
      sms += WTR_OFF;
    else
      sms += WTR_ON;
          
  }

  // тут отсылаем SMS
  SendSMS(sms);

}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::SendSMS(const String& sms, bool isSMSInUCS2Format)
{
  #ifdef GSM_DEBUG_MODE
    Serial.print(F("Send SMS:  ")); Serial.println(sms);
  #endif

  if(!flags.isModuleRegistered)
  {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Module not registered!"));
    #endif

    return;
  }

  GlobalSettings* Settings = MainController->GetSettings();
  String num = Settings->GetSmsPhoneNumber();
  if(num.length() < 1)
  {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("No phone number saved in controller!"));
    #endif
    
    return;
  }
  
  PDUOutgoingMessage pduMessage = PDU.Encode(num,sms,true, smsToSend,isSMSInUCS2Format);
  *commandToSend = F("AT+CMGS="); *commandToSend += String(pduMessage.MessageLength);

  #ifdef GSM_DEBUG_MODE
    Serial.print(F("commandToSend = ")); Serial.println(*commandToSend);
    Serial.print(F("SMS message length = ")); Serial.println(pduMessage.MessageLength);    
    Serial.print(F("SMS to send = ")); Serial.println(*smsToSend);
  #endif

  WaitForSMSWelcome = true; // выставляем флаг, что мы ждём >
  actionsQueue.push_back(smaStartSendSMS); // добавляем команду на обработку
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::RequestBalance() 
{
  // выставляем только флаг запроса баланса, и баланс будет запрошен только тогда, когда очередь пустая, т.е. не разрываем цепочки команд
  flags.wantBalanceToProcess = true;
}
//--------------------------------------------------------------------------------------------------------------------------------
bool  SMSModule::ExecCommand(const Command& command, bool wantAnswer)
{
  UNUSED(wantAnswer);

  size_t argsCount = command.GetArgsCount();
  GlobalSettings* Settings = MainController->GetSettings();
  
  if(command.GetType() == ctSET) 
  {
    if(!argsCount) // нет аргументов
    {
      PublishSingleton = PARAMS_MISSED;
    }
    else
    {
      String t = command.GetArg(0);
      if(t == F("ADD"))
      {
        if(argsCount < 4)
        {
          PublishSingleton = PARAMS_MISSED;
        }
        else
        {
            if(MainController->HasSDCard())
            {
              
              // добавить кастомное СМС

              // получаем закодированное в HEX сообщение
              const char* hexMessage = command.GetArg(1);
              String message;

              // переводим его в UTF-8
              while(*hexMessage)
              {
                message += (char) WorkStatus::FromHex(hexMessage);
                hexMessage += 2;
              }

              // получаем его хэш
              unsigned int hash = hash_str(message.c_str());

              #ifdef GSM_DEBUG_MODE
                Serial.print(F("passed message = "));
                Serial.println(message);
                Serial.print(F("computed hash = "));
                Serial.println(hash);
              #endif
              // создаём имя файлв
              String filePath = F("sms");
              SD.mkdir(filePath);
              filePath += F("/");
              filePath += hash;
              filePath += F(".sms");

              File smsFile = SD.open(filePath,FILE_WRITE | O_TRUNC);
              if(smsFile)
              {
                // в аргументе номер 2 у нас лежит ответ, который надо послать
                hexMessage = command.GetArg(2);
                message = "";
    
                  // переводим его в UTF-8
                  while(*hexMessage)
                  {
                    message += (char) WorkStatus::FromHex(hexMessage);
                    hexMessage += 2;
                  }

                // пишем первой строчкой ответ, который надо послать
                smsFile.print(message.c_str());
                smsFile.println("");
                
                // теперь пишем команду, которую надо выполнить
                for(uint8_t i=3;i<argsCount;i++)
                {
                  const char* arg = command.GetArg(i);
                  smsFile.print(arg);
                  if(i < (argsCount-1))
                    smsFile.write('|');
                } // for
                
                smsFile.println("");

                // закрываем файл
                smsFile.close();
              } // if(smsFile)

    
              PublishSingleton = REG_SUCC;
              PublishSingleton.Status = true;
              
            } // if(MainController->HasSDCard())
            else
              PublishSingleton = NOT_SUPPORTED;
        } // else
        
      } // ADD
      else if(t == F("PROV"))
      {
        // запросили установить провайдера GSM
        if(argsCount < 2)
        {
          PublishSingleton = F("PROV");
          PublishSingleton << PARAM_DELIMITER;
          PublishSingleton << PARAMS_MISSED;
        }
        else
        {
          String helper = command.GetArg(1);
          byte p = (byte) helper.toInt();
          GlobalSettings* s = MainController->GetSettings();
          if(s->SetGSMProvider(p))
          {
            PublishSingleton.Status = true;
            PublishSingleton = F("PROV");
            PublishSingleton << PARAM_DELIMITER << REG_SUCC;
//            s->Save();
          }
          else
          {
            
            PublishSingleton = F("PROV");
            PublishSingleton << PARAM_DELIMITER << PARAMS_MISSED;
          }
                    
        } // enough args
        
        
      }
      else
        PublishSingleton = UNKNOWN_COMMAND;
      
    } // else have args
  }
  else
  if(command.GetType() == ctGET) //получить статистику
  {

    if(!argsCount) // нет аргументов
    {
      PublishSingleton = PARAMS_MISSED;
    }
    else
    {
      String t = command.GetArg(0);

        if(t == STAT_COMMAND) // запросили данные статистики
        {
          SendStatToCaller(Settings->GetSmsPhoneNumber()); // посылаем статистику на указанный номер телефона
        
          PublishSingleton.Status = true;
          PublishSingleton = STAT_COMMAND; 
          PublishSingleton << PARAM_DELIMITER << REG_SUCC;
        }
        else if(t == F("PROV")) // запросили провайдера GSM
        {
          PublishSingleton.Status = true;
          PublishSingleton = F("PROV");
          PublishSingleton << PARAM_DELIMITER;
          PublishSingleton << MainController->GetSettings()->GetGSMProvider();
        }
        else if(t == BALANCE_COMMAND) 
        { // получить баланс

          RequestBalance();
          
          PublishSingleton.Status = true;
          PublishSingleton = BALANCE_COMMAND; 
          PublishSingleton << PARAM_DELIMITER << REG_SUCC;
          
        }
        else
        {
          // неизвестная команда
          PublishSingleton = UNKNOWN_COMMAND;
        } // else
    } // else have arguments
    
  } // if
 
 // отвечаем на команду
    MainController->Publish(this,command);
    
  return PublishSingleton.Status;
}
//--------------------------------------------------------------------------------------------------------------------------------

