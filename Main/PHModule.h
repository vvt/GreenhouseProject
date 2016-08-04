#ifndef _PH_MODULE_H
#define _PH_MODULE_H
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#include "AbstractModule.h"
//-------------------------------------------------------------------------------------------------------------------------------------------------------
class PhModule : public AbstractModule // модуль контроля pH
{
  private:

    byte phSensorPin;
    unsigned long measureTimer;
    bool inMeasure;
    byte samplesDone;
    byte samplesTimer;

    int calibration; // калибровка, в сотых долях

    unsigned long dataArray;

    void ReadSettings();
    void SaveSettings();
  
  public:
    PhModule() : AbstractModule("PH") {}

    bool ExecCommand(const Command& command, bool wantAnswer);
    void Setup();
    void Update(uint16_t dt);

};
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#endif
