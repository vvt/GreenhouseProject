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
    int16_t ph4Voltage; // показания в милливольтах для тестового раствора 4 pH
    int16_t ph7Voltage; // показания в милливольтах для тестового раствора 7 pH
    int16_t ph10Voltage; // показания в милливольтах для тестового раствора 10 pH
    int8_t phTemperatureSensorIndex; // индекс датчика температуры, который завязан на измерения pH
    Temperature phSamplesTemperature; // температура, при которой производилась калибровка

    uint16_t phTarget; // значение pH, за которым следим
    uint16_t phHisteresis; // гистерезис
    uint16_t phMixPumpTime; // время работы насоса перемешивания, с
    uint16_t phReagentPumpTime; // время работы подачи реагента, с

    unsigned long dataArray;

    void ReadSettings();
    void SaveSettings();
  
  public:
    PhModule() : AbstractModule("PH") {}

    bool ExecCommand(const Command& command, bool wantAnswer);
    void Setup();
    void Update(uint16_t dt);

    void ApplyCalculation(Temperature* temp);

};
//-------------------------------------------------------------------------------------------------------------------------------------------------------
class PHCalculator
{
  public:

    void ApplyCalculation(Temperature* temp);
  
    PHCalculator();
};
//-------------------------------------------------------------------------------------------------------------------------------------------------------
extern PHCalculator PHCalculation;
#endif
