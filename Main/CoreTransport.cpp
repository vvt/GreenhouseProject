#include "CoreTransport.h"
#include "Settings.h"
#include "ModuleController.h"
#include "Memory.h"
#include "InteropStream.h"
//--------------------------------------------------------------------------------------------------------------------------------------
#include <SdFat.h>
//--------------------------------------------------------------------------------------------------------------------------------------
#define CIPSEND_COMMAND F("AT+CIPSENDBUF=")
//--------------------------------------------------------------------------------------------------------------------------------------
// CoreTransportClient
//--------------------------------------------------------------------------------------------------------------------------------------
CoreTransportClient::CoreTransportClient()
{
  socket = NO_CLIENT_ID;
  dataBuffer = NULL;
  dataBufferSize = 0;
  parent = NULL;
}
//--------------------------------------------------------------------------------------------------------------------------------------
CoreTransportClient::~CoreTransportClient()
{
  clear();
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransportClient::accept(CoreTransport* _parent)
{
  parent = _parent;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransportClient::clear()
{
    delete [] dataBuffer; 
    dataBuffer = NULL;
    dataBufferSize = 0;
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransportClient::disconnect()
{
  if(!parent)
    return;
  
    if(!connected())
      return;

    parent->doDisconnect(*this);
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransportClient::connect(const char* ip, uint16_t port)
{
  if(!parent)
    return;
  
    if(connected()) // уже присоединены, нельзя коннектится до отсоединения!!!
      return;
          
    parent->doConnect(*this,ip,port);
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreTransportClient::write(uint8_t* buff, size_t sz)
{
  if(!parent)
    return false;
  
    if(!sz || !buff || !connected() || socket == NO_CLIENT_ID)
    {
      #ifdef WIFI_DEBUG
        Serial.println(F("CoreTransportClient - CAN'T WRITE!"));
      #endif
      return false;
    }

  clear();
  dataBufferSize = sz; 
  if(dataBufferSize)
  {
      dataBuffer = new  uint8_t[dataBufferSize];
      memcpy(dataBuffer,buff,dataBufferSize);
  }

    parent->doWrite(*this);
    
   return true;
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreTransportClient::connected() 
{
  if(!parent || socket == NO_CLIENT_ID)
    return false;
    
  return parent->connected(socket);
}
//--------------------------------------------------------------------------------------------------------------------------------------
// ESPClient
//--------------------------------------------------------------------------------------------------------------------------------------
ESPClient::ESPClient() : CoreTransportClient()
{
  #ifdef USE_WIFI_MODULE
  accept(&ESP);
  #endif
}
//--------------------------------------------------------------------------------------------------------------------------------------
ESPClient::~ESPClient()
{
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
// CoreTransport
//--------------------------------------------------------------------------------------------------------------------------------------
CoreTransport::CoreTransport(uint8_t clientsPoolSize)
{
  for(uint8_t i=0;i<clientsPoolSize;i++)
  {
    CoreTransportClient* client = new CoreTransportClient();
    client->accept(this);
    client->bind(i);
    
    pool.push_back(client);
    status.push_back(false);
  }
}
//--------------------------------------------------------------------------------------------------------------------------------------
CoreTransport::~CoreTransport()
{
  for(size_t i=0;i<pool.size();i++)
  {
    delete pool[i];
  }
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransport::initPool()
{
  for(size_t i=0;i<status.size();i++)
  {
    status[i] = false;
  }
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreTransport::connected(uint8_t socket)
{
  return status[socket];
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransport::doWrite(CoreTransportClient& client)
{
  if(!client.connected())
  {
    client.clear();
    return;
  }

   beginWrite(client); 
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransport::doConnect(CoreTransportClient& client, const char* ip, uint16_t port)
{
  if(client.connected())
    return;

   // запоминаем нашего клиента
   client.accept(this);

  // если внешний клиент - будем следить за его статусом соединения/подсоединения
   if(isExternalClient(client))
    closedCatchList.push_back(&client);

   beginConnect(client,ip,port); 
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransport::doDisconnect(CoreTransportClient& client)
{
  if(!client.connected())
    return;

    beginDisconnect(client);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransport::subscribe(IClientEventsSubscriber* subscriber)
{
  for(size_t i=0;i<subscribers.size();i++)
  {
    if(subscribers[i] == subscriber)
      return;
  }

  subscribers.push_back(subscriber);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransport::unsubscribe(IClientEventsSubscriber* subscriber)
{
  for(size_t i=0;i<subscribers.size();i++)
  {
    if(subscribers[i] == subscriber)
    {
      for(size_t k=i+1;k<subscribers.size();k++)
      {
        subscribers[k-1] = subscribers[k];
      }
      subscribers.pop();
      break;
    }
  }  
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreTransport::isExternalClient(CoreTransportClient& client)
{
  // если клиент не в нашем пуле - это экземпляр внешнего клиента
  for(size_t i=0;i<pool.size();i++)
  {
    if(pool[i] == &client)
      return false;
  }

  return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransport::notifyClientConnected(CoreTransportClient& client, bool connected, int16_t errorCode)
{

   // тут надо синхронизировать с пулом клиентов
   if(client.socket != NO_CLIENT_ID)
   {
      status[client.socket] = connected;
   }
  
    for(size_t i=0;i<subscribers.size();i++)
    {
      subscribers[i]->OnClientConnect(client,connected,errorCode);
    }

      // возможно, это внешний клиент, надо проверить - есть ли он в списке слежения
      if(!connected) // пришло что-то типа 1,CLOSED
      {         
        // клиент отсоединился, надо освободить его сокет
        for(size_t i=0;i<closedCatchList.size();i++)
        {
          if(closedCatchList[i]->socket == client.socket)
          {
            closedCatchList[i]->clear();
            closedCatchList[i]->release(); // освобождаем внешнему клиенту сокет
            for(size_t k=i+1;k<closedCatchList.size();k++)
            {
              closedCatchList[k-1] = closedCatchList[k];
            }
            closedCatchList.pop();
            break;
          }
        } // for
      } // if(!connected)
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransport::notifyDataWritten(CoreTransportClient& client, int16_t errorCode)
{
    for(size_t i=0;i<subscribers.size();i++)
    {
      subscribers[i]->OnClientDataWritten(client,errorCode);
    } 
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreTransport::notifyDataAvailable(CoreTransportClient& client, uint8_t* data, size_t dataSize, bool isDone)
{
    for(size_t i=0;i<subscribers.size();i++)
    {
      subscribers[i]->OnClientDataAvailable(client,data,dataSize,isDone);
    }  
}
//--------------------------------------------------------------------------------------------------------------------------------------
CoreTransportClient* CoreTransport::getClient(uint8_t socket)
{
  if(socket != NO_CLIENT_ID)
    return pool[socket];

  for(size_t i=0;i<pool.size();i++)
  {
    if(!pool[i]->connected())
      return pool[i];
  }

  return NULL;
}
//--------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_WIFI_MODULE
//--------------------------------------------------------------------------------------------------------------------------------------
// CoreESPTransport
//--------------------------------------------------------------------------------------------------------------------------------------
CoreESPTransport ESP;
//--------------------------------------------------------------------------------------------------------------------------------------
CoreESPTransport::CoreESPTransport() : CoreTransport(ESP_MAX_CLIENTS)
{

  wiFiReceiveBuff = new String();
  flags.bPaused = false;

  waitCipstartConnect = false;
  cipstartConnectClient = NULL;

}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::waitTransmitComplete()
{
  // ждём завершения передачи по UART
 if(!workStream)
    return;
    
  #if TARGET_BOARD == MEGA_BOARD

    if(workStream == &Serial)
      while(!(UCSR0A & _BV(TXC0) ));
    else
    if(workStream == &Serial1)
      while(!(UCSR1A & _BV(TXC1) ));
    else
    if(workStream == &Serial2)
      while(!(UCSR2A & _BV(TXC2) ));
    else
    if(workStream == &Serial3)
      while(!(UCSR3A & _BV(TXC3) ));

  #elif TARGET_BOARD == DUE_BOARD

    if(workStream == &Serial)
      while((UART->UART_SR & UART_SR_TXRDY) != UART_SR_TXRDY);
    else
    if(workStream == &Serial1)
      while((USART0->US_CSR & US_CSR_TXEMPTY) == 0);
    else
    if(workStream == &Serial2)
      while((USART1->US_CSR & US_CSR_TXEMPTY)  == 0);      
    else
    if(workStream == &Serial3)
      while((USART3->US_CSR & US_CSR_TXEMPTY)  == 0); 
  #else
    #error "Unknown target board!"
  #endif  

}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::sendCommand(const String& command, bool addNewLine)
{
  #ifdef WIFI_DEBUG
    Serial.print(F("ESP: ==>> "));
    Serial.println(command);
  #endif
  
  workStream->write(command.c_str(),command.length());
  
  if(addNewLine)
  {
    workStream->println();
  }

  waitTransmitComplete();

  machineState = espWaitAnswer; // говорим, что надо ждать ответа от ESP
  // запоминаем время отсылки последней команды
  timer = millis();
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreESPTransport::pingGoogle(bool& result)
{
    if(machineState != espIdle || !workStream || !ready() || initCommandsQueue.size()) // чего-то делаем, не могём
    {
      return false;
    }
    
    pause();

        ESPKnownAnswer ka;
        workStream->println(F("AT+PING=\"google.com\""));
        // поскольку у нас serialEvent не основан на прерываниях, на самом-то деле (!),
        // то мы должны получить ответ вот прямо вот здесь, и разобрать его.

        String line; // тут принимаем данные до конца строки
        bool  pingDone = false;
        
        char ch;
        uint32_t startTime = millis();
        
        while(millis() - startTime < 10000) // таймаут в 10 секунд
        { 
          if(pingDone) // получили ответ на PING
            break;
            
          while(workStream->available())
          {
            ch = workStream->read();
            timer = millis(); // не забываем обновлять таймер ответа - поскольку у нас что-то пришло - значит, модем отвечает
        
            if(ch == '\r')
              continue;
            
            if(ch == '\n')
            {
              // получили строку, разбираем её
              
              // здесь надо обработать известные статусы
                 if(checkIPD(line))
                 {     
                    processIPD(line);
                    continue;
                 }
                 processKnownStatusFromESP(line);
              
                 if(isKnownAnswer(line, ka))
                 {
                    result = (ka == kaOK);
                    pingDone = true;
                 }
             line = "";
            } // ch == '\n'
            else
              line += ch;
        
          if(pingDone) // получили ответ на PING
              break;
 
          } // while
          
        } // while(1)    

  resume();

  return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreESPTransport::getMAC(String& staMAC, String& apMAC)
{
    if(machineState != espIdle || !workStream || !ready() || initCommandsQueue.size()) // чего-то делаем, не могём
    {
      return false;
    }

    pause();

        ESPKnownAnswer ka;
        workStream->println(F("AT+CIPSTAMAC?"));

        String line; // тут принимаем данные до конца строки
        staMAC = "-";
        apMAC = "-";
        
        bool  apMACDone = false, staMACDone=false;
        char ch;
        uint32_t startTime = millis();
        
        while(millis() - startTime < 10000) // таймаут в 10 секунд
        { 
          if(staMACDone) // получили MAC-адрес станции
            break;
            
          while(workStream->available())
          {
            ch = workStream->read();
            timer = millis(); // не забываем обновлять таймер ответа - поскольку у нас что-то пришло - значит, модем отвечает
        
            if(ch == '\r')
              continue;
            
            if(ch == '\n')
            {
              // получили строку, разбираем её
              // здесь надо обработать известные статусы
                 if(checkIPD(line))
                 {     
                    processIPD(line);
                    continue;
                 }
                 processKnownStatusFromESP(line);
              
                 if(line.startsWith(F("+CIPSTAMAC:"))) // MAC станции
                 {
                  #ifdef WIFI_DEBUG
                    Serial.println(F("Station MAC found, parse..."));
                  #endif  
            
                   staMAC = line.substring(11);                      
                  
                 } // if(line.startsWith
                 else
                 if(isKnownAnswer(line, ka))
                 {
                    staMACDone = true;
                 }
             line = "";
            } // ch == '\n'
            else
              line += ch;
        
          if(staMACDone) // получили MAC станции
              break;
 
          } // while
          
        } // while(1)

        // теперь получаем MAC точки доступа
        workStream->println(F("AT+CIPAPMAC?"));
        line = "";

        startTime = millis();
        
        while(millis() - startTime < 10000) // таймаут в 10 секунд
        { 
          if(apMACDone) // получили MAC-адрес точки доступа
            break;
            
          while(workStream->available())
          {
            ch = workStream->read();
            timer = millis(); // не забываем обновлять таймер ответа - поскольку у нас что-то пришло - значит, модем отвечает
        
            if(ch == '\r')
              continue;
            
            if(ch == '\n')
            {
              // получили строку, разбираем её
              
              // здесь надо обработать известные статусы
                 if(checkIPD(line))
                 {     
                    processIPD(line);
                    continue;
                 }
                 processKnownStatusFromESP(line);
              
                 if(line.startsWith(F("+CIPAPMAC:"))) // MAC нашей точки доступа
                 {
                   #ifdef WIFI_DEBUG
                    Serial.println(F("softAP MAC found, parse..."));
                   #endif
            
                   apMAC = line.substring(10);                      
                  
                 } // if(line.startsWith
                 else
                 if(isKnownAnswer(line,ka))
                 {
                    apMACDone = true;
                 }
             line = "";
            } // ch == '\n'
            else
              line += ch;
        
          if(apMACDone) // получили MAC точки доступа
              break;
 
          } // while
          
        } // while(1)

    resume();

  return true;              
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreESPTransport::getIP(String& stationCurrentIP, String& apCurrentIP)
{
    if(machineState != espIdle || !workStream || !ready() || initCommandsQueue.size()) // чего-то делаем, не могём
    {
      return false;
    }


    pause();

    workStream->println(F("AT+CIFSR"));  
    
    String line; // тут принимаем данные до конца строки
    bool knownAnswerFound = false;
    ESPKnownAnswer ka;  

    char ch;
    uint32_t startTime = millis();
    
    while(millis() - startTime < 10000) // таймаут в 10 секунд
    { 
      if(knownAnswerFound) // получили оба IP
        break;
        
      while(workStream->available())
      {
        ch = (char) workStream->read();
        timer = millis(); // не забываем обновлять таймер ответа - поскольку у нас что-то пришло - значит, модем отвечает
    
        if(ch == '\r')
          continue;
        
        if(ch == '\n')
        {
          // получили строку, разбираем её
          
          // здесь надо обработать известные статусы
           if(checkIPD(line))
           {     
              processIPD(line);
              continue;
           }
           processKnownStatusFromESP(line);
                     
            if(line.startsWith(F("+CIFSR:APIP"))) // IP нашей точки доступа
             {
               #ifdef WIFI_DEBUG
                Serial.println(F("AP IP found, parse..."));
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

             } // if(line.startsWith(F("+CIFSR:APIP")))
             else
              if(line.startsWith(F("+CIFSR:STAIP"))) // IP нашей точки доступа, назначенный роутером
             {
                  #ifdef WIFI_DEBUG
                    Serial.println(F("STA IP found, parse..."));
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

             } // if(line.startsWith(F("+CIFSR:STAIP")))

           if(isKnownAnswer(line,ka))
            knownAnswerFound = true;
         
          line = "";
        } // ch == '\n'
        else
        {
              line += ch;
        }
    
     if(knownAnswerFound) // получили оба IP
        break;

      } // while
      
    } // while(1)

    resume();

    return true;
    
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::sendCommand(ESPCommands command)
{
  currentCommand = command;
  
  // тут посылаем команду в ESP
  switch(command)
  {
    case cmdNone:
    case cmdCIPCLOSE: // ничего тут не надо, эти команды формируем не здесь
    case cmdCIPSTART:
    case cmdCIPSEND:
    case cmdWaitSendDone:
    break;

    case cmdWantReady:
    {
      #ifdef WIFI_DEBUG
        Serial.println(F("ESP: reset..."));
      #endif

      // принудительно очищаем очередь клиентов
      clearClientsQueue(true);
      // и говорим, что все слоты свободны
      initPool();
      sendCommand(F("AT+RST"));
    }
    break;

    case cmdEchoOff:
    {
      #ifdef WIFI_DEBUG
        Serial.println(F("ESP: echo OFF..."));
      #endif
      sendCommand(F("ATE0"));
    }
    break;

    case cmdCWMODE:
    {
      #ifdef WIFI_DEBUG
        Serial.println(F("ESP: softAP mode..."));
      #endif
      sendCommand(F("AT+CWMODE_CUR=3"));
    }
    break;

    case cmdCWSAP:
    {
        #ifdef WIFI_DEBUG
          Serial.println(F("ESP: Creating the access point..."));
        #endif

        GlobalSettings* Settings = MainController->GetSettings();
      
        String com = F("AT+CWSAP_CUR=\"");
        com += Settings->GetStationID();
        com += F("\",\"");
        com += Settings->GetStationPassword();
        com += F("\",8,4");
        
        sendCommand(com);      
    }
    break;

    case cmdCWJAP:
    {
      #ifdef WIFI_DEBUG
        Serial.println(F("ESP: Connecting to the router..."));
      #endif

        GlobalSettings* Settings = MainController->GetSettings();
        
        String com = F("AT+CWJAP_CUR=\"");
        com += Settings->GetRouterID();
        com += F("\",\"");
        com += Settings->GetRouterPassword();
        com += F("\"");
        sendCommand(com);      
    }
    break;

    case cmdCWQAP:
    {
      #ifdef WIFI_DEBUG
        Serial.println(F("ESP: Disconnect from router..."));
      #endif
      sendCommand(F("AT+CWQAP"));
    }
    break;

    case cmdCIPMODE:
    {
      #ifdef WIFI_DEBUG
        Serial.println(F("ESP: Set the TCP server mode to 0..."));
      #endif
      sendCommand(F("AT+CIPMODE=0"));
    }
    break;

    case cmdCIPMUX:
    {
      #ifdef WIFI_DEBUG
        Serial.println(F("ESP: Allow multiple connections..."));
      #endif
      sendCommand(F("AT+CIPMUX=1"));
    }
    break;

    case cmdCIPSERVER:
    {
      #ifdef WIFI_DEBUG
        Serial.println(F("ESP: Starting TCP-server..."));
      #endif
      sendCommand(F("AT+CIPSERVER=1,1975"));
    }
    break;

    case cmdCheckModemHang:
    {
      #ifdef WIFI_DEBUG
        Serial.println(F("ESP: check for ESP available..."));
      #endif
      
      flags.wantReconnect = false;
      sendCommand(F("AT+CWJAP?")); // проверяем, подконнекчены ли к роутеру
    }
    break;
    
  } // switch

}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreESPTransport::isESPBootFound(const String& line)
{
  return (line == F("ready")) || line.startsWith(F("Ai-Thinker Technology"));
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreESPTransport::isKnownAnswer(const String& line, ESPKnownAnswer& result)
{
  result = kaNone;
  
  if(line == F("OK"))
  {
    result = kaOK;
    return true;
  }
  if(line == F("ERROR"))
  {
    result = kaError;
    return true;
  }
  if(line == F("FAIL"))
  {
    result = kaFail;
    return true;
  }
  if(line.endsWith(F("SEND OK")))
  {
    result = kaSendOk;
    return true;
  }
  if(line.endsWith(F("SEND FAIL")))
  {
    result = kaSendFail;
    return true;
  }
  if(line.endsWith(F("ALREADY CONNECTED")))
  {
    result = kaAlreadyConnected;
    return true;
  }
  return false;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::processIPD(const String& line)
{
#ifdef WIFI_DEBUG  
  Serial.print(F("ESP: start parse +IPD, received="));
  Serial.println(line);
#endif

  // здесь в line лежит только команда вида +IPD,<id>,<len>:
  // все данные надо вычитывать из потока
        
    int16_t idx = line.indexOf(F(",")); // ищем первую запятую после +IPD
    const char* ptr = line.c_str();
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

    // получили ID клиента и длину его данных, которые - пока в потоке, и надо их быстро попакетно вычитать
    int16_t clientID = connectedClientID.toInt();
    size_t lengthOfData = dataLen.toInt();
    
    if(clientID >=0 && clientID < ESP_MAX_CLIENTS)
    {

       CoreTransportClient* client = getClient(clientID);

       // у нас есть lengthOfData с данными для клиента, нам надо побить это на пакеты длиной N байт,
       // и последовательно вызывать событие прихода данных. Это нужно для того, чтобы не переполнить оперативку,
       // поскольку у нас её - не вагон.

      // пусть у нас будет максимум 512 байт на пакет
      const uint16_t MAX_PACKET_SIZE = 512;
      
      // если длина всех данных меньше MAX_PACKET_SIZE - просто тупо все сразу вычитаем
       uint16_t packetSize = min(MAX_PACKET_SIZE,lengthOfData);

        // теперь выделяем буфер под данные
        uint8_t* buff = new uint8_t[packetSize];

        // у нас есть буфер, в него надо скопировать данные из потока
        uint8_t* writePtr = buff;
        
        size_t packetWritten = 0; // записано в пакет
        size_t totalWritten = 0; // всего записано

        pause();

            uint32_t startTime = millis();
            bool hasTimeout = false;

            while(totalWritten < lengthOfData) // пока не запишем все данные с клиента
            {
              if(millis() - startTime > WIFI_IPD_READING_TIMEOUT)
              {
                hasTimeout = true;
                break;
              }
                if(workStream->available())
                { 
                  startTime = millis();               
                  *writePtr++ = (uint8_t) workStream->read();
                  packetWritten++;
                  totalWritten++;
                }
                else
                  continue;

                if(packetWritten >= packetSize)
                {
                  
                  // скопировали один пакет    
                  // сообщаем подписчикам, что данные для клиента получены
                  notifyDataAvailable(*client, buff, packetWritten, totalWritten >= lengthOfData);

                  // чистим память
                  delete [] buff;
                      
                  // пересчитываем длину пакета, вдруг там мало осталось, и незачем выделять под несколько байт огромный буфер
                  packetSize =  min(MAX_PACKET_SIZE, lengthOfData - totalWritten);
                  buff = new uint8_t[packetSize];
                  writePtr = buff; // на начало буфера
                  packetWritten = 0;
                }
              
            } // while

            #ifdef WIFI_DEBUG
            if(hasTimeout)
            {
              Serial.print(F("DATA LENGTH="));
              Serial.print(lengthOfData);
              Serial.print(F("; READED="));
              Serial.println(totalWritten);
              Serial.println(F("TIMEOUT TIMEOUT TIMEOUT TIMEOUT TIMEOUT"));
            }
            #endif
            
           resume();

            // проверяем - есть ли остаток?
            if(packetWritten > 0)
            {            
              // после прохода цикла есть остаток данных, уведомляем клиента
              // сообщаем подписчикам, что данные для клиента получены
              notifyDataAvailable(*client, buff, packetWritten, hasTimeout ? true : totalWritten >= lengthOfData);
            }
            
            delete [] buff;  
       
    } // if(clientID >=0 && clientID < ESP_MAX_CLIENTS)
    

#ifdef WIFI_DEBUG
  Serial.println(F("ESP: +IPD parsed."));  
#endif

}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::processConnect(const String& line)
{
     // клиент подсоединился
     
    int idx = line.indexOf(F(",CONNECT"));
    
    if(idx == -1)
      return;
    
    String s = line.substring(0,idx);
    int16_t clientID = s.toInt();
    
    if(clientID >=0 && clientID < ESP_MAX_CLIENTS)
    {
      #ifdef WIFI_DEBUG
        Serial.print(F("ESP: client connected - #"));
        Serial.println(clientID);
      #endif

      // тут смотрим - посылали ли мы запрос на коннект?
      if(waitCipstartConnect && cipstartConnectClient != NULL && clientID == cipstartConnectClientID)
      {
        #ifdef WIFI_DEBUG
        Serial.println(F("ESP: WAIT CIPSTART CONNECT, CATCH OUTGOING CLIENT!"));
        #endif
        // есть клиент, для которого надо установить ID.
        // тут у нас может возникнуть коллизия, когда придёт коннект с внешнего адреса.
        // признаком этой коллизии является тот факт, что если мы в этой ветке - мы ОБЯЗАНЫ
        // получить один из известных ответов OK, ERROR, ALREADY CONNECTED
        // ДО ТОГО, как придёт статус ID,CONNECT
        cipstartConnectClient->bind(clientID);
        
        if(!cipstartConnectKnownAnswerFound)
        {
        #ifdef WIFI_DEBUG
        Serial.println(F("ESP: WAIT CIPSTART CONNECT, NO OK FOUND!"));
        #endif
          
          // не найдено ни одного ответа из известных. Проблема в том, что у внешнего клиента ещё нет слота,
          // но там надо ему временно выставить слот (мы это сделали выше), потом вызвать событие отсоединения, потом - очистить ему слот
          removeClientFromQueue(cipstartConnectClient);
          notifyClientConnected(*cipstartConnectClient,false,CT_ERROR_CANT_CONNECT);
          cipstartConnectClient->release();

          // при этом, поскольку мы освободили нашего клиента на внешнее соединение и это коллизия,
          // мы должны сообщить, что клиент от ESP подсоединился
          CoreTransportClient* client = getClient(clientID);            
          notifyClientConnected(*client,true,CT_ERROR_NONE);
          
          // поскольку строка ID,CONNECT пришла ДО известного ответа - это коллизия, и мы в ветке cmdCIPSTART,
          // поэтому мы здесь сами должны удалить клиента из очереди и переключиться на ожидание
          machineState = espIdle;
        }
        else
        {
        #ifdef WIFI_DEBUG
        Serial.println(F("ESP: WAIT CIPSTART CONNECT, CLIENT CONNECTED!"));
        #endif
          
          // если вы здесь - ответ OK получен сразу после команды AT+CIPSTART,
          // клиент из очереди удалён, и, раз мы получили ID,CONNECT - мы можем сообщать, что клиент подсоединён
          CoreTransportClient* client = getClient(clientID);    
          notifyClientConnected(*client,true,CT_ERROR_NONE);          
        }
      
          waitCipstartConnect = false;
          cipstartConnectClient = NULL;
          cipstartConnectClientID = NO_CLIENT_ID;
          cipstartConnectKnownAnswerFound = false;
        
      } // if
      else
      {            
        // если мы здесь - то мы не ждём подсоединения клиента на исходящий адрес
        // просто выставляем клиенту флаг, что он подсоединён
        CoreTransportClient* client = getClient(clientID);            
        notifyClientConnected(*client,true,CT_ERROR_NONE);
      }
      
    } // if
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::processDisconnect(const String& line)
{
  // клиент отсоединился
    int idx = line.indexOf(F(",CLOSED"));
    
    if(idx == -1)
      idx = line.indexOf(F(",CONNECT FAIL"));
      
    if(idx == -1)
      return;
      
    String s = line.substring(0,idx);
    int16_t clientID = s.toInt();
    
    if(clientID >=0 && clientID < ESP_MAX_CLIENTS)
    {
      #ifdef WIFI_DEBUG
        Serial.print(F("ESP: client disconnected - #"));
        Serial.println(clientID);
      #endif

      // выставляем клиенту флаг, что он отсоединён
      CoreTransportClient* client = getClient(clientID);            
      notifyClientConnected(*client,false,CT_ERROR_NONE);
      
    }

    // тут смотрим - посылали ли мы запрос на коннект?
    if(waitCipstartConnect && cipstartConnectClient != NULL && clientID == cipstartConnectClientID)
    {

      #ifdef WIFI_DEBUG
        Serial.print(F("ESP: waitCipstartConnect - #"));
        Serial.println(clientID);
      #endif
      
      // есть клиент, для которого надо установить ID
      cipstartConnectClient->bind(clientID);
      notifyClientConnected(*cipstartConnectClient,false,CT_ERROR_NONE);
      cipstartConnectClient->release();
      removeClientFromQueue(cipstartConnectClient);
      
      waitCipstartConnect = false;
      cipstartConnectClient = NULL;
      cipstartConnectClientID = NO_CLIENT_ID;
      
    } // if            
          
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::processKnownStatusFromESP(const String& line)
{
   // смотрим, подсоединился ли клиент?
   if(line.endsWith(F(",CONNECT")))
   {
    processConnect(line);
   } // if
   else 
   if(line.endsWith(F(",CLOSED")) || line.endsWith(F(",CONNECT FAIL")))
   {
    processDisconnect(line);
   } // if(idx != -1)
   else
   if(line == F("WIFI CONNECTED"))
   {
      flags.connectedToRouter = true;
      #ifdef WIFI_DEBUG
        Serial.println(F("ESP: connected to router!"));
      #endif
   }
   else
   if(line == F("WIFI DISCONNECT"))
   {
      flags.connectedToRouter = false;
      #ifdef WIFI_DEBUG
        Serial.println(F("ESP: disconnected from router!"));
      #endif
   }  
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreESPTransport::checkIPD(const String& line)
{
  return line.startsWith(F("+IPD")) && (line.indexOf(":") != -1);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::update()
{ 
  if(!workStream || paused()) // либо нет рабочего потока, либо кто-то нас попросил ничего не вычитывать пока из ESP
    return;

  if(flags.onIdleTimer) // попросили подождать определённое время, в течение которого просто ничего не надо делать
  {
      if(millis() - timer > idleTime)
      {
        //DBGLN(F("ESP: idle done!"));
        flags.onIdleTimer = false;
      }
  } 

  // флаг, что есть ответ от ESP, выставляется по признаку наличия хоть чего-то в буфере приёма
  bool hasAnswer = workStream->available() > 0;

  // выставляем флаг, что мы хотя бы раз получили хоть чего-то от ESP
  flags.isAnyAnswerReceived = flags.isAnyAnswerReceived || hasAnswer;

  bool hasAnswerLine = false; // флаг, что мы получили строку ответа от модема

  char ch;
  while(workStream->available())
  {
    // здесь мы должны детектировать - пришло IPD или нет.
    // если пришли данные - мы должны вычитать их длину, и уже после этого - отправить данные клиенту,
    // напрямую читая из потока. Это нужно, потому что данные могут быть бинарными, и мы никогда в них не дождёмся
    // перевода строки.

     if(checkIPD(*wiFiReceiveBuff))
     {     
        processIPD(*wiFiReceiveBuff);
                
        delete wiFiReceiveBuff;
        wiFiReceiveBuff = new String();
        timer = millis();        
        return; // надо вывалится из цикла, поскольку мы уже всё отослали клиенту, для которого пришли данные
     }
    
    ch = workStream->read();

    if(ch == '\r') // ненужный нам символ
      continue;
    else
    if(ch == '\n')
    {   
        hasAnswerLine = true; // выставляем флаг, что у нас есть строка ответа от ESP  
        break; // выходим из цикла, остальное дочитаем позже, если это потребуется кому-либо
    }
    else
    {     
        if(flags.waitForDataWelcome && ch == '>') // ждём команду >  (на ввод данных)
        {
          flags.waitForDataWelcome = false;
          *wiFiReceiveBuff = F(">");
          hasAnswerLine = true;
          break; // выходим из цикла, получили приглашение на ввод данных
        }
        else
        {
          *wiFiReceiveBuff += ch;
                         
          if(wiFiReceiveBuff->length() > 512) // буфер слишком длинный
          {
            #ifdef WIFI_DEBUG
              Serial.println(F("ESP: incoming data too long, skip it!"));
            #endif
            delete wiFiReceiveBuff;
            wiFiReceiveBuff = new String();
          }
        }
    } // any char except '\r' and '\n' 
    
  } // while(workStream->available())

  if(hasAnswer)
  {
     timer = millis(); // не забываем обновлять таймер ответа - поскольку у нас что-то пришло - значит, модем отвечает
  }

    if(hasAnswerLine && !wiFiReceiveBuff->length()) // пустая строка, не надо обрабатывать
      hasAnswerLine = false;

   #ifdef WIFI_DEBUG
    if(hasAnswerLine)
    {
      Serial.print(F("<== ESP: "));
      Serial.println(*wiFiReceiveBuff);
    }
   #endif

    // тут анализируем ответ от ESP, если он есть, на предмет того - соединён ли клиент, отсоединён ли клиент и т.п.
    // это нужно делать именно здесь, поскольку в этот момент в ESP может придти внешний коннект.
    if(hasAnswerLine)
      processKnownStatusFromESP(*wiFiReceiveBuff);

  // при разборе ответа тут будет лежать тип ответа, чтобы часто не сравнивать со строкой
  ESPKnownAnswer knownAnswer = kaNone;

  if(!flags.onIdleTimer) // только если мы не в режиме простоя
  {
    // анализируем состояние конечного автомата, чтобы понять, что делать
    switch(machineState)
    {
        case espIdle: // ничего не делаем, можем работать с очередью команд и клиентами
        {            
            // смотрим - если есть хоть одна команда в очереди инициализации - значит, мы в процессе инициализации, иначе - можно работать с очередью клиентов
            if(initCommandsQueue.size())
            {
                #ifdef WIFI_DEBUG
                  Serial.println(F("ESP: process next init command..."));
                #endif
                currentCommand = initCommandsQueue[initCommandsQueue.size()-1];
                initCommandsQueue.pop();
                sendCommand(currentCommand);
            } // if
            else
            {
              // в очереди команд инициализации ничего нет, значит, можем выставить флаг, что мы готовы к работе с клиентами
              flags.ready = true;
              
              if(clientsQueue.size())
              {
                  // получаем первого клиента в очереди
                  ESPClientQueueData dt = clientsQueue[0];
                  int clientID = dt.client->socket;
                  
                  // смотрим, чего он хочет от нас
                  switch(dt.action)
                  {
                    case actionDisconnect:
                    {
                      // хочет отсоединиться
                      currentCommand = cmdCIPCLOSE;
                      String cmd = F("AT+CIPCLOSE=");
                      cmd += clientID;
                      sendCommand(cmd);
                      
                    }
                    break; // actionDisconnect

                    case actionConnect:
                    {

                      // здесь надо искать первый свободный слот для клиента
                      CoreTransportClient* freeSlot = getClient(NO_CLIENT_ID);
                      clientID = freeSlot ? freeSlot->socket : NO_CLIENT_ID;
                      
                      if(flags.connectedToRouter)
                      {
                        waitCipstartConnect = true;
                        cipstartConnectClient = dt.client;
                        cipstartConnectClientID = clientID;
                        cipstartConnectKnownAnswerFound = false;
  
                        currentCommand = cmdCIPSTART;
                        String comm = F("AT+CIPSTART=");
                        comm += clientID;
                        comm += F(",\"TCP\",\"");
                        comm += dt.ip;
                        comm += F("\",");
                        comm += dt.port;
  
                        delete [] clientsQueue[0].ip;
                        clientsQueue[0].ip = NULL;
              
                        // и отсылаем её
                        sendCommand(comm);
                      } // flags.connectedToRouter
                      else
                      {
                        // не законнекчены к роутеру, не можем устанавливать внешние соединения!!!
                        removeClientFromQueue(dt.client);
                        dt.client->bind(clientID);
                        notifyClientConnected(*(dt.client),false,CT_ERROR_CANT_CONNECT);
                        dt.client->release();
                        
                      }
                    }
                    break; // actionConnect

                    case actionWrite:
                    {
                      // хочет отослать данные

                      currentCommand = cmdCIPSEND;

                      size_t dataSize;
                      uint8_t* buffer = dt.client->getBuffer(dataSize);
                      dt.client->releaseBuffer();

                      clientsQueue[0].data = buffer;
                      clientsQueue[0].dataLength = dataSize;

                      String command = CIPSEND_COMMAND;
                      command += clientID;
                      command += F(",");
                      command += dataSize;
                      flags.waitForDataWelcome = true; // выставляем флаг, что мы ждём >
                      
                      sendCommand(command);
                      
                    }
                    break; // actionWrite
                  } // switch
              }
              else
              {
                timer = millis(); // обновляем таймер в режиме ожидания, поскольку мы не ждём ответа на команды

                // у нас прошла инициализация, нет клиентов в очереди на обработку, следовательно - мы можем проверять модем на зависание
                // тут смотрим - не пора ли послать команду для проверки на зависание. Слишком часто её звать нельзя, что очевидно,
                // поэтому мы будем звать её минимум раз в N секунд. При этом следует учитывать, что мы всё равно должны звать эту команду
                // вне зависимости от того, откликается ли ESP или нет, т.к. в этой команде мы проверяем - есть ли соединение с роутером.
                // эту проверку надо делать периодически, чтобы форсировать переподсоединение, если роутер отвалился.
                static uint32_t hangTimer = 0;
                if(millis() - hangTimer > WIFI_AVAILABLE_CHECK_TIME)
                {
                  hangTimer = millis();
                  sendCommand(cmdCheckModemHang);
                  
                } // if
                
              } // else
            } // else inited
        }
        break; // espIdle

        case espWaitAnswer: // ждём ответа от модема на посланную ранее команду (функция sendCommand переводит конечный автомат в эту ветку)
        {
          // команда, которую послали - лежит в currentCommand, время, когда её послали - лежит в timer.
              if(hasAnswerLine)
              {                
                // есть строка ответа от модема, можем её анализировать, в зависимости от посланной команды (лежит в currentCommand)
                switch(currentCommand)
                {
                  case cmdNone:
                  {
                    // ничего не делаем
                  }
                  break; // cmdNone

                  case cmdCIPCLOSE:
                  {
                    // отсоединялись. Здесь не надо ждать известного ответа, т.к. ответ может придти асинхронно
                    //if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                    {
                      if(clientsQueue.size())
                      {
                        // клиент отсоединён, ставим ему соответствующий флаг, освобождаем его и удаляем из очереди
                        ESPClientQueueData dt = clientsQueue[0];

                        CoreTransportClient* thisClient = dt.client;
                        removeClientFromQueue(thisClient);

                        // событие здесь не надо отсылать, т.к. в ветке обработки ...,CLOSED оно само обработается
                        //notifyClientConnected(*thisClient,false,CT_ERROR_NONE);

                      } // if(clientsQueue.size()) 
                      
                        machineState = espIdle; // переходим к следующей команде
                    }
                  }
                  break; // cmdCIPCLOSE

                  case cmdCIPSTART:
                  {
                    // соединялись, коннект у нас только с внутреннего соединения, поэтому в очереди лежит по-любому
                    // указатель на связанного с нами клиента, который использует внешний пользователь транспорта
                    
                        if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                        {
                          if(knownAnswer == kaOK || knownAnswer == kaError || knownAnswer == kaAlreadyConnected)
                          {
                            cipstartConnectKnownAnswerFound = true;
                          }
                            
                          if(knownAnswer == kaOK)
                          {
                            // законнектились удачно, после этого должна придти строка ID,CONNECT
                            if(clientsQueue.size())
                            {
                               ESPClientQueueData dt = clientsQueue[0];
                               removeClientFromQueue(dt.client);                              
                            }
                          }
                          else
                          {
                              
                            if(clientsQueue.size())
                            {
                               #ifdef WIFI_DEBUG
                                Serial.print(F("ESP: Client connect ERROR, received: "));
                                Serial.println(*wiFiReceiveBuff);
                               #endif
                               
                               ESPClientQueueData dt = clientsQueue[0];

                               CoreTransportClient* thisClient = dt.client;
                               removeClientFromQueue(thisClient);

                               // если мы здесь, то мы получили ERROR или ALREADY CONNECTED сразу после команды
                               // AT+CIPSTART. Это значит, что пока у внешнего клиента нет ID, мы его должны
                               // временно назначить, сообщить клиенту, и освободить этот ID.
                               thisClient->bind(cipstartConnectClientID);                               
                               notifyClientConnected(*thisClient,false,CT_ERROR_CANT_CONNECT);
                               thisClient->release();
                            }

                            // ошибка соединения, строка ID,CONNECT нас уже не волнует
                            waitCipstartConnect = false;
                            cipstartConnectClient = NULL;
                            
                          } // else
                          machineState = espIdle; // переходим к следующей команде
                        }                    
                    
                  }
                  break; // cmdCIPSTART


                  case cmdWaitSendDone:
                  {
                    // дожидаемся результата отсыла данных
                      
                      if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                      {
                        if(knownAnswer == kaSendOk)
                        {
                          // send ok
                          if(clientsQueue.size())
                          {
                             ESPClientQueueData dt = clientsQueue[0];
                             
                             CoreTransportClient* thisClient = dt.client;
                             removeClientFromQueue(thisClient);

                             // очищаем данные у клиента
                             thisClient->clear();

                             notifyDataWritten(*thisClient,CT_ERROR_NONE);
                          }                     
                        } // send ok
                        else
                        {
                          // send fail
                          if(clientsQueue.size())
                          {
                             ESPClientQueueData dt = clientsQueue[0];

                             CoreTransportClient* thisClient = dt.client;
                             removeClientFromQueue(thisClient);
                                                          
                             // очищаем данные у клиента
                             thisClient->clear();
                             
                             notifyDataWritten(*thisClient,CT_ERROR_CANT_WRITE);
                          }                     
                        } // else send fail
  
                        machineState = espIdle; // переходим к следующей команде
                        
                      } // if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                       

                  }
                  break; // cmdWaitSendDone

                  case cmdCIPSEND:
                  {
                    // тут отсылали запрос на запись данных с клиента
                    if(*wiFiReceiveBuff == F(">"))
                    {
                       // дождались приглашения, можем писать в ESP
                       // тут пишем напрямую
                       if(clientsQueue.size())
                       {
                          // говорим, что ждём окончания отсыла данных
                          currentCommand = cmdWaitSendDone;                          
                          ESPClientQueueData dt = clientsQueue[0];

                          #ifdef WIFI_DEBUG
                            Serial.print(F("ESP: > RECEIVED, CLIENT #"));
                            Serial.print(dt.client->socket);
                            Serial.print(F("; LENGTH="));
                            Serial.println(dt.dataLength);
                          #endif

                          workStream->write(dt.data,dt.dataLength);

                          waitTransmitComplete();

                          #ifdef WIFI_DEBUG
                            Serial.println(F("CLEAR CLIENT DATA !!!"));
                          #endif
                          
                          delete [] clientsQueue[0].data;
                          delete [] clientsQueue[0].ip;
                          clientsQueue[0].data = NULL;
                          clientsQueue[0].ip = NULL;
                          clientsQueue[0].dataLength = 0;

                          // очищаем данные у клиента сразу после отсыла
                          dt.client->clear();
                       }
                    } // if
                    else
                    if(wiFiReceiveBuff->indexOf(F("FAIL")) != -1 || wiFiReceiveBuff->indexOf(F("ERROR")) != -1)
                    {
                       // всё плохо, не получилось ничего записать
                      if(clientsQueue.size())
                      {
                         
                         ESPClientQueueData dt = clientsQueue[0];

                         CoreTransportClient* thisClient = dt.client;
                         removeClientFromQueue(thisClient);

                         #ifdef WIFI_DEBUG
                          Serial.print(F("ESP: CLIENT WRITE ERROR #"));
                          Serial.println(thisClient->socket);
                         #endif

                         // очищаем данные у клиента
                         thisClient->clear();

                         notifyDataWritten(*thisClient,CT_ERROR_CANT_WRITE);
                        
                      }                     
                      
                      machineState = espIdle; // переходим к следующей команде
              
                    } // else can't write
                    
                  }
                  break; // cmdCIPSEND
                  
                  case cmdWantReady: // ждём загрузки модема в ответ на команду AT+RST
                  {
                    if(isESPBootFound(*wiFiReceiveBuff))
                    {
                      #ifdef WIFI_DEBUG
                        Serial.println(F("ESP: BOOT FOUND!!!"));
                      #endif
                      
                      machineState = espIdle; // переходим к следующей команде
                    }
                  }
                  break; // cmdWantReady

                  case cmdEchoOff:
                  {
                    if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                    {
                      #ifdef WIFI_DEBUG
                        Serial.println(F("ESP: Echo OFF command processed."));
                      #endif
                      machineState = espIdle; // переходим к следующей команде
                    }
                  }
                  break; // cmdEchoOff

                  case cmdCWMODE:
                  {
                    if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                    {
                      #ifdef WIFI_DEBUG
                        Serial.println(F("ESP: CWMODE command processed."));
                      #endif
                      machineState = espIdle; // переходим к следующей команде
                    }
                  }
                  break; // cmdCWMODE

                  case cmdCWSAP:
                  {
                    if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                    {
                      #ifdef WIFI_DEBUG
                        Serial.println(F("ESP: CWSAP command processed."));
                      #endif
                      machineState = espIdle; // переходим к следующей команде
                    }  
                  }
                  break; // cmdCWSAP

                  case cmdCWJAP:
                  {                    
                    if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                    {

                      machineState = espIdle; // переходим к следующей команде

                      if(knownAnswer != kaOK)
                      {
                        // ошибка подсоединения к роутеру
                        #ifdef WIFI_DEBUG
                          Serial.println(F("ESP: CWJAP command FAIL, RESTART!"));
                        #endif
                        restart();
                      }
                      else
                      {
                        // подсоединились успешно
                        #ifdef WIFI_DEBUG
                          Serial.println(F("ESP: CWJAP command processed."));
                        #endif                        
                      }
                  
                    }  
                  }
                  break; // cmdCWJAP

                  case cmdCWQAP:
                  {                    
                    if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                    {
                      #ifdef WIFI_DEBUG
                        Serial.println(F("ESP: CWQAP command processed."));
                      #endif
                      machineState = espIdle; // переходим к следующей команде
                    }  
                  }
                  break; // cmdCWQAP

                  case cmdCIPMODE:
                  {                    
                    if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                    {
                      #ifdef WIFI_DEBUG
                        Serial.println(F("ESP: CIPMODE command processed."));
                      #endif
                      machineState = espIdle; // переходим к следующей команде
                    }  
                  }
                  break; // cmdCIPMODE

                  case cmdCIPMUX:
                  {                    
                    if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                    {
                      #ifdef WIFI_DEBUG
                        Serial.println(F("ESP: CIPMUX command processed."));
                      #endif
                      machineState = espIdle; // переходим к следующей команде
                    }  
                  }
                  break; // cmdCIPMUX
                  
                  case cmdCIPSERVER:
                  {                    
                    if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                    {
                      #ifdef WIFI_DEBUG
                        Serial.println(F("ESP: CIPSERVER command processed."));
                      #endif
                      machineState = espIdle; // переходим к следующей команде
                    }  
                  }
                  break; // cmdCIPSERVER

                  case cmdCheckModemHang:
                  {                    
                    if(isKnownAnswer(*wiFiReceiveBuff,knownAnswer))
                    {
                      #ifdef WIFI_DEBUG
                        Serial.println(F("ESP: ESP answered and available."));
                      #endif
                      machineState = espIdle; // переходим к следующей команде

                       if(flags.wantReconnect)
                       {
                          // требуется переподсоединение к роутеру. Проще всего это сделать вызовом restart - тогда весь цикл пойдёт с начала
                          restart();

                          // чтобы часто не дёргать реконнект - мы говорим, что после рестарта надо подождать 5 секунд перед тем, как обрабатывать следующую команду
                          #ifdef WIFI_DEBUG
                            Serial.println(F("ESP: Wait 5 seconds before reconnect..."));
                          #endif
                          flags.onIdleTimer = true;
                          timer = millis();
                          idleTime = 5000;
                          
                       } // if(flags.wantReconnect)
                      
                    } // if(isKnownAnswer

                     if(*wiFiReceiveBuff == F("No AP"))
                     {
                        GlobalSettings* Settings = MainController->GetSettings();
                        if(Settings->GetWiFiState() & 0x01)
                        {
                          #ifdef WIFI_DEBUG
                            Serial.println(F("ESP: No connect to router, want to reconnect..."));
                          #endif
                          // нет соединения с роутером, надо переподсоединиться, как только это будет возможно.
                          flags.wantReconnect = true;
                          flags.connectedToRouter = false;
                        }
                      
                     } // if
                      else
                      {
                        // на случай, когда ESP не выдаёт WIFI CONNECTED в порт - проверяем статус коннекта тут,
                        // как признак, что строчка содержит ID нашей сети, проще говоря - не равна No AP
                        if(wiFiReceiveBuff->startsWith(F("+CWJAP")))
                          flags.connectedToRouter = true;
                        
                      }
                    
                  }
                  break; // cmdCheckModemHang
                                    
                } // switch

                
              } // if(hasAnswerLine)
              
         
        }
        break; // espWaitAnswer

        case espReboot:
        {
          // ждём перезагрузки модема
          uint32_t powerOffTime = WIFI_REBOOT_TIME;
          
          if(millis() - timer > powerOffTime)
          {
            #ifdef WIFI_DEBUG
              Serial.println(F("ESP: turn power ON!"));
            #endif

            bool useRebootPin =
            #ifdef USE_WIFI_REBOOT_PIN
              true;
            #else
              false;
            #endif
            
            if(useRebootPin)
            {
              pinMode(WIFI_REBOOT_PIN,OUTPUT);
              digitalWrite(WIFI_REBOOT_PIN,WIFI_POWER_ON);
            }

            machineState = espWaitInit;
            timer = millis();
            
          } // if
        }
        break; // espReboot

        case espWaitInit:
        {
          uint32_t waitTime = WIFI_WAIT_AFTER_REBOOT_TIME;
          if(millis() - timer > waitTime)
          {            
            restart();
            #ifdef WIFI_DEBUG
              Serial.println(F("ESP: inited after reboot!"));
            #endif
          } // 
        }
        break;
      
    } // switch

  } // if(!flags.onIdleTimer)


  if(hasAnswerLine)
  {
    // не забываем чистить за собой
      delete wiFiReceiveBuff;
      wiFiReceiveBuff = new String();       
  }


    if(!hasAnswer) // проверяем на зависание
    {

      // нет ответа от ESP, проверяем, зависла ли она?
      uint32_t hangTime = WIFI_MAX_ANSWER_TIME;
      if(millis() - timer > hangTime)
      {
        #ifdef WIFI_DEBUG
          Serial.println(F("ESP: modem not answering, reboot!"));
        #endif

        bool useRebootPin = 
        #ifdef USE_WIFI_REBOOT_PIN
          true;
        #else
          false;
        #endif

        if(useRebootPin)
        {
          // есть пин, который надо использовать при зависании
          pinMode(WIFI_REBOOT_PIN,OUTPUT);
          digitalWrite(WIFI_REBOOT_PIN,WIFI_POWER_OFF);
        }

        machineState = espReboot;
        timer = millis();
        
      } // if   
         
    }
    
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::begin()
{

  #ifdef WIFI_DEBUG
    Serial.println(F("ESP: begin."));
  #endif
    
  workStream = &WIFI_SERIAL;
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
  

  restart();

  bool useRebootPin = 
  #ifdef USE_WIFI_REBOOT_PIN
    true;
  #else
    false;
  #endif

  if(useRebootPin)
  {
    // есть пин, который надо использовать при зависании
    pinMode(WIFI_REBOOT_PIN,OUTPUT);
    digitalWrite(WIFI_REBOOT_PIN, WIFI_POWER_OFF);
    machineState = espReboot;
  }

  #ifdef WIFI_DEBUG
    Serial.println(F("ESP: started."));
  #endif

}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::restart()
{
  delete wiFiReceiveBuff;
  wiFiReceiveBuff = new String();

  // очищаем очередь клиентов, заодно им рассылаем события
  clearClientsQueue(true);

  // т.к. мы ничего не инициализировали - говорим, что мы не готовы предоставлять клиентов
  flags.ready = false;
  flags.isAnyAnswerReceived = false;
  flags.waitForDataWelcome = false;
  flags.connectedToRouter = false;
  flags.wantReconnect = false;
  flags.onIdleTimer = false;
  flags.bPaused = false;
  
  timer = millis();

  currentCommand = cmdNone;
  machineState = espIdle;

  // инициализируем очередь командами по умолчанию
 createInitCommands(true);
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::createInitCommands(bool addResetCommand)
{  
  // очищаем очередь команд
  clearInitCommands();


  GlobalSettings* Settings = MainController->GetSettings();
 
  if(Settings->GetWiFiState() & 0x01) // коннектимся к роутеру
    initCommandsQueue.push_back(cmdCWJAP); // коннектимся к роутеру совсем в конце
  else  
    initCommandsQueue.push_back(cmdCWQAP); // отсоединяемся от роутера
    
  initCommandsQueue.push_back(cmdCIPSERVER); // сервер поднимаем в последнюю очередь
  initCommandsQueue.push_back(cmdCIPMUX); // разрешаем множественные подключения
  initCommandsQueue.push_back(cmdCIPMODE); // устанавливаем режим работы
  initCommandsQueue.push_back(cmdCWSAP); // создаём точку доступа
  initCommandsQueue.push_back(cmdCWMODE); // // переводим в смешанный режим
  initCommandsQueue.push_back(cmdEchoOff); // выключаем эхо
  
  if(addResetCommand)
    initCommandsQueue.push_back(cmdWantReady); // надо получить ready от модуля путём его перезагрузки      
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::clearInitCommands()
{
  initCommandsQueue.clear();
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::clearClientsQueue(bool raiseEvents)
{  
  // тут попросили освободить очередь клиентов.
  // для этого нам надо выставить каждому клиенту флаг того, что он свободен,
  // плюс - сообщить, что текущее действие над ним не удалось.  

    for(size_t i=0;i<clientsQueue.size();i++)
    {
        ESPClientQueueData dt = clientsQueue[i];
        delete [] dt.data;
        delete [] dt.ip;

        // если здесь в очереди есть хоть один клиент с неназначенным ID (ждёт подсоединения) - то в события он не придёт,
        // т.к. там сравнивается по назначенному ID. Поэтому мы назначаем ID клиенту в первый свободный слот.
        if(dt.client->socket == NO_CLIENT_ID)
        {
          CoreTransportClient* cl = getClient(NO_CLIENT_ID);
          if(cl)
            dt.client->bind(cl->socket);
        }
        
        if(raiseEvents)
        {
          switch(dt.action)
          {
            case actionDisconnect:
                // при дисконнекте всегда считаем, что ошибок нет
                notifyClientConnected(*(dt.client),false,CT_ERROR_NONE);
            break;
  
            case actionConnect:
                // если было запрошено соединение клиента с адресом - говорим, что соединиться не можем
                notifyClientConnected(*(dt.client),false,CT_ERROR_CANT_CONNECT);
            break;
  
            case actionWrite:
              // если попросили записать данные - надо сообщить подписчикам, что не можем записать данные
              notifyDataWritten(*(dt.client),CT_ERROR_CANT_WRITE);
              notifyClientConnected(*(dt.client),false,CT_ERROR_NONE);
            break;
          } // switch
          

        } // if(raiseEvents)

        dt.client->clear();
        
    } // for

  clientsQueue.clear();

}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreESPTransport::isClientInQueue(CoreTransportClient* client, ESPClientAction action)
{
  for(size_t i=0;i<clientsQueue.size();i++)
  {
    if(clientsQueue[i].client == client && clientsQueue[i].action == action)
      return true;
  }

  return false;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::addClientToQueue(CoreTransportClient* client, ESPClientAction action, const char* ip, uint16_t port)
{
  while(isClientInQueue(client, action))
  {
    #ifdef WIFI_DEBUG
      Serial.print(F("ESP: Client #"));
      Serial.print(client->socket);
      Serial.print(F(" with same action already in queue, ACTION="));
      Serial.print(action);
      Serial.println(F(" - remove that client!"));
    #endif
    removeClientFromQueue(client,action);
  }

    ESPClientQueueData dt;
    dt.client = client;
    dt.action = action;
    
    dt.ip = NULL;
    if(ip)
    {
      dt.ip = new char[strlen(ip)+1];
      strcpy(dt.ip,ip);
    }
    dt.port = port;

    clientsQueue.push_back(dt);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::removeClientFromQueue(CoreTransportClient* client, ESPClientAction action)
{
  
  for(size_t i=0;i<clientsQueue.size();i++)
  {
    if(clientsQueue[i].client == client && clientsQueue[i].action == action)
    {
      delete [] clientsQueue[i].ip;
      delete [] clientsQueue[i].data;
      client->clear();
      
        for(size_t j=i+1;j<clientsQueue.size();j++)
        {
          clientsQueue[j-1] = clientsQueue[j];
        }
        
        clientsQueue.pop();
        break;
    } // if
    
  } // for  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::removeClientFromQueue(CoreTransportClient* client)
{
  
  for(size_t i=0;i<clientsQueue.size();i++)
  {
    if(clientsQueue[i].client == client)
    {
      
      delete [] clientsQueue[i].ip;
      delete [] clientsQueue[i].data;
      client->clear();
      
        for(size_t j=i+1;j<clientsQueue.size();j++)
        {
          clientsQueue[j-1] = clientsQueue[j];
        }
        
        clientsQueue.pop();
        break;
    } // if
    
  } // for
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::beginWrite(CoreTransportClient& client)
{
  
  // добавляем клиента в очередь на запись
  addClientToQueue(&client, actionWrite);

  // клиент добавлен, теперь при обновлении транспорта мы начнём работать с записью в поток с этого клиента
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::beginConnect(CoreTransportClient& client, const char* ip, uint16_t port)
{

  if(client.connected())
  {
    
    #ifdef WIFI_DEBUG
      Serial.println(F("ESP: client already connected!"));
    #endif
    return;
    
  }
  
  // добавляем клиента в очередь на соединение
  addClientToQueue(&client, actionConnect, ip, port);

  // клиент добавлен, теперь при обновлении транспорта мы начнём работать с соединением клиента
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreESPTransport::beginDisconnect(CoreTransportClient& client)
{
  if(!client.connected())
  {
    return;
  }

  // добавляем клиента в очередь на соединение
  addClientToQueue(&client, actionDisconnect);

  // клиент добавлен, теперь при обновлении транспорта мы начнём работать с отсоединением клиента
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreESPTransport::ready()
{
  return flags.ready && flags.isAnyAnswerReceived; // если мы полностью инициализировали ESP - значит, можем работать
}
//--------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_WIFI_MODULE
//--------------------------------------------------------------------------------------------------------------------------------------
// CoreMQTT
//--------------------------------------------------------------------------------------------------------------------------------------
CoreMQTT::CoreMQTT()
{
  timer = 0;
  machineState = mqttWaitClient;
  currentTransport = NULL;
  mqttMessageId = 0;
  streamBuffer = new String();
  currentTopicNumber = 0;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::AddTopic(const char* topicIndex, const char* topicName, const char* moduleName, const char* sensorType, const char* sensorIndex, const char* topicType)
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
void CoreMQTT::DeleteAllTopics()
{
  // удаляем все топики
  FileUtils::RemoveFiles(F("MQTT"));
  currentTopicNumber = 0;
}
//--------------------------------------------------------------------------------------------------------------------------------
uint16_t CoreMQTT::GetSavedTopicsCount()
{
  if(!MainController->HasSDCard()) // нет SD-карты, деградируем в жёстко прошитые настройки
    return 0;

    String folderName = F("MQTT");
    return FileUtils::CountFiles(folderName);
        
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::reset()
{

  currentClient.disconnect();
  timer = 0;
  machineState = mqttWaitClient;
  mqttMessageId = 0;

  clearReportsQueue();
  clearPublishQueue();
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::clearPublishQueue()
{
  for(size_t i=0;i<publishList.size();i++)
  {
    delete [] publishList[i].payload;
    delete [] publishList[i].topic;
  }

  publishList.clear();
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::processIncomingPacket(CoreTransportClient* client, uint8_t* packet, size_t dataLen)
{
  UNUSED(client);
  
  if(!dataLen)
    return;

  if(dataLen > 0)
  {

    uint8_t bCommand = packet[0];
    if((bCommand & MQTT_PUBLISH_COMMAND) == MQTT_PUBLISH_COMMAND)
    {
      // это к нам опубликовали топик
      #ifdef MQTT_DEBUG
        Serial.println(F("MQTT: PUBLISH topic found!!!"));
      #endif

      bool isQoS1 = (bCommand & 6) == MQTT_QOS1;

      // декодируем длину сообщения
      
        uint32_t multiplier = 1;
        int16_t remainingLength = 0;
        uint16_t curReadPos = 1;
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


      if(curReadPos >= dataLen) // malformed
      {
      //    DBGLN(F("MQTT: MALFORMED 1"));
        return;
      }

      // теперь получаем имя топика
      uint8_t topicLengthMSB = packet[curReadPos];    
      curReadPos++;

      if(curReadPos >= dataLen) // malformed
      {
      //    DBGLN(F("MQTT: MALFORMED 2"));
        return;
      }
            
      uint8_t topicLengthLSB = packet[curReadPos];
      curReadPos++;

      uint16_t topicLength = (topicLengthMSB<<8)+topicLengthLSB;
      
      // теперь собираем топик
      String topic;
      for(uint16_t j=0;j<topicLength;j++)
      {
        if(curReadPos >= dataLen) // malformed
        {
       //     DBGLN(F("MQTT: MALFORMED 3"));
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
              Serial.print(F("MQTT: Payload are: "));
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
              Serial.print(F("MQTT: Topic are: "));
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

            // удаляем ненужные префиксы
            topic.remove(0,(normalizedTopic - topic.c_str()) + 4 );

            for(uint16_t k=0;k<topic.length();k++)
            {
              if(topic[k] == '/')
              {
                  topic[k] = '|';             
              }
            } // for

              #ifdef MQTT_DEBUG
                Serial.print(F("Normalized topic are: "));
                Serial.println(topic);
              #endif

              delete streamBuffer;
              streamBuffer = new String();

              //ставим транспорт на паузу, чтобы избежать сайд-эффектов по yield
              currentTransport->pause();
                ModuleInterop.QueryCommand(isSetCommand ? ctSET : ctGET , topic, false);
              currentTransport->resume();
              
              if(PublishSingleton.Flags.Status)
                *streamBuffer = OK_ANSWER;
              else
                *streamBuffer = ERR_ANSWER;

             *streamBuffer += '=';
  
            int idx = topic.indexOf(PARAM_DELIMITER);
            if(idx == -1)
              *streamBuffer += topic;
            else
              *streamBuffer += topic.substring(0,idx);
            
            if(PublishSingleton.Text.length())
            {
              *streamBuffer += "|";
              *streamBuffer += PublishSingleton.Text;
            }                

              pushToReportQueue(streamBuffer);
            
          } // if(isSetCommand || isGetCommand)
          else // unsupported topic
          {
              #ifdef MQTT_DEBUG
                Serial.print(F("Unsupported topic: "));
                Serial.println(topic);
              #endif
          } // else
          
      } // if(topic.length())
      else
      {
        #ifdef MQTT_DEBUG
          Serial.println(F("Malformed topic name!!!"));
        #endif
      }

    } // if((bCommand & MQTT_PUBLISH_COMMAND) == MQTT_PUBLISH_COMMAND)
    
  } // if(dataLen > 0)    
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::pushToReportQueue(String* toReport)
{
  
  String* newReport = new String();
  *newReport = *toReport;

#ifdef MQTT_DEBUG
  Serial.print(F("MQTT: Want to report - "));
  Serial.println(*newReport);
#endif  

  reportQueue.push_back(newReport);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::OnClientDataAvailable(CoreTransportClient& client, uint8_t* data, size_t dataSize, bool isDone)
{
  UNUSED(isDone);
  
  if(!currentClient || client != currentClient) // не наш клиент
    return;

  timer = millis();

  if(machineState == mqttWaitSendConnectPacketDone)
  {
    machineState = mqttSendSubscribePacket;
  }
  else
  if(machineState == mqttWaitSendSubscribePacketDone)
  {
    machineState = mqttSendPublishPacket;
  }
  else
  if(machineState == mqttWaitSendPublishPacketDone)
  {
    // отсылали пакет публикации, тут к нам пришла обратка,
    // поскольку мы подписались на все топики для нашего клиента, на будущее
     machineState = mqttSendPublishPacket;

     #ifdef MQTT_DEBUG
      Serial.println(F("MQTT: process incoming packet, machineState == mqttWaitSendPublishPacketDone"));
     #endif

    // по-любому обрабатываем обратку
    processIncomingPacket(&currentClient, data, dataSize);
  }
  else
  {
     #ifdef MQTT_DEBUG
      Serial.println(F("MQTT: process incoming packet on idle mode..."));
     #endif    
      // тут разбираем, что пришло от брокера. Если мы здесь, значит данные от брокера
      // пришли в необрабатываемую ветку, т.е. это публикация прямо с брокера.
      processIncomingPacket(&currentClient, data, dataSize);
  }
    
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::OnClientDataWritten(CoreTransportClient& client, int16_t errorCode)
{

  UNUSED(errorCode);
  
  if(!currentClient || client != currentClient) // не наш клиент
    return;
  
  timer = millis();
   
  if(errorCode != CT_ERROR_NONE)
  {
    #ifdef MQTT_DEBUG
      Serial.println(F("MQTT: Can't write to client!"));
    #endif
    clearReportsQueue();
    clearPublishQueue();
    machineState = mqttWaitReconnect;

    return;
  }
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::OnClientConnect(CoreTransportClient& client, bool connected, int16_t errorCode)
{
  UNUSED(errorCode);
  
  if(!currentClient || client != currentClient) // не наш клиент
    return;

  if(!connected)
  {
    // клиент не подсоединился, сбрасываем текущего клиента и вываливаемся в ожидание переподсоединения.
    #ifdef MQTT_DEBUG
      Serial.println(F("MQTT: Disconnected from broker, try to reconnect..."));
    #endif
    
    clearReportsQueue();
    clearPublishQueue();
    machineState = mqttWaitReconnect;
    timer = millis();    
  }
  else
  {
    // клиент подсоединён, переходим на отсыл пакета с авторизацией
    machineState = mqttSendConnectPacket;
  }
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::convertAnswerToJSON(const String& answer, String* resultBuffer)
{
  // тут мы должны сформировать объект JSON из ответа, для этого надо разбить ответ по разделителям, и для каждого параметра создать именованное поле
  // в анонимном JSON-объекте
  // прикинем, сколько нам памяти надо резервировать, чтобы вместиться
  int16_t neededJsonLen = 3; // {} - под скобки и завершающий ноль
  // считаем кол-во параметров ответа
  int16_t jsonParamsCount=1; // всегда есть один ответ
  int16_t answerLen = answer.length();
  
  for(int16_t j=0;j<answerLen;j++)
  {
    if(answer[j] == '|') // разделитель
      jsonParamsCount++;
  }
  // у нас есть количество параметров, под каждый параметр нужно минимум 6 символов ("p":""), плюс длина числа, которое будет как имя
  // параметра, плюс длина самого параметра, плюс запятые между параметрами
  int16_t paramNameCharsCount = jsonParamsCount > 9 ? 2 : 1;

   neededJsonLen += (6 + paramNameCharsCount)*jsonParamsCount + (jsonParamsCount-1) + answer.length();

   // теперь можем резервировать память
   resultBuffer->reserve(neededJsonLen);

   // теперь формируем наш JSON-объект
   *resultBuffer = '{'; // начали объект

    if(answerLen > 0)
    {
       int16_t currentParamNumber = 1;

       *resultBuffer += F("\"p");
       *resultBuffer += currentParamNumber;
       *resultBuffer += F("\":\"");
       
       for(int16_t j=0;j<answerLen;j++)
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
bool CoreMQTT::publish(const char* topicName, const char* payload)
{
  
  if(!enabled() || !currentTransport || !currentClient || !topicName) // выключены
    return false; 
    
  MQTTPublishQueue pq;
  int16_t tnLen = strlen(topicName);
  pq.topic = new char[tnLen+1];
  memset(pq.topic,0,tnLen+1);
  strcpy(pq.topic,topicName);

  pq.payload = NULL;
  if(payload)
  {
    int16_t pllen = strlen(payload);
    pq.payload = new char[pllen+1];
    memset(pq.payload,0,pllen+1);
    strcpy(pq.payload,payload);    
  }

  publishList.push_back(pq);

  return true;
}
//--------------------------------------------------------------------------------------------------------------------------------
bool CoreMQTT::enabled()
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
MQTTSettings CoreMQTT::getSettings()
{
    MQTTSettings result;
    // Тут читаем настройки с SD
    String mqttSettingsFileName = F("mqtt.ini");

    SdFile f;
    if(f.open(mqttSettingsFileName.c_str(),FILE_READ))
    {
      // первые две строки - адрес сервера и порт        
      FileUtils::readLine(f,result.serverAddress);
      String dummy;
      FileUtils::readLine(f,dummy);
      result.port = dummy.toInt();

      // в третьей строке - ID клиента
      FileUtils::readLine(f,result.clientID);

      // в четвёртой - пользователь
      FileUtils::readLine(f,result.userName);
      
      // в пятой - пароль
      FileUtils::readLine(f,result.password);
      
      f.close();
    } // if(f) 

    if(!result.clientID.length())
      result.clientID = DEFAULT_MQTT_CLIENT;


  return result;
}
//--------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::reloadSettings()
{
  currentSettings = getSettings();
  intervalBetweenTopics = MemRead(MQTT_INTERVAL_BETWEEN_TOPICS_ADDRESS);
  
  if(!intervalBetweenTopics || intervalBetweenTopics == 0xFF)
    intervalBetweenTopics = 10; // 10 секунд по умолчанию на публикацию между топиками

  intervalBetweenTopics *= 1000;

  if(currentClient.connected())
    currentClient.disconnect();
}
//--------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::update()
{
  if(!enabled() || !currentTransport) // выключены
    return; 
  
  switch(machineState)
  {
    
      case mqttWaitClient:
      {
        if(currentTransport->ready())
        {
          #ifdef MQTT_DEBUG
            Serial.println(F("MQTT: Start connect!"));
          #endif
            currentClient.connect(currentSettings.serverAddress.c_str(), currentSettings.port);
            machineState = mqttWaitConnection; 
            timer = millis();
        } // if(currentTransport->ready())
      }
      break; // mqttWaitClient

      case mqttWaitConnection:
      {
        uint32_t toWait = 20000;        
        if(millis() - timer > toWait)
        {
          #ifdef MQTT_DEBUG
            Serial.print(F("MQTT: unable to connect within "));
            Serial.print(toWait/1000);
            Serial.println(F(" seconds, try to reconnect..."));
          #endif
          
          // долго ждали, переподсоединяемся
          clearReportsQueue();
          clearPublishQueue();
          machineState = mqttWaitReconnect;
          timer = millis();
        }
      }
      break; // mqttWaitConnection

      case mqttWaitReconnect:
      {
        if(millis() - timer > 10000)
        {
          #ifdef MQTT_DEBUG
            Serial.println(F("MQTT: start reconnect!"));
          #endif
          clearReportsQueue();
          clearPublishQueue();
          machineState = mqttWaitClient;
        }
      }
      break; // mqttWaitReconnect

      case mqttSendConnectPacket:
      {
        if(currentClient.connected())
        {
          #ifdef MQTT_DEBUG
            Serial.println(F("MQTT: start send connect packet!"));
          #endif  
  
          String mqttBuffer;
          int16_t mqttBufferLength;
          
          constructConnectPacket(mqttBuffer,mqttBufferLength,
            currentSettings.clientID.c_str() // client id
          , currentSettings.userName.length() ? currentSettings.userName.c_str() : NULL // user
          , currentSettings.password.length() ? currentSettings.password.c_str() : NULL // pass
          , NULL // will topic
          , 0 // willQoS
          , 0 // willRetain
          , NULL // will message
          );

          // переключаемся на ожидание результата отсылки пакета
          machineState = mqttWaitSendConnectPacketDone;

          #ifdef MQTT_DEBUG
            Serial.println(F("MQTT: WRITE CONNECT PACKET TO CLIENT!"));
          #endif
          
          // сформировали пакет CONNECT, теперь отсылаем его брокеру
          currentClient.write((uint8_t*) mqttBuffer.c_str(),mqttBufferLength);
         
          timer = millis();
        }  // if(currentClient)
        else
        {
          #ifdef MQTT_DEBUG
            Serial.println(F("MQTT: client not connected in construct CONNECT packet mode!"));
          #endif
          machineState = mqttWaitReconnect;
          timer = millis();          
        } // no client
        
      }
      break; // mqttSendConnectPacket

      case mqttSendSubscribePacket:
      {

        #ifdef MQTT_DEBUG
          Serial.println(F("MQTT: Subscribe to topics!"));
        #endif

        if(currentClient.connected())
        {
          String mqttBuffer;
          int16_t mqttBufferLength;

          // конструируем пакет подписки
          String topic = currentSettings.clientID;
          topic +=  F("/#");
          constructSubscribePacket(mqttBuffer,mqttBufferLength, topic.c_str());
  
          // переключаемся на ожидание результата отсылки пакета
          machineState = mqttWaitSendSubscribePacketDone;

          #ifdef MQTT_DEBUG
            Serial.println(F("MQTT: WRITE SUBSCRIBE PACKET TO CLIENT!"));
          #endif
          
          // сформировали пакет SUBSCRIBE, теперь отсылаем его брокеру
          currentClient.write((uint8_t*) mqttBuffer.c_str(),mqttBufferLength);
          timer = millis();
        }
        else
        {
          #ifdef MQTT_DEBUG
            Serial.println(F("MQTT: client not connected in construct SUBSCRIBE packet mode!"));
          #endif
          machineState = mqttWaitReconnect;
          timer = millis();          
        } // no client
      
      }
      break; // mqttSendSubscribePacket

      case mqttSendPublishPacket:
      {
        // тут мы находимся в процессе публикации, поэтому можем проверять - есть ли топики для репорта
        bool hasReportTopics = reportQueue.size() > 0;
        bool hasPublishTopics = publishList.size() > 0;
        
        uint32_t interval = intervalBetweenTopics;
        if(hasReportTopics || hasPublishTopics || millis() - timer > interval)
        {
          if(currentClient.connected())
          {
            String mqttBuffer;
            int16_t mqttBufferLength;
  
            String topicName, data;

            if(hasReportTopics)
            {
              // у нас есть топик для репорта
              topicName =  currentSettings.clientID + REPORT_TOPIC_NAME;

              // удаляем перевод строки
              reportQueue[0]->trim();

              // тут в имя топика надо добавить запрошенную команду, чтобы в клиенте можно было ориентироваться
              // на конкретные топики отчёта
              int16_t idx = reportQueue[0]->indexOf("=");
              String commandStatus = reportQueue[0]->substring(0,idx);
              reportQueue[0]->remove(0,idx+1);

              // теперь в reportQueue[0] у нас лежит ответ после OK= или ER=
              String delim = PARAM_DELIMITER;
              idx = reportQueue[0]->indexOf(delim);
              if(idx != -1)
              {
                // есть ответ с параметрами, выцепляем первый - это и будет дополнением к имени топика
                topicName += reportQueue[0]->substring(0,idx);
                reportQueue[0]->remove(0,idx);
                *reportQueue[0] = commandStatus + *reportQueue[0];
              }
              else
              {
                // только один ответ - имя команды, без возвращённых параметров
                topicName += *reportQueue[0];
                *reportQueue[0] = commandStatus;
              }
              

              #ifdef MQTT_REPORT_AS_JSON
                convertAnswerToJSON(*(reportQueue[0]),&data);
              #else
                data = *(reportQueue[0]);            
              #endif

              // тут удаляем из очереди первое вхождение отчёта
              if(reportQueue.size() < 2)
                clearReportsQueue();
              else
              {
                  delete reportQueue[0];
                  for(size_t k=1;k<reportQueue.size();k++)
                  {
                    reportQueue[k-1] = reportQueue[k];
                  }
                  reportQueue.pop();
              }
            } // hasReportTopics
            else
            if(hasPublishTopics)
            {
              // есть пакеты для публикации
              MQTTPublishQueue pq = publishList[0];

              // тут публикуем из пакета для публикации
              topicName =  currentSettings.clientID + "/";
              topicName += pq.topic;

              if(pq.payload)
                data = pq.payload;

              // чистим память
              delete [] pq.topic;
              delete [] pq.payload;
              
              // и удаляем из списка
              if(publishList.size() < 2)
                publishList.clear();
              else
              {
                for(size_t kk=1;kk<publishList.size();kk++)
                {
                  publishList[kk-1] = publishList[kk];  
                }
                publishList.pop();
              }
            } // hasPublishTopics
            else
            {
                // обычный режим работы, отсылаем показания с хранилища
                getNextTopic(topicName,data);

            } // else send topics

              if(data.length() && topicName.length())
              {
                 // конструируем пакет публикации
                 constructPublishPacket(mqttBuffer,mqttBufferLength,topicName.c_str(), data.c_str()); 
      
                // переключаемся на ожидание результата отсылки пакета
                machineState = mqttWaitSendPublishPacketDone;

                #ifdef MQTT_DEBUG
                  Serial.println(F("MQTT: WRITE PUBLISH PACKET TO CLIENT!"));
                #endif

                // сформировали пакет PUBLISH, теперь отсылаем его брокеру
                currentClient.write((uint8_t*) mqttBuffer.c_str(),mqttBufferLength);
                timer = millis();
              }
          }
          else
          {
            #ifdef MQTT_DEBUG
              Serial.println(F("MQTT: client not connected in construct PUBLISH packet mode!"));
            #endif
            machineState = mqttWaitReconnect;
            timer = millis();          
          } // no client          
           
        }
      }
      break; // mqttSendPublishPacket

      case mqttWaitSendConnectPacketDone:
      case mqttWaitSendSubscribePacketDone:
      case mqttWaitSendPublishPacketDone:
      {
        if(millis() - timer > 20000)
        {
          #ifdef MQTT_DEBUG
            Serial.println(F("MQTT: wait for send results timeout, reconnect!"));
          #endif
          // долго ждали результата записи в клиента, переподсоединяемся
          clearReportsQueue();
          clearPublishQueue();
          machineState = mqttWaitReconnect;
          timer = millis();
        }        
      }
      break;
      
    
  } // switch

  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::getNextTopic(String& topicName, String& data)
{
  topicName = "";
  data = "";

  String topicFileName = MQTT_FILENAME_PATTERN;
  topicFileName += String(currentTopicNumber);

  if(!SDFat.exists(topicFileName.c_str())) // нет топика
  {
    currentTopicNumber = 0; // переключаемся на первый топик
    return;
  }

  // тут можем читать из файла настроек топика
  SdFile f;
  
  if(!f.open(topicFileName.c_str(),FILE_READ)) // не получилось открыть файл
  {
    switchToNextTopic();
    return;          
  }

  // теперь читаем настройки топика
  // первой строкой идёт имя топика
  FileUtils::readLine(f,topicName);

  // добавляем ID клиента перед именем топика
  topicName = currentSettings.clientID + "/" + topicName;  

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

      // тут тонкость - команда у нас с изменёнными параметрами, где все разделители заменены на символ @
      // поэтому перед выполнением - меняем назад
      moduleName.replace('@','|');

      // ставим текущий транспорт на паузу, чтобы не было сайд-эффектов обнлоления
      currentTransport->pause();
        ModuleInterop.QueryCommand(ctGET, moduleName, true);
      currentTransport->resume();

      #ifdef MQTT_REPORT_AS_JSON
        convertAnswerToJSON(PublishSingleton.Text,&data);
      #else // ответ как есть, в виде RAW
        dataOut = PublishSingleton.Text;
      #endif

       switchToNextTopic();
      
      return; // нашли и отослали показания

    
  } // if
  else // топик с показаниями датчика
  {
      // теперь получаем модуль у контроллера
      AbstractModule* mod = MainController->GetModuleByID(moduleName.c_str());

      if(!mod) // не нашли такой модуль
      {
        switchToNextTopic();
        return;
      }

      // получаем состояние
      OneState* os = mod->State.GetState(sensorType,sensorIndex);

      if(!os) // нет такого состояния
      {
        switchToNextTopic();
        return;          
      }

      // теперь получаем данные состояния
      if(os->HasData()) // данные с датчика есть, можем читать
        data = *os;
      else
        data = "-"; // нет данных с датчика  

       switchToNextTopic();
      
      return; // нашли и отослали показания
       
  } // sensor data topic    
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::switchToNextTopic()
{
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
void CoreMQTT::clearReportsQueue()
{
  for(size_t i=0;i<reportQueue.size();i++)
  {
    delete reportQueue[i];
  }

  reportQueue.clear();
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::constructPublishPacket(String& mqttBuffer,int16_t& mqttBufferLength, const char* topic, const char* payload)
{
  MQTTBuffer byteBuffer; // наш буфер из байт, в котором будет содержаться пакет

  // тут формируем пакет

  // кодируем топик
  encode(byteBuffer,topic);

  // теперь пишем данные топика
  int16_t sz = strlen(payload);
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
void CoreMQTT::constructSubscribePacket(String& mqttBuffer,int16_t& mqttBufferLength, const char* topic)
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
void CoreMQTT::constructConnectPacket(String& mqttBuffer,int16_t& mqttBufferLength,const char* id, const char* user, const char* pass
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
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::writePacket(MQTTBuffer& fixedHeader, MQTTBuffer& payload, String& mqttBuffer,int16_t& mqttBufferLength)
{
  mqttBuffer = "";
  
// запомнили, сколько байт надо послать в ESP
   mqttBufferLength = fixedHeader.size() + payload.size();

   // теперь записываем это в строку, перед этим зарезервировав память, и заполнив строку пробелами
   mqttBuffer.reserve(mqttBufferLength);
   for(int16_t i=0;i<mqttBufferLength;i++)
    mqttBuffer += ' ';

  // теперь можем копировать данные в строку побайтово
  int16_t writePos = 0;

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
void CoreMQTT::constructFixedHeader(uint8_t command, MQTTBuffer& fixedHeader, size_t payloadSize)
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
void CoreMQTT::encode(MQTTBuffer& buff,const char* str)
{
  if(!str)
    return;

    size_t sz = buff.size(); // запоминаем текущий размер

    // записываем нули, как длину строки, потом мы это поправим
    buff.push_back(0);
    buff.push_back(0);

    const char* ptr = str;
    int16_t strLen = 0;
    while(*ptr)
    {
      buff.push_back(*ptr++);
      strLen++;
    }

    // теперь записываем актуальную длину
    buff[sz] = (strLen >> 8);
    buff[sz+1] = (strLen & 0xFF);
    
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CoreMQTT::begin(CoreTransport* transport)
{
  // попросили начать работу
  // для начала - освободим клиента
  machineState = mqttWaitClient;
  currentTransport = transport;
  mqttMessageId = 0;

  // подписываемся на события клиентов
  if(currentTransport)
  {
    currentTransport->subscribe(this);  
  }

  reloadSettings();
    
  // ну и запомним, когда вызвали начало работы
  timer = millis();
}
//--------------------------------------------------------------------------------------------------------------------------------------

