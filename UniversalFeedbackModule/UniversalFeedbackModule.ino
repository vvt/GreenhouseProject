#include <avr/io.h>
#include <avr/interrupt.h>
#include "Common.h"
#include "LowLevel.h"
#include "OneWireSlave.h"
//----------------------------------------------------------------------------------------------------------------
/*
Прошивка для модуля, предназначена для получения обратной связи
по положению окон.
Поддерживается работа по RS-485.

ВНИМАНИЕ!

RS-485 работает через аппаратный UART (RX0 и TX0 ардуины)!
Перед прошивкой выдёргивать контакты модуля RS-485 из пинов аппаратного UART!

*/
//----------------------------------------------------------------------------------------------------------------
// настройки чтения номера модуля. Номер модуля читается с трёх пинов, и принимает значение от 0 до 3,
// т.е. на линии могут быть 4 модуля, каждый в этом случае может обслуживать 4 окна. Максимальное кол-во
// окон, обслуживаемых модулем - 16. В зависимости от номера модуля главный контроллер принимает решение,
// какие окна модуль обслуживает: первый номер - с нуля до кол-ва поддерживаемых модулем окон, второй - с 
// последнего обслуженного контроллером окна и до кол-ва окон, обслуживаемых модулем и т.п.
//----------------------------------------------------------------------------------------------------------------
#define ADDRESS_PIN1 5 // пин адреса 1
#define ADDRESS_PIN2 6 // пин адреса 2
#define ADDRESS_PIN3 7 // пин адреса 3
//----------------------------------------------------------------------------------------------------------------
// настройки кол-ва обслуживаемых окон
//----------------------------------------------------------------------------------------------------------------
#define WINDOWS_SERVED 16 // Сколько окон обслуживается?
//----------------------------------------------------------------------------------------------------------------
// настройки RS-485
//----------------------------------------------------------------------------------------------------------------
#define RS485_SPEED 57600 // скорость работы по RS-485
#define RS485_DE_PIN 4 // номер пина, на котором будем управлять направлением приём/передача по RS-485
//----------------------------------------------------------------------------------------------------------------
// настройки
//----------------------------------------------------------------------------------------------------------------
// настройки инициализации, привязка слотов к пинам и первоначальному состоянию
//----------------------------------------------------------------------------------------------------------------
// Дальше лазить - неосмотрительно :)
//----------------------------------------------------------------------------------------------------------------
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// \/
//----------------------------------------------------------------------------------------------------------------
// ДАЛЕЕ ИДУТ СЛУЖЕБНЫЕ НАСТРОЙКИ И КОД - МЕНЯТЬ С ПОЛНЫМ ПОНИМАНИЕМ ТОГО, ЧТО ХОДИМ СДЕЛАТЬ !!!
//----------------------------------------------------------------------------------------------------------------
//#define _DEBUG // раскомментировать для отладочного режима
//----------------------------------------------------------------------------------------------------------------
byte myAddress = 0;
//----------------------------------------------------------------------------------------------------------------
/*
 Структура пакета, передаваемого по RS-495:
 
   0xAB - первый байт заголовка
   0xBA - второй байт заголовка

   данные, в зависимости от типа пакета
   
   0xDE - первый байт окончания
   0xAD - второй байт окончания

 
 */
