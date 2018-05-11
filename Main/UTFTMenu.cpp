#include "UTFTMenu.h"
#include "AbstractModule.h"
#include "ModuleController.h"
#include "TempSensors.h"
#include "InteropStream.h"

#ifdef USE_TFT_MODULE

#ifdef USE_BUZZER_ON_TOUCH
#include "Buzzer.h"
#endif
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTMenu* tftMenuManager;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void BuzzerOn(int btn)
{
  if(btn != -1)
  {
    #ifdef USE_BUZZER_ON_TOUCH
    Buzzer.buzz();
    #endif
  }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawButtonsYield() // вызывается после отрисовки каждой кнопки
{
  yield();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// This code block is only needed to support multiple
// MCU architectures in a single sketch.
#if defined(__AVR__)
  #define imagedatatype  unsigned int
#elif defined(__PIC32MX__)
  #define imagedatatype  unsigned short
#elif defined(__arm__)
  #define imagedatatype  unsigned short
#endif
// End of multi-architecture block
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#if TFT_SENSOR_BOXES_COUNT > 0
TFTSensorInfo TFTSensors [TFT_SENSOR_BOXES_COUNT] = { TFT_SENSORS };
#endif
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTInfoBox
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTInfoBox::TFTInfoBox(const char* caption, int width, int height, int x, int y, int cxo)
{
  boxCaption = caption;
  boxWidth = width;
  boxHeight = height;
  posX = x;
  posY = y;
  captionXOffset = cxo;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTInfoBox::~TFTInfoBox()
{
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTInfoBox::drawCaption(TFTMenu* menuManager, const char* caption)
{
  UTFT* dc = menuManager->getDC();
  dc->setBackColor(TFT_BACK_COLOR);
  dc->setColor(INFO_BOX_CAPTION_COLOR);
  menuManager->getRusPrinter()->print(caption,posX+captionXOffset,posY);
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTInfoBox::draw(TFTMenu* menuManager)
{
  drawCaption(menuManager,boxCaption);
  
  int curTop = posY;

  UTFT* dc = menuManager->getDC();
  int fontHeight = dc->getFontYsize();
  
  curTop += fontHeight + INFO_BOX_CONTENT_PADDING;

  dc->setColor(INFO_BOX_BACK_COLOR);
  dc->fillRoundRect(posX, curTop, posX+boxWidth, curTop + (boxHeight - fontHeight - INFO_BOX_CONTENT_PADDING));

  yield();

  dc->setColor(INFO_BOX_BORDER_COLOR);
  dc->drawRoundRect(posX, curTop, posX+boxWidth, curTop + (boxHeight - fontHeight - INFO_BOX_CONTENT_PADDING));

  yield();
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTInfoBoxContentRect TFTInfoBox::getContentRect(TFTMenu* menuManager)
{
    TFTInfoBoxContentRect result;
    UTFT* dc = menuManager->getDC();

    int fontHeight = dc->getFontYsize();

    result.x = posX + INFO_BOX_CONTENT_PADDING;
    result.y = posY + fontHeight + INFO_BOX_CONTENT_PADDING*2;

    result.w = boxWidth - INFO_BOX_CONTENT_PADDING*2;
    result.h = boxHeight - (fontHeight + INFO_BOX_CONTENT_PADDING*3);

    return result;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern imagedatatype tft_back_button[];
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int addBackButton(TFTMenu* menuManager,UTFT_Buttons_Rus* buttons,int leftOffset)
{
  int buttonsTop = menuManager->getDC()->getDisplayYSize() - TFT_IDLE_SCREEN_BUTTON_HEIGHT - 20; // координата Y для кнопки "Назад"
  int screenWidth = menuManager->getDC()->getDisplayXSize();

  // если передан 0 как leftOffset - позиционируемся по центру экрана, иначе - как запросили. Y-координата при этом остаётся нетронутой, т.к. у нас фиксированное место для размещения кнопок управления
  int curButtonLeft = leftOffset ? leftOffset : (screenWidth - TFT_IDLE_SCREEN_BUTTON_WIDTH )/2;

 return buttons->addButton( curButtonLeft ,  buttonsTop, TFT_IDLE_SCREEN_BUTTON_WIDTH,  TFT_IDLE_SCREEN_BUTTON_HEIGHT, tft_back_button ,BUTTON_BITMAP | BUTTON_NO_BORDER);
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractTFTScreen::AbstractTFTScreen()
{
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractTFTScreen::~AbstractTFTScreen()
{
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_WATERING_MODULE
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTWateringScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTWateringScreen::TFTWateringScreen()
{
  inited = false;
  lastWaterChannelsState = 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTWateringScreen::~TFTWateringScreen()
{
 delete screenButtons;
 for(size_t i=0;i<labels.size();i++)
 {
  delete labels[i];
 }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTWateringScreen::setup(TFTMenu* menuManager)
{

  //TODO: Тут создаём наши контролы

  #if WATER_RELAYS_COUNT > 0
  
    screenButtons = new UTFT_Buttons_Rus(menuManager->getDC(), menuManager->getTouch(),menuManager->getRusPrinter());
    screenButtons->setTextFont(BigRusFont);
    screenButtons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);

    // первая - кнопка назад
    backButton = addBackButton(menuManager,screenButtons,0);
 
    int buttonsTop = INFO_BOX_V_SPACING;
    int screenWidth = menuManager->getDC()->getDisplayXSize();

    // добавляем кнопки для управления всеми каналами
    int allChannelsLeft = (screenWidth - (ALL_CHANNELS_BUTTON_WIDTH*3) - INFO_BOX_V_SPACING*2)/2;
    screenButtons->addButton( allChannelsLeft ,  buttonsTop, ALL_CHANNELS_BUTTON_WIDTH,  ALL_CHANNELS_BUTTON_HEIGHT, TURN_ON_ALL_WATER_LABEL);
    allChannelsLeft += ALL_CHANNELS_BUTTON_WIDTH + INFO_BOX_V_SPACING;

    screenButtons->addButton( allChannelsLeft ,  buttonsTop, ALL_CHANNELS_BUTTON_WIDTH,  ALL_CHANNELS_BUTTON_HEIGHT, TURN_OFF_ALL_WATER_LABEL);
    allChannelsLeft += ALL_CHANNELS_BUTTON_WIDTH + INFO_BOX_V_SPACING;

    int addedId = screenButtons->addButton( allChannelsLeft ,  buttonsTop, ALL_CHANNELS_BUTTON_WIDTH,  ALL_CHANNELS_BUTTON_HEIGHT, AUTO_MODE_LABEL);
    screenButtons->setButtonFontColor(addedId,CHANNELS_BUTTONS_TEXT_COLOR);

    buttonsTop += ALL_CHANNELS_BUTTON_HEIGHT + INFO_BOX_V_SPACING;

    int computedButtonLeft = (screenWidth - (CHANNELS_BUTTON_WIDTH*CHANNELS_BUTTONS_PER_LINE) - ((CHANNELS_BUTTONS_PER_LINE-1)*INFO_BOX_V_SPACING))/2;
    int curButtonLeft = computedButtonLeft;
  
    // теперь проходимся по кол-ву каналов и добавляем наши кнопки - дя каждого канала - по кнопке
    for(int i=0;i<WATER_RELAYS_COUNT;i++)
    {
       if( i > 0 && !(i%CHANNELS_BUTTONS_PER_LINE))
       {
        buttonsTop += CHANNELS_BUTTON_HEIGHT + INFO_BOX_V_SPACING;
        curButtonLeft = computedButtonLeft;
       }
       
       String* label = new String('#');
       *label += (i+1);
       labels.push_back(label);
       
       addedId = screenButtons->addButton(curButtonLeft ,  buttonsTop, CHANNELS_BUTTON_WIDTH,  CHANNELS_BUTTON_HEIGHT, label->c_str());
       screenButtons->setButtonFontColor(addedId,CHANNELS_BUTTONS_TEXT_COLOR);
       
       curButtonLeft += CHANNELS_BUTTON_WIDTH + INFO_BOX_V_SPACING;


    } // for
    
  
    #endif // WATER_RELAYS_COUNT > 0
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTWateringScreen::update(TFTMenu* menuManager,uint16_t dt)
{
 UNUSED(dt);

 #if WATER_RELAYS_COUNT > 0

  byte BUTTONS_OFFSET = 4; // с какого индекса начинаются наши кнопки
 
 if(screenButtons)
 {
    int pressed_button = screenButtons->checkButtons(BuzzerOn);
  
    if(pressed_button == backButton)
    {
      menuManager->switchToScreen("IDLE");
      return;
    }

    if(pressed_button == 3)
    {
      // Кнопка смены режима
      bool waterAutoMode = WORK_STATUS.GetStatus(WATER_MODE_BIT);
      waterAutoMode = !waterAutoMode;
      String command = waterAutoMode ? F("WATER|MODE|AUTO") : F("WATER|MODE|MANUAL");
      yield();
      ModuleInterop.QueryCommand(ctSET,command,false);
      yield();

      menuManager->resetIdleTimer();

      return;
    }

    if(pressed_button == 1)
    {
      // включить все каналы
      ModuleInterop.QueryCommand(ctSET,F("WATER|ON"),false);
      menuManager->resetIdleTimer();
      yield();
      return;
    }
    
    if(pressed_button == 2)
    {
      // выключить все каналы
      ModuleInterop.QueryCommand(ctSET,F("WATER|OFF"),false);
      menuManager->resetIdleTimer();
      yield();
      return;
    }

    if(pressed_button > 3)
    {
      // кнопки управления каналами
      int channelNum = pressed_button - BUTTONS_OFFSET;

      ControllerState state = WORK_STATUS.GetState();

      bool isWaterOn = state.WaterChannelsState & (1 << channelNum);
      String command = F("WATER|");

      command += isWaterOn ? F("OFF|") : F("ON|");
      command += channelNum;
      
      ModuleInterop.QueryCommand(ctSET,command,false);
      menuManager->resetIdleTimer();
      yield();

      return;
    }

    
    // тут нам надо обновить состояние кнопок для каналов
    
     ControllerState state = WORK_STATUS.GetState();

     // проходимся по всем каналам
     for(int i=0;i<WATER_RELAYS_COUNT;i++)
     {
       int buttonID = i+BUTTONS_OFFSET; // у нас первые 4 кнопки - не для каналов

       bool isChannelActive = (state.WaterChannelsState & (1 << i));
       bool savedIsChannelActive =  (lastWaterChannelsState & (1 << i));
       
       bool wantRedrawChannel = !inited || (isChannelActive != savedIsChannelActive);

        if(wantRedrawChannel)
        {
            if(isChannelActive)
            {
               screenButtons->setButtonBackColor(buttonID,MODE_ON_COLOR);
            }
            else
            {
              screenButtons->setButtonBackColor(buttonID,MODE_OFF_COLOR);
            }

          screenButtons->drawButton(buttonID);
        }
       
     } // for


     bool wateringAutoMode = WORK_STATUS.GetStatus(WATER_MODE_BIT);
     if(wateringAutoMode)
     {
        screenButtons->setButtonBackColor(3,MODE_ON_COLOR);
        screenButtons->relabelButton(3,AUTO_MODE_LABEL,!inited || (wateringAutoMode != lastWateringAutoMode));
     }
     else
     {
        screenButtons->setButtonBackColor(3,MODE_OFF_COLOR);      
        screenButtons->relabelButton(3,MANUAL_MODE_LABEL,!inited || (wateringAutoMode != lastWateringAutoMode));
     }

      // сохраняем состояние каналов полива
     lastWaterChannelsState = state.WaterChannelsState;
     lastWateringAutoMode = wateringAutoMode;
     inited = true;
     
 } // if(screenButtons)
 
 #endif // WATER_RELAYS_COUNT > 0
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTWateringScreen::draw(TFTMenu* menuManager)
{
  UNUSED(menuManager);

 #if WATER_RELAYS_COUNT > 0
  if(screenButtons)
  {
    screenButtons->drawButtons(drawButtonsYield);
  }
 #endif 

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_WATERING_MODULE
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_LUMINOSITY_MODULE
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTLightScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTLightScreen::TFTLightScreen()
{
  inited = false;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTLightScreen::~TFTLightScreen()
{
 delete screenButtons;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTLightScreen::setup(TFTMenu* menuManager)
{

  
    screenButtons = new UTFT_Buttons_Rus(menuManager->getDC(), menuManager->getTouch(),menuManager->getRusPrinter());
    screenButtons->setTextFont(BigRusFont);
    screenButtons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);

    // первая - кнопка назад
    backButton = addBackButton(menuManager,screenButtons,0);
 
    int screenWidth = menuManager->getDC()->getDisplayXSize();
    int screenHeight = menuManager->getDC()->getDisplayYSize();

    int buttonsTop = (screenHeight - ALL_CHANNELS_BUTTON_HEIGHT)/2;//INFO_BOX_V_SPACING;

    // добавляем кнопки для управления досветкой
    bool isLightOn = WORK_STATUS.GetStatus(LIGHT_STATUS_BIT);
    
    int allChannelsLeft = (screenWidth - (ALL_CHANNELS_BUTTON_WIDTH*3) - INFO_BOX_V_SPACING*2)/2;
    int addedId = screenButtons->addButton( allChannelsLeft ,  buttonsTop, ALL_CHANNELS_BUTTON_WIDTH,  ALL_CHANNELS_BUTTON_HEIGHT, TURN_ON_ALL_LIGHT_LABEL);
    screenButtons->setButtonBackColor(addedId,isLightOn ? MODE_ON_COLOR : CHANNELS_BUTTONS_BG_COLOR);
    allChannelsLeft += ALL_CHANNELS_BUTTON_WIDTH + INFO_BOX_V_SPACING;

    addedId = screenButtons->addButton( allChannelsLeft ,  buttonsTop, ALL_CHANNELS_BUTTON_WIDTH,  ALL_CHANNELS_BUTTON_HEIGHT, TURN_OFF_ALL_LIGHT_LABEL);
    screenButtons->setButtonBackColor(addedId,!isLightOn ? MODE_ON_COLOR : CHANNELS_BUTTONS_BG_COLOR);

    allChannelsLeft += ALL_CHANNELS_BUTTON_WIDTH + INFO_BOX_V_SPACING;

    addedId = screenButtons->addButton( allChannelsLeft ,  buttonsTop, ALL_CHANNELS_BUTTON_WIDTH,  ALL_CHANNELS_BUTTON_HEIGHT, AUTO_MODE_LABEL);
    screenButtons->setButtonFontColor(addedId,CHANNELS_BUTTONS_TEXT_COLOR);
    
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTLightScreen::update(TFTMenu* menuManager,uint16_t dt)
{
 UNUSED(dt);

 
 if(screenButtons)
 {
    int pressed_button = screenButtons->checkButtons(BuzzerOn);
   
    if(pressed_button == backButton)
    {
      menuManager->switchToScreen("IDLE");
      return;
    }

    if(pressed_button == 3)
    {
      // Кнопка смены режима
      bool lightAutoMode = WORK_STATUS.GetStatus(LIGHT_MODE_BIT);
      lightAutoMode = !lightAutoMode;
      String command = lightAutoMode ? F("LIGHT|MODE|AUTO") : F("LIGHT|MODE|MANUAL");
      ModuleInterop.QueryCommand(ctSET,command,false);
      menuManager->resetIdleTimer();
      yield();

      return;
    }

    if(pressed_button == 1)
    {
      // включить досветку
      ModuleInterop.QueryCommand(ctSET,F("LIGHT|ON"),false);
      menuManager->resetIdleTimer();
      yield();
      return;
    }
    
    if(pressed_button == 2)
    {
      // выключить досветку
      ModuleInterop.QueryCommand(ctSET,F("LIGHT|OFF"),false);
      menuManager->resetIdleTimer();
      yield();
      return;
    }

    bool lightIsOn = WORK_STATUS.GetStatus(LIGHT_STATUS_BIT);
    bool lightAutoMode = WORK_STATUS.GetStatus(LIGHT_MODE_BIT);

    if(!inited || (lastLightIsOn != lightIsOn))
    {
        screenButtons->setButtonBackColor(1,lightIsOn ? MODE_ON_COLOR : CHANNELS_BUTTONS_BG_COLOR);
        screenButtons->setButtonBackColor(2,!lightIsOn ? MODE_ON_COLOR : CHANNELS_BUTTONS_BG_COLOR);

        screenButtons->setButtonFontColor(1,lightIsOn ? CHANNELS_BUTTONS_TEXT_COLOR : CHANNEL_BUTTONS_TEXT_COLOR);
        screenButtons->setButtonFontColor(2,!lightIsOn ? CHANNELS_BUTTONS_TEXT_COLOR : CHANNEL_BUTTONS_TEXT_COLOR);
        
        screenButtons->drawButton(1);
        screenButtons->drawButton(2);
    }
     
     if(lightAutoMode)
     {
        screenButtons->setButtonBackColor(3,MODE_ON_COLOR);
        screenButtons->relabelButton(3,AUTO_MODE_LABEL,!inited || (lightAutoMode != lastLightAutoMode));
     }
     else
     {
        screenButtons->setButtonBackColor(3,MODE_OFF_COLOR);      
        screenButtons->relabelButton(3,MANUAL_MODE_LABEL,!inited || (lightAutoMode != lastLightAutoMode));
     }

      // сохраняем состояние досветки
     lastLightIsOn = lightIsOn;
     lastLightAutoMode = lightAutoMode;
     inited = true; 
     
 } // if(screenButtons)
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTLightScreen::draw(TFTMenu* menuManager)
{
  UNUSED(menuManager);

  if(screenButtons)
  {
    screenButtons->drawButtons(drawButtonsYield);
  }

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_LUMINOSITY_MODULE
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_TEMP_SENSORS
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTWindowScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTWindowScreen::TFTWindowScreen()
{
  inited = false;
  lastWindowsState = 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTWindowScreen::~TFTWindowScreen()
{
 delete screenButtons;
 for(size_t i=0;i<labels.size();i++)
 {
  delete labels[i];
 }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTWindowScreen::setup(TFTMenu* menuManager)
{

  //TODO: Тут создаём наши контролы

  #if SUPPORTED_WINDOWS > 0
  
    screenButtons = new UTFT_Buttons_Rus(menuManager->getDC(), menuManager->getTouch(),menuManager->getRusPrinter());
    screenButtons->setTextFont(BigRusFont);
    screenButtons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);

    // первая - кнопка назад
    backButton = addBackButton(menuManager,screenButtons,0);
 
    int buttonsTop = INFO_BOX_V_SPACING;
    int screenWidth = menuManager->getDC()->getDisplayXSize();

    // добавляем кнопки для управления всеми каналами
    int allChannelsLeft = (screenWidth - (ALL_CHANNELS_BUTTON_WIDTH*3) - INFO_BOX_V_SPACING*2)/2;
    screenButtons->addButton( allChannelsLeft ,  buttonsTop, ALL_CHANNELS_BUTTON_WIDTH,  ALL_CHANNELS_BUTTON_HEIGHT, OPEN_ALL_LABEL);
    allChannelsLeft += ALL_CHANNELS_BUTTON_WIDTH + INFO_BOX_V_SPACING;

    screenButtons->addButton( allChannelsLeft ,  buttonsTop, ALL_CHANNELS_BUTTON_WIDTH,  ALL_CHANNELS_BUTTON_HEIGHT, CLOSE_ALL_LABEL);
    allChannelsLeft += ALL_CHANNELS_BUTTON_WIDTH + INFO_BOX_V_SPACING;

    int addedId = screenButtons->addButton( allChannelsLeft ,  buttonsTop, ALL_CHANNELS_BUTTON_WIDTH,  ALL_CHANNELS_BUTTON_HEIGHT, AUTO_MODE_LABEL);
    screenButtons->setButtonFontColor(addedId,CHANNELS_BUTTONS_TEXT_COLOR);

    buttonsTop += ALL_CHANNELS_BUTTON_HEIGHT + INFO_BOX_V_SPACING;

    int computedButtonLeft = (screenWidth - (CHANNELS_BUTTON_WIDTH*CHANNELS_BUTTONS_PER_LINE) - ((CHANNELS_BUTTONS_PER_LINE-1)*INFO_BOX_V_SPACING))/2;
    int curButtonLeft = computedButtonLeft;
  
    // теперь проходимся по кол-ву каналов и добавляем наши кнопки - дя каждого канала - по кнопке
    for(int i=0;i<SUPPORTED_WINDOWS;i++)
    {
       if( i > 0 && !(i%CHANNELS_BUTTONS_PER_LINE))
       {
        buttonsTop += CHANNELS_BUTTON_HEIGHT + INFO_BOX_V_SPACING;
        curButtonLeft = computedButtonLeft;
       }
       
       String* label = new String('#');
       *label += (i+1);
       labels.push_back(label);
       
       addedId = screenButtons->addButton(curButtonLeft ,  buttonsTop, CHANNELS_BUTTON_WIDTH,  CHANNELS_BUTTON_HEIGHT, label->c_str());
       screenButtons->setButtonFontColor(addedId,CHANNELS_BUTTONS_TEXT_COLOR);
       
       curButtonLeft += CHANNELS_BUTTON_WIDTH + INFO_BOX_V_SPACING;


    } // for
    
  
    #endif // SUPPORTED_WINDOWS > 0
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTWindowScreen::update(TFTMenu* menuManager,uint16_t dt)
{
 UNUSED(dt);

 #if SUPPORTED_WINDOWS > 0

  byte BUTTONS_OFFSET = 4; // с какого индекса начинаются наши кнопки
 
 if(screenButtons)
 {
    int pressed_button = screenButtons->checkButtons(BuzzerOn);
 
    if(pressed_button == backButton)
    {
      menuManager->switchToScreen("IDLE");
      return;
    }

    if(pressed_button == 3)
    {
      // Кнопка смены режима
      bool windowsAutoMode = WORK_STATUS.GetStatus(WINDOWS_MODE_BIT);
      windowsAutoMode = !windowsAutoMode;
      String command = windowsAutoMode ? F("STATE|MODE|AUTO") : F("STATE|MODE|MANUAL");
      ModuleInterop.QueryCommand(ctSET,command,false);
      menuManager->resetIdleTimer();
      yield();

      return;
    }

    if(pressed_button == 1)
    {
      // открыть все окна
      ModuleInterop.QueryCommand(ctSET,F("STATE|WINDOW|ALL|OPEN"),false);
      menuManager->resetIdleTimer();
      yield();
      return;
    }
    
    if(pressed_button == 2)
    {
      // закрыть все окна
      ModuleInterop.QueryCommand(ctSET,F("STATE|WINDOW|ALL|CLOSE"),false);
      menuManager->resetIdleTimer();
      yield();
      return;
    }

    if(pressed_button > 3)
    {
      // кнопки управления каналами
      int channelNum = pressed_button - BUTTONS_OFFSET;

      bool isWindowOpen = WindowModule->IsWindowOpen(channelNum);
      String command = F("STATE|WINDOW|");
      command += channelNum;
      command += '|';

      command += isWindowOpen ? F("CLOSE") : F("OPEN");
      ModuleInterop.QueryCommand(ctSET,command,false);
      menuManager->resetIdleTimer();
      yield();

      return;
    }
    // тут нам надо обновить состояние кнопок для каналов.
    // если канал окна в движении - надо кнопку выключать,
    // иначе - в зависимости от состояния канала
    
     ControllerState state = WORK_STATUS.GetState();

     // проходимся по всем каналам
     bool anyChannelActive = false;

     for(int i=0, channelNum=0;i<SUPPORTED_WINDOWS;i++, channelNum+=2)
     {
       int buttonID = i+BUTTONS_OFFSET; // у нас первые 4 кнопки - не для каналов
       
       // если канал включен - в бите единичка, иначе - нолик. оба выключены - окно не движется
       bool firstChannelBit = (state.WindowsState & (1 << channelNum));
       bool secondChannelBit = (state.WindowsState & (1 << (channelNum+1)));

       bool savedFirstChannelBit =  (lastWindowsState & (1 << channelNum));
       bool savedSecondChannelBit = (lastWindowsState & (1 << (channelNum+1)));
       
       bool isChannelOnIdle = !firstChannelBit && !secondChannelBit;


       // теперь смотрим, в какую сторону движется окно
       // если оно открывается - первый бит канала единичка, второй - нолик
       // если закрывается - наоборот
       // тут выяснение необходимости перерисовать кнопку. 
       // перерисовываем только тогда, когда состояние сменилось
       // или мы ещё не сохранили у себя первое состояние
       bool wantRedrawChannel = !inited || (firstChannelBit != savedFirstChannelBit || secondChannelBit != savedSecondChannelBit);

       if(isChannelOnIdle)
       {
          // смотрим - окно открыто или закрыто?
          bool isWindowOpen = WindowModule->IsWindowOpen(i);
          
          if(isWindowOpen)
          {
             screenButtons->setButtonBackColor(i+BUTTONS_OFFSET,MODE_ON_COLOR);
          }
          else
          {
            screenButtons->setButtonBackColor(i+BUTTONS_OFFSET,MODE_OFF_COLOR);
          }
          // включаем кнопку
          screenButtons->enableButton(buttonID, wantRedrawChannel);
       }
       else
       {
          anyChannelActive = true;
         // окно движется, блокируем кнопку
         screenButtons->disableButton(buttonID, wantRedrawChannel);
       }
       
     } // for

     // теперь проверяем, надо ли вкл/выкл кнопки управления всеми окнами
     bool wantRedrawAllChannelsButtons = !inited || (lastAnyChannelActive != anyChannelActive);

     if(anyChannelActive)
     {
      screenButtons->disableButton(1, wantRedrawAllChannelsButtons);
      screenButtons->disableButton(2, wantRedrawAllChannelsButtons);
     }
     else
     {
      screenButtons->enableButton(1, wantRedrawAllChannelsButtons);
      screenButtons->enableButton(2, wantRedrawAllChannelsButtons);      
     }

     bool windowsAutoMode = WORK_STATUS.GetStatus(WINDOWS_MODE_BIT);
     if(windowsAutoMode)
     {
        screenButtons->setButtonBackColor(3,MODE_ON_COLOR);
        screenButtons->relabelButton(3,AUTO_MODE_LABEL,!inited || (windowsAutoMode != lastWindowsAutoMode));
     }
     else
     {
        screenButtons->setButtonBackColor(3,MODE_OFF_COLOR);      
        screenButtons->relabelButton(3,MANUAL_MODE_LABEL,!inited || (windowsAutoMode != lastWindowsAutoMode));
     }

      // сохраняем состояние окон
     lastWindowsState = state.WindowsState;
     lastAnyChannelActive = anyChannelActive;
     lastWindowsAutoMode = windowsAutoMode;
     inited = true;
     
 } // if(screenButtons)
 #endif // SUPPORTED_WINDOWS > 0
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTWindowScreen::draw(TFTMenu* menuManager)
{
  //TODO: тут рисуем наш экран
  UNUSED(menuManager);

 #if SUPPORTED_WINDOWS > 0
  if(screenButtons)
  {
    screenButtons->drawButtons(drawButtonsYield);
  }
 #endif 

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_TEMP_SENSORS
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTSettingsScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTSettingsScreen::TFTSettingsScreen()
{
  inited = false;
  
  #ifdef USE_DS3231_REALTIME_CLOCK
  selectedTimePartButton = -1;
  #endif  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTSettingsScreen::~TFTSettingsScreen()
{
 delete screenButtons;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTSettingsScreen::setup(TFTMenu* menuManager)
{

    // получаем температуры
    GlobalSettings* s = MainController->GetSettings();
    openTemp = s->GetOpenTemp();
    closeTemp = s->GetCloseTemp();
    unsigned long ulI = s->GetOpenInterval()/1000;
    interval = ulI;

    UTFT* dc = menuManager->getDC();

    dc->setFont(BigRusFont);
    int textFontHeight = dc->getFontYsize();
  
    screenButtons = new UTFT_Buttons_Rus(dc, menuManager->getTouch(),menuManager->getRusPrinter());
    screenButtons->setSymbolFont(SensorFont);
    screenButtons->setTextFont(SevenSegNumFontMDS);
  
    screenButtons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);

    // первая - кнопка назад
    backButton = addBackButton(menuManager,screenButtons,0);

 
    int screenWidth = dc->getDisplayXSize();

    // добавляем кнопки для управления температурой

    // у нас - 4 кнопки руления температурой, и два бокса для её вывода. Между кнопками и боксом - просвет, между двумя параметрами - двойной просвет.


    // вычисляем ширину всего занятого пространства
    int widthOccupied = TFT_TEXT_INPUT_WIDTH*2 + TFT_ARROW_BUTTON_WIDTH*4 + INFO_BOX_V_SPACING*6;
    
    // теперь вычисляем левую границу для начала рисования
    int leftPos = (screenWidth - widthOccupied)/2;

    // теперь вычисляем верхнюю границу для отрисовки кнопок
    int topPos = INFO_BOX_V_SPACING*2;//(screenHeight - TFT_ARROW_BUTTON_HEIGHT*2 - INFO_BOX_V_SPACING*5)/2;
    int secondRowTopPos = topPos + TFT_ARROW_BUTTON_HEIGHT + INFO_BOX_V_SPACING*2;
    int thirdRowTopPos = secondRowTopPos + TFT_ARROW_BUTTON_HEIGHT + INFO_BOX_V_SPACING*2;

    UTFTRus* rusPrinter = menuManager->getRusPrinter();

    static char leftArrowCaption[2] = {0};
    static char rightArrowCaption[2] = {0};

    leftArrowCaption[0] = rusPrinter->mapChar(charLeftArrow);
    rightArrowCaption[0] = rusPrinter->mapChar(charRightArrow);

    int textBoxHeightWithCaption =  TFT_TEXT_INPUT_HEIGHT + textFontHeight + INFO_BOX_CONTENT_PADDING;
    int textBoxTopPos = topPos - textFontHeight - INFO_BOX_CONTENT_PADDING;
    int secondRowTextBoxTopPos = secondRowTopPos - textFontHeight - INFO_BOX_CONTENT_PADDING;

    // теперь добавляем наши кнопки
    decCloseTempButton = screenButtons->addButton( leftPos ,  topPos, TFT_ARROW_BUTTON_WIDTH,  TFT_ARROW_BUTTON_HEIGHT, leftArrowCaption, BUTTON_SYMBOL);
    decIntervalButton = screenButtons->addButton( leftPos ,  secondRowTopPos, TFT_ARROW_BUTTON_WIDTH,  TFT_ARROW_BUTTON_HEIGHT, leftArrowCaption, BUTTON_SYMBOL);

    #ifdef USE_DS3231_REALTIME_CLOCK
    
    decTimePartButton = screenButtons->addButton( leftPos ,  thirdRowTopPos, TFT_ARROW_BUTTON_WIDTH,  TFT_ARROW_BUTTON_HEIGHT, leftArrowCaption, BUTTON_SYMBOL);
    screenButtons->disableButton(decTimePartButton);
    
    dc->setFont(SevenSegNumFontMDS);
    int fontWidth = dc->getFontXsize();
    int curTimePartLeftPos = leftPos + TFT_ARROW_BUTTON_WIDTH + INFO_BOX_V_SPACING;
    
    const int timeButtonPadding = 9;
    const int timeButtonHSpacing = INFO_BOX_V_SPACING;

    int timeButtonWidth = fontWidth*2+timeButtonPadding*2;
    int timeButtonWidth2 = fontWidth*4+timeButtonPadding*2;    
    
    dayButton = screenButtons->addButton( curTimePartLeftPos ,  thirdRowTopPos, timeButtonWidth,  TFT_ARROW_BUTTON_HEIGHT, strDay.c_str());
    screenButtons->setButtonFontColor(dayButton,TIME_PART_FONT_COLOR);
    screenButtons->setButtonBackColor(dayButton,TIME_PART_BG_COLOR);
    curTimePartLeftPos += timeButtonWidth + timeButtonHSpacing;

    monthButton = screenButtons->addButton( curTimePartLeftPos ,  thirdRowTopPos, timeButtonWidth,  TFT_ARROW_BUTTON_HEIGHT, strMonth.c_str());
    screenButtons->setButtonFontColor(monthButton,TIME_PART_FONT_COLOR);
    screenButtons->setButtonBackColor(monthButton,TIME_PART_BG_COLOR);
    curTimePartLeftPos += timeButtonWidth + timeButtonHSpacing;
    
    yearButton = screenButtons->addButton( curTimePartLeftPos ,  thirdRowTopPos, timeButtonWidth2,  TFT_ARROW_BUTTON_HEIGHT, strYear.c_str());
    screenButtons->setButtonFontColor(yearButton,TIME_PART_FONT_COLOR);
    screenButtons->setButtonBackColor(yearButton,TIME_PART_BG_COLOR);
    curTimePartLeftPos += timeButtonWidth2 + timeButtonHSpacing;

    hourButton = screenButtons->addButton( curTimePartLeftPos ,  thirdRowTopPos, timeButtonWidth,  TFT_ARROW_BUTTON_HEIGHT, strHour.c_str());
    screenButtons->setButtonFontColor(hourButton,TIME_PART_FONT_COLOR);
    screenButtons->setButtonBackColor(hourButton,TIME_PART_BG_COLOR);
    curTimePartLeftPos += timeButtonWidth + timeButtonHSpacing;

    minuteButton = screenButtons->addButton( curTimePartLeftPos ,  thirdRowTopPos, timeButtonWidth,  TFT_ARROW_BUTTON_HEIGHT, strMinute.c_str());
    screenButtons->setButtonFontColor(minuteButton,TIME_PART_FONT_COLOR);
    screenButtons->setButtonBackColor(minuteButton,TIME_PART_BG_COLOR);

    
    dc->setFont(BigRusFont);
    #endif
    
    leftPos += INFO_BOX_V_SPACING + TFT_ARROW_BUTTON_WIDTH;

    closeTempBox = new TFTInfoBox(TFT_TCLOSE_CAPTION,TFT_TEXT_INPUT_WIDTH,textBoxHeightWithCaption,leftPos,textBoxTopPos,-(TFT_ARROW_BUTTON_WIDTH+INFO_BOX_V_SPACING));
    intervalBox = new TFTInfoBox(TFT_INTERVAL_CAPTION,TFT_TEXT_INPUT_WIDTH,textBoxHeightWithCaption,leftPos,secondRowTextBoxTopPos,-(TFT_ARROW_BUTTON_WIDTH+INFO_BOX_V_SPACING));
    leftPos += INFO_BOX_V_SPACING + TFT_TEXT_INPUT_WIDTH;
        
    incCloseTempButton = screenButtons->addButton( leftPos ,  topPos, TFT_ARROW_BUTTON_WIDTH,  TFT_ARROW_BUTTON_HEIGHT, rightArrowCaption, BUTTON_SYMBOL);
    incIntervalButton = screenButtons->addButton( leftPos ,  secondRowTopPos, TFT_ARROW_BUTTON_WIDTH,  TFT_ARROW_BUTTON_HEIGHT, rightArrowCaption, BUTTON_SYMBOL);
    leftPos += INFO_BOX_V_SPACING*2 + TFT_ARROW_BUTTON_WIDTH;

    
    decOpenTempButton = screenButtons->addButton( leftPos ,  topPos, TFT_ARROW_BUTTON_WIDTH,  TFT_ARROW_BUTTON_HEIGHT, leftArrowCaption, BUTTON_SYMBOL);
    leftPos += INFO_BOX_V_SPACING + TFT_ARROW_BUTTON_WIDTH;

    openTempBox = new TFTInfoBox(TFT_TOPEN_CAPTION,TFT_TEXT_INPUT_WIDTH,textBoxHeightWithCaption,leftPos,textBoxTopPos,-(TFT_ARROW_BUTTON_WIDTH+INFO_BOX_V_SPACING));
    leftPos += INFO_BOX_V_SPACING + TFT_TEXT_INPUT_WIDTH;
   
    incOpenTempButton = screenButtons->addButton( leftPos ,  topPos, TFT_ARROW_BUTTON_WIDTH,  TFT_ARROW_BUTTON_HEIGHT, rightArrowCaption, BUTTON_SYMBOL);

    #ifdef USE_DS3231_REALTIME_CLOCK
      incTimePartButton = screenButtons->addButton( leftPos ,  thirdRowTopPos, TFT_ARROW_BUTTON_WIDTH,  TFT_ARROW_BUTTON_HEIGHT, rightArrowCaption, BUTTON_SYMBOL);
      screenButtons->disableButton(incTimePartButton);    
    #endif
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_DS3231_REALTIME_CLOCK
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint16_t TFTSettingsScreen::stepVal(int8_t dir, uint16_t minVal,uint16_t maxVal, int16_t val)
{
  val += dir;

  if(val < minVal)
    val = maxVal;

  if(val > maxVal)
    val = minVal;

  return val;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
String TFTSettingsScreen::addLeadingZero(int val)
{
  String result;

  if(val < 0)
    result += '-';
    
  if(abs(val) < 10)
    result += '0';

  result += abs(val);

  return result;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTSettingsScreen::updateTimeButtons(DS3231Time& tm, bool redraw)
{
  
    strDay = addLeadingZero(tm.dayOfMonth);
    screenButtons->relabelButton(dayButton,strDay.c_str(),redraw);

    strMonth = addLeadingZero(tm.month);
    screenButtons->relabelButton(monthButton,strMonth.c_str(),redraw);

    strYear = tm.year;
    screenButtons->relabelButton(yearButton,strYear.c_str(),redraw);

    strHour = addLeadingZero(tm.hour);
    screenButtons->relabelButton(hourButton,strHour.c_str(),redraw);

    strMinute = addLeadingZero(tm.minute);
    screenButtons->relabelButton(minuteButton,strMinute.c_str(),redraw);
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_DS3231_REALTIME_CLOCK
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTSettingsScreen::drawValueInBox(TFTMenu* menuManager, TFTInfoBox* box, uint16_t val)
{
  UTFT* dc = menuManager->getDC();
  TFTInfoBoxContentRect rc =  box->getContentRect(menuManager);
  dc->setColor(INFO_BOX_BACK_COLOR);
  dc->fillRect(rc.x,rc.y,rc.x+rc.w,rc.y+rc.h);
  yield();

  dc->setFont(SevenSegNumFontMDS);
  dc->setBackColor(INFO_BOX_BACK_COLOR);
  dc->setColor(SENSOR_BOX_FONT_COLOR);

  String strVal;
  strVal = val;
  
  int fontWidth = dc->getFontXsize();
  int fontHeight = dc->getFontYsize();

  int leftPos = rc.x + (rc.w - (strVal.length()*fontWidth))/2;
  int topPos = rc.y + (rc.h - fontHeight)/2;
  dc->print(strVal.c_str(),leftPos,topPos);
  yield();
  dc->setFont(BigRusFont);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTSettingsScreen::onActivate(TFTMenu* menuManager)
{
  #ifdef USE_DS3231_REALTIME_CLOCK
  DS3231Clock rtc = MainController->GetClock();
  controllerTime = rtc.getTime();  
  updateTimeButtons(controllerTime,false);
  controllerTimeChanged = false;
  #endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTSettingsScreen::update(TFTMenu* menuManager,uint16_t dt)
{
 UNUSED(dt);

 if(screenButtons)
 {
    int pressed_button = screenButtons->checkButtons(BuzzerOn);
    
    if(pressed_button == backButton)
    {
      #ifdef USE_DS3231_REALTIME_CLOCK
      if(controllerTimeChanged)
      {
        controllerTimeChanged = false;
        DS3231Clock rtc = MainController->GetClock();
        rtc.setTime(controllerTime);
      }
      #endif
      
      menuManager->switchToScreen("IDLE");
      return;
    }

    if(pressed_button == decOpenTempButton)
    {
      if(openTemp < 1)
        return;

      openTemp--;
      MainController->GetSettings()->SetOpenTemp(openTemp);
      drawValueInBox(menuManager,openTempBox,openTemp);  
      menuManager->resetIdleTimer();
      return;
    }
    
    if(pressed_button == incOpenTempButton)
    {
      if(openTemp > 49)
        return;

      openTemp++;
      MainController->GetSettings()->SetOpenTemp(openTemp);
      drawValueInBox(menuManager,openTempBox,openTemp);  
      menuManager->resetIdleTimer();
      return;
    }
    
    if(pressed_button == decCloseTempButton)
    {
      if(closeTemp < 1)
        return;

      closeTemp--;
      MainController->GetSettings()->SetCloseTemp(closeTemp);
      drawValueInBox(menuManager,closeTempBox,closeTemp);  
      menuManager->resetIdleTimer();
      return;
    }

    if(pressed_button == incCloseTempButton)
    {
      if(closeTemp > 49)
        return;

      closeTemp++;
      MainController->GetSettings()->SetCloseTemp(closeTemp);
      drawValueInBox(menuManager,closeTempBox,closeTemp);  
      menuManager->resetIdleTimer();
      return;
    }

    if(pressed_button == decIntervalButton)
    {
      if(interval < 2)
        return;

      interval--;
      unsigned long ulInterval = interval;
      ulInterval *= 1000;
      MainController->GetSettings()->SetOpenInterval(ulInterval);
      drawValueInBox(menuManager,intervalBox,interval);  
      menuManager->resetIdleTimer();
      return;
    }
    
    if(pressed_button == incIntervalButton)
    {
      if(interval >= UINT_MAX)
        return;

      interval++;
      unsigned long ulInterval = interval;
      ulInterval *= 1000;
      MainController->GetSettings()->SetOpenInterval(ulInterval);
      drawValueInBox(menuManager,intervalBox,interval);  
      menuManager->resetIdleTimer();
      return;
    }    

    #ifdef USE_DS3231_REALTIME_CLOCK
    static uint32_t controllerTimeChangedTimer = 0;
    
    if(pressed_button == dayButton || pressed_button == monthButton || pressed_button == yearButton
    || pressed_button == hourButton || pressed_button == minuteButton)
    {
      if(selectedTimePartButton != -1)
      {
        if(selectedTimePartButton == pressed_button)
        {
          // та же самая кнопка, что была уже выделена, снимаем выделение
          screenButtons->setButtonBackColor(selectedTimePartButton,TIME_PART_BG_COLOR);
          screenButtons->setButtonFontColor(selectedTimePartButton,TIME_PART_FONT_COLOR);
          screenButtons->drawButton(selectedTimePartButton);
          selectedTimePartButton = -1;
          
          screenButtons->disableButton(incTimePartButton,screenButtons->buttonEnabled(incTimePartButton));
          screenButtons->disableButton(decTimePartButton,screenButtons->buttonEnabled(decTimePartButton));
        }
        else
        {
          // выделили другую кнопку, поэтому для предыдущей надо убрать выделение
          screenButtons->setButtonBackColor(selectedTimePartButton,TIME_PART_BG_COLOR);
          screenButtons->setButtonFontColor(selectedTimePartButton,TIME_PART_FONT_COLOR);
          screenButtons->drawButton(selectedTimePartButton);
          selectedTimePartButton = pressed_button;
          screenButtons->setButtonBackColor(selectedTimePartButton,TIME_PART_SELECTED_BG_COLOR);
          screenButtons->setButtonFontColor(selectedTimePartButton,TIME_PART_SELECTED_FONT_COLOR);
          screenButtons->drawButton(selectedTimePartButton);

          screenButtons->enableButton(incTimePartButton,!screenButtons->buttonEnabled(incTimePartButton));
          screenButtons->enableButton(decTimePartButton,!screenButtons->buttonEnabled(decTimePartButton));
          
        }
      }
      else
      {
        // ничего не выделено, выделяем нажатую кнопку
        selectedTimePartButton = pressed_button; 
        screenButtons->setButtonBackColor(selectedTimePartButton,TIME_PART_SELECTED_BG_COLOR);
        screenButtons->setButtonFontColor(selectedTimePartButton,TIME_PART_SELECTED_FONT_COLOR);
        screenButtons->drawButton(selectedTimePartButton);

        screenButtons->enableButton(incTimePartButton,!screenButtons->buttonEnabled(incTimePartButton));
        screenButtons->enableButton(decTimePartButton,!screenButtons->buttonEnabled(decTimePartButton));
      }

      menuManager->resetIdleTimer();
      return;
    }

    if(pressed_button == decTimePartButton || pressed_button == incTimePartButton)
    {
      if(selectedTimePartButton == -1)
      {
        menuManager->resetIdleTimer();
        return;
      }

       int dir = pressed_button == decTimePartButton ? -1 : 1;
       
      if(selectedTimePartButton == dayButton)
      {
        controllerTime.dayOfMonth = stepVal(dir, 0,31, controllerTime.dayOfMonth);
        strDay = addLeadingZero(controllerTime.dayOfMonth);
        screenButtons->relabelButton(dayButton,strDay.c_str(),true);
      }
      else
      if(selectedTimePartButton == monthButton)
      {
        controllerTime.month = stepVal(dir, 0,12, controllerTime.month);
        strMonth = addLeadingZero(controllerTime.month);
        screenButtons->relabelButton(monthButton,strMonth.c_str(),true);
      }
      else 
      if(selectedTimePartButton == yearButton)
      {
       if(controllerTime.year == 2000)
       {
          dir = 0;
          controllerTime.year = 2018;
       }
                  
        controllerTime.year = stepVal(dir, 2018,2100, controllerTime.year);
        strYear = controllerTime.year;
        screenButtons->relabelButton(yearButton,strYear.c_str(),true);
      }
     else
     if(selectedTimePartButton == hourButton)
     {
        controllerTime.hour = stepVal(dir, 0,23, controllerTime.hour);
        strHour = addLeadingZero(controllerTime.hour);
        screenButtons->relabelButton(hourButton,strHour.c_str(),true);
     }
     else
     if(selectedTimePartButton == minuteButton)
     {
        controllerTime.minute = stepVal(dir, 0,59, controllerTime.minute);
        strMinute = addLeadingZero(controllerTime.minute);
        screenButtons->relabelButton(minuteButton,strMinute.c_str(),true);
    }
        
      // вычисляем день недели
       int32_t dow;
       byte mArr[12] = {6,2,2,5,0,3,5,1,4,6,2,4};
       dow = (controllerTime.year % 100);
       dow = dow*1.25;
       dow += controllerTime.dayOfMonth;
       dow += mArr[controllerTime.month-1];
       
       if (((controllerTime.year % 4)==0) && (controllerTime.month<3))
         dow -= 1;
         
       while (dow>7)
         dow -= 7;     

      controllerTime.dayOfWeek = dow;
      controllerTimeChanged = true;
      controllerTimeChangedTimer = millis();
      
      menuManager->resetIdleTimer();
      return;
    }

     
     if(controllerTimeChanged)
     {
        if(millis() - controllerTimeChangedTimer > 5000)
        {
          controllerTimeChangedTimer = millis();
          controllerTimeChanged = false;
          DS3231Clock rtc = MainController->GetClock();
          rtc.setTime(controllerTime);
        }
     }
     else
     {
        controllerTimeChangedTimer = millis();
     }

    
    #endif // USE_DS3231_REALTIME_CLOCK        
       
    inited = true;
     
 } // if(screenButtons)
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTSettingsScreen::draw(TFTMenu* menuManager)
{
  UNUSED(menuManager);

  if(screenButtons)
  {
    screenButtons->drawButtons(drawButtonsYield);
  }

  closeTempBox->draw(menuManager);
  openTempBox->draw(menuManager);
  intervalBox->draw(menuManager);

  #ifdef USE_DS3231_REALTIME_CLOCK

  UTFT* dc = menuManager->getDC();
  dc->setFont(BigRusFont);
  dc->setBackColor(TFT_BACK_COLOR);
  dc->setColor(INFO_BOX_CAPTION_COLOR);
  menuManager->getRusPrinter()->print(TFT_CURRENTTIME_CAPTION,30,255);  

  DS3231Clock rtc = MainController->GetClock();
  controllerTime = rtc.getTime();  
  updateTimeButtons(controllerTime,true);
    
  #endif

  drawValueInBox(menuManager,closeTempBox,closeTemp);
  drawValueInBox(menuManager,openTempBox,openTemp);
  drawValueInBox(menuManager,intervalBox,interval);

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_TEMP_SENSORS
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern imagedatatype tft_windows_button[];
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::drawWindowStatus(TFTMenu* menuManager)
{
  drawStatusesInBox(menuManager, windowStatusBox, flags.isWindowsOpen, flags.windowsAutoMode, TFT_WINDOWS_OPEN_CAPTION, TFT_WINDOWS_CLOSED_CAPTION, TFT_AUTO_MODE_CAPTION, TFT_MANUAL_MODE_CAPTION);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_WATERING_MODULE
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern imagedatatype tft_water_button[];
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTIdleScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::drawWaterStatus(TFTMenu* menuManager)
{
  drawStatusesInBox(menuManager, waterStatusBox, flags.isWaterOn, flags.waterAutoMode, TFT_WATER_ON_CAPTION, TFT_WATER_OFF_CAPTION, TFT_AUTO_MODE_CAPTION, TFT_MANUAL_MODE_CAPTION);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_LUMINOSITY_MODULE
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern imagedatatype tft_light_button[];
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::drawLightStatus(TFTMenu* menuManager)
{
  drawStatusesInBox(menuManager, lightStatusBox, flags.isLightOn, flags.lightAutoMode, TFT_LIGHT_ON_CAPTION, TFT_LIGHT_OFF_CAPTION, TFT_AUTO_MODE_CAPTION, TFT_MANUAL_MODE_CAPTION);  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern imagedatatype tft_options_button[];
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTIdleScreen::TFTIdleScreen() : AbstractTFTScreen()
{
  #ifdef USE_DS3231_REALTIME_CLOCK
      lastMinute = -1;
  #endif  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTIdleScreen::~TFTIdleScreen()
{
  delete screenButtons;
  
  #ifdef USE_TEMP_SENSORS
    delete windowStatusBox;
  #endif

  #ifdef USE_WATERING_MODULE
    delete waterStatusBox;
  #endif

  #ifdef USE_LUMINOSITY_MODULE
    delete lightStatusBox;
  #endif

  #if TFT_SENSOR_BOXES_COUNT > 0
  for(int i=0;i<TFT_SENSOR_BOXES_COUNT;i++)
  {
    delete sensors[i];
  }
#endif  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_DS3231_REALTIME_CLOCK
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::DrawDateTime(TFTMenu* menuManager)
{
    DS3231Clock rtc = MainController->GetClock();
    DS3231Time tm = rtc.getTime();

  if(lastMinute == tm.minute)
    return;

    static char dt_buff[20] = {0};
    sprintf_P(dt_buff,(const char*) F("%02d.%02d.%d %02d:%02d"), tm.dayOfMonth, tm.month, tm.year, tm.hour, tm.minute);

    lastMinute = tm.minute;

    UTFT* dc = menuManager->getDC();
    dc->setFont(BigRusFont);
    dc->setBackColor(TFT_BACK_COLOR);
    dc->setColor(INFO_BOX_CAPTION_COLOR); 

    int screenWidth = dc->getDisplayXSize();
    int screenHeight = dc->getDisplayYSize();
    int fontWidth = dc->getFontXsize();
    int fontHeight = dc->getFontYsize();
    int textLen = menuManager->getRusPrinter()->print(dt_buff,0, 0, 0, true);

    int left = (screenWidth - (textLen*fontWidth))/2;
    int top = screenHeight - (fontHeight + 4);
    dc->print(dt_buff,left,top);
    yield();
      
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_DS3231_REALTIME_CLOCK
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::drawSensorData(TFTMenu* menuManager,TFTInfoBox* box, int sensorIndex, bool forceDraw)
{

  if(!box) // нет переданного бокса для отрисовки
    return;

  TFTSensorInfo* sensorInfo = &(TFTSensors[sensorIndex]);
  ModuleStates sensorType = (ModuleStates) sensorInfo->sensorType;

  //Тут получение OneState для датчика, и выход, если не получили
  AbstractModule* module = MainController->GetModuleByID(sensorInfo->moduleName);
  if(!module)
    return;

  OneState* sensorState = module->State.GetState(sensorType,sensorInfo->sensorIndex);
  if(!sensorState)
    return;

  //Тут проверка на то, что данные с датчика изменились. И если не изменились - не надо рисовать, только если forceDraw не true.
  bool sensorDataChanged = sensorState->IsChanged();

  if(!sensorDataChanged && !forceDraw)
  {
    // ничего не надо перерисовывать, выходим
    return;
  }


  TFTInfoBoxContentRect rc = box->getContentRect(menuManager);
  UTFT* dc = menuManager->getDC();
  UTFTRus* rusPrinter = menuManager->getRusPrinter();

  dc->setColor(INFO_BOX_BACK_COLOR);
  dc->fillRect(rc.x,rc.y,rc.x+rc.w,rc.y+rc.h);
  yield();
  
  dc->setBackColor(INFO_BOX_BACK_COLOR);
  dc->setColor(SENSOR_BOX_FONT_COLOR);  

  dc->setFont(SensorFont);

  // тут сами данные с датчика, в двух частях
  String sensorValue;
  String sensorFract;

  
  //Если данных с датчика нет - выводим два минуса, не забыв поменять шрифт на  SensorFont.
  bool hasSensorData = sensorState->HasData();
  bool minusVisible = false;
  TFTSpecialSimbol unitChar = charUnknown;
  bool dotAvailable = true;

  if(hasSensorData)
  {
      switch(sensorType)
      {
        case StateTemperature:
        {
        
          unitChar = charDegree;
          
          //Тут получение данных с датчика
          TemperaturePair tp = *sensorState;
          
          sensorValue = (byte) abs(tp.Current.Value);
          
          int fract = tp.Current.Fract;
          if(fract < 10)
            sensorFract += '0';
            
          sensorFract += fract;
         
          //Тут проверяем на отрицательную температуру
          minusVisible = tp.Current.Value < 0;    
        }
        break;
    
        case StateHumidity:
        case StateSoilMoisture:
        {
          unitChar = charPercent;

          //Тут получение данных с датчика

          HumidityPair hp = *sensorState;
          sensorValue = hp.Current.Value;
          
          int fract = hp.Current.Fract;
          if(fract < 10)
            sensorFract += '0';
            
          sensorFract += fract;
        }
        break;
    
        case StateLuminosity:
        {
          unitChar = charLux;
        
          //Тут получение данных с датчика
          LuminosityPair lp = *sensorState;
          long lum = lp.Current;
          sensorValue = lum;
          dotAvailable = false;
        }
        break;
    
        case StateWaterFlowInstant:
        case StateWaterFlowIncremental:
        {
          //Тут получение данных с датчика
          WaterFlowPair wp = *sensorState;
          unsigned long flow = wp.Current;
          sensorValue = flow;
          dotAvailable = false;
        }
        break;
        
        case StatePH:
        {
          //Тут получение данных с датчика
          HumidityPair ph = *sensorState;
          sensorValue = (byte) ph.Current.Value;
          int fract = ph.Current.Fract;
          if(fract < 10)
            sensorFract += '0';
            
          sensorFract += fract;
        } 
        break;
        
        case StateUnknown:
          dotAvailable = false;
        break;
        
      } // switch
    
  } // hasSensorData
  else
  {
    sensorValue = rusPrinter->mapChar(charMinus);
    sensorValue += rusPrinter->mapChar(charMinus);
    sensorFract = "";
    dotAvailable = false;
    minusVisible = false;
    unitChar = charUnknown;
  }
  

  // у нас длина строки будет - длина показаний + точка + единицы измерения + опциональный минус
  int totalStringLength = 1; // place for dot
  
  if(minusVisible) // минус у нас просчитывается, только если есть показания с датчика
    totalStringLength++;

   if(unitChar != charUnknown) // единицы измерения у нас просчитываются, только если есть показания с датчика
    totalStringLength++; // есть единицы измерения

  //Если тип показаний не подразумевает точку - убираем её
  if(!dotAvailable)
    totalStringLength--;


  int curTop = rc.y + (rc.h - dc->getFontYsize())/2;
  int fontWidth = dc->getFontXsize();

  int valueLen = sensorValue.length();
  int fractLen = sensorFract.length();

  totalStringLength += valueLen + fractLen;

  // рисуем данные с датчика
  int curLeft = rc.x + (rc.w - totalStringLength*fontWidth)/2;

  if(minusVisible)
  {
    rusPrinter->printSpecialChar(charMinus,curLeft,curTop);
    curLeft += fontWidth;
  }

  // теперь рисуем целое значение
  if(hasSensorData)
    dc->setFont(SevenSegNumFontMDS); // есть данные, рисуем числа
  else
   dc->setFont(SensorFont); // нет данных, рисуем два минуса
  
  dc->print(sensorValue.c_str(),curLeft,curTop);
  yield();
  curLeft += fontWidth*valueLen;

  if(hasSensorData)
  {
      // теперь рисуем запятую, если надо
      // признаком служит доступность разделителя к рисованию
      // также не надо рисовать, если длина дробной части - 0
      if(dotAvailable && sensorFract.length())
      {
          dc->setFont(SensorFont);
          rusPrinter->printSpecialChar(TFT_SENSOR_DECIMAL_SEPARATOR,curLeft,curTop);
          curLeft += fontWidth;
      
          // теперь рисуем дробную часть
          dc->setFont(SevenSegNumFontMDS);
          dc->print(sensorFract.c_str(),curLeft,curTop);
          yield();
          curLeft += fontWidth*fractLen;
    
      } // if(dotAvailable)
      
  } // if(hasSensorData)

 

  if(unitChar != charUnknown) // если надо рисовать единицы измерений - рисуем
  {
    // теперь рисуем единицы измерения
    dc->setFont(SensorFont);  
    dc->setColor(SENSOR_BOX_UNIT_COLOR);
    rusPrinter->printSpecialChar(unitChar,curLeft,curTop);
  }


  // сбрасываем на шрифт по умолчанию
  dc->setFont(BigRusFont);
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::drawStatusesInBox(TFTMenu* menuManager,TFTInfoBox* box, bool status, bool mode, const char* onStatusString, const char* offStatusString, const char* autoModeString, const char* manualModeString)
{
  TFTInfoBoxContentRect rc = box->getContentRect(menuManager);
  UTFT* dc = menuManager->getDC();

  dc->setColor(INFO_BOX_BACK_COLOR);
  dc->fillRect(rc.x,rc.y,rc.x+rc.w,rc.y+rc.h);
  yield();

  dc->setBackColor(INFO_BOX_BACK_COLOR);
  dc->setColor(SENSOR_BOX_FONT_COLOR);

  int curTop = rc.y;
  int curLeft = rc.x;
  int fontHeight = dc->getFontYsize();
  int fontWidth = dc->getFontXsize();

  // рисуем заголовки режимов
  menuManager->getRusPrinter()->print(TFT_STATUS_CAPTION,curLeft,curTop);
  curTop += fontHeight + INFO_BOX_CONTENT_PADDING;
  menuManager->getRusPrinter()->print(TFT_MODE_CAPTION,curLeft,curTop);

  // теперь рисуем статусы режимов

  curTop = rc.y;
  const char* toDraw;
  
  if(status)
  {
    dc->setColor(STATUS_ON_COLOR);
    toDraw = onStatusString;
  }
  else
  {
    dc->setColor(SENSOR_BOX_FONT_COLOR);
    toDraw = offStatusString;
  }

  int captionLen = menuManager->getRusPrinter()->print(toDraw,0, 0, 0, true);
  
  curLeft = (rc.x + rc.w) - (captionLen*fontWidth);
  menuManager->getRusPrinter()->print(toDraw,curLeft,curTop);

  curTop += fontHeight + INFO_BOX_CONTENT_PADDING;

  if(mode)
  {
    dc->setColor(STATUS_ON_COLOR);
    toDraw = autoModeString;
  }
  else
  {
    dc->setColor(SENSOR_BOX_FONT_COLOR);
    toDraw = manualModeString;
  }

  captionLen = menuManager->getRusPrinter()->print(toDraw,0, 0, 0, true);
  
  curLeft = (rc.x + rc.w) - (captionLen*fontWidth);
  menuManager->getRusPrinter()->print(toDraw,curLeft,curTop);

  yield();
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::updateStatuses(TFTMenu* menuManager)
{
  bool wOpen = WORK_STATUS.GetStatus(WINDOWS_STATUS_BIT);
  bool wAutoMode = WORK_STATUS.GetStatus(WINDOWS_MODE_BIT);

  bool waterOn = WORK_STATUS.GetStatus(WATER_STATUS_BIT);
  bool waterAutoMode = WORK_STATUS.GetStatus(WATER_MODE_BIT);

  bool lightOn = WORK_STATUS.GetStatus(LIGHT_STATUS_BIT);
  bool lightAutoMode = WORK_STATUS.GetStatus(LIGHT_MODE_BIT);

  bool windowChanges = flags.isWindowsOpen != wOpen || flags.windowsAutoMode != wAutoMode;
  bool waterChanges = flags.isWaterOn != waterOn || flags.waterAutoMode != waterAutoMode;
  bool lightChanges = flags.isLightOn != lightOn || flags.lightAutoMode != lightAutoMode;

  flags.isWindowsOpen = wOpen;
  flags.windowsAutoMode = wAutoMode;
  
  flags.isWaterOn = waterOn;
  flags.waterAutoMode = waterAutoMode;
  
  flags.isLightOn = lightOn;
  flags.lightAutoMode = lightAutoMode;

  #ifdef USE_TEMP_SENSORS
    if(windowChanges)
    {
      drawWindowStatus(menuManager);
    }
  #endif

  #ifdef USE_WATERING_MODULE
    if(waterChanges)
    {
      drawWaterStatus(menuManager);
    }
  #endif

  #ifdef USE_LUMINOSITY_MODULE
    if(lightChanges)
    {
      drawLightStatus(menuManager);
    }
  #endif
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::setup(TFTMenu* menuManager)
{
  screenButtons = new UTFT_Buttons_Rus(menuManager->getDC(), menuManager->getTouch(),menuManager->getRusPrinter());
  screenButtons->setTextFont(BigRusFont);
  screenButtons->setButtonColors(TFT_BUTTON_COLORS);

  int buttonsTop = menuManager->getDC()->getDisplayYSize() - TFT_IDLE_SCREEN_BUTTON_HEIGHT - 30; // координата Y для кнопок стартового экрана
  int screenWidth = menuManager->getDC()->getDisplayXSize();

  // вычисляем, сколько кнопок доступно
  int availButtons = 1; // кнопка "Опции" всегда доступна

  int availStatusBoxes = 0;
  #ifdef USE_TEMP_SENSORS
    availStatusBoxes++;
  #endif
  #ifdef USE_WATERING_MODULE
    availStatusBoxes++;
  #endif
  #ifdef USE_LUMINOSITY_MODULE
    availStatusBoxes++;
  #endif

  int curInfoBoxLeft;

  if(availStatusBoxes > 0)
  {
    curInfoBoxLeft = (screenWidth - (availStatusBoxes*INFO_BOX_WIDTH + (availStatusBoxes-1)*INFO_BOX_V_SPACING))/2;//INFO_BOX_V_SPACING;
  
    #ifdef USE_TEMP_SENSORS
        windowStatusBox = new TFTInfoBox(TFT_WINDOW_STATUS_CAPTION,INFO_BOX_WIDTH,INFO_BOX_HEIGHT,curInfoBoxLeft,INFO_BOX_V_SPACING);
        curInfoBoxLeft += INFO_BOX_WIDTH + INFO_BOX_V_SPACING;
        availButtons++;
    #endif
    
    #ifdef USE_WATERING_MODULE
        waterStatusBox = new TFTInfoBox(TFT_WATER_STATUS_CAPTION,INFO_BOX_WIDTH,INFO_BOX_HEIGHT,curInfoBoxLeft,INFO_BOX_V_SPACING);
        curInfoBoxLeft += INFO_BOX_WIDTH + INFO_BOX_V_SPACING;
        availButtons++;
    #endif
    
    #ifdef USE_LUMINOSITY_MODULE
        lightStatusBox = new TFTInfoBox(TFT_LIGHT_STATUS_CAPTION,INFO_BOX_WIDTH,INFO_BOX_HEIGHT,curInfoBoxLeft,INFO_BOX_V_SPACING);
        curInfoBoxLeft += INFO_BOX_WIDTH + INFO_BOX_V_SPACING;
        availButtons++;
    #endif

  }


  // теперь добавляем боксы для датчиков
  #if TFT_SENSOR_BOXES_COUNT > 0

   for(int i=0;i<TFT_SENSOR_BOXES_COUNT;i++)
   {
    sensors[i] = NULL;
   }

  
  int startLeft = (screenWidth - (SENSOR_BOXES_PER_LINE*SENSOR_BOX_WIDTH + (SENSOR_BOXES_PER_LINE-1)*SENSOR_BOX_V_SPACING))/2;
  curInfoBoxLeft = startLeft;
  int sensorsTop = availStatusBoxes ? (INFO_BOX_V_SPACING + INFO_BOX_HEIGHT + SENSOR_BOX_V_SPACING) : SENSOR_BOX_V_SPACING;
  int sensorBoxesPlacedInLine = 0;
  int createdSensorIndex = 0;
  
  for(int i=0;i<TFT_SENSOR_BOXES_COUNT;i++)
  {

    TFTSensorInfo* inf = &(TFTSensors[i]);
    
    //ТУТ проверка на существование модуля в прошивке
    bool moduleAvailable = MainController->GetModuleByID(inf->moduleName) != NULL;
    
    if(!moduleAvailable) // нет модуля в прошивке
      continue;
    
    if(sensorBoxesPlacedInLine == SENSOR_BOXES_PER_LINE)
    {
      sensorBoxesPlacedInLine = 0;
      curInfoBoxLeft = startLeft;
      sensorsTop += SENSOR_BOX_HEIGHT + SENSOR_BOX_V_SPACING;
    }
      
    sensors[createdSensorIndex] = new TFTInfoBox("",SENSOR_BOX_WIDTH,SENSOR_BOX_HEIGHT,curInfoBoxLeft,sensorsTop);
    curInfoBoxLeft += SENSOR_BOX_WIDTH + SENSOR_BOX_V_SPACING;
    sensorBoxesPlacedInLine++;
    createdSensorIndex++;
  }
  #endif

  // у нас есть availButtons кнопок, между ними - availButtons-1 пробел, вычисляем левую координату для первой кнопки
  int curButtonLeft = (screenWidth - ( availButtons*TFT_IDLE_SCREEN_BUTTON_WIDTH + (availButtons-1)*TFT_IDLE_SCREEN_BUTTON_SPACING ))/2;

  #ifdef USE_TEMP_SENSORS
    windowsButton = screenButtons->addButton( curButtonLeft ,  buttonsTop, TFT_IDLE_SCREEN_BUTTON_WIDTH,  TFT_IDLE_SCREEN_BUTTON_HEIGHT, tft_windows_button ,BUTTON_BITMAP);
    curButtonLeft += TFT_IDLE_SCREEN_BUTTON_WIDTH + TFT_IDLE_SCREEN_BUTTON_SPACING;
  #endif

  #ifdef USE_WATERING_MODULE
    waterButton = screenButtons->addButton( curButtonLeft ,  buttonsTop, TFT_IDLE_SCREEN_BUTTON_WIDTH,  TFT_IDLE_SCREEN_BUTTON_HEIGHT, tft_water_button ,BUTTON_BITMAP);
    curButtonLeft += TFT_IDLE_SCREEN_BUTTON_WIDTH + TFT_IDLE_SCREEN_BUTTON_SPACING;
  #endif

  #ifdef USE_LUMINOSITY_MODULE
    lightButton = screenButtons->addButton( curButtonLeft ,  buttonsTop, TFT_IDLE_SCREEN_BUTTON_WIDTH,  TFT_IDLE_SCREEN_BUTTON_HEIGHT, tft_light_button ,BUTTON_BITMAP);
    curButtonLeft += TFT_IDLE_SCREEN_BUTTON_WIDTH + TFT_IDLE_SCREEN_BUTTON_SPACING;
  #endif

    optionsButton = screenButtons->addButton( curButtonLeft ,  buttonsTop, TFT_IDLE_SCREEN_BUTTON_WIDTH,  TFT_IDLE_SCREEN_BUTTON_HEIGHT, tft_options_button ,BUTTON_BITMAP);
    curButtonLeft += TFT_IDLE_SCREEN_BUTTON_WIDTH + TFT_IDLE_SCREEN_BUTTON_SPACING;
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::onActivate(TFTMenu* menuManager)
{
  #ifdef USE_DS3231_REALTIME_CLOCK
  lastMinute = -1;
  #endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::update(TFTMenu* menuManager,uint16_t dt)
{

  #ifdef USE_DS3231_REALTIME_CLOCK
  //if(dt == 0)
   // lastMinute = -1;
    
    DrawDateTime(menuManager);
  #endif

  // Смотрим, какая кнопка нажата
  int pressed_button = screenButtons->checkButtons(BuzzerOn);

#ifdef USE_TEMP_SENSORS
  if(pressed_button == windowsButton)
  {
    menuManager->switchToScreen("WINDOW");
    return;
  }
#endif  

#ifdef USE_WATERING_MODULE
  if(pressed_button == waterButton)
  {
    menuManager->switchToScreen("WATER");
    return;
  }
#endif  

#ifdef USE_LUMINOSITY_MODULE
  if(pressed_button == lightButton)
  {
    menuManager->switchToScreen("LIGHT");
    return;
  }
#endif  

  if(pressed_button == optionsButton)
  {
    menuManager->switchToScreen("OPTIONS");
    return;
  }

  updateStatuses(menuManager); // update statuses now

  #if TFT_SENSOR_BOXES_COUNT > 0
  
    sensorsTimer += dt;
        
    if(sensorsTimer > TFT_SENSORS_UPDATE_INTERVAL)
    {
      sensorsTimer = 0;

      for(int i=0;i<TFT_SENSOR_BOXES_COUNT;i++)
      {
        drawSensorData(menuManager,sensors[i],i);
      }
      
    }
  #endif
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::draw(TFTMenu* menuManager)
{
  screenButtons->drawButtons(drawButtonsYield); // рисуем наши кнопки

  //int availStatusBoxes = 0;

  #ifdef USE_TEMP_SENSORS // рисуем статус окон
    //availStatusBoxes++;
    windowStatusBox->draw(menuManager);
    drawWindowStatus(menuManager);
  #endif

  #ifdef USE_WATERING_MODULE // рисуем статус полива
    //availStatusBoxes++;
    waterStatusBox->draw(menuManager);
    drawWaterStatus(menuManager);
  #endif

  #ifdef USE_LUMINOSITY_MODULE // рисуем статус досветки
    //availStatusBoxes++;
    lightStatusBox->draw(menuManager);
    drawLightStatus(menuManager);
  #endif


  #if TFT_SENSOR_BOXES_COUNT > 0
  
  //Тут отрисовка боксов с показаниями датчиков
  for(int i=0;i<TFT_SENSOR_BOXES_COUNT;i++)
  {
    if(!sensors[i])
      continue;
    
    sensors[i]->draw(menuManager);
    TFTSensorInfo* sensorInfo = &(TFTSensors[i]);
    sensors[i]->drawCaption(menuManager,sensorInfo->sensorLabel);
    drawSensorData(menuManager,sensors[i],i,true); // тут перерисовываем показания по-любому
    sensorsTimer = 0; // сбрасываем таймер перерисовки показаний датчиков

  }

  #endif

  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTMenu
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTMenu::TFTMenu()
{
  currentScreenIndex = -1;
  flags.isLCDOn = true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::setup()
{
  tftMenuManager = this;

  lcdOn(); // включаем подсветку
  
  tftDC = new UTFT(TFT_MODEL,TFT_RS_PIN,TFT_WR_PIN,TFT_CS_PIN,TFT_RST_PIN);
  tftTouch = new URTouch(TFT_TOUCH_CLK_PIN,TFT_TOUCH_CS_PIN,TFT_TOUCH_DIN_PIN,TFT_TOUCH_DOUT_PIN,TFT_TOUCH_IRQ_PIN);

  #ifdef TFT_EXTRA_RESET
    pinMode(TFT_RST_PIN,OUTPUT);
    digitalWrite(TFT_RST_PIN,HIGH);
    delay(10);
    digitalWrite(TFT_RST_PIN,LOW);
    delay(10);
    digitalWrite(TFT_RST_PIN,HIGH);        
  #endif

  #if TFT_INIT_DELAY > 0
  delay(TFT_INIT_DELAY);
  #endif
  
  tftDC->InitLCD(LANDSCAPE);
  tftDC->fillScr(TFT_BACK_COLOR);
  yield();
  tftDC->setFont(BigRusFont);

  tftTouch->InitTouch(LANDSCAPE);
  tftTouch->setPrecision(PREC_HI);
  
  rusPrint.init(tftDC);

  resetIdleTimer();

  // добавляем экран ожидания
  AbstractTFTScreen* idleScreen = new TFTIdleScreen();
  idleScreen->setup(this);
  TFTScreenInfo si; 
  si.screenName = "IDLE"; 
  si.screen = idleScreen;  
  screens.push_back(si);


  // добавляем экран управления фрамугами
  #ifdef USE_TEMP_SENSORS
    AbstractTFTScreen* windowScreen = new TFTWindowScreen();
    windowScreen->setup(this);
    TFTScreenInfo wsi; 
    wsi.screenName = "WINDOW"; 
    wsi.screen = windowScreen;  
    screens.push_back(wsi);
  #endif

  // добавляем экран управления каналами полива
  #ifdef USE_WATERING_MODULE
    AbstractTFTScreen* wateringScreen = new TFTWateringScreen();
    wateringScreen->setup(this);
    TFTScreenInfo watersi; 
    watersi.screenName = "WATER"; 
    watersi.screen = wateringScreen;  
    screens.push_back(watersi);
  #endif


  // добавляем экран управления досветкой
  #ifdef USE_LUMINOSITY_MODULE
    AbstractTFTScreen* lightScreen = new TFTLightScreen();
    lightScreen->setup(this);
    TFTScreenInfo lsi; 
    lsi.screenName = "LIGHT"; 
    lsi.screen = lightScreen;  
    screens.push_back(lsi);
  #endif

  // добавляем экран настроек
    AbstractTFTScreen* settingsScreen = new TFTSettingsScreen();
    settingsScreen->setup(this);
    TFTScreenInfo ssi; 
    ssi.screenName = "OPTIONS"; 
    ssi.screen = settingsScreen;  
    screens.push_back(ssi);
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::switchBacklight(uint8_t level)
{
 #ifdef USE_TFT_BACKLIGHT_MANAGE
      
      #if TFT_BACKLIGHT_DRIVE_MODE == DRIVE_DIRECT
      
        WORK_STATUS.PinMode(TFT_BACKLIGHT_DRIVE_PIN,OUTPUT);
        WORK_STATUS.PinWrite(TFT_BACKLIGHT_DRIVE_PIN,level);
        
      #elif TFT_BACKLIGHT_DRIVE_MODE == DRIVE_MCP23S17
      
        #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
        
          WORK_STATUS.MCP_SPI_PinMode(TFT_BACKLIGHT_MCP23S17_ADDRESS,TFT_BACKLIGHT_DRIVE_PIN,OUTPUT);
          WORK_STATUS.MCP_SPI_PinWrite(TFT_BACKLIGHT_MCP23S17_ADDRESS,TFT_BACKLIGHT_DRIVE_PIN,level);
          
        #endif
        
      #elif TFT_BACKLIGHT_DRIVE_MODE == DRIVE_MCP23017
      
        #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
        
          WORK_STATUS.MCP_I2C_PinMode(TFT_BACKLIGHT_MCP23017_ADDRESS,TFT_BACKLIGHT_DRIVE_PIN,OUTPUT);
          WORK_STATUS.MCP_I2C_PinWrite(TFT_BACKLIGHT_MCP23017_ADDRESS,TFT_BACKLIGHT_DRIVE_PIN,level);
          
        #endif
        
      #endif

    
  #endif // USE_TFT_BACKLIGHT_MANAGE  
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::lcdOn()
{
  #ifdef USE_TFT_BACKLIGHT_MANAGE
    switchBacklight(TFT_BACKLIGHT_ON);
  #endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::lcdOff()
{
  #ifdef USE_TFT_BACKLIGHT_MANAGE
    switchBacklight(TFT_BACKLIGHT_OFF);
  #endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::update(uint16_t dt)
{
  if(currentScreenIndex == -1) // ни разу не рисовали ещё ничего, исправляемся
  {
     switchToScreen("IDLE"); // переключаемся на стартовый экран, если ещё ни разу не показали ничего     
  }


  if(flags.isLCDOn)
  {
    if(millis() - idleTimer > TFT_OFF_DELAY)
    {
      flags.isLCDOn = false;
      lcdOff();
    }
  }
  else
  {
    // LCD currently off, check the touch on screen
    if(tftTouch->dataAvailable())
    {
      tftTouch->read();
      lcdOn();
      resetIdleTimer();
      flags.isLCDOn = true;
    }
  }

  // обновляем текущий экран
  TFTScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
  currentScreenInfo->screen->update(this,dt);
  yield();
  
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::switchToScreen(const char* screenName)
{
  
  // переключаемся на запрошенный экран
  for(size_t i=0;i<screens.size();i++)
  {
    TFTScreenInfo* si = &(screens[i]);
    if(!strcmp(si->screenName,screenName))
    {
      tftDC->fillScr(TFT_BACK_COLOR); // clear screen first      
      yield();
      currentScreenIndex = i;
      si->screen->onActivate(this);
      si->screen->update(this,0);
      yield();
      si->screen->draw(this);
      yield();
      resetIdleTimer(); // сбрасываем таймер ничегонеделанья
      break;
    }
  }

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenu::resetIdleTimer()
{
  idleTimer = millis();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif

