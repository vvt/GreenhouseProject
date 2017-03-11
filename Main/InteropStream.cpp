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
    */
      cmd.SetIncomingStream(NULL);

    MainController->ProcessModuleCommand(cmd,NULL);
    return true;
    
}
/*
size_t InteropStream::write(uint8_t toWr)
{
  data += (char) toWr;
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
  
/*
  UNUSED(dt);
  
  if(!needUpdate)
    return;

 needUpdate = false;   
 String s;
  
#ifdef USE_LOOP_MODULE 

  //s  = loopName;
  s = F("LOOP|");
  s += loopName;
  s += F("|SET|"); 
  s += lastBlinkInterval;
  //s += pinCommand;
  s += F("|0|PIN|");
  s += String(pin);
  s += F("|T");
  

  ModuleInterop.QueryCommand(ctSET,s,false);//,false);
#endif

#ifdef USE_PIN_MODULE 
      if(!lastBlinkInterval) // не надо зажигать диод, принудительно гасим его
      {
        s = F("PIN|");
        s += String(pin);
        s += PARAM_DELIMITER;
        s += F("0");

        ModuleInterop.QueryCommand(ctSET,s,false);//,false);
 
      } // if
 #endif   
*/
    
}
void BlinkModeInterop::begin(uint8_t p)//, const String& lName)
{
  pin = p;
  WORK_STATUS.PinMode(pin,OUTPUT);
  /*
  loopName = lName;
  //loopName = F("LOOP|");
 // loopName += lName;
 // loopName += F("|SET|");
  
 // pinCommand = F("|0|PIN|");
 // pinCommand += String(pin);
 // pinCommand += F("|T");

  lastBlinkInterval = 0xFFFF;
  */
}
void BlinkModeInterop::blink(uint16_t interval)
{

  blinkInterval = interval;
  
  if(!blinkInterval)
    WORK_STATUS.PinWrite(pin,LOW);
/*

 if(lastBlinkInterval == blinkInterval)
  // незачем выполнять команду с тем же интервалом
    return;

  needUpdate = true;
  lastBlinkInterval = blinkInterval;
*/  

}

