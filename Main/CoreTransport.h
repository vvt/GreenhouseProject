#pragma once
//--------------------------------------------------------------------------------------------------------------------------------
#include <Arduino.h>
#include "TinyVector.h"
#include "Globals.h"
//--------------------------------------------------------------------------------------------------------------------------------
#define MQTT_FILENAME_PATTERN F("MQTT/MQTT.")
#define DEFAULT_MQTT_CLIENT F("greenhouse")
#define REPORT_TOPIC_NAME F("/REPORT/")
//--------------------------------------------------------------------------------------------------------------------------------
class CoreTransportClient;
#ifdef USE_WIFI_MODULE
class CoreESPTransport;
#endif
//--------------------------------------------------------------------------------------------------------------------------------
// Коды ошибок транспорта
//--------------------------------------------------------------------------------------------------------------------------------
#define CT_ERROR_NONE             0 // нет ошибки
#define CT_ERROR_CANT_CONNECT     1 // не удалось установить соединение
#define CT_ERROR_CANT_WRITE       2 // ошибка записи данных из клиента в поток
//--------------------------------------------------------------------------------------------------------------------------------
// типы событий
//--------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
  etConnect,      // соединено
  etDataWritten,    // данные записаны
  etDataAvailable   // доступны входящие данные
  
} ClientEventType;
//---------------------------------------------------------------------------------------------------------------------------------
struct IClientEventsSubscriber
{
  virtual void OnClientConnect(CoreTransportClient& client, bool connected, int16_t errorCode) = 0; // событие "Статус соединения клиента"
  virtual void OnClientDataWritten(CoreTransportClient& client, int16_t errorCode) = 0; // событие "Данные из клиента записаны в поток"
  virtual void OnClientDataAvailable(CoreTransportClient& client, uint8_t* data, size_t dataSize, bool isDone) = 0; // событие "Для клиента поступили данные", флаг - все ли данные приняты
};
//--------------------------------------------------------------------------------------------------------------------------------
typedef Vector<IClientEventsSubscriber*> ClientSubscribers;
#define NO_CLIENT_ID 0xFF
//--------------------------------------------------------------------------------------------------------------------------------
class CoreTransport
{
  public:
  
    CoreTransport(uint8_t clientsPoolSize);
    virtual ~CoreTransport();

    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual bool paused() = 0;
    
    // обновляем состояние транспорта
    virtual void update() = 0; 

    // начинаем работу
    virtual void begin() = 0; 

    // проверяет, готов ли транспорт к работе (например, проведена ли первичная инициализация)
    virtual bool ready() = 0; 

   // подписка на события клиентов
   void subscribe(IClientEventsSubscriber* subscriber);
   
   // отписка от событий клиентов
   void unsubscribe(IClientEventsSubscriber* subscriber);


private:

    ClientSubscribers subscribers;
    Vector<CoreTransportClient*> pool;
    Vector<bool> status;

    Vector<CoreTransportClient*> closedCatchList;
    bool isExternalClient(CoreTransportClient& client);

protected:

    void initPool();

  friend class CoreTransportClient;

  bool connected(uint8_t socket);

  void doWrite(CoreTransportClient& client); // начинаем писать в транспорт с клиента
  void doConnect(CoreTransportClient& client, const char* ip, uint16_t port); // начинаем коннектиться к адресу
  void doDisconnect(CoreTransportClient& client); // начинаем отсоединение от адреса
  
  CoreTransportClient* getClient(uint8_t socket);

  // вызов событий для клиента
  void notifyClientConnected(CoreTransportClient& client, bool connected, int16_t errorCode);  
  void notifyDataWritten(CoreTransportClient& client, int16_t errorCode);
  void notifyDataAvailable(CoreTransportClient& client, uint8_t* data, size_t dataSize, bool isDone);
      
    
  virtual void beginWrite(CoreTransportClient& client) = 0; // начинаем писать в транспорт с клиента
  virtual void beginConnect(CoreTransportClient& client, const char* ip, uint16_t port) = 0; // начинаем коннектиться к адресу
  virtual void beginDisconnect(CoreTransportClient& client) = 0; // начинаем отсоединение от адреса
  
};
//--------------------------------------------------------------------------------------------------------------------------------
class CoreTransportClient
{
  public:

