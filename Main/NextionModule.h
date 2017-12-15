#ifndef _NEXTION_MODULE_H
#define _NEXTION_MODULE_H

#include "AbstractModule.h"
#include "NextionController.h"
#include "Settings.h"

#ifdef USE_NEXTION_MODULE
//--------------------------------------------------------------------------------------------------------------------------------------
 typedef struct
 {
  uint8_t sensorType;
  uint8_t sensorIndex;
  const char* moduleName;
    
 } NextionWaitScreenInfo; // структура для хранения информации, которую необходимо показывать на экране ожидания
//--------------------------------------------------------------------------------------------------------------------------------------
typedef struct
{
    bool  isDisplaySleep : 1;
    bool bInited : 1;
    bool isWindowsOpen : 1;
    bool isWindowAutoMode : 1;
    bool isWaterOn : 1;
    bool isWaterAutoMode : 1;
    bool isLightOn : 1;
    bool isLightAutoMode : 1;
    
    bool windowChanged : 1;
    bool windowModeChanged : 1;
    bool waterChanged : 1;
    bool waterModeChanged : 1;
    bool lightChanged : 1;
    bool lightModeChanged : 1;
    bool openTempChanged : 1;
    bool closeTempChanged : 1;
  
} NextionModuleFlags;
//--------------------------------------------------------------------------------------------------------------------------------------
class NextionModule : public AbstractModule // модуль управления дисплеем Nextion
{
  private:
  
    NextionController nextion; // класс для управления дисплеем
    NextionModuleFlags flags;
    
    uint8_t openTemp, closeTemp;

    unsigned long rotationTimer;
    
    GlobalSettings* sett;
    
    void updateDisplayData();

    void displayNextSensorData(int8_t dir=1);
    int8_t currentSensorIndex;
  
  public:
    NextionModule() : AbstractModule("NXT") {}

    bool ExecCommand(const Command& command, bool wantAnswer);
    void Setup();
    void Update(uint16_t dt);
    
    void SetSleep(bool bSleep);
    void StringReceived(const char* str);

};
#endif // USE_NEXTION_MODULE
//--------------------------------------------------------------------------------------------------------------------------------------
#endif
