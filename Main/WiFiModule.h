#ifndef _WIFI_MODULE_H
#define _WIFI_MODULE_H

#include "AbstractModule.h"
#include "TinyVector.h"
#include "Settings.h"
#include "TCPClient.h"

#if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
#include "IoT.h"
#endif
//--------------------------------------------------------------------------------------------------------------------------------
#include "HTTPInterfaces.h" // подключаем интерфейсы для работы с HTTP-запросами
//--------------------------------------------------------------------------------------------------------------------------------
#define MAX_WIFI_CLIENTS 4 // максимальное кол-во клиентов
#define WIFI_PACKET_LENGTH 2048 // по скольку байт в пакете отсылать данные
//--------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
#include <SdFat.h>
//--------------------------------------------------------------------------------------------------------------------------------
#define MQTT_RECONNECT_WAIT 10000
//--------------------------------------------------------------------------------------------------------------------------------
#define MQTT_CONNECT_COMMAND (1 << 4)
#define MQTT_PUBLISH_COMMAND (3 << 4)
#define MQTT_SUBSCRIBE_COMMAND (8 << 4)
#define MQTT_QOS1 (1 << 1)
//--------------------------------------------------------------------------------------------------------------------------------
typedef Vector<uint8_t> MQTTBuffer;
//--------------------------------------------------------------------------------------------------------------------------------
typedef struct
{
    bool isConnected : 1; // законнекчены?
    bool wantToSendConnectPacket : 1; // надо отправить пакет CONNECT ?
    bool wantToSendSubscribePacket : 1; // надо ли отправить пакет для подписки на топики?
    bool wantToSendReportTopic: 1; // надо ли отсылать топик с ответом на команду ?
    bool reconnectTimerEnabled : 1; // таймер переподключения активен ?
    bool haveTopics : 1; // есть топики для публикации ?
    bool busy : 1; // заняты ?
    byte pad : 1; // добивка до границы байта
  
} MQTTClientFlags;
//--------------------------------------------------------------------------------------------------------------------------------
class MQTTClient
{
  public:
    MQTTClient();
    void setConnected(bool flag);
    void packetWriteError();
    void packetWriteSuccess();
    void process(MQTTBuffer& packet);
    void init();
    bool enabled();
    bool connected();
    bool canConnect();
    void connecting();
    bool wantToSay(String& mqttBuffer,int& mqttBufferLength);
    void getMQTTServer(String& host,int& port);
    void update(uint16_t dt);

    void reloadSettings();

    byte GetSavedTopicsCount();
    void DeleteAllTopics();
    void AddTopic(const char* topicIndex, const char* topicName, const char* moduleName, const char* sensorType, const char* sensorIndex, const char* topicType);

  private:

    MQTTClientFlags flags;

    String* reportTopicString;
    String reportModuleName;

    void switchToNextTopic();

    uint16_t mqttMessageId;

    uint16_t reconnectTimer;
    unsigned long updateTopicsTimer;

    uint8_t intervalBetweenTopics; // интервал между публикацией топиков
    uint8_t currentTopicNumber; // номер текущего топика для опубликования

    void convertAnswerToJSON(const String& answer, String* resultBuffer);

    void constructConnectPacket(String& mqttBuffer,int& mqttBufferLength,const char* id, const char* user, const char* pass,const char* willTopic,uint8_t willQoS, uint8_t willRetain, const char* willMessage);
    void encode(MQTTBuffer& buff,const char* str);
    void constructFixedHeader(byte command, MQTTBuffer& fixedHeader,size_t payloadSize);
    void constructPublishPacket(String& mqttBuffer,int& mqttBufferLength, const char* topic, const char* payload);
    void constructSubscribePacket(String& mqttBuffer,int& mqttBufferLength, const char* topic);

    void writePacket(MQTTBuffer& fixedHeader,MQTTBuffer& payload, String& mqttBuffer,int& mqttBufferLength);
    
};
//--------------------------------------------------------------------------------------------------------------------------------
#endif // USE_WIFI_MODULE_AS_MQTT_CLIENT
//--------------------------------------------------------------------------------------------------------------------------------
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
  /*15*/ wfaStartSendIoTData, // посылаем команду на отсыл данных в IoT
  /*16*/ wfaActualSendIoTData, // актуальный отсыл данных в IoT
  /*17*/ wfaCloseIoTConnection, // закрываем соединение
  
#endif

  /*18*/ wfaStartHTTPSend, // начинаем запрос HTTP
  /*19*/ wfaStartSendHTTPData, // начинаем отсылать данные по HTTP
  /*20*/ wfaCloseHTTPConnection, // закрываем HTTP-соединение
  /*21*/ wfaActualSendHTTPData, // актуальный отсыл данных HTTP-запроса

#ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT

  /*22*/ wfaConnectToMQTT, // коннектимся к MQTT-серверу
  /*23*/ wfaWriteToMQTT, // пишем в MQTT-сервер данные
  /*24*/ wfaActualWriteToMQTT, // актуальная запись в порт пакета к MQTT-брокеру
  
#endif  
  
} WIFIActions;

typedef Vector<WIFIActions> ActionsVector;

typedef struct
{

  bool inSendData : 1; 
  bool isAnyAnswerReceived : 1;
  bool inRebootMode : 1;
  bool wantIoTToProcess : 1; // нас попросили отправить данные в IoT
  bool wantHTTPRequest : 1; // нас попросили запросить URI по HTTP и получить ответ
  bool inHTTPRequestMode: 1; // мы в процессе работы с HTTP-запросом
  bool isConnected: 1; 
  bool wantReconnect : 1;
      
} WiFiModuleFlags;

class WiFiModule : public AbstractModule // модуль поддержки WI-FI
#if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
, public IoTGate
#endif
, public HTTPQueryProvider

{
  private:

    #ifdef USE_WIFI_MODULE_AS_MQTT_CLIENT
      MQTTClient mqtt;
      String* mqttBuffer;
      int mqttBufferLength;
    #endif

    WiFiModuleFlags flags;

    HTTPRequestHandler* httpHandler; // интерфейс перехватчика работы с HTTP-запросами
    String* httpData; // данные для отсыла по HTTP
    void EnsureHTTPProcessed(uint16_t statusCode); // убеждаемся, что мы сообщили вызывающей стороне результат запроса по HTTP
    
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

    void ProcessAnswerLine(String& line);
    volatile bool WaitForDataWelcome; // флаг, что мы ждём приглашения на отсыл данных - > (плохое ООП, негодное :) )

#if defined(USE_IOT_MODULE) && defined(USE_WIFI_MODULE_AS_IOT_GATE)
    virtual void SendData(IoTService service,uint16_t dataLength, IOT_OnWriteToStream writer, IOT_OnSendDataDone onDone);
#endif 

  virtual bool CanMakeQuery(); // тестирует, может ли модуль сейчас сделать запрос
  virtual void MakeQuery(HTTPRequestHandler* handler); // начинаем запрос по HTTP

};


#endif