   CoreTransportClient();
   /*virtual */
   ~CoreTransportClient();
  
  bool connected();
  void accept(CoreTransport* _parent);
     
  operator bool()
  {
    return  (socket != NO_CLIENT_ID); 
  }

  bool operator==(const CoreTransportClient& rhs)
  {
    return (rhs.socket == socket);
  }

  bool operator!=(const CoreTransportClient& rhs)
  {
    return !(operator==(rhs));
  }

  void connect(const char* ip, uint16_t port);
  void disconnect();
  
  bool write(uint8_t* buff, size_t sz);  


 protected:

    friend class CoreTransport;
    #ifdef USE_WIFI_MODULE
    friend class CoreESPTransport;
    #endif
    
    CoreTransport* parent;

    void releaseBuffer()
    {
      dataBuffer = NULL;
      dataBufferSize = 0;
    }

    // установка ID клиента транспортом
    void bind(uint8_t _socket)
    {
      socket = _socket;
    }
    void release()
    {
      socket = NO_CLIENT_ID;
    }

    void clear();
    uint8_t* getBuffer(size_t& sz)
    {
      sz =  dataBufferSize;
      return dataBuffer;
    }

 private:

    CoreTransportClient(const CoreTransportClient& rhs);
    CoreTransportClient& operator=(const CoreTransportClient& rhs);

    uint8_t* dataBuffer;
    size_t dataBufferSize;
    uint8_t socket;
    
};
//--------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_WIFI_MODULE
//--------------------------------------------------------------------------------------------------------------------------------
#define ESP_MAX_CLIENTS 4 // наш пул клиентов
//--------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
  
  actionDisconnect, // запрошено отсоединение клиента
  actionConnect, // запрошено подсоединение клиента
  actionWrite, // запрошена запись из клиента в ESP
  
} ESPClientAction;
//--------------------------------------------------------------------------------------------------------------------------------
struct ESPClientQueueData // данные по клиенту в очереди
{
  ESPClientAction action; // действие, которое надо выполнить с клиентом
  CoreTransportClient* client; // ссылка на клиента
  char* ip; // IP для подсоединения
  uint16_t port; // порт для подсоединения
  size_t dataLength;
  uint8_t* data;

  ESPClientQueueData()
  {
   client = NULL;
   ip = NULL;
   data = NULL;
  }
   
};
//--------------------------------------------------------------------------------------------------------------------------------
typedef Vector<ESPClientQueueData> ESPClientsQueue; // очередь клиентов на совершение какой-либо исходящей операции (коннект, дисконнект, запись)
//--------------------------------------------------------------------------------------------------------------------------------
typedef struct
{
  bool ready                : 1; // флаг готовности
  bool connectedToRouter    : 1; // флаг того, что заокннекчены к роутеру
  bool isAnyAnswerReceived  : 1; // флаг, что мы получили хотя бы один ответ от модема
  bool waitForDataWelcome   : 1; // флаг, что ждём приглашения на отсыл данных
  bool wantReconnect        : 1; // флаг, что мы должны переподсоединиться к роутеру
  bool onIdleTimer          : 1; // флаг, что мы в режиме простоя
  bool bPaused              : 1; // флаг, что мы на паузе
  
} CoreESPTransportFlags;
//--------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
  cmdNone, // ничего не делаем
  cmdWantReady, // надо получить ready от модуля
  cmdEchoOff, // выключаем эхо
  cmdCWMODE, // переводим в смешанный режим
  cmdCWSAP, // создаём точку доступа
  cmdCWJAP, // коннектимся к роутеру
  cmdCWQAP, // отсоединяемся от роутера
  cmdCIPMODE, // устанавливаем режим работы
  cmdCIPMUX, // разрешаем множественные подключения
  cmdCIPSERVER, // запускаем сервер
  cmdCheckModemHang, // проверяем на зависание модема 
  cmdCIPCLOSE, // отсоединямся
  cmdCIPSTART, // соединяемся
  cmdCIPSEND, // начинаем слать данные
  cmdWaitSendDone, // ждём окончания отсылки данных
  
} ESPCommands;
//--------------------------------------------------------------------------------------------------------------------------------
typedef Vector<ESPCommands> ESPCommandsList;
//--------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
  espIdle,        // состояние "ничего не делаем"
  espWaitAnswer,  // состояние "ждём ответа на команду, посланную ESP"
  espReboot,      // состояние "ESP в процессе перезагрузки"
  espWaitInit,    // ждём инициализации после подачи питания
  
} ESPMachineState;
//--------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
  kaNone,
  kaOK,             // OK
  kaError,          // ERROR
  kaFail,           // FAIL
  kaSendOk,         // SEND OK
  kaSendFail,       // SEND FAIL
  kaAlreadyConnected, // ALREADY CONNECTED
  
} ESPKnownAnswer;
//--------------------------------------------------------------------------------------------------------------------------------
class CoreESPTransport : public CoreTransport
{
  public:
    CoreESPTransport();

