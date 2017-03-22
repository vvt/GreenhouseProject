#ifndef _SMS_MODULE_H
#define _SMS_MODULE_H

#include "AbstractModule.h"
#include "Settings.h"
#include "TinyVector.h"

#if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)
#include "IoT.h"
#endif

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
  smaCheckModemHardware, // запрос, какой модем подключен (AT+CGMM)
  
#if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)
  
  smaStartIoTSend, // начинаем отсыл данных в IoT

  // Команды, специфичные для M590
  smaGDCONT, // задаём параметры PDP-контекста (AT+CGDCONT)
  smaXGAUTH, // авторизация в APN (AT+XGAUTH)
  smaXIIC, // установка соединения PPP (AT+XIIC=1)
  smaCheckPPPIp, // проверяем выданный IP (AT+XIIC?)
  smaTCPSETUP, // устанавливаем TCP-соединение
  smaTCPSEND, // начинаем посылать данные
  smaTCPSendData, // отсылаем данные
  smaTCPClose, // закрываем соединение
  smaTCPWaitAnswer, // ждём ответа

  // команды, специфичные для SIM800L
  smaStartGPRSConnection,
  smaCheckGPRSConnection,
  smaCloseGPRSConnection,
  smaConnectToIOT,
  smaStartSendIoTData,
  smaSendDataToSIM800,
  smaWaitForIoTAnswer,
  
#endif  
  
} SMSActions;

typedef Vector<SMSActions> SMSActionsVector;

enum
{
  M590,
  SIM800
};

typedef struct
{
    bool waitForSMSInNextLine : 1;
    bool isModuleRegistered : 1; // зарегистрирован ли модуль у оператора?
    bool isAnyAnswerReceived : 1;
    bool inRebootMode : 1;
    bool wantIoTToProcess : 1;
    byte model : 2;
    bool isIPAssigned : 1;
    
    bool wantBalanceToProcess : 1;
    byte pad : 7;
      
} SMSModuleFlags;

class SMSModule : public AbstractModule, public Stream // модуль поддержки управления по SMS
#if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)
, public IoTGate
#endif
{
  private:

    #if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)
      IOT_OnWriteToStream iotWriter;
      IOT_OnSendDataDone iotDone;
      IoTService iotService;
      String* iotDataHeader;
      String* iotDataFooter;
      uint16_t iotDataLength;
      void EnsureIoTProcessed(bool success=false);
      String GetAPN();
      void GetAPNUserPass(String& user, String& pass);
    #endif

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

#if defined(USE_IOT_MODULE) && defined(USE_GSM_MODULE_AS_IOT_GATE)
    virtual void SendData(IoTService service,uint16_t dataLength, IOT_OnWriteToStream writer, IOT_OnSendDataDone onDone);
#endif               

};


#endif
