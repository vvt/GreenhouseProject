#include "Globals.h"

#ifdef _DEBUG
#include "PDUClasses.h"
#endif

#include "CommandBuffer.h"
#include "CommandParser.h"
#include "ModuleController.h"
#include "AlertModule.h"
#include "ZeroStreamListener.h"

#ifdef USE_PIN_MODULE
#include "PinModule.h"
#endif

#ifdef USE_LOOP_MODULE
#include "LoopModule.h"
#endif

#ifdef USE_STAT_MODULE
#include "StatModule.h"
#endif

#ifdef USE_TEMP_SENSORS
#include "TempSensors.h"
#endif

#ifdef USE_SMS_MODULE
#include "SMSModule.h"
#endif

#ifdef USE_WATERING_MODULE
#include "WateringModule.h"
#endif

#ifdef USE_LUMINOSITY_MODULE
#include "LuminosityModule.h"
#endif

#ifdef USE_HUMIDITY_MODULE
#include "HumidityModule.h"
#endif

#ifdef USE_WIFI_MODULE
#include "WiFiModule.h"
#endif

#ifdef USE_LOG_MODULE
#include "LogModule.h"
#endif

#ifdef USE_DELTA_MODULE
#include "DeltaModule.h"
#endif

#ifdef USE_LCD_MODULE
#include "LCDModule.h"
#endif

#ifdef USE_NEXTION_MODULE
#include "NextionModule.h"
#endif

#ifdef USE_WATERFLOW_MODULE
#include "WaterflowModule.h"
#endif

#ifdef USE_COMPOSITE_COMMANDS_MODULE
#include "CompositeCommandsModule.h"
#endif

#ifdef USE_SOIL_MOISTURE_MODULE
#include "SoilMoistureModule.h"
#endif

#ifdef USE_W5100_MODULE
#include "EthernetModule.h"
#endif

#ifdef USE_RESERVATION_MODULE
#include "ReservationModule.h"
#endif

#ifdef USE_TIMER_MODULE
#include "TimerModule.h"
#endif

#ifdef USE_PH_MODULE
#include "PHModule.h"
#endif

#ifdef USE_IOT_MODULE
#include "IoTModule.h"
#endif


// таймер
unsigned long lastMillis = 0;


// Ждем команды из сериала
CommandBuffer commandsFromSerial(&Serial);

// Парсер команд
CommandParser commandParser;

// Контроллер модулей
ModuleController controller;


#ifdef USE_PIN_MODULE
//  Модуль управления цифровыми пинами
PinModule pinModule;
#endif

#ifdef USE_LOOP_MODULE
// Модуль поддержки периодически повторяемых операций
LoopModule loopModule;
#endif

#ifdef USE_STAT_MODULE
// Модуль вывода статистики
StatModule statModule;
#endif

#ifdef USE_TEMP_SENSORS
// модуль опроса температурных датчиков и управления фрамугами
TempSensors tempSensors;
#endif

#ifdef USE_IOT_MODULE
// модуль отсыла данных на внешние IoT-хранилища
IoTModule iotModule; 
#endif

#ifdef USE_SMS_MODULE
// модуль управления по SMS
 SMSModule smsModule;
 String* smsReceiveBuff;
 
void GSM_EVENT_FUNC()
{
  char ch;
  while(GSM_SERIAL.available())
  {
    ch = GSM_SERIAL.read();
    
    if(ch == '\r')
      continue;
    
    if(ch == '\n')
    {
      smsModule.ProcessAnswerLine(*smsReceiveBuff);
      //smsReceiveBuff = F("");
      delete smsReceiveBuff;
      smsReceiveBuff = new String();
    }
    
    else
    {
        
        if(smsModule.WaitForSMSWelcome && ch == '>') // ждут команду >
        {         
          smsModule.WaitForSMSWelcome = false;
          smsModule.ProcessAnswerLine(F(">"));
          //smsReceiveBuff = F("");
          delete smsReceiveBuff;
          smsReceiveBuff = new String();
        }
        else
          *smsReceiveBuff += ch;
    }

    
  } // while

}
#endif

#ifdef USE_WATERING_MODULE
// модуль управления поливом
WateringModule wateringModule;
#endif

#ifdef USE_LUMINOSITY_MODULE
// модуль управления досветкой и получения значений освещённости
LuminosityModule luminosityModule;
#endif

#ifdef USE_HUMIDITY_MODULE
// модуль работы с датчиками влажности DHT
HumidityModule humidityModule;
#endif