    virtual void update(); // обновляем состояние транспорта
    virtual void begin(); // начинаем работу

    // возвращает IP клиента и станции
    bool getIP(String& staIP, String& apIP);
    
    // возвращает MAC клиента и станции
    bool getMAC(String& staMAC, String& apMAC);

    // пропинговать гугл
    bool pingGoogle(bool& result);

    virtual bool ready(); // проверяем на готовность к работе
       
    void restart();

    virtual void pause()
    {
      flags.bPaused = true;
    }

    virtual void resume()
    {
      flags.bPaused = false;
    }

    virtual bool paused()
    {
      return flags.bPaused;
    }

  protected:

    virtual void beginWrite(CoreTransportClient& client); // начинаем писать в транспорт с клиента
    virtual void beginConnect(CoreTransportClient& client, const char* ip, uint16_t port); // начинаем коннектиться к адресу
    virtual void beginDisconnect(CoreTransportClient& client); // начинаем отсоединение от адреса

  private:

      void waitTransmitComplete();

      bool waitCipstartConnect;
      CoreTransportClient* cipstartConnectClient;
      uint8_t cipstartConnectClientID;
      bool cipstartConnectKnownAnswerFound;


      bool checkIPD(const String& line);
      void processKnownStatusFromESP(const String& line);
      void processIPD(const String& line);
      void processConnect(const String& line);
      void processDisconnect(const String& line);

      bool isESPBootFound(const String& line);
      bool isKnownAnswer(const String& line, ESPKnownAnswer& result);

      CoreESPTransportFlags flags; // флаги состояния
      ESPMachineState machineState; // состояние конечного автомата

      ESPCommands currentCommand;
      ESPCommandsList initCommandsQueue; // очередь команд на инициализацию
      uint32_t timer; // общий таймер
      uint32_t idleTime; // время, которое нам надо подождать, ничего не делая
      
      void clearInitCommands();
      void createInitCommands(bool addResetCommand);
      
      void sendCommand(const String& command, bool addNewLine=true);
      void sendCommand(ESPCommands command);
      
      Stream* workStream; // поток, с которым мы работаем (читаем/пишем в/из него)

      String* wiFiReceiveBuff;

      void clearClientsQueue(bool raiseEvents);

      ESPClientsQueue clientsQueue; // очередь действий с клиентами

      bool isClientInQueue(CoreTransportClient* client, ESPClientAction action); // тестирует - не в очереди ли уже клиент?
      void addClientToQueue(CoreTransportClient* client, ESPClientAction action, const char* ip=NULL, uint16_t port=0); // добавляет клиента в очередь
      void removeClientFromQueue(CoreTransportClient* client); // удаляет клиента из очереди  
      void removeClientFromQueue(CoreTransportClient* client, ESPClientAction action); // удаляет клиента из очереди  
      
