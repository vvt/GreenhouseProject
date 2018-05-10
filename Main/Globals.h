#ifndef _GLOBALS_H
#define _GLOBALS_H

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// НАСТРОЙКИ ПРОШИВКИ
// FIRMWARE SETTINGS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------------------------------------------------------------------
// определяем типы плат
// board types definition

// Arduino Mega
#define MEGA_BOARD 1

// Arduino Due, настройки в файле Configuration_DUE.h
// Arduino Due, settings located in Configuration_DUE.h
#define DUE_BOARD 2

// STM32F1 support, настройки в файле Configuration_STM32.h
// STM32F1 support, settings located in Configuration_STM32.h
#define STM32_BOARD 3
//--------------------------------------------------------------------------------------------------------------------------------
// типы подплат
// board subtypes definition

// обычная Arduino Mega, настройки в файле Configuration_MEGA.h
// Genuine Arduino Mega, settings located in Configuration_MEGA.h
#define MEGA_GENUINE  1

// мини-вариант платы (датчики только через универсальные модули, без nRF и пр.), настройки в файле Configuration_MEGA_MiniBoard.h
// DIY mini-board (sensors trough universal modules, no nRF and so on), settings located in Configuration_MEGA_MiniBoard.h
#define MEGA_MINI 2
//--------------------------------------------------------------------------------------------------------------------------------
// определяем, под какую плату сейчас компилируем
// автовыбор платы
//--------------------------------------------------------------------------------------------------------------------------------
#if defined(__AVR_ATmega2560__)
  #define TARGET_BOARD MEGA_BOARD
#elif defined (__arm__) && defined (__SAM3X8E__) // Arduino Due compatible
  #define TARGET_BOARD DUE_BOARD
#elif defined (__STM32F1__)
  #define TARGET_BOARD STM32_BOARD
#else
  #error "Unknown target board!"
#endif
//--------------------------------------------------------------------------------------------------------------------------------

// определяем подвариант платы
// обычная мега, если используется мини-вариант - вместо MEGA_GENUINE поставить MEGA_MINI

// what board subtype we are using?
// Genuine Mega by default, if you are using DIY mini-board - change MEGA_GENUINE to MEGA_MINI
#define BOARD_SUBVERSION MEGA_GENUINE 

//--------------------------------------------------------------------------------------------------------------------------------
// подключаем файлы настроек. 
// Настройки для Arduino Mega находятся в файле Configuraton_MEGA.h и Configuration_MEGA_MiniBoard.h, 
// для Arduino Due - в файле Configuration_DUE.h.

// include settings file. 
// Settings for Arduino Mega are located in Configuraton_MEGA.h and Configuration_MEGA_MiniBoard.h, 
// For Arduino Due - in Configuration_DUE.h.

#if (TARGET_BOARD == MEGA_BOARD)

  #if (BOARD_SUBVERSION == MEGA_GENUINE)
    #include "Configuration_MEGA.h"
  #elif (BOARD_SUBVERSION == MEGA_MINI)
    #include "Configuration_MEGA_MiniBoard.h"
  #else
    #error "Unknown board subversion!"
  #endif
  
#elif (TARGET_BOARD == DUE_BOARD)
  #include "Configuration_DUE.h"
  
#elif (TARGET_BOARD == STM32_BOARD)
  #include "Configuration_STM32.h"  
#else
  #error "Unknown target board!"
#endif

//--------------------------------------------------------------------------------------------------------------------------------
// подключаем настройки отладочных режимов
// include debug modes (dump to Serial)
#include "Configuration_DEBUG.h"

//--------------------------------------------------------------------------------------------------------------------------------
// подключаем общие для всех плат настройки
// include shared settings
#include "Configuration_Shared.h"

//--------------------------------------------------------------------------------------------------------------------------------

#endif