#ifdef USE_LOG_MODULE
// модуль логгирования информации
LogModule logModule;
#endif

#ifdef USE_DELTA_MODULE
// модуль сбора дельт с датчиков
DeltaModule deltaModule;
#endif

#ifdef USE_LCD_MODULE
// модуль LCD
LCDModule lcdModule;
#endif

#ifdef USE_NEXTION_MODULE
// модуль Nextion
NextionModule nextionModule;
#endif

#ifdef USE_WATERFLOW_MODULE
// модуль учёта расхода воды
WaterflowModule waterflowModule;
#endif

#ifdef USE_COMPOSITE_COMMANDS_MODULE
// модуль составных команд
CompositeCommandsModule compositeCommands;
#endif

#ifdef USE_SOIL_MOISTURE_MODULE
// модуль датчиков влажности почвы
SoilMoistureModule soilMoistureModule;
#endif

#ifdef USE_PH_MODULE
// модуль контроля pH
PhModule phModule;
#endif

#ifdef USE_W5100_MODULE
// модуль поддержки W5100
EthernetModule ethernetModule;
#endif

#ifdef USE_RESERVATION_MODULE
ReservationModule reservationModule;
#endif

#ifdef USE_TIMER_MODULE
TimerModule timerModule;
#endif

#ifdef USE_READY_DIODE
  #ifdef BLINK_READY_DIODE
   BlinkModeInterop readyDiodeBlinker;
  #endif
#endif

#ifdef USE_WIFI_MODULE
// модуль работы по Wi-Fi
WiFiModule wifiModule;
String* wiFiReceiveBuff;

void WIFI_EVENT_FUNC()
{
  char ch;
  while(WIFI_SERIAL.available())
  {
    ch = WIFI_SERIAL.read();

   
    if(ch == '\r')
      continue;
    
    if(ch == '\n')
    {    
        if(wiFiReceiveBuff->startsWith(F("+IPD")))
        {
          // Не убираем переводы строки, когда пришёл пакет с данными, поскольку \r\n может придти прямо в пакете данных.
          // Т.к. у нас \r\n служит признаком окончания команды - значит, мы должны учитывать эти символы в пакете,
          // и не можем самовоизвольно их отбрасывать.
          *wiFiReceiveBuff += NEWLINE; 
        }
          
        wifiModule.ProcessAnswerLine(*wiFiReceiveBuff);
        //wiFiReceiveBuff = F("");
        delete wiFiReceiveBuff;
        wiFiReceiveBuff = new String();
    }
    else
    {     
        if(wifiModule.WaitForDataWelcome && ch == '>') // ждут команду >
        {
          wifiModule.WaitForDataWelcome = false;
          wifiModule.ProcessAnswerLine(F(">"));
        }
        else
          *wiFiReceiveBuff += ch;
    }
  
    
  } // while
   
}

#endif

ZeroStreamListener zeroStreamModule;
AlertModule alertsModule;


#ifdef USE_EXTERNAL_WATCHDOG
  typedef enum
  {
    WAIT_FOR_HIGH,
    WAIT_FOR_LOW 
  } ExternalWatchdogState;
  
  typedef struct
  {
    uint16_t timer;
    ExternalWatchdogState state;
  } ExternalWatchdogSettings;

  ExternalWatchdogSettings watchdogSettings;
#endif

