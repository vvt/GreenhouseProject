#include <avr/io.h>
#include <avr/interrupt.h>
#include "Common.h"
#include "PushButton.h"
#include "CorePinScenario.h"
//----------------------------------------------------------------------------------------------------------------
/*
Прошивка для модуля, предназначена для выноса
управления контроллером по RS-485.

Поддерживается до 7 кнопок управления, на каждую кнопку можно назначить свою команду для управления.

Прошивка тестировалась под Arduino Uno, но должна без особых помех
работать и под Pro Mini и пр. семейством на Atmega328.

КНОПКИ ДОЛЖНЫ БЫТЬ ПОДТЯНУТЫ К ПИТАНИЮ, И СРАБАТЫВАТЬ ПО НИЗКОМУ УРОВНЮ!

*/
//----------------------------------------------------------------------------------------------------------------
// настройки RS-485
//----------------------------------------------------------------------------------------------------------------
#define RS485_SPEED 57600 // скорость работы по RS-485
#define RS485_DE_PIN 4 // номер пина для управления переключением приём/передача RS485
//----------------------------------------------------------------------------------------------------------------
//#define _DEBUG // раскомментировать для отладочного режима (плюётся в Serial, RS485, ясное дело, в таком режиме не работает)
//----------------------------------------------------------------------------------------------------------------
// настройки кнопок и привязки их к командам
//----------------------------------------------------------------------------------------------------------------
BUTTON commands[7] = {

  // каждая запись содержит номер пина кнопки, имя привязанной команды, и дополнительные параметры, зависящие от команды.
  // ниже приведены примеры разных настроек:

  // нет привязки кнопки к команде
  // {0, emCommandNone, 0,0}

  // кнопка "Открыть все окна" на пине номер 7
  // {7, emCommandOpenWindows,0,0}

  // кнопка "Закрыть все окна" на пине номер 9
  // {9, emCommandCloseWindows,0,0}

  // кнопка "Открыть окно номер 2" (нумерация окон - с нуля)  на пине номер 9
  // {9, emCommandOpenWindow,1,0}

  // кнопка "Закрыть окно номер 2" (нумерация окон - с нуля)  на пине номер 9
  // {9, emCommandCloseWindow,1,0}

  // кнопка "Включить полив" на пине номер 7
  // {7, emCommandWaterOn,0,0}

  // кнопка "Выключить полив" на пине номер 7
  // {7, emCommandWaterOff,0,0}

  // кнопка "Включить канал полива номер 4" (нумерация каналов - с нуля)  на пине номер 9
  // {9, emCommandWaterChannelOn,3,0}

  // кнопка "Выключить канал полива номер 4" (нумерация каналов - с нуля)  на пине номер 9
  // {9, emCommandWaterChannelOff,3,0}

  // кнопка "Включить досветку" на пине номер 7
  // {7, emCommandLightOn,0,0}

  // кнопка "Выключить досветку" на пине номер 7
  // {7, emCommandLigntOff,0,0}

  // кнопка "Включить пин номер 13" на пине номер 7
  // {7, emCommandPinOn,13,0}

  // кнопка "Выключить пин номер 13" на пине номер 7
  // {7, emCommandPinOff,13,0}

  // далее идут настройки кнопок по умолчанию, поменяйте под ваши потребности
  {8, emCommandWaterOn, 0,0}, // включаем полив по нажатию кнопки на пине номер 8
  {0, emCommandNone, 0,0},
  {0, emCommandNone, 0,0},
  {0, emCommandNone, 0,0},
  {0, emCommandNone, 0,0},
  {0, emCommandNone, 0,0},
  {0, emCommandNone, 0,0},
  
};
//----------------------------------------------------------------------------------------------------------------
// Настройки информационного диода. Диод мигает после того, как подучено подтверждение от контроллера о выполнении команды.
//----------------------------------------------------------------------------------------------------------------
#define USE_INFO_DIODE // закомментировать, если не надо использовать информационный диод
#define INFO_DIODE_PIN 6 // номер пина информационного диода
#define INFO_DIODE_ON_LEVEL HIGH // уровень включения диода
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
RS485Packet rs485Packet; // пакет, в который мы принимаем данные
volatile byte* rsPacketPtr = (byte*) &rs485Packet;
volatile byte  rs485WritePtr = 0; // указатель записи в пакет
PushButton* buttons[7] = {0};
CommandToExecute currentState[7] = {0,0,0};
#ifdef USE_INFO_DIODE
  CorePinScenario blinker;
  unsigned long blinkerTimer = 0;
  bool anyCommandExistsToExecute = false;
