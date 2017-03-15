#ifndef _WIFI_MODULE_H
#define _WIFI_MODULE_H

#include "AbstractModule.h"
#include "TinyVector.h"
#include "Settings.h"
#include "TCPClient.h"

#if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
#include "IoT.h"
#endif

#define MAX_WIFI_CLIENTS 4 // максимальное кол-во клиентов
#define WIFI_PACKET_LENGTH 2048 // по скольку байт в пакете отсылать данные


typedef enum
{
  /*0*/  wfaIdle, // пустое состояние
  /*1*/  wfaWantReady, // надо получить ready от модуля
  /*2*/  wfaEchoOff, // выключаем эхо
  /*3*/  wfaCWMODE, // переводим в смешанный режим
  /*4*/  wfaCWSAP, // создаём точку доступа
  /*5*/  wfaCWJAP, // коннектимся к роутеру
  /*6*/  wfaCWQAP, // отсоединяемся от роутера
  /*7*/  wfaCIPMODE, // устанавливаем режим работы
  /*8*/  wfaCIPMUX, // разрешаем множественные подключения
  /*9*/  wfaCIPSERVER, // запускаем сервер
  /*10*/ wfaCIPSEND, // отсылаем команду на передачу данных
  /*11*/ wfaACTUALSEND, // отсылаем данные
  /*12*/ wfaCIPCLOSE, // закрываем соединение
  /*13*/ wfaCheckModemHang, // проверяем на зависание модема

#if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
  
  /*14*/ wfaStartIoTSend, // начинаем отсыл данных в IoT
  /*15*/ wfaStartSendIoTData, // посылаем команду на отсыл данныз в IoT
  /*16*/ wfaActualSendIoTData, // актуальный отсыл данных в IoT
  /*17*/ wfaCloseIoTConnection, // закрываем соединение
#endif
  
} WIFIActions;

typedef Vector<WIFIActions> ActionsVector;

typedef struct
{

  bool inSendData : 1; 
  bool isAnyAnswerReceived : 1;
  bool inRebootMode : 1;
  bool wantIoTToProcess : 1;
  byte pad : 4;
      
} WiFiModuleFlags;

class WiFiModule : public AbstractModule // модуль поддержки WI-FI
#if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
, public IoTGate
#endif
{
  private:

    WiFiModuleFlags flags;
    
    long needToWaitTimer; // таймер ожидания до запроса следующей команды   
    unsigned long sendCommandTime, answerWaitTimer;
    void RebootModem(); // перезагружаем модем
    unsigned long rebootStartTime;

    
    bool IsKnownAnswer(const String& line); // если ответ нам известный, то возвращает true
    void SendCommand(const String& command, bool addNewLine=true); // посылает команды модулю вай-фай
    void ProcessQueue(); // разбираем очередь команд
    void ProcessQuery(const String& command); // обрабатываем запрос
    void ProcessCommand(int clientID, int dataLen,const char* command);
    void UpdateClients();
    
    uint8_t currentAction; // текущая операция, завершения которой мы ждём
    ActionsVector actionsQueue; // что надо сделать, шаг за шагом 
    
    uint8_t currentClientIDX; // индекс клиента, с которым мы работаем сейчас
    uint8_t nextClientIDX; // индекс клиента, статус которого надо проверить в следующий раз

    // список клиентов
    TCPClient clients[MAX_WIFI_CLIENTS];

    void InitQueue(bool addRebootCommand=true);

    #if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
      IOT_OnWriteToStream iotWriter;
      IOT_OnSendDataDone iotDone;
      IoTService iotService;
      String* iotDataHeader;
      String* iotDataFooter;
      uint16_t iotDataLength;
      void EnsureIoTProcessed(bool success=false);
    #endif
  
  public:
    WiFiModule() : AbstractModule("WIFI") {}

    bool ExecCommand(const Command& command, bool wantAnswer);
    void Setup();
    void Update(uint16_t dt);

    void ProcessAnswerLine(const String& line);
    volatile bool WaitForDataWelcome; // флаг, что мы ждём приглашения на отсыл данных - > (плохое ООП, негодное :) )

#if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
    virtual void SendData(IoTService service,uint16_t dataLength, IOT_OnWriteToStream writer, IOT_OnSendDataDone onDone);
#endif    

};


#endif
