#include "InteropStream.h"
#include "Globals.h"

InteropStream ModuleInterop;


InteropStream::InteropStream()// : Stream()
{
}

InteropStream::~InteropStream()
{
}
bool InteropStream::QueryCommand(COMMAND_TYPE cType, const String& command, bool isInternalCommand)//,bool wantAnwer)
{
  
 
 CHECK_PUBLISH_CONSISTENCY; // проверяем структуру публикации на предмет того, что там ничего нет

  String data = command; // копируем во внутренний буфер, т.к. входной параметр - const
   
  int delimIdx = data.indexOf('|');
  const char* params = NULL;
  if(delimIdx != -1)
  {
    data[delimIdx] = '\0';
    params = &(data[delimIdx+1]);
  }

  const char* moduleId = data.c_str();
  
  Command cmd;
  cmd.Construct(moduleId,params,cType);

  //data = F("");
  
 
    cmd.SetInternal(isInternalCommand); // устанавливаем флаг команды
    /*
    if(wantAnwer)
    {
      cmd.SetIncomingStream(this); // просим контроллер опубликовать ответ в нас - мы сохраним ответ в data
    }
    else
      cmd.SetIncomingStream(NULL);
    */

   // cmd.SetIncomingStream(this); // просим контроллер опубликовать ответ в нас - мы сохраним ответ в data
    MainController->ProcessModuleCommand(cmd,NULL);
    return true;
    
}
/*
size_t InteropStream::write(uint8_t toWr)
{
//  data += (char) toWr;
  return 1;
}
*/

BlinkModeInterop::BlinkModeInterop()
{
  /*
  lastBlinkInterval = 0xFFFF;
  needUpdate = false;
  */
  blinkInterval = 0;
  timer = 0;
  pinState = LOW;
}
void BlinkModeInterop::update(uint16_t dt)
{
  if(!blinkInterval) // выключены
    return;

  timer += dt;

  if(timer < blinkInterval) // не настало время
    return;

  timer -= blinkInterval;
  pinState = pinState == LOW ? HIGH : LOW;
  WORK_STATUS.PinWrite(pin,pinState);
  
}
void BlinkModeInterop::begin(uint8_t p)//, const String& lName)
{
  pin = p;
  WORK_STATUS.PinMode(pin,OUTPUT);
}
void BlinkModeInterop::blink(uint16_t interval)
{

  blinkInterval = interval;
  
  if(!blinkInterval)
    WORK_STATUS.PinWrite(pin,LOW);

}