void setup() 
{ 
  Serial.begin(SERIAL_BAUD_RATE); // запускаем Serial на нужной скорости

  WORK_STATUS.PinMode(0,INPUT,false);
  WORK_STATUS.PinMode(1,OUTPUT,false);

  #ifdef USE_EXTERNAL_WATCHDOG
    WORK_STATUS.PinMode(WATCHDOG_REBOOT_PIN,OUTPUT,true);
    digitalWrite(WATCHDOG_REBOOT_PIN,LOW);

    watchdogSettings.timer = 0;
    watchdogSettings.state = WAIT_FOR_HIGH;
  #endif
 
  // настраиваем все железки
  controller.Setup();
   
  // устанавливаем провайдера команд для контроллера
  controller.SetCommandParser(&commandParser);


  // регистрируем модули  
  #ifdef USE_PIN_MODULE  
  controller.RegisterModule(&pinModule);
  #endif
  
  #ifdef USE_LOOP_MODULE
  controller.RegisterModule(&loopModule);
  #endif

  #ifdef USE_STAT_MODULE
  controller.RegisterModule(&statModule);
  #endif

  #ifdef USE_TEMP_SENSORS
  controller.RegisterModule(&tempSensors);
  #endif

  #ifdef USE_WATERING_MODULE
  controller.RegisterModule(&wateringModule);
  #endif

  #ifdef USE_LUMINOSITY_MODULE
  controller.RegisterModule(&luminosityModule);
  #endif

  #ifdef USE_HUMIDITY_MODULE
  controller.RegisterModule(&humidityModule);
  #endif

  #ifdef USE_DELTA_MODULE
  controller.RegisterModule(&deltaModule);
  #endif
  
  #ifdef USE_LCD_MODULE
  controller.RegisterModule(&lcdModule);
  #endif

  #ifdef USE_NEXTION_MODULE
  controller.RegisterModule(&nextionModule);
  #endif

  #ifdef USE_WATERFLOW_MODULE
  controller.RegisterModule(&waterflowModule);
  #endif

  #ifdef USE_COMPOSITE_COMMANDS_MODULE
  controller.RegisterModule(&compositeCommands);
  #endif
  
  #ifdef USE_SOIL_MOISTURE_MODULE
  controller.RegisterModule(&soilMoistureModule);
  #endif

  #ifdef USE_PH_MODULE
  controller.RegisterModule(&phModule);
  #endif

  #ifdef USE_W5100_MODULE
  controller.RegisterModule(&ethernetModule);
  #endif

  #ifdef USE_RESERVATION_MODULE
  controller.RegisterModule(&reservationModule);
  #endif

  #ifdef USE_TIMER_MODULE
  controller.RegisterModule(&timerModule);
  #endif

  #ifdef USE_LOG_MODULE
  controller.RegisterModule(&logModule);
  controller.SetLogWriter(&logModule); // задаём этот модуль как модуль, который записывает события в лог
  #endif

  // модуль Wi-Fi регистрируем до модуля SMS, поскольку Wi-Fi дешевле, чем GPRS, для отсыла данных в IoT-хранилища
  #ifdef USE_WIFI_MODULE
  wiFiReceiveBuff = new String();
  controller.RegisterModule(&wifiModule);
  #endif 

  #ifdef USE_SMS_MODULE
  smsReceiveBuff = new String();
  controller.RegisterModule(&smsModule);
  #endif

  #ifdef USE_IOT_MODULE
    controller.RegisterModule(&iotModule);
  #endif
  
 // модуль алертов регистрируем последним, т.к. он должен вычитать зависимости с уже зарегистрированными модулями
  controller.RegisterModule(&zeroStreamModule);
  controller.RegisterModule(&alertsModule);

  controller.begin(); // начинаем работу

//    ProcessInitCommands(); // запускаем на обработку команды инициализации

  // Печатаем в Serial готовность
  Serial.print(READY);

  // тест часов реального времени
  #ifdef USE_DS3231_REALTIME_CLOCK
  
   DS3231Clock rtc = controller.GetClock();
   DS3231Time tm = rtc.getTime();

   Serial.print(F(", "));
   Serial.print(rtc.getDayOfWeekStr(tm));
   Serial.print(F(" "));
   Serial.print(rtc.getDateStr(tm));
   Serial.print(F(" - "));
   Serial.print(rtc.getTimeStr(tm));

   
  #endif 

    Serial.println(F(""));

  #ifdef USE_LOG_MODULE
    controller.Log(&logModule,READY); // печатаем в файл действий строчку Ready, которая скажет нам, что мега стартовала
  #endif

}
// эта функция вызывается после обновления состояния каждого модуля.
// передаваемый параметр - указатель на обновлённый модуль.
// если модулю предстоит долгая работа - помимо этого инструмента
// модуль должен дёргать функцию yield, если чем-то долго занят!
void ModuleUpdateProcessed(AbstractModule* module)
{
  UNUSED(module);

  // используем её, чтобы проверить состояние порта UART для WI-FI-модуля - вдруг надо внеочередно обновить
    #ifdef USE_WIFI_MODULE
    // модуль Wi-Fi обновляем каждый раз после обновления очередного модуля
    WIFI_EVENT_FUNC(); // вызываем функцию проверки данных в порту
    #endif

   #ifdef USE_SMS_MODULE
   // и модуль GSM тоже тут обновим
   GSM_EVENT_FUNC();
   #endif     
}