#endif
//----------------------------------------------------------------------------------------------------------------
void RS485Receive()
{
   digitalWrite(RS485_DE_PIN,LOW);
  #ifdef _DEBUG
    Serial.println(F("Switch RS485 to receive."));
  #endif  
}
//----------------------------------------------------------------------------------------------------------------
void RS485Send()
{
  // переводим контроллер RS-485 на передачу
    digitalWrite(RS485_DE_PIN,HIGH);
    
  #ifdef _DEBUG
    Serial.println(F("Switch RS485 to send."));
  #endif  
}
//----------------------------------------------------------------------------------------------------------------
void RS485waitTransmitComplete()
{
  // ждём завершения передачи по UART
  while(!(UCSR0A & _BV(TXC0) ));
}
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
bool GotRS485Packet()
{
  // проверяем, есть ли у нас валидный RS-485 пакет
  return rs485WritePtr > ( sizeof(RS485Packet)-1 );
}
//----------------------------------------------------------------------------------------------------------------
byte crc8(const byte *addr, byte len)
{
  byte crc = 0;
  while (len--) 
    {
    byte inbyte = *addr++;
    for (byte i = 8; i; i--)
      {
      byte mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) 
        crc ^= 0x8C;
      inbyte >>= 1;
      }  // end of for
    }  // end of while
  return crc;  
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
void SendCurrentCommandsState(RS485Packet& packet)
{
  packet.direction = RS485FromSlave;
  packet.type = RS485RequestCommandsPacket;
  CommandsToExecutePacket* ce = (CommandsToExecutePacket*) &(packet.data);
  memcpy(ce->commands,&currentState,sizeof(currentState));

  packet.crc8 = crc8((const byte*)&packet,sizeof(packet)-1);

  RS485Send();

  Serial.write((byte*) &packet,sizeof(packet));

  RS485waitTransmitComplete();

  RS485Receive();
  
}
//----------------------------------------------------------------------------------------------------------------
void ProcessReceiptPacket(const RS485Packet& packet)
{
  UNUSED(packet);
  
    // тут пока просто обнуляем текущее состояние для отсыла, без анализа на ID модуля
    memset(currentState,0,sizeof(currentState));  

  // сообщаем пользователю, что команда отработана контроллером
  #ifdef USE_INFO_DIODE

     if(anyCommandExistsToExecute)
     {
      anyCommandExistsToExecute = false;
      blinkerTimer = millis();
      blinker.reset();
      blinker.enable();
     }
      
  #endif    
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
    byte crc = crc8((const byte*) rsPacketPtr,sizeof(RS485Packet) - 1);
    if(crc != rs485Packet.crc8)
    {
      // не сошлось, игнорируем
      return;
    }


    // всё в пакете правильно, анализируем и выполняем
    // проверяем, наш ли пакет
    if(rs485Packet.direction != RS485FromMaster) // не от мастера пакет
      return;

    if(!(rs485Packet.type == RS485RequestCommandsPacket 
    || rs485Packet.type == RS485CommandsToExecuteReceipt
    
    )) // пакет не наш
      return;


      if(rs485Packet.type == RS485RequestCommandsPacket)
      {
       SendCurrentCommandsState(rs485Packet);
      }
      else if(rs485Packet.type == RS485CommandsToExecuteReceipt)
      {
        ProcessReceiptPacket(rs485Packet);
      }


    
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
void InitRS485()
{
  memset(&rs485Packet,0,sizeof(RS485Packet));

  #ifdef _DEBUG
    Serial.println(F("Init RS485 DE pin..."));
  #endif

    pinMode(RS485_DE_PIN,OUTPUT);
    
  RS485Receive();

}
//----------------------------------------------------------------------------------------------------------------
void setupButtons()
{
  #ifdef _DEBUG
    Serial.println(F("Setup buttons..."));
  #endif
    
  memset(currentState,0,sizeof(currentState));
  
  for(int i=0;i<7;i++)
  {
    currentState[i].whichCommand = emCommandNone;
     
    buttons[i] = NULL;
    if(commands[i].whichCommand != emCommandNone && commands[i].pin > 0)
    {
      #ifdef _DEBUG
        Serial.print(F("Setup button  on pin #"));
        Serial.println(commands[i].pin);
      #endif      
      buttons[i] = new PushButton(commands[i].pin);
      buttons[i]->init();
    }
  }

  #ifdef _DEBUG
    Serial.println(F("Setup buttons done."));
  #endif
    
}
//----------------------------------------------------------------------------------------------------------------
void updateButtons()
{

            
  for(int i=0;i<7;i++)
  {
    if(!buttons[i])
      continue;

    buttons[i]->update();

    if(buttons[i]->isClicked())
    {
      #ifdef _DEBUG
        Serial.print(F("Button #"));
        Serial.print(i);
        Serial.println(F(" clicked, save command!"));
      #endif

      #ifdef USE_INFO_DIODE
        anyCommandExistsToExecute = true;
      #endif

      currentState[i].whichCommand = commands[i].whichCommand;
      currentState[i].param1 = commands[i].param1;
      currentState[i].param2 = commands[i].param2;    
        
    } // if
  } // for


}
//----------------------------------------------------------------------------------------------------------------
void setup()
{
 
  Serial.begin(RS485_SPEED);

 #ifdef _DEBUG
  Serial.println(F("Debug mode..."));
 #endif

 setupButtons();
  

  InitRS485(); // настраиваем RS-485 на приём

  #ifdef USE_INFO_DIODE
  blinker.add({INFO_DIODE_PIN,INFO_DIODE_ON_LEVEL,100});
  blinker.add({INFO_DIODE_PIN,!INFO_DIODE_ON_LEVEL,100});
  blinker.add({INFO_DIODE_PIN,INFO_DIODE_ON_LEVEL,100});
  blinker.add({INFO_DIODE_PIN,!INFO_DIODE_ON_LEVEL,100});
  blinker.add({INFO_DIODE_PIN,INFO_DIODE_ON_LEVEL,500});
  blinker.add({INFO_DIODE_PIN,!INFO_DIODE_ON_LEVEL,1000});
  blinker.disable();
  pinMode(INFO_DIODE_PIN,OUTPUT);
  digitalWrite(INFO_DIODE_PIN,!INFO_DIODE_ON_LEVEL);
  #endif
  
  
}
//----------------------------------------------------------------------------------------------------------------
void loop()
{
    ProcessIncomingRS485Packets(); // обрабатываем входящие пакеты по RS-485

    updateButtons();

    #ifdef USE_INFO_DIODE
      if(blinker.enabled())
      {
        blinker.update();
        unsigned long now = millis();
        if(now - blinkerTimer > 1900)
        {
          blinker.disable();
          digitalWrite(INFO_DIODE_PIN,!INFO_DIODE_ON_LEVEL);
        }
      }
    #endif
}
//----------------------------------------------------------------------------------------------------------------
