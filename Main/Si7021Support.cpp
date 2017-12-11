#include "Si7021Support.h"
#include "AbstractModule.h"
//--------------------------------------------------------------------------------------------------------------------------------------
Si7021::Si7021()
{
}
//--------------------------------------------------------------------------------------------------------------------------------------
void Si7021::begin()
{
  /*
  Wire.begin();
  WORK_STATUS.PinMode(SDA,INPUT,false);
  WORK_STATUS.PinMode(SCL,OUTPUT,false);  
  setResolution();
  */
  sensor.begin();
}
//--------------------------------------------------------------------------------------------------------------------------------------
/*
uint8_t Si7021::read8(uint8_t reg)
{
  Wire.beginTransmission(Si7021Address);
  SI7021_WRITE(reg);
  Wire.endTransmission();

  Wire.requestFrom(Si7021Address, 1);
  return SI7021_READ();
 
}
//--------------------------------------------------------------------------------------------------------------------------------------
void Si7021::setResolution()
{
  uint8_t userRegisterData;

  userRegisterData = read8(0xE7);

  userRegisterData &= 0x7E;
  userRegisterData |= 0x00;

  Wire.beginTransmission(Si7021Address);
    SI7021_WRITE(0xE6);
    SI7021_WRITE(userRegisterData);
  Wire.endTransmission();

}
*/
//--------------------------------------------------------------------------------------------------------------------------------------
const HumidityAnswer& Si7021::read()
{
 
  dt.IsOK = false;
  dt.Humidity = NO_TEMPERATURE_DATA;
  dt.Temperature = NO_TEMPERATURE_DATA;
  
  float humidity, temperature;
  humidity = sensor.readHumidity();
  temperature = sensor.readTemperature();

  byte humError = (byte) humidity;
  byte tempError = (byte) temperature;

  if(humError == HTU21D_ERROR || tempError == HTU21D_ERROR)
  {
    dt.IsOK = false;
  }
  else
  {
     dt.IsOK = true;
     
    int iTmp = humidity*100;
    
    dt.Humidity = iTmp/100;
    dt.HumidityDecimal = iTmp%100;

    if(dt.Humidity < 0 || dt.Humidity > 100)
    {
      dt.Humidity = NO_TEMPERATURE_DATA;
      dt.HumidityDecimal = 0;
    }
      
    
    iTmp = temperature*100;
    
    dt.Temperature = iTmp/100;
    dt.TemperatureDecimal = iTmp%100;

    if(dt.Temperature < -40 || dt.Temperature > 125)
    {
      dt.Temperature = NO_TEMPERATURE_DATA;
      dt.TemperatureDecimal = 0;
    }
       
  }

 /* 
  uint16_t humidity = 0;
  uint16_t temp = 0;
  bool crcOk = false;
  static byte buffer[] = {0, 0, 0};
 
  
  Wire.beginTransmission(Si7021Address);
  SI7021_WRITE(Si7021_E5);
  Wire.endTransmission();

  delay(16);
  
  Wire.requestFrom(Si7021Address, 3);

  uint8_t   pollCounter = 0;
  while (Wire.available() < 3)
  {
    pollCounter++;
    if (pollCounter > 8)
    {
      return dt;
    }
    delay(8);
    yield();
  }  
  
  if(Wire.available() >= 3)
  {
  
    
    buffer[0] = SI7021_READ();
    buffer[1] = SI7021_READ();
    buffer[2] = SI7021_READ();
    
    humidity =  (buffer[0]<<8) | buffer[1];

    uint8_t calcCRC = 0;  

    for (uint8_t i = 0; i < 2; i++)
    {
      calcCRC ^= buffer[i];
  
        for (uint8_t j = 8; j > 0; j--)
        {
           if (calcCRC & 0x80)
              calcCRC = (calcCRC << 1) ^ 0x131;
          else
              calcCRC = (calcCRC << 1);
        } // for

    } // for

    crcOk = (calcCRC == buffer[2]);

  } // if
  
  Wire.beginTransmission(Si7021Address);
  SI7021_WRITE(Si7021_E0);
  Wire.endTransmission();
  
  Wire.requestFrom(Si7021Address, 2);
  
  if(Wire.available() >= 2)
  {
    buffer[0] = SI7021_READ(); 
    buffer[1] = SI7021_READ();
    
    temp =  (buffer[0]<<8) | buffer[1];
  }
  
  if(temp != 0 && humidity != 0)
  {
    dt.IsOK = crcOk;
    
    int iTmp = ((125.0*humidity)/65536 - 6)*100;
    
    dt.Humidity = iTmp/100;
    dt.HumidityDecimal = iTmp%100;
    
    iTmp = (175.72*temp/65536 - 46.85)*100;
    
    dt.Temperature = iTmp/100;
    dt.TemperatureDecimal = iTmp%100;
    
  }
  */
  
  return dt;
}
//--------------------------------------------------------------------------------------------------------------------------------------
