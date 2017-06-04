#ifndef _HTTP_MODULE_H
#define _HTTP_MODULE_H

#include "AbstractModule.h"
//--------------------------------------------------------------------------------------------------------------------------------
#include "HTTPInterfaces.h" // подключаем интерфейсы для работы с HTTP-запросами
#include "TinyVector.h"
//--------------------------------------------------------------------------------------------------------------------------------
struct HttpModuleFlags
{
  bool inProcessQuery: 1;
  byte currentAction: 2;
  byte pad: 5;
};
//--------------------------------------------------------------------------------------------------------------------------------
enum
{
    HTTP_ASK_FOR_COMMANDS,
    HTTP_REPORT_TO_SERVER
};
//--------------------------------------------------------------------------------------------------------------------------------
typedef Vector<String*> HTTPReportList;
//--------------------------------------------------------------------------------------------------------------------------------
class HttpModule : public AbstractModule, public HTTPRequestHandler
{
  private:
  
   long waitTimer;
   unsigned long commandsCheckTimer;
   HttpModuleFlags flags;

   HTTPReportList commandsToReport;
   
   void CheckForIncomingCommands(byte wantedAction);
  
  public:
    HttpModule() : AbstractModule("HTTP") {}

    bool ExecCommand(const Command& command, bool wantAnswer);
    void Setup();
    void Update(uint16_t dt);
    
  virtual void OnAskForHost(String& host); // вызывается для запроса имени хоста
  virtual void OnAskForData(String* data); // вызывается для запроса данных, которые надо отправить HTTP-запросом
  virtual void OnAnswerLineReceived(String& line, bool& enough); // вызывается по приходу строки ответа от сервера, вызываемая сторона должна сама определить, когда достаточно данных.
  virtual void OnHTTPResult(uint16_t statusCode); // вызывается по завершению HTTP-запроса и получению ответа от сервера    

};
//--------------------------------------------------------------------------------------------------------------------------------
#endif
