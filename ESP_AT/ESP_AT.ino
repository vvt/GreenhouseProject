//----------------------------------------------------------------------------------------------------------------------------------------------------------
// СКОРОСТЬ РАБОТЫ С ПОРТОМ !!!
//----------------------------------------------------------------------------------------------------------------------------------------------------------
#define UART_SPEED 115200
//----------------------------------------------------------------------------------------------------------------------------------------------------------
#include <ESP8266WiFi.h>
#include "SerialCommand.h"
#include "Atomic.h"
#include "ESP8266Ping.h"
//----------------------------------------------------------------------------------------------------------------------------------------------------------
#define AT_OK "OK"
#define AT_ERROR "ERROR"
#define AT_FAIL "FAIL"
//----------------------------------------------------------------------------------------------------------------------------------------------------------
// variables
//----------------------------------------------------------------------------------------------------------------------------------------------------------
bool echoOn = true;
String softAPPassword;
String softAPName;
int softAPencMethod;
int softAPchannelNum;
int cipserverMode = 0;
uint16_t cipserverPort = 0;
WiFiServer* server = NULL;
String apName;
String apPassword;
WiFiMode_t cwMode = WIFI_OFF;
uint8_t statusHelper = 0;
//uint32_t segmentID = 0;
//----------------------------------------------------------------------------------------------------------------------------------------------------------
SerialCommand commandStream;
//----------------------------------------------------------------------------------------------------------------------------------------------------------
template <typename T> inline void echo(const char* command, T last,void (*function)(void) = NULL)
{
  if(echoOn)
    Serial << command << ENDLINE;

  if(function)
    function();

  Serial << last << ENDLINE;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void unQuote(String& str)
{
  if(!str.length())
    return;
    
   if(str.startsWith("\""))
    str.remove(0,1);

  if(str.endsWith("\""))
    str.remove(str.length()-1);  
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void printReady(Stream& s)
{
  s << ENDLINE << "ready" << ENDLINE;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void AT(const char* command)
{
  echo(command, AT_OK);
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void ATE0(const char* command)
{
  echo(command, AT_OK);
  echoOn = false;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void ATE1(const char* command)
{
  echoOn = true;
  echo(command, AT_OK);
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void RESTART(const char* command)
{
  echo(command, AT_OK);
  ESP.restart();
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void GMR(const char* command)
{
  CRITICAL_SECTION;
  echo(command, AT_OK,[](){
      Serial << ESP.getCoreVersion() << ENDLINE;
      Serial << ESP.getSdkVersion() << ENDLINE;
      Serial << __DATE__ << " " __TIME__ << ENDLINE;
    });
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CWMODE_CUR(const char* command)
{
   CRITICAL_SECTION;
   char* arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

    if(*arg == '?')
    {
        echo(command, AT_OK,[](){
        Serial << WiFi.getMode() << ENDLINE;    
      });      

      return;
    }

    int mode = *arg - '0';

    if(mode < WIFI_STA || mode > WIFI_AP_STA )
    {
      echo(command, AT_ERROR);
      return;      
    }

   cwMode = WiFiMode_t(mode);
   WiFi.mode(cwMode);
    
   echo(command, AT_OK);
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CWSAP_CUR(const char* command)
{
   CRITICAL_SECTION;
   
   char* arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   softAPName = arg;
   arg = commandStream.next();

   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   softAPPassword = arg;
   arg = commandStream.next();

   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   softAPchannelNum = atoi(arg);
   arg = commandStream.next();

   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   softAPencMethod = atoi(arg);

   unQuote(softAPName);
   unQuote(softAPPassword);
      
  if(!WiFi.softAP(softAPName.c_str(), softAPPassword.c_str(), softAPchannelNum))
  {
    echo(command, AT_ERROR);
    return;    
  }

  echo(command, AT_OK,[](){
      Serial << "+CWSAP_CUR:\"" << softAPName << "\",\"" << softAPPassword << "\"," << softAPchannelNum << "," << softAPencMethod << ENDLINE;
  });
    
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CWJAP_TEST(const char* command)
{
  CRITICAL_SECTION;
  
  if(!WiFi.isConnected())
  {
     echo(command, AT_OK,[](){
        Serial << "No AP" << ENDLINE;
      });
    return;
  }

 echo(command, AT_OK,[](){
        Serial << "+CWJAP:\"" << WiFi.SSID() << "\",\"" << WiFi.BSSIDstr() << "\"," << WiFi.channel() << "," << WiFi.RSSI() << ENDLINE;
      });
 
  
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CWJAP_CUR(const char* command)
{
  CRITICAL_SECTION;

  if(!(cwMode == WIFI_STA || cwMode == WIFI_AP_STA)) // команда подсоединения требует активным режим станции
  {
    DBGLN(F("not configured in STA mode!"));
    echo(command, AT_ERROR);
    return;
  }

   char* arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   apName = arg;
   arg = commandStream.next();

   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }
   
   apPassword = arg;

   unQuote(apName);
   unQuote(apPassword);

   // коннектимся к точке доступа
   /*
   if(WiFi.isConnected())
   {
      DBGLN(F("already connected, disconnect..."));
      WiFi.disconnect();
      DBGLN(F("Disconnected."));
   }
   */

  WiFi.disconnect();
  while (WiFi.status() == WL_CONNECTED)
  {
    delay(10);
  }   

   DBGLN(F("Connect..."));
   WiFi.begin(apName.c_str(),apPassword.c_str());
   DBGLN(F("Connect done, wait for connect result..."));
   uint8_t status = WiFi.waitForConnectResult();

   DBG(F("Connect result done: "));
   DBGLN(status);

   switch(status)
   {
      case WL_CONNECTED: // подсоединились
      {
        DBGLN(F("Connected!"));
        echo(command, AT_OK);
      }
      break;

      case WL_NO_SSID_AVAIL: // не найдена станция
      {
        DBGLN(F("No SSID found!"));
        
        echo(command, AT_FAIL,[](){
            Serial << "+CWJAP_CUR:" << 3 << ENDLINE;
        });        
      }
      break;

      case WL_CONNECT_FAILED: // неправильный пароль
      {
        DBGLN(F("Password incorrect!"));
        
        echo(command, AT_FAIL,[](){
            Serial << "+CWJAP_CUR:" << 2 << ENDLINE;
        });        
      }
      break;

      case WL_IDLE_STATUS:
      {
        DBGLN(F("WL_IDLE_STATUS!"));
        
        echo(command, AT_FAIL,[](){
            Serial << "+CWJAP_CUR:" << 4 << ENDLINE;
        });            
      }
      break;

      case WL_DISCONNECTED: // модуль не сконфигурирован как станция
      {
        DBGLN(F("Not configured as STATION!"));
        
        echo(command, AT_FAIL,[](){
            Serial << "+CWJAP_CUR:" << 4 << ENDLINE;
        });        
        
      }
      break;

      default:
      {
        DBGLN(F("Unknown status!"));
        
        statusHelper = status;
        echo(command, AT_FAIL,[](){
            Serial << "+CWJAP_CUR:" << statusHelper << ENDLINE;
        });   
      }
      break;
    
   } // switch
   
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CWQAP(const char* command)
{
  CRITICAL_SECTION;

  if(WiFi.isConnected())
  {
    WiFi.disconnect(false);
  }

  while (WiFi.status() == WL_CONNECTED)
  {
    delay(10);
  }

   echo(command, AT_OK);
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CIPMODE(const char* command)
{
  CRITICAL_SECTION;

   char* arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   echo(command, AT_OK);
  
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CIPMUX(const char* command)
{
  CRITICAL_SECTION;

   char* arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   echo(command, AT_OK);
  
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void stopServer()
{
  if(!server)
    return;

  server->stop();
  delete server;
  server = NULL;    
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CIPSERVER(const char* command)
{
  CRITICAL_SECTION;

   char* arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   cipserverMode = atoi(arg);
   
   arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   cipserverPort = atoi(arg);

   switch(cipserverMode)
   {
     case 0:
     {
       stopServer();
     }
     break;

     case 1:
     {
        stopServer();
        server = new WiFiServer(cipserverPort);
        server->begin();
     }
     break;
   }

   echo(command, AT_OK);
  
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
int pingAvgTimeHelper = 0;
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void PING(const char* command)
{
   CRITICAL_SECTION;

   char* arg = commandStream.next();
   if(!arg || !WiFi.isConnected())
   {
    echo(command, AT_ERROR);
    return;
   }
   

   String remoteIP = arg;
   unQuote(remoteIP);

   if(Ping.ping(remoteIP.c_str()))
   {
      pingAvgTimeHelper = Ping.averageTime();
      echo(command, AT_OK,[](){

          Serial << "+" << pingAvgTimeHelper << ENDLINE;
        
        });
   }
   else
   {
     echo(command, AT_ERROR,[](){

          Serial << "+timeout" << ENDLINE;
        
        });
   }
   
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void getCIPSTAMAC(const char* command)
{
 CRITICAL_SECTION;

 echo(command, AT_OK,[](){

          Serial << "+CIPSTAMAC:\"" << WiFi.macAddress() << "\"" << ENDLINE;
        
        });  
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void getCIPAPMAC(const char* command)
{
 CRITICAL_SECTION;

 echo(command, AT_OK,[](){

          Serial << "+CIPAPMAC:\"" << WiFi.softAPmacAddress() << "\"" << ENDLINE;
        
        });  
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void getCIFSR(const char* command)
{
  CRITICAL_SECTION;

 echo(command, AT_OK,[](){

          Serial << "+CIFSR:APIP,\"" << WiFi.softAPIP() << "\"" << ENDLINE;
          Serial << "+CIFSR:APMAC,\"" << WiFi.softAPmacAddress() << "\"" << ENDLINE;
          Serial << "+CIFSR:STAIP,\"" << WiFi.localIP() << "\"" << ENDLINE;
          Serial << "+CIFSR:STAMAC,\"" << WiFi.macAddress() << "\"" << ENDLINE;
        
        });    
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CIPCLOSE(const char* command)
{
  CRITICAL_SECTION;

   char* arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_OK);
    return;
   }

   int clientNumber = atoi(arg);
   if(clientNumber < 0)
   {
    echo(command, AT_ERROR);
    return;
   }
    
   if(clientNumber > (MAX_CLIENTS-1))
   {
    echo(command, AT_ERROR);
    return;
   }

   if(Clients[clientNumber].connected())
   {
      DBG(F("Client #"));
      DBG(clientNumber);
      DBGLN(F(" connected, disconnect..."));
      Clients[clientNumber].stop();
      
   }

   echo(command, AT_OK);
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CIPSTART(const char* command)
{
  CRITICAL_SECTION;

  if(!WiFi.isConnected()) // не приконнекчены к роутеру - не можем никуда коннектиться
  {
    echo(command, AT_ERROR);
    return;    
  }
  
   char* arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   int linkID = atoi(arg);
   if( linkID < 0)
   {
    echo(command, AT_ERROR);
    return;
   }

  if(linkID > (MAX_CLIENTS-1))
  {
    echo(command, AT_ERROR);
    return;    
  }

   arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   String connectionType = arg;
   unQuote(connectionType);

   arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   String remoteIP = arg;
   unQuote(remoteIP);

   arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   int remotePort = atoi(arg);

   if(Clients[linkID].connected())
   {
    echo(command, "ALREADY CONNECTED");
    return;    
   }

   if(!Clients[linkID].connect(remoteIP.c_str(),remotePort))
   {
    echo(command, AT_ERROR);
    return;
   }

   echo(command, AT_OK);   
  
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CIPSENDBUF(const char* command)
{
  CRITICAL_SECTION;
  
   char* arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   int linkID = atoi(arg);
   if( linkID < 0)
   {
    echo(command, AT_ERROR);
    return;
   }

  if(linkID > (MAX_CLIENTS-1))
  {
    echo(command, AT_ERROR);
    return;
  }

   arg = commandStream.next();
   if(!arg)
   {
    echo(command, AT_ERROR);
    return;
   }

   size_t dataLength = atoi(arg);
   if(dataLength < 1)
   {
    echo(command, AT_ERROR);
    return;    
   }

   if(!Clients[linkID].connected())
   {
    echo(command, AT_ERROR);
    return;    
   }

   echo(command, AT_OK);

   // вот тут приглашение выводить нельзя, поскольку у нас команда могла дойти
   // только тогда, когда уже Events отработали и выклюнули в поток что-то типа +IPD
   // В этом случае принимающая сторона должна отработать +IPD,
   // а мы, в свою очередь - должны отправить приглашение только тогда, когда
   // уже ничего не делается.
   // для этого надо заводить отдельную очередь для отсыла приглашений на ввод данных, сохраняя там
   // переданного клиента.

   Cipsend.add(dataLength,linkID);

   /*

   Serial << '>'; // выводим приглашение
   // читаем данные
   size_t readed = 0;

   uint8_t* data = new uint8_t[dataLength];
   memset(data,0,dataLength);
   
   while(readed < dataLength)
   {
      if(!Serial.available())
      {
        delay(0);
        continue;
      }

      uint8_t ch = Serial.read();
      data[readed] = ch;
      readed++;
   } // while

   String sendFail = ENDLINE;
   sendFail = linkID;
   sendFail += ",SEND FAIL";

   // данные получили, можно их писать в клиента
   if(!Clients[linkID].connected())
   {
    echo("", sendFail);

    delete [] data;
    return;    
   }   

   if(!Clients[linkID].write((const uint8_t*)data,dataLength))
   {
     delete [] data;
     echo("", sendFail);
     return;
   }

   String sendOK = ENDLINE;
   sendOK = linkID;
   sendOK += ",";
   sendOK += segmentID;
   segmentID++;
   sendOK += ",SEND OK";

   delete [] data;
   echo("", sendOK);
   */

}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void raiseClientStatus(uint8_t clientNumber, bool connected)
{
  String message;
  message = clientNumber;
  if(connected)
    message += ",CONNECT";
  else
    message += ",CLOSED";
    
  message += ENDLINE;
  Events.raise(message.c_str(),message.length());  
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void handleClientConnectStatus()
{
  // обновляем статус клиентов
  for(uint8_t i=0;i<MAX_CLIENTS;i++)
  {
      uint8_t curStatus = Clients[i].connected() ? 1 : 0;
      uint8_t lastStatus = ClientConnectStatus[i];
      ClientConnectStatus[i] = curStatus;
      
      if(curStatus != lastStatus)
      {
          // сообщаем статус клиента
          raiseClientStatus(i,!lastStatus);
                  
      } // if(curStatus != lastStatus)
    
  } // for 
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void handleIncomingConnections()
{
  if(!server)
    return;

  // проверяем на входящие подключения.
  // их принимать можно только тогда, когда есть хотя бы один свободный слот
  uint8_t freeSlotNumber = 0xFF;
  for(uint8_t i=0;i<MAX_CLIENTS;i++)
  {
    if(!Clients[i].connected())
    {
      freeSlotNumber = i;
      break;
    }
  } // for    

  WiFiClient client = server->available();
  if(client.connected())
  {
    DBGLN("CATCH CONNECTED CLIENT IN SERVER!!!");
    DBG("Local port: ");
    DBGLN(client.localPort());
    DBG("Local IP: ");
    DBGLN(client.localIP());
    DBG("Remote port: ");
    DBGLN(client.remotePort());
    DBG("Remote IP: ");
    DBGLN(client.remoteIP());
    DBGLN("");

    if(freeSlotNumber != 0xFF)
    {
      DBG("HAVE FREE SLOT #");
      DBG(freeSlotNumber);
      DBGLN(", MOVE INCOMING CLIENT TO SLOT.");
      // есть свободный слот, помещаем туда клиента
      Clients[freeSlotNumber] = client;     
    } // if(freeSlotNumber != 0xFF)
    else
    {
      // свободных слотов нет, отсоединяем клиента
      DBGLN("NO FREE SLOTS, STOP INCOMING CLIENT.");
      client.stop();
    } // else
  }    
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void handleClientData(uint8_t clientNumber, WiFiClient& client)
{
  const uint16_t buf_sz = 2048;
  static uint8_t read_buff[buf_sz];

  if(!client.available())
    return;
  
  memset(read_buff,0,buf_sz);

  int readed = client.read(read_buff,buf_sz);
  if(readed > 0)
  {
    // есть данные, сообщаем о них. Приходится через raise, чтобы не вклиниться между команд
    String toRaise;
    toRaise.reserve(readed + 20); // резервируем буфер сразу
    toRaise = "+IPD,";
    toRaise += clientNumber;
    toRaise += ",";
    toRaise += readed;
    toRaise += ":";
    for(int i=0;i<readed;i++)
      toRaise += (char) read_buff[i];

    toRaise += ENDLINE;

    Events.raise(toRaise.c_str(),toRaise.length()); 
    
  } // if(readed > 0)
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void handleClientsData()
{
  // тут смотрим, есть ли у клиентов входящие данные.
  // проходимся равномерно по всем клиентам, чтобы не было ситуации,
  // когда пришедший в меньший слот всегда обслуживается первым.
  static uint8_t currentClientNumber = 0;

 if(Clients[currentClientNumber].connected())
      handleClientData(currentClientNumber,Clients[currentClientNumber]);

 currentClientNumber++;

 if(currentClientNumber >= MAX_CLIENTS)
    currentClientNumber = 0;

}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void unknownCommand(const char* command)
{
  CRITICAL_SECTION;
  echo(command, AT_ERROR);
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void setup() 
{
  Serial.begin(UART_SPEED);

  #ifdef _DEBUG
    Serial.setDebugOutput(true);
  #endif

  Events.begin();
  
 // WiFi.setOutputPower(20);
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);   // this is a temporary line, to be removed after SDK update to 1.5.4
  //WiFi.setAutoConnect(false);
  //WiFi.setAutoReconnect(false);

  commandStream.setDefaultHandler(unknownCommand);

  commandStream.addCommand("AT",AT);
  commandStream.addCommand("ATE0",ATE0);
  commandStream.addCommand("ATE1",ATE1);
  commandStream.addCommand("AT+RST",RESTART);
  commandStream.addCommand("AT+GMR",GMR);
  commandStream.addCommand("AT+CWMODE_CUR",CWMODE_CUR);
  commandStream.addCommand("AT+CWMODE_DEF",CWMODE_CUR);
  commandStream.addCommand("AT+CWSAP_CUR",CWSAP_CUR);
  commandStream.addCommand("AT+CWSAP_DEF",CWSAP_CUR);
  commandStream.addCommand("AT+CWQAP",CWQAP);
  commandStream.addCommand("AT+CIPMODE",CIPMODE);
  commandStream.addCommand("AT+CIPMUX",CIPMUX);
  commandStream.addCommand("AT+CIPSERVER",CIPSERVER);
  commandStream.addCommand("AT+CWJAP_CUR",CWJAP_CUR);
  commandStream.addCommand("AT+CWJAP_DEF",CWJAP_CUR);
  commandStream.addCommand("AT+CWJAP?",CWJAP_TEST);
  commandStream.addCommand("AT+PING",PING);
  commandStream.addCommand("AT+CIPSTAMAC?",getCIPSTAMAC);
  commandStream.addCommand("AT+CIPAPMAC?",getCIPAPMAC);
  commandStream.addCommand("AT+CIFSR",getCIFSR);
  commandStream.addCommand("AT+CIPCLOSE",CIPCLOSE);
  commandStream.addCommand("AT+CIPSTART",CIPSTART);
  commandStream.addCommand("AT+CIPSENDBUF",CIPSENDBUF);
  commandStream.addCommand("AT+CIPSEND",CIPSENDBUF);
  
  

  printReady(Serial);


}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void loop() 
{
  // здесь мы уже обработали входящую команду, если это запрос на подсоединение -
  // слот клиента будет занят
  commandStream.readSerial();

  // теперь отрабатываем соединения от сервера
  if(!commandStream.waitingCommand())
    handleIncomingConnections();

  // выводим статус соединений
  handleClientConnectStatus();

  // обрабатываем данные для клиентов
  handleClientsData();

  // и выводим их через события
  if(!commandStream.waitingCommand())
    Events.update();

  Cipsend.update();
    
  delay(0);
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------

