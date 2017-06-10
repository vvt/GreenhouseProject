#include "HttpModule.h"
#include "ModuleController.h"
#include "InteropStream.h"
//--------------------------------------------------------------------------------------------------------------------------------
#define HTTP_START_OF_HEADERS F("POST /check_commands.php HTTP/1.1\r\nConnection: close\r\nContent-Type: application/x-www-form-urlencoded\r\nHost: ")
#define HTTP_CONTENT_LENGTH_HEADER F("Content-Length: ")
#define HTTP_END_OF_HEADER F("\r\n")
#define HTTP_START_OF_COMMAND F("[~]")
//--------------------------------------------------------------------------------------------------------------------------------
/*
    Список внутренних идентификаторов команд:
    
      1 - открыть все окна
      2 - закрыть все окна
      3 - открыть определённое окно
      4 - закрыть определённое окно
      5 - включить полив на всех каналах
      6 - выключить полив на всех каналах
      7 - включить полив на определённом канале
      8 - выключить полив на определённом канале
      9 - включить досветку
      10 - выключить досветку
      11 - включить пин
      12 - выключить пин
 */
//--------------------------------------------------------------------------------------------------------------------------------
#define HTTP_COMMAND_OPEN_WINDOWS 1
#define HTTP_COMMAND_CLOSE_WINDOWS 2
#define HTTP_COMMAND_OPEN_WINDOW 3
#define HTTP_COMMAND_CLOSE_WINDOW 4
#define HTTP_COMMAND_WATER_ON 5
#define HTTP_COMMAND_WATER_OFF 6
#define HTTP_COMMAND_WATER_CHANNEL_ON 7
#define HTTP_COMMAND_WATER_CHANNEL_OFF 8
#define HTTP_COMMAND_LIGHT_ON 9
#define HTTP_COMMAND_LIGHT_OFF 10
#define HTTP_COMMAND_PIN_ON 11
#define HTTP_COMMAND_PIN_OFF 12
//--------------------------------------------------------------------------------------------------------------------------------
void HttpModule::Setup()
{
  // настройка модуля тут
  flags.inProcessQuery = false;
  flags.currentAction = HTTP_ASK_FOR_COMMANDS; // пытаемся запросить команды

  commandsCheckTimer = 0;
  waitTimer = 0;
}
//--------------------------------------------------------------------------------------------------------------------------------
void HttpModule::CheckForIncomingCommands(byte wantedAction)
{
  HTTPQueryProvider* prov = MainController->GetHTTPProvider();

   // выставляем флаг, что мы в процессе обработки запроса
   flags.inProcessQuery = true;

   // и запоминаем, какое действие мы делаем
   flags.currentAction = wantedAction;
   
   // просим провайдера выполнить запрос
   prov->MakeQuery(this);
}
//--------------------------------------------------------------------------------------------------------------------------------
void HttpModule::OnAskForHost(String& host)
{
  #ifdef HTTP_DEBUG
    Serial.println(F("Provider asking for host..."));
  #endif

  // сообщаем, куда коннектиться
  host = F(HTTP_SERVER_IP);
}
//--------------------------------------------------------------------------------------------------------------------------------
void HttpModule::OnAskForData(String* data)
{
  #ifdef HTTP_DEBUG
    Serial.println(F("Provider asking for data..."));
  #endif

  /*
   Здесь мы, в зависимости от типа текущего действия - формируем тот или иной запрос
   */

   switch(flags.currentAction)
   {
      case HTTP_ASK_FOR_COMMANDS: // запрашиваем команды
      {
        #ifdef HTTP_DEBUG
          Serial.println(F("Asking for commands..."));
        #endif 

        // формируем запрос

        // для начала подсчитываем длину контента
        String key = MainController->GetSettings()->GetHttpApiKey(); // ключ доступа к API
        int contentLength = 2 + key.length(); // 2 - на имя переменной и знак равно, т.е. k=ТУТ_КЛЮЧ_API

        // теперь начинаем формировать запрос
        *data = HTTP_START_OF_HEADERS;
        *data += HTTP_SERVER_HOST;
        *data += HTTP_END_OF_HEADER;
        *data += HTTP_CONTENT_LENGTH_HEADER;
        *data += contentLength;
        // дальше идут два перевода строки, затем - данные
        *data += HTTP_END_OF_HEADER;
        *data += HTTP_END_OF_HEADER;
        *data += F("k=");
        *data += key;

        // запрос сформирован

        #ifdef HTTP_DEBUG
          Serial.println(F("QUERY IS: "));
          Serial.println(*data);
        #endif
      
        
      }
      break;

      case HTTP_REPORT_TO_SERVER: // рапортуем на сервер
      {
        #ifdef HTTP_DEBUG
          Serial.println(F("Report to server..."));
        #endif 

        // сначала получаем ID команды
        String* commandId = commandsToReport[commandsToReport.size()-1];
        commandsToReport.pop(); // удаляем из списка

        // теперь формируем запрос
        String key = MainController->GetSettings()->GetHttpApiKey(); // ключ доступа к API
        int contentLength = 2 + key.length(); // 2 - на имя переменной и знак равно, т.е. k=ТУТ_КЛЮЧ_API
        contentLength += 4; // на переменную &r=1, т.е. сообщаем серверу, что этот запрос - со статусом выполнения команды
        contentLength += 3; // на переменную &c=, содержащую ID команды
        contentLength += commandId->length(); // ну и, собственно, длину ID команды тоже считаем
        
        // теперь начинаем формировать запрос
        *data = HTTP_START_OF_HEADERS;
        *data += HTTP_SERVER_HOST;
        *data += HTTP_END_OF_HEADER;
        *data += HTTP_CONTENT_LENGTH_HEADER;
        *data += contentLength;
        // дальше идут два перевода строки, затем - данные
        *data += HTTP_END_OF_HEADER;
        *data += HTTP_END_OF_HEADER;
        *data += F("k=");
        *data += key;
        *data += F("&r=1&c=");
        *data += *commandId;

        delete commandId; // не забываем чистить за собой

        // запрос сформирован        

      }
      break;
    
   } // switch
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void HttpModule::OnAnswerLineReceived(String& line, bool& enough)
{ 
  // ищем - не пришёл ли конец команды, если пришёл - говорим, что нам хватит
  enough = line.startsWith(F("[CMDEND]"));

  if(!line.length()) // пустая строка, нечего обрабатывать
    return;

/*
 Ситуация крайне занимательная: если мы выполняем команды сразу по приходу, да даже и не сразу, неважно,
 то модуль, которому предназначена команда, может вызвать функцию yield, внутри которой, в свою очередь,
 мы проверяем на наличие данных в порту ESP. Однако, т.к. мы находимся здесь, в режиме обработки команды,
 то в буфере приёма УЖЕ лежит полная строка команды, которая будет очищена ПОСЛЕ того, как мы выйдем из 
 нашего обработчика.

 В итоге - получаем ситуацию, когда две строки, пришедшие одна за другой - будут склеиваться друг с другом,
 потому что мы не сигнализировали о том, что этот буфер нам больше не нужен, и его можно очистить.

 Поэтому мы убираем квалификатор const из параметров функции, и теперь можем свободно чистить этот буфер,
 как только в этом возникнет необходимость, а именно - ПЕРЕД вызовом любой команды любого модуля.

 Таким образом мы очистим текущую строку, не парясь больше с вложенными вызовами yield, которые, строго говоря,
 предназначены для того, чтобы не потерять данные в порту.
 
*/

  bool isCommand = line.startsWith(HTTP_START_OF_COMMAND);
  
  if(!isCommand) // не команда точно
    return;


  // теперь парсим команду
  int startLen = 0;
  {
    String _s = HTTP_START_OF_COMMAND;
    startLen = _s.length();
  }

  // с позиции startLen у нас идёт идентификатор команды
  String* commandId = new String;
  const char* strPtr = line.c_str() + startLen;

  while(*strPtr && *strPtr != '#') // собираем идентификатор команды
  {
    *commandId += *strPtr++;
  } // while

  #ifdef HTTP_DEBUG
    Serial.print(F("Command ID: "));
    Serial.println(*commandId);
  #endif

  // если мы в конце строки - бяда!
  if(!*strPtr)
  {
    delete commandId;
    return;
  }

  // перемещаемся за идентификатор команды
  strPtr++;

  if(!*strPtr) // опять бяда
  {
    delete commandId;
    return;
  }

    if(*strPtr == '?')
    {
      // короткая команда
      strPtr++;
      if(!*strPtr) // опять бяда!
      {
        delete commandId;
        return;
      }

        // теперь собираем внутренний идентификатор команды на выполнение
        String knownIdent;
        while(*strPtr)
        {
          if(*strPtr == '?') // дошли до параметра
          {
            // перемещаемся на начало параметра и выходим
            strPtr++;
            break;
          }

           knownIdent += *strPtr++;
        } // while

        #ifdef HTTP_DEBUG
          Serial.print(F("Known command: "));
          Serial.println(knownIdent);

          if(*strPtr)
          {
            Serial.print(F("Command param: "));
            Serial.println(strPtr);          
          }
        #endif

        // копируем параметры, если есть, в локальную переменную
        String localParams = strPtr;

        // и очищаем буфер приёма ESP, теперь там пусто, а локальные параметры - у нас.
        // следовательно, мы можем не беспокоиться о вложенных вызовах yield.
        line = "";
        
        // теперь смотрим, что за команда пришла
        switch(knownIdent.toInt())
        {
          case HTTP_COMMAND_OPEN_WINDOWS:
          {
            // открываем все окна
            ModuleInterop.QueryCommand(ctSET, F("STATE|WINDOW|ALL|OPEN"), false);
          }
          break;

          case HTTP_COMMAND_CLOSE_WINDOWS:
          {
            // закрываем все окна
            ModuleInterop.QueryCommand(ctSET, F("STATE|WINDOW|ALL|CLOSE"), false);
          }
          break;

          case HTTP_COMMAND_OPEN_WINDOW:
          {
            // открываем определённое окно
            String c = F("STATE|WINDOW|");
            c += localParams;
            c += F("|OPEN");

            ModuleInterop.QueryCommand(ctSET, c, false);
          }
          break;

          case HTTP_COMMAND_CLOSE_WINDOW:
          {
            // закрываем определённое окно
            String c = F("STATE|WINDOW|");
            c += localParams;
            c += F("|CLOSE");

            ModuleInterop.QueryCommand(ctSET, c, false);
            
          }
          break;

          case HTTP_COMMAND_WATER_ON:
          {
            // включаем полив
            ModuleInterop.QueryCommand(ctSET, F("WATER|ON"), false);
          }
          break;

          case HTTP_COMMAND_WATER_OFF:
          {
            // выключаем полив
            ModuleInterop.QueryCommand(ctSET, F("WATER|OFF"), false);
          }
          break;

          case HTTP_COMMAND_WATER_CHANNEL_ON:
          {
            // включаем полив на определённом канале
            String c =F("WATER|ON|");
            c += localParams;
            ModuleInterop.QueryCommand(ctSET, c, false);
          }
          break;
          
          case HTTP_COMMAND_WATER_CHANNEL_OFF:
          {
            // выключаем полив на определённом канале
            String c =F("WATER|OFF|");
            c += localParams;
            ModuleInterop.QueryCommand(ctSET, c, false);
          }
          break;

          case HTTP_COMMAND_LIGHT_ON:
          {
            // включаем досветку
            ModuleInterop.QueryCommand(ctSET, F("LIGHT|ON"), false);
          }
          break;

          case HTTP_COMMAND_LIGHT_OFF:
          {
            // выключаем досветку
            ModuleInterop.QueryCommand(ctSET, F("LIGHT|OFF"), false);
          }
          break;

          case HTTP_COMMAND_PIN_ON:
          {
            // включаем пин
            String c =F("PIN|");
            c += localParams;
            c += F("|ON");
            ModuleInterop.QueryCommand(ctSET, c, false);            
          }
          break;
          
          case HTTP_COMMAND_PIN_OFF:
          {
            // выключаем пин
            String c =F("PIN|");
            c += localParams;
            c += F("|OFF");
            ModuleInterop.QueryCommand(ctSET, c, false);            
          }
          break;
          
        } // switch
      
    } // if short command format
    else
    {
      // текстовая команда, собираем её всю
      if(strstr_P(strPtr, (const char*) F("CTSET=")) == strPtr)
      {
        // нашли валидную команду
        strPtr += 6; // перемещаемся на её текст

        if(!*strPtr) // беда
        {
          delete commandId;
          return;
        }
        
        #ifdef HTTP_DEBUG
          Serial.print(F("RAW command: "));
          Serial.println(strPtr);
        #endif

         // копируем команду к нам - и очищаем буфер приёма ESP
         String localParams = strPtr;
         // теперь можно не беспокоиться о склейке команд вложенными вызовами yield
         line = "";

        // выполняем её
        ModuleInterop.QueryCommand(ctSET, localParams, false);    
      }
      
    } // else

    // и не забываем сохранить команду, чтобы отрапортовать о статусе её выполнения
    bool found = false;

    // ищем, нет ли уже в очереди такой же команды
    for(size_t i=0;i<commandsToReport.size();i++)
    {
      if(*(commandsToReport[i]) == *commandId)
      {
        found = true;
        break;
      } // if
    } // for

    
    if(!found)
      commandsToReport.push_back(commandId);
    else
      delete commandId;
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void HttpModule::OnHTTPResult(uint16_t statusCode)
{
  #ifdef HTTP_DEBUG
    Serial.print(F("Provider reports DONE: "));
    Serial.println(statusCode);
  #endif

  flags.inProcessQuery = false; // говорим, что свободны как ветер

  if(flags.currentAction == HTTP_ASK_FOR_COMMANDS && statusCode != HTTP_REQUEST_COMPLETED)
  {
    // запрашивали команды, не удалось, поэтому попробуем ещё через 5 секунд
    commandsCheckTimer = HTTP_POLL_INTERVAL;
    commandsCheckTimer *= 1000;
    waitTimer = 5000;
  }

  if(flags.currentAction == HTTP_REPORT_TO_SERVER)
  {
    // после каждого репорта дадим поработать другим командам
    waitTimer = 5000;
  }
  
  
  flags.currentAction = HTTP_ASK_FOR_COMMANDS;
}
//--------------------------------------------------------------------------------------------------------------------------------
void HttpModule::Update(uint16_t dt)
{ 
  if(flags.inProcessQuery) // занимаемся обработкой запроса
    return;

  commandsCheckTimer += dt; // прибавляем время простоя

  waitTimer -= dt; // уменьшаем таймер ожидания
  
  if(waitTimer > 0) // если таймер ожидания больше нуля, значит чего-то ждём
    return;

  waitTimer = 0; // сбрасываем таймер ожидания
  

  HTTPQueryProvider* prov = MainController->GetHTTPProvider();
  
  if(!prov)
    return;
  
  if(!prov->CanMakeQuery()) // провайдер не может выполнить запрос
  {
    #ifdef HTTP_DEBUG
      Serial.println(F("WIFI - busy, try again after 5 seconds..."));
    #endif 
       
    waitTimer = 5000; // через 5 секунд повторим
    return;    
  }

    // а теперь проверяем, есть ли у нас репорт для команд
    if(commandsToReport.size())
    {
    #ifdef HTTP_DEBUG
      Serial.println(F("WIFI - report to server..."));
    #endif      
      // есть, надо сперва разрулить это дело
      CheckForIncomingCommands(HTTP_REPORT_TO_SERVER);
      return;
    }


   // тут проверяем - не пора ли нам отправить запрос на входящие команды.
   // мы его отправляем только тогда, когда истекло время ожидания.
   // оно истечёт тогда, когда провайдер сможет выполнить запрос,
   // и в очереди не останется команд на отсыл статуса обратно.
   unsigned long waitFor = HTTP_POLL_INTERVAL;
   waitFor *= 1000;
  if(commandsCheckTimer >= waitFor)
  {

    #ifdef HTTP_DEBUG
      Serial.println(F("WIFI - check for commands..."));
    #endif
        
    commandsCheckTimer = 0;

    // получаем API KEY из настроек
    String apyKey = MainController->GetSettings()->GetHttpApiKey();
    
    if(apyKey.length()) // только если ключ есть в настройках
      CheckForIncomingCommands(HTTP_ASK_FOR_COMMANDS);
  }

}
//--------------------------------------------------------------------------------------------------------------------------------
bool  HttpModule::ExecCommand(const Command& command, bool wantAnswer)
{
  UNUSED(wantAnswer);

  uint8_t argsCnt = command.GetArgsCount();
 
  
  if(command.GetType() == ctSET) // установка свойств
  {
      if(argsCnt < 2)
      {
        PublishSingleton = PARAMS_MISSED;
      }
      else
      {
        String which = command.GetArg(0);
        if(which == F("KEY")) // установка ключа API, CTSET=HTTP|KEY|here
        {
          MainController->GetSettings()->SetHttpApiKey(command.GetArg(1));
          PublishSingleton.Status = true;
          PublishSingleton = which;
          PublishSingleton << PARAM_DELIMITER;
          PublishSingleton << REG_SUCC;

        }
      } // else
  }
  else // получение свойств
  {
      if(argsCnt < 1)
      {
        PublishSingleton = PARAMS_MISSED;
      }
      else
      {
        String which = command.GetArg(0);

        if(which == F("KEY")) // запрос ключа API, CTGET=HTTP|KEY
        {
          PublishSingleton.Status = true;
          PublishSingleton = which;
          PublishSingleton << PARAM_DELIMITER;
          PublishSingleton << (MainController->GetSettings()->GetHttpApiKey());
          
        } // if(which == F("KEY"))
        
      } // else
    
  } // ctGET

  MainController->Publish(this,command); 
   
  return true;
}
//--------------------------------------------------------------------------------------------------------------------------------
