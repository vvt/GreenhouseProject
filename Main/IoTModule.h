#ifndef _IOT_MODULE_H
#define _IOT_MODULE_H

#include "AbstractModule.h"
#include "IoT.h"

/*
typedef struct
{
  byte type;
  byte index;
  const char* module;
} IoTSensorData;
*/

class IoTModule : public AbstractModule // модуль отсылки данных в IoT-хранилища
#if defined(USE_IOT_MODULE) && defined(IOT_UNIT_TEST)
, public IoTGate
#endif
{
  private:

#if defined(USE_IOT_MODULE)

  void CollectDataForThingSpeak();

  void SwitchToWaitMode();
  void SwitchToNextService();

  String* dataToSend; // данные для отсылки
  unsigned long updateTimer;
  bool inSendData;

  IoTServices services;
  IoTService currentService;
  int currentGateIndex;

  void SendDataToIoT();
  void ProcessNextGate();

  AbstractModule* FindModule(byte index);
  
#endif  

#if defined(USE_IOT_MODULE) && defined(IOT_UNIT_TEST)
    virtual void SendData(IoTService service,uint16_t dataLength, IOT_OnWriteToStream writer, IOT_OnSendDataDone onDone);
#endif
  
  public:
    IoTModule() : AbstractModule("IOT") {}

    bool ExecCommand(const Command& command, bool wantAnswer);
    void Setup();
    void Update(uint16_t dt);

#if defined(USE_IOT_MODULE)
    void Write(Stream* writeTo);
    void Done(const IoTCallResult& result);
 #endif

};


#endif
