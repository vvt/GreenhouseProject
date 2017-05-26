#ifndef _WATERING_MODULE_H
#define _WATERING_MODULE_H

#include "AbstractModule.h"
#include "Globals.h"
#include "InteropStream.h"


typedef enum
{
  wwmAutomatic, // в автоматическом режиме
  wwmManual // в ручном режиме
  
} WateringWorkMode; // режим работы полива


typedef struct
{
  
  bool rel_on : 1; // включено ли реле канала?
  bool last_rel_on : 1; // последнее состояние реле канала
  byte pad : 6;
    
} WateringChannelState;

class WateringChannel // канал для полива
{
  
private:

  WateringChannelState state;

public:

  bool IsChannelRelayOn() {return state.rel_on;}
  void SetRelayOn(bool bOn) { state.last_rel_on = state.rel_on; state.rel_on = bOn; }
  bool IsChanged() {return state.last_rel_on != state.rel_on; }
  
  unsigned long WateringTimer; // таймер полива для канала
  unsigned long WateringDelta; // дельта дополива
    
};

typedef struct
{
  uint8_t workMode : 4; // текущий режим работы
  bool bIsRTClockPresent : 1; // флаг наличия модуля часов реального времени
  bool bPumpIsOn : 1;
  bool bPump2IsOn : 1;
  bool internalNeedChange : 1;
  
} WateringModuleFlags;

class WateringModule : public AbstractModule // модуль управления поливом
{
  private:

  #if WATER_RELAYS_COUNT > 0
  
  WateringChannel wateringChannels[WATER_RELAYS_COUNT]; // каналы полива
  WateringChannel dummyAllChannels; // управляем всеми каналами посредством этой структуры
  void UpdateChannel(int8_t channelIdx, WateringChannel* channel, uint16_t dt); // обновляем состояние канала
  void HoldChannelState(int8_t channelIdx, WateringChannel* channel);  // поддерживаем состояние реле для канала.
  bool IsAnyChannelActive(uint8_t wateringOption, bool& shouldTurnOnPump1, bool& shouldTurnOnPump2); // возвращает true, если хотя бы один из каналов активен

  #endif

  int8_t lastAnyChannelActiveFlag; // флаг последнего состояния активности каналов

  WateringModuleFlags flags;
   
  uint8_t lastDOW; // день недели с момента предыдущего опроса
  uint8_t currentDOW; // текущий день недели
  uint8_t currentHour; // текущий час
  
#ifdef USE_WATERING_MANUAL_MODE_DIODE
  BlinkModeInterop blinker;
#endif



#ifdef USE_PUMP_RELAY   
   void HoldPumpState(bool shouldTurnOnPump1, bool shouldTurnOnPump2); // поддерживаем состояние реле насосов
#endif

    
  public:
    WateringModule() : AbstractModule("WATER") {}

    bool ExecCommand(const Command& command, bool wantAnswer);
    void Setup();
    void Update(uint16_t dt);

};


#endif
