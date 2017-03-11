#ifndef _SMS_MODULE_H
#define _SMS_MODULE_H

#include "AbstractModule.h"
#include "Settings.h"
#include "TinyVector.h"

typedef enum
{
  smaIdle, // ничего не делаем, просто ждём
  smaCheckReady, // проверяем готовность (AT+CPAS)
  smaEchoOff, // выключаем эхо (ATE0)
  smaDisableCellBroadcastMessages, // AT+CSCB=1
  smaAON, // включаем АОН (AT+CLIP=1)
  smaPDUEncoding, // включаем кодировку PDU (AT+CMGF=0)
  smaUCS2Encoding, // включаем кодировку UCS2 (AT+CSCS="UCS2")
  smaSMSSettings, // включаем вывод входящих смс сразу в порт (AT+CNMI=2,2)
  smaWaitReg, // ждём регистрации (AT+CREG?)
  smaHangUp, // кладём трубку (ATH)
  smaStartSendSMS, // начинаем отсылать SMS (AT+CMGS=)
  smaSmsActualSend, // актуальный отсыл SMS
  smaClearAllSMS, // очистка всех SMS (AT+CMGD=0,4)
  smaCheckModemHang, // проверяем, не завис ли модем (AT)
  smaRequestBalance, // запрос баланса (ATD#100#;)
  
} SMSActions;

typedef Vector<SMSActions> SMSActionsVector;

typedef struct
{
    bool waitForSMSInNextLine : 1;
    bool isModuleRegistered : 1; // зарегистрирован ли модуль у оператора?
    bool isAnyAnswerReceived : 1;
    bool inRebootMode : 1;
    byte pad : 4;
      
} SMSModuleFlags;

class SMSModule : public AbstractModule, public Stream // модуль поддержки управления по SMS
{
  private:
    GlobalSettings* Settings;

    uint8_t currentAction; // текущая операция, завершения которой мы ждём
    SMSActionsVector actionsQueue; // что надо сделать, шаг за шагом 
    bool IsKnownAnswer(const String& line, bool& okFound); // если ответ нам известный, то возвращает true
    void SendCommand(const String& command, bool addNewLine=true); // посылает команды модулю GSM
    void ProcessQueue(); // разбираем очередь команд
    void InitQueue(); // инициализируем очередь

    String* cusdSMS;
    String* smsToSend; // какое SMS отправить
    String* commandToSend; // какую команду сперва отправить для отсыла SMS

    String* queuedWindowCommand; // команда на выполнение управления окнами, должна выполняться только когда окна не в движении
    uint16_t queuedTimer; // таймер, чтобы не дёргать часто проверку состояния окон - это незачем
    void ProcessQueuedWindowCommand(uint16_t dt); // обрабатываем команду управления окнами, помещенную в очередь

    long needToWaitTimer; // таймер ожидания до запроса следующей команды

    void ProcessIncomingCall(const String& line); // обрабатываем входящий звонок
    void ProcessIncomingSMS(const String& line); // обрабатываем входящее СМС

    void RequestBalance();

    String* customSMSCommandAnswer;

    unsigned long sendCommandTime, answerWaitTimer;

    void RebootModem(); // перезагружаем модем
    unsigned long rebootStartTime;

    SMSModuleFlags flags;
        
  public:
    SMSModule() : AbstractModule("SMS") {}

    bool ExecCommand(const Command& command, bool wantAnswer);
    void Setup();
    void Update(uint16_t dt);

    void SendStatToCaller(const String& phoneNum);
    void SendSMS(const String& sms, bool isSMSInUCS2Format=false);

    void ProcessAnswerLine(const String& line);
    volatile bool WaitForSMSWelcome; // флаг, что мы ждём приглашения на отсыл SMS - > (плохое ООП, негодное :) )

    virtual int available(){ return false; };
    virtual int read(){ return -1;};
    virtual int peek(){return -1;};
    virtual void flush(){};

 
    virtual size_t write(uint8_t toWr);         

};


#endif