//----------------------------------------------------------------------------------------------------------------
void ReadModuleAddress() // читаем адрес модуля
{
  myAddress = 0;
  
  pinMode(ADDRESS_PIN1,INPUT);
  pinMode(ADDRESS_PIN2,INPUT);
  pinMode(ADDRESS_PIN3,INPUT);

  byte bit1 = digitalRead(ADDRESS_PIN1);
  byte bit2 = digitalRead(ADDRESS_PIN2);
  byte bit3 = digitalRead(ADDRESS_PIN3);

  myAddress = bit1 | (bit2 << 1) | (bit3 << 2);
}
//----------------------------------------------------------------------------------------------------------------
void GetWindowsStatus(byte windowNumber, byte& isCloseSwitchTriggered, byte& isOpenSwitchTriggered, byte& hasPosition, byte& position)
{
 
  //TODO: Тут актуальное чтение позиций окон!!!
  isCloseSwitchTriggered = 0;
  isOpenSwitchTriggered = 0;
  
  hasPosition = 1;
  position = (windowNumber+1)*5; // пока тупо, чисто для теста

}
//----------------------------------------------------------------------------------------------------------------
RS485Packet rs485Packet; // пакет, в который мы принимаем данные
volatile byte* rsPacketPtr = (byte*) &rs485Packet;
volatile byte  rs485WritePtr = 0; // указатель записи в пакет
//----------------------------------------------------------------------------------------------------------------
void FillRS485PacketWithData()
{
  byte isCloseSwitchTriggered;
  byte isOpenSwitchTriggered;
  byte hasPosition;
  byte position;

  byte currentByteNumber = 0;
  int8_t currentBitNumber = 7;

  memset(rs485Packet.data.windowsStatus,0,sizeof(rs485Packet.data.windowsStatus));
  
  for(int i=0;i<WINDOWS_SERVED;i++)
  {
    GetWindowsStatus(i, isCloseSwitchTriggered, isOpenSwitchTriggered, hasPosition, position);
    // тут мы получили состояния. У нас есть номер байта и номер бита,
    // с которого надо писать в поток. Как только бит исчерпан - переходим на следующий байт.
    // position  у нас занимает старшие биты, причём самый старший там - нам не нужен
    for(int k=6;k>=0;k--)
    {
      byte b = bitRead(position,k);
      rs485Packet.data.windowsStatus[currentByteNumber] |= (b << currentBitNumber);
      
      currentBitNumber--;
      if(currentBitNumber < 0)
      {
        currentBitNumber = 7;
        currentByteNumber++;
      }
      
    } // for

    // записали позицию, пишем информацию о том, есть ли позиция окна (третий по старшинству бит)
    rs485Packet.data.windowsStatus[currentByteNumber] |= (hasPosition << currentBitNumber);
    currentBitNumber--;
    if(currentBitNumber < 0)
    {
      currentBitNumber = 7;
      currentByteNumber++;
    }
    // теперь пишем информацию о концевике закрытия (второй бит)
    rs485Packet.data.windowsStatus[currentByteNumber] |= (isCloseSwitchTriggered << currentBitNumber);
    currentBitNumber--;
    if(currentBitNumber < 0)
    {
      currentBitNumber = 7;
      currentByteNumber++;
    }
    
    // теперь пишем информацию о концевике открытия (первый бит)
    rs485Packet.data.windowsStatus[currentByteNumber] |= (isOpenSwitchTriggered << currentBitNumber);
    currentBitNumber--;
    if(currentBitNumber < 0)
    {
      currentBitNumber = 7;
      currentByteNumber++;
    }
    
  } // for
}
//----------------------------------------------------------------------------------------------------------------
bool GotRS485Packet()
{
  // проверяем, есть ли у нас валидный RS-485 пакет
  return rs485WritePtr > ( sizeof(RS485Packet)-1 );
}
//----------------------------------------------------------------------------------------------------------------
void ProcessRS485Packet()
{

  // обрабатываем входящий пакет. Тут могут возникнуть проблемы с синхронизацией
  // начала пакета, поэтому мы сначала ищем заголовок и убеждаемся, что он валидный. 
  // если мы нашли заголовок и он не в начале пакета - значит, с синхронизацией проблемы,
  // и мы должны сдвинуть заголовок в начало пакета, чтобы потом дочитать остаток.
  if(!(rs485Packet.header1 == 0xAB && rs485Packet.header2 == 0xBA))
  {
     // заголовок неправильный, ищем возможное начало пакета
     byte readPtr = 0;
     bool startPacketFound = false;
     while(readPtr < sizeof(RS485Packet))
     {
       if(rsPacketPtr[readPtr] == 0xAB)
       {
        startPacketFound = true;
        break;
       }
        readPtr++;
     } // while

     if(!startPacketFound) // не нашли начало пакета
     {
        rs485WritePtr = 0; // сбрасываем указатель чтения и выходим
        return;
     }

     if(readPtr == 0)
     {
      // стартовый байт заголовка найден, но он в нулевой позиции, следовательно - что-то пошло не так
        rs485WritePtr = 0; // сбрасываем указатель чтения и выходим
        return;       
     } // if

     // начало пакета найдено, копируем всё, что после него, перемещая в начало буфера
     byte writePtr = 0;
     byte bytesWritten = 0;
     while(readPtr < sizeof(RS485Packet) )
     {
      rsPacketPtr[writePtr++] = rsPacketPtr[readPtr++];
      bytesWritten++;
     }

     rs485WritePtr = bytesWritten; // запоминаем, куда писать следующий байт
     return;
         
  } // if
  else
  {
    // заголовок правильный, проверяем окончание
    if(!(rs485Packet.tail1 == 0xDE && rs485Packet.tail2 == 0xAD))
    {
      // окончание неправильное, сбрасываем указатель чтения и выходим
      rs485WritePtr = 0;
      return;
    }
    // данные мы получили, сразу обнуляем указатель записи, чтобы не забыть
    rs485WritePtr = 0;

    // проверяем контрольную сумму
    byte crc = OneWireSlave::crc8((const byte*) rsPacketPtr,sizeof(RS485Packet) - 1);
    if(crc != rs485Packet.crc8)
    {
      // не сошлось, игнорируем
      return;
    }


    // всё в пакете правильно, анализируем и выполняем
    // проверяем, наш ли пакет
    if(rs485Packet.direction != RS485FromMaster) // не от мастера пакет
      return;

    if(rs485Packet.type != RS485WindowsPositionPacket) // пакет не c запросом показаний положения окон
      return;

    if(rs485Packet.data.moduleNumber != myAddress) // не для нашего адреса запрос
      return;

     /*
     // теперь приводим пакет к нужному виду
     byte* readPtr = rs485Packet.data;
     // в первом байте у нас идёт тип датчика для опроса
     byte sensorType =  *readPtr++;
     // во втором - индекс датчика в системе
     byte sensorIndex = *readPtr++;

     // теперь нам надо найти, есть ли у нас этот датчик
     sensor* sMatch = NULL;
     if(scratchpadS.sensor1.type == sensorType && scratchpadS.sensor1.index == sensorIndex)
        sMatch = &(scratchpadS.sensor1);

     if(!sMatch)
     {
     if(scratchpadS.sensor2.type == sensorType && scratchpadS.sensor2.index == sensorIndex)
        sMatch = &(scratchpadS.sensor2);        
     }

     if(!sMatch)
     {
     if(scratchpadS.sensor3.type == sensorType && scratchpadS.sensor3.index == sensorIndex)
        sMatch = &(scratchpadS.sensor3);        
     }

     if(!sMatch) {// не нашли у нас такого датчика

      return;
     }

     memcpy(readPtr,sMatch->data,4); // у нас 4 байта на показания, копируем их все
*/

     // тут заполняем пакет данными
     FillRS485PacketWithData();


     // выставляем нужное направление пакета
     rs485Packet.direction = RS485FromSlave;
     rs485Packet.type = RS485WindowsPositionPacket;
     rs485Packet.data.moduleNumber = myAddress;
     rs485Packet.data.windowsSupported = WINDOWS_SERVED;

     // подсчитываем CRC
     rs485Packet.crc8 = OneWireSlave::crc8((const byte*) &rs485Packet,sizeof(RS485Packet)-1 );

     // теперь переключаемся на передачу
     RS485Send();

     // пишем в порт данные
     Serial.write((const uint8_t *)&rs485Packet,sizeof(RS485Packet));

     // ждём окончания передачи
     RS485waitTransmitComplete();
     
    // переключаемся на приём
    RS485Receive();


    
  } // else
}
//----------------------------------------------------------------------------------------------------------------
void ProcessIncomingRS485Packets() // обрабатываем входящие пакеты по RS-485
{
  while(Serial.available())
  {
    rsPacketPtr[rs485WritePtr++] = (byte) Serial.read();
   
    if(GotRS485Packet())
      ProcessRS485Packet();
  } // while
  
}
//----------------------------------------------------------------------------------------------------------------
void RS485Receive()
{
  digitalWrite(RS485_DE_PIN,LOW); // переводим контроллер RS-485 на приём
}
//----------------------------------------------------------------------------------------------------------------
void RS485Send()
{
  digitalWrite(RS485_DE_PIN,HIGH); // переводим контроллер RS-485 на передачу
}
//----------------------------------------------------------------------------------------------------------------
void InitRS485()
{
  memset(&rs485Packet,0,sizeof(RS485Packet));
  pinMode(RS485_DE_PIN,OUTPUT);
  RS485Receive();
}
//----------------------------------------------------------------------------------------------------------------
void RS485waitTransmitComplete()
{
  // ждём завершения передачи по UART
  while(!(UCSR0A & _BV(TXC0) ));
}
//----------------------------------------------------------------------------------------------------------------
void setup()
{
    Serial.begin(RS485_SPEED);

    ReadModuleAddress(); // читаем адрес
    
    InitRS485(); // настраиваем RS-485 на приём
  
}
//----------------------------------------------------------------------------------------------------------------
void loop()
{

    ProcessIncomingRS485Packets(); // обрабатываем входящие пакеты по RS-485


}
//----------------------------------------------------------------------------------------------------------------
