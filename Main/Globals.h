#ifndef _GLOBALS_H
#define _GLOBALS_H

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// НАСТРОЙКИ ПРОШИВКИ
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------------------------------------------------------------------
// определяем типы плат
#define MEGA_BOARD 1 // Arduino Mega
#define DUE_BOARD 2 // Arduino Due
//--------------------------------------------------------------------------------------------------------------------------------
// типы подплат
#define MEGA_GENUINE  1 // обычная Arduino Mega
#define MEGA_MINI 2 // мини-вариант платы (датчики только через уничерсальные модули, без nRF и пр.)
//--------------------------------------------------------------------------------------------------------------------------------

// определяем, под какую плату сейчас компилируем
#define TARGET_BOARD MEGA_BOARD // по умолчанию - Arduino Mega, подставить вместо MEGA_BOARD значение DUE_BOARD, если компилируем под Due.

// определяем подвариант платы
#define BOARD_SUBVERSION MEGA_GENUINE // обычная мега, если используется мини-вариант - вместо MEGA_GENUINE поставить MEGA_MINI

// подключаем файлы настроек. 
// Настройки для Arduino Mega находятся в файле Configuraton_MEGA.h, 
// для Arduino Due - в файле Configuration_DUE.h.

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
#else
  #error "Unknown target board!"
#endif

// подключаем настройки отладочных режимов
#include "Configuration_DEBUG.h"

// подключаем общие для всех плат настройки
#include "Configuration_Shared.h"


#endif