#ifdef USE_EXTERNAL_WATCHDOG
void updateExternalWatchdog()
{
  static unsigned long watchdogLastMillis = millis();
  unsigned long watchdogCurMillis = millis();

  uint16_t dt = watchdogCurMillis - watchdogLastMillis;
  watchdogLastMillis = watchdogCurMillis;

      watchdogSettings.timer += dt;
      switch(watchdogSettings.state)
      {
        case WAIT_FOR_HIGH:
        {
          if(watchdogSettings.timer >= WATCHDOG_WORK_INTERVAL)
          {
           // Serial.println("set high");
            watchdogSettings.timer = 0;
            watchdogSettings.state = WAIT_FOR_LOW;
            digitalWrite(WATCHDOG_REBOOT_PIN, HIGH);
          }
        }
        break;

        case WAIT_FOR_LOW:
        {
          if(watchdogSettings.timer >= WATCHDOG_PULSE_DURATION)
          {
          //  Serial.println("set low");
            watchdogSettings.timer = 0;
            watchdogSettings.state = WAIT_FOR_HIGH;
            digitalWrite(WATCHDOG_REBOOT_PIN, LOW);
          }          
        }
        break;
      }  
  
}
#endif


void loop() 
{
// отсюда можно добавлять любой сторонний код

// до сюда можно добавлять любой сторонний код

  
    // вычисляем время, прошедшее с момента последнего вызова
    unsigned long curMillis = millis();
    uint16_t dt = curMillis - lastMillis;
    
    lastMillis = curMillis; // сохраняем последнее значение вызова millis()


   #ifdef USE_EXTERNAL_WATCHDOG
     updateExternalWatchdog();
   #endif // USE_EXTERNAL_WATCHDOG
    
#ifdef USE_READY_DIODE

  #ifdef BLINK_READY_DIODE
   // static uint16_t ready_diode_timer = 0;
    static bool blink_ready_diode_inited = false;
    //ready_diode_timer += dt;
    if(/*ready_diode_timer > 2000 && */!blink_ready_diode_inited) {
      blink_ready_diode_inited = true;
      readyDiodeBlinker.begin(DIODE_READY_PIN);//,F("SD"));
      readyDiodeBlinker.blink(READY_DIODE_BLINK_INTERVAL);
    }

    readyDiodeBlinker.update(dt);
  #else
    static bool blink_ready_diode_inited = false;
    if(!blink_ready_diode_inited) {
      blink_ready_diode_inited = true;

      // просто зажигаем светодиод при старте
      WORK_STATUS.PinMode(6,OUTPUT);
      WORK_STATUS.PinWrite(6,HIGH);
    }
  #endif

#endif



  // смотрим, есть ли входящие команды
   if(commandsFromSerial.HasCommand())
   {
    // есть новая команда
    Command cmd;
    if(commandParser.ParseCommand(commandsFromSerial.GetCommand(), cmd))
    {
       Stream* answerStream = commandsFromSerial.GetStream();
      // разобрали, назначили поток, с которого пришла команда
        cmd.SetIncomingStream(answerStream);

      // запустили команду в обработку
       controller.ProcessModuleCommand(cmd);
 
    } // if
    else
    {
      // что-то пошло не так, игнорируем команду
    } // else
    
    commandsFromSerial.ClearCommand(); // очищаем полученную команду
   } // if
    
    // обновляем состояние всех зарегистрированных модулей
   controller.UpdateModules(dt,ModuleUpdateProcessed);


   
// отсюда можно добавлять любой сторонний код


// до сюда можно добавлять любой сторонний код

}
// обработчик простоя, используем и его. Если сторонняя библиотека устроена правильно - она будет
// вызывать yield периодически - этим грех не воспользоваться, чтобы избежать потери данных
// в портах UART. 
void yield()
{
// отсюда можно добавлять любой сторонний код, который надо вызывать, когда МК чем-то долго занят (например, чтобы успокоить watchdog)


// до сюда можно добавлять любой сторонний код

   #ifdef USE_EXTERNAL_WATCHDOG
     updateExternalWatchdog();
   #endif // USE_EXTERNAL_WATCHDOG

   #ifdef USE_WIFI_MODULE
    // модуль Wi-Fi обновляем каждый раз при вызове функции yield
    WIFI_EVENT_FUNC(); // вызываем функцию проверки данных в порту
    #endif

   #ifdef USE_SMS_MODULE
   // и модуль GSM тоже тут обновим
   GSM_EVENT_FUNC();
   #endif 

   #ifdef USE_LCD_MODULE
    rotaryEncoder.update(); // обновляем энкодер меню
   #endif

// отсюда можно добавлять любой сторонний код, который надо вызывать, когда МК чем-то долго занят (например, чтобы успокоить watchdog)


// до сюда можно добавлять любой сторонний код

}