      void initClients();
    
};
//--------------------------------------------------------------------------------------------------------------------------------
extern CoreESPTransport ESP;
//--------------------------------------------------------------------------------------------------------------------------------
#endif // USE_WIFI_MODULE
//--------------------------------------------------------------------------------------------------------------------------------------
#define MQTT_CONNECT_COMMAND (1 << 4)
#define MQTT_PUBLISH_COMMAND (3 << 4)
#define MQTT_SUBSCRIBE_COMMAND (8 << 4)
#define MQTT_QOS1 (1 << 1)
//--------------------------------------------------------------------------------------------------------------------------------------
typedef Vector<uint8_t> MQTTBuffer;
//--------------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
  mqttWaitClient, // ожидаем свободного клиента
  mqttWaitConnection, // ожидаем подсоединения к брокеру
  mqttWaitReconnect, // ожидаем переподсоединения к брокеру (в случае неуспешного соединения)
  mqttSendConnectPacket, // отсылаем пакет с информацией о подсоединении к брокеру
  mqttWaitSendConnectPacketDone,
  mqttSendSubscribePacket, // отсылаем пакет с информацией о подписке
  mqttWaitSendSubscribePacketDone,
  mqttSendPublishPacket, // отсылаем пакет публикации
  mqttWaitSendPublishPacketDone,
  
} MQTTState;
//--------------------------------------------------------------------------------------------------------------------------------------
typedef struct
{
  char* topic;
  char* payload;
  
} MQTTPublishQueue;
//--------------------------------------------------------------------------------------------------------------------------------------
typedef Vector<MQTTPublishQueue> MQTTPublishList;
//--------------------------------------------------------------------------------------------------------------------------------------
typedef struct
{
  String serverAddress;
  int port;
  String clientID;
  String userName;
  String password;
  
} MQTTSettings;
//--------------------------------------------------------------------------------------------------------------------------------------
class CoreMQTT : public IClientEventsSubscriber//, public Stream
{
  public:
    CoreMQTT();
    void reset();
    void update();
    void begin(CoreTransport* transport);
    bool enabled();

  // IClientEventsSubscriber
  virtual void OnClientConnect(CoreTransportClient& client, bool connected, int16_t errorCode); // событие "Статус соединения клиента"
  virtual void OnClientDataWritten(CoreTransportClient& client, int16_t errorCode); // событие "Данные из клиента записаны в поток"
  virtual void OnClientDataAvailable(CoreTransportClient& client, uint8_t* data, size_t dataSize, bool isDone); // событие "Для клиента поступили данные", флаг - все ли данные приняты

/*
  // Stream
  virtual void flush(){}
  virtual int peek() {return 0;}
  virtual int read() {return 0;}
  virtual int available() {return 0;}
  virtual size_t write(uint8_t ch) { *streamBuffer += (char) ch; return 1;}
*/
  // для публикации любого стороннего топика
  bool publish(const char* topicName, const char* payload);

  void reloadSettings();

  void AddTopic(const char* topicIndex, const char* topicName, const char* moduleName, const char* sensorType, const char* sensorIndex, const char* topicType);
  void DeleteAllTopics();
  uint16_t GetSavedTopicsCount();


private:

  void getNextTopic(String& topicName, String& data);
  void switchToNextTopic();

  MQTTSettings currentSettings;
  MQTTSettings getSettings();

  MQTTPublishList publishList;
  void clearPublishQueue();

  CoreTransportClient currentClient;
  CoreTransport* currentTransport;
  uint32_t timer;

  MQTTState machineState;
  uint16_t mqttMessageId;
  String* streamBuffer;

  uint32_t intervalBetweenTopics;
  uint16_t currentTopicNumber;

  void pushToReportQueue(String* toReport);
  Vector<String*> reportQueue;
  void clearReportsQueue();

  void constructFixedHeader(byte command, MQTTBuffer& fixedHeader,size_t payloadSize);

  void constructConnectPacket(String& mqttBuffer,int16_t& mqttBufferLength,const char* id, const char* user, const char* pass,const char* willTopic,uint8_t willQoS, uint8_t willRetain, const char* willMessage);
  void constructSubscribePacket(String& mqttBuffer,int16_t& mqttBufferLength, const char* topic);
  void constructPublishPacket(String& mqttBuffer,int16_t& mqttBufferLength, const char* topic, const char* payload);
  
  void encode(MQTTBuffer& buff,const char* str);

  void writePacket(MQTTBuffer& fixedHeader,MQTTBuffer& payload, String& mqttBuffer,int16_t& mqttBufferLength);

  void processIncomingPacket(CoreTransportClient* client, uint8_t* data, size_t dataLength);

  void convertAnswerToJSON(const String& answer, String* resultBuffer);
};
//--------------------------------------------------------------------------------------------------------------------------------------

