#include "UTFTMenu.h"
#include "AbstractModule.h"
#include "ModuleController.h"
#include "TempSensors.h"
#include "InteropStream.h"

#ifdef USE_TFT_MODULE
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTMenu* tftMenuManager;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawButtonsYield() // вызывается после отрисовки каждой кнопки
{
  tftMenuManager->updateBuzzer();
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
TFTInfoBox::TFTInfoBox(const char* caption, int width, int height, int x, int y)
{
  boxCaption = caption;
  boxWidth = width;
  boxHeight = height;
  posX = x;
  posY = y;
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
  menuManager->getRusPrinter()->print(caption,posX,posY);
  
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

  dc->setColor(INFO_BOX_BORDER_COLOR);
  dc->drawRoundRect(posX, curTop, posX+boxWidth, curTop + (boxHeight - fontHeight - INFO_BOX_CONTENT_PADDING));
  
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
#ifdef USE_TEMP_SENSORS
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTWindowScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTWindowScreen::TFTWindowScreen()
{
  
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
    int allChannelsLeft = (screenWidth - (WINDOWS_ALL_CHANNELS_BUTTON_WIDTH*3) - INFO_BOX_V_SPACING*2)/2;
    screenButtons->addButton( allChannelsLeft ,  buttonsTop, WINDOWS_ALL_CHANNELS_BUTTON_WIDTH,  WINDOWS_ALL_CHANNELS_BUTTON_HEIGHT, OPEN_ALL_LABEL);
    allChannelsLeft += WINDOWS_ALL_CHANNELS_BUTTON_WIDTH + INFO_BOX_V_SPACING;

    screenButtons->addButton( allChannelsLeft ,  buttonsTop, WINDOWS_ALL_CHANNELS_BUTTON_WIDTH,  WINDOWS_ALL_CHANNELS_BUTTON_HEIGHT, CLOSE_ALL_LABEL);
    allChannelsLeft += WINDOWS_ALL_CHANNELS_BUTTON_WIDTH + INFO_BOX_V_SPACING;

    int addedId = screenButtons->addButton( allChannelsLeft ,  buttonsTop, WINDOWS_ALL_CHANNELS_BUTTON_WIDTH,  WINDOWS_ALL_CHANNELS_BUTTON_HEIGHT, AUTO_MODE_LABEL);
    screenButtons->setButtonFontColor(addedId,WINDOWS_BUTTONS_TEXT_COLOR);

    buttonsTop += WINDOWS_ALL_CHANNELS_BUTTON_HEIGHT + INFO_BOX_V_SPACING;

    int computedButtonLeft = (screenWidth - (WINDOWS_CHANNELS_BUTTON_WIDTH*WINDOWS_CHANNELS_BUTTONS_PER_LINE) - ((WINDOWS_CHANNELS_BUTTONS_PER_LINE-1)*INFO_BOX_V_SPACING))/2;
    int curButtonLeft = computedButtonLeft;
  
    // теперь проходимся по кол-ву каналов и добавляем наши кнопки - дя каждого канала - по кнопке
    for(int i=0;i<SUPPORTED_WINDOWS;i++)
    {
       if( i > 0 && !(i%WINDOWS_CHANNELS_BUTTONS_PER_LINE))
       {
        buttonsTop += WINDOWS_CHANNELS_BUTTON_HEIGHT + INFO_BOX_V_SPACING;
        curButtonLeft = computedButtonLeft;
       }
       
       String* label = new String('#');
       *label += (i+1);
       labels.push_back(label);
       
       addedId = screenButtons->addButton(curButtonLeft ,  buttonsTop, WINDOWS_CHANNELS_BUTTON_WIDTH,  WINDOWS_CHANNELS_BUTTON_HEIGHT, label->c_str());
       screenButtons->setButtonFontColor(addedId,WINDOWS_BUTTONS_TEXT_COLOR);
       
       curButtonLeft += WINDOWS_CHANNELS_BUTTON_WIDTH + INFO_BOX_V_SPACING;


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
    int pressed_button = screenButtons->checkButtons();

    if(pressed_button != -1)
    {
      // есть клик на кнопку
      menuManager->buzzer(); // пискнули
    }
    
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

      return;
    }

    if(pressed_button == 1)
    {
      // открыть все окна
      ModuleInterop.QueryCommand(ctSET,F("STATE|WINDOW|ALL|OPEN"),false);
      return;
    }
    
    if(pressed_button == 2)
    {
      // закрыть все окна
      ModuleInterop.QueryCommand(ctSET,F("STATE|WINDOW|ALL|CLOSE"),false);
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
// TFTIdleScreen
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
  menuManager->updateBuzzer();
  
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
  curLeft += fontWidth*valueLen;
  menuManager->updateBuzzer();

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
          curLeft += fontWidth*fractLen;

          menuManager->updateBuzzer();
    
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

  menuManager->updateBuzzer();
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::drawStatusesInBox(TFTMenu* menuManager,TFTInfoBox* box, bool status, bool mode, const char* onStatusString, const char* offStatusString, const char* autoModeString, const char* manualModeString)
{
  TFTInfoBoxContentRect rc = box->getContentRect(menuManager);
  UTFT* dc = menuManager->getDC();

  dc->setColor(INFO_BOX_BACK_COLOR);
  dc->fillRect(rc.x,rc.y,rc.x+rc.w,rc.y+rc.h);

  menuManager->updateBuzzer();

  dc->setBackColor(INFO_BOX_BACK_COLOR);
  dc->setColor(TFT_FONT_COLOR);

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
    dc->setColor(MODE_ON_COLOR);
    toDraw = onStatusString;
  }
  else
  {
    dc->setColor(MODE_OFF_COLOR);
    toDraw = offStatusString;
  }

  int captionLen = menuManager->getRusPrinter()->print(toDraw,0, 0, 0, true);
  menuManager->updateBuzzer();
  
  curLeft = (rc.x + rc.w) - (captionLen*fontWidth);
  menuManager->getRusPrinter()->print(toDraw,curLeft,curTop);
  menuManager->updateBuzzer();

  curTop += fontHeight + INFO_BOX_CONTENT_PADDING;

  if(mode)
  {
    dc->setColor(MODE_ON_COLOR);
    toDraw = autoModeString;
  }
  else
  {
    dc->setColor(MODE_OFF_COLOR);
    toDraw = manualModeString;
  }

  captionLen = menuManager->getRusPrinter()->print(toDraw,0, 0, 0, true);
  menuManager->updateBuzzer();
  
  curLeft = (rc.x + rc.w) - (captionLen*fontWidth);
  menuManager->getRusPrinter()->print(toDraw,curLeft,curTop);
  menuManager->updateBuzzer();  

  
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

  int buttonsTop = menuManager->getDC()->getDisplayYSize() - TFT_IDLE_SCREEN_BUTTON_HEIGHT - 20; // координата Y для кнопок стартового экрана
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

  
  int startLeft = (screenWidth - (SENSOR_BOXES_PER_LINE*SENSOR_BOX_WIDTH + (SENSOR_BOXES_PER_LINE-1)*SENSOR_BOX_V_SPACING))/2;
  curInfoBoxLeft = startLeft;
  int sensorsTop = availStatusBoxes ? (INFO_BOX_V_SPACING + INFO_BOX_HEIGHT + SENSOR_BOX_V_SPACING*2) : SENSOR_BOX_V_SPACING;
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
void TFTIdleScreen::update(TFTMenu* menuManager,uint16_t dt)
{
  // Смотрим, какая кнопка нажата
  int pressed_button = screenButtons->checkButtons();

#ifdef USE_TEMP_SENSORS
  if(pressed_button == windowsButton)
  {
    menuManager->buzzer(); // пискнули
    menuManager->switchToScreen("WINDOW");
    return;
  }
#endif  

#ifdef USE_WATERING_MODULE
  if(pressed_button == waterButton)
  {
    menuManager->buzzer(); // пискнули
    menuManager->switchToScreen("WATER");
    return;
  }
#endif  

#ifdef USE_LUMINOSITY_MODULE
  if(pressed_button == lightButton)
  {
    menuManager->buzzer(); // пискнули
    menuManager->switchToScreen("LIGHT");
    return;
  }
#endif  

  if(pressed_button == optionsButton)
  {
    menuManager->buzzer(); // пискнули
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
        menuManager->updateBuzzer();
        drawSensorData(menuManager,sensors[i],i);
      }
      
    }
  #endif
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTIdleScreen::draw(TFTMenu* menuManager)
{
  menuManager->updateBuzzer();  
  screenButtons->drawButtons(drawButtonsYield); // рисуем наши кнопки

  //int availStatusBoxes = 0;

  #ifdef USE_TEMP_SENSORS // рисуем статус окон
    //availStatusBoxes++;
    menuManager->updateBuzzer();
    windowStatusBox->draw(menuManager);
    drawWindowStatus(menuManager);
  #endif

  #ifdef USE_WATERING_MODULE // рисуем статус полива
    //availStatusBoxes++;
    menuManager->updateBuzzer();
    waterStatusBox->draw(menuManager);
    drawWaterStatus(menuManager);
  #endif

  #ifdef USE_LUMINOSITY_MODULE // рисуем статус досветки
    //availStatusBoxes++;
    menuManager->updateBuzzer();
    lightStatusBox->draw(menuManager);
    drawLightStatus(menuManager);
  #endif

/*
  if(availStatusBoxes > 0)
  {
    // рисуем линию, разделяющую боксы статусов с боксами показаний датчиков
    int lineTop = INFO_BOX_V_SPACING + INFO_BOX_HEIGHT + INFO_BOX_V_SPACING;
    UTFT* dc = menuManager->getDC();
    dc->setColor(INFO_BOX_BORDER_COLOR);
    dc->drawLine(INFO_BOX_V_SPACING,lineTop,dc->getDisplayXSize() - INFO_BOX_V_SPACING,lineTop);
  }
*/

  #if TFT_SENSOR_BOXES_COUNT > 0
  
  //Тут отрисовка боксов с показаниями датчиков
  for(int i=0;i<TFT_SENSOR_BOXES_COUNT;i++)
  {
    if(!sensors[i])
      continue;

    menuManager->updateBuzzer();
    
    sensors[i]->draw(menuManager);
    TFTSensorInfo* sensorInfo = &(TFTSensors[i]);
    sensors[i]->drawCaption(menuManager,sensorInfo->sensorLabel);
    drawSensorData(menuManager,sensors[i],i,true); // тут перерисовываем показания по-любому
    sensorsTimer = 0; // сбрасываем таймер перерисовки показаний датчиков
  }

  menuManager->updateBuzzer();
  
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
  
  resetIdleTimer();
  
  tftDC = new UTFT(TFT_MODEL,TFT_RS_PIN,TFT_WR_PIN,TFT_CS_PIN,TFT_RST_PIN);
  tftTouch = new URTouch(TFT_TOUCH_CLK_PIN,TFT_TOUCH_CS_PIN,TFT_TOUCH_DIN_PIN,TFT_TOUCH_DOUT_PIN,TFT_TOUCH_IRQ_PIN);
  
  tftDC->InitLCD(LANDSCAPE);
  tftDC->fillScr(TFT_BACK_COLOR);
  tftDC->setFont(BigRusFont);

  tftTouch->InitTouch(LANDSCAPE);
  tftTouch->setPrecision(PREC_HI);
  
  rusPrint.init(tftDC);


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

  #ifdef USE_BUZZER_ON_TOUCH
  
    //TODO: Тут инициализация пищалки, проверить !!!
    
      #if BUZZER_DRIVE_MODE == DRIVE_DIRECT
      
        WORK_STATUS.PinMode(BUZZER_DRIVE_PIN,OUTPUT);
        WORK_STATUS.PinWrite(BUZZER_DRIVE_PIN,BUZZER_OFF);
        
      #elif BUZZER_DRIVE_MODE == DRIVE_MCP23S17
      
        #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
        
          WORK_STATUS.MCP_SPI_PinMode(BUZZER_MCP23S17_ADDRESS,BUZZER_DRIVE_PIN,OUTPUT);
          WORK_STATUS.MCP_SPI_PinWrite(BUZZER_MCP23S17_ADDRESS,BUZZER_DRIVE_PIN,BUZZER_OFF);
          
        #endif
        
      #elif BUZZER_DRIVE_MODE == DRIVE_MCP23017
      
        #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
        
          WORK_STATUS.MCP_I2C_PinMode(BUZZER_MCP23017_ADDRESS,BUZZER_DRIVE_PIN,OUTPUT);
          WORK_STATUS.MCP_I2C_PinWrite(BUZZER_MCP23017_ADDRESS,BUZZER_DRIVE_PIN,BUZZER_OFF);
          
        #endif
        
      #endif

    // пискнем баззером при инициализации экрана
    buzzer(); 
    
  #endif // USE_BUZZER_ON_TOUCH
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::lcdOn()
{
  //TODO: Включение дисплея!!!
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::lcdOff()
{
  //TODO: Выключение дисплея!!!
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::update(uint16_t dt)
{

  updateBuzzer();
  
  if(currentScreenIndex == -1) // ни разу не рисовали ещё ничего, исправляемся
  {
     switchToScreen("IDLE"); // переключаемся на стартовый экран, если ещё ни разу не показали ничего     
  }

  // обновляем текущий экран
  TFTScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
  currentScreenInfo->screen->update(this,dt);

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
      currentScreenIndex = i;
      si->screen->update(this,0);
      si->screen->draw(this);
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
void TFTMenu::updateBuzzer()
{
  #ifdef USE_BUZZER_ON_TOUCH
    if(flags.buzzerActive)
    {
      if(millis() - buzzerTimer > BUZZER_DURATION)
      {
        flags.buzzerActive = false;
        
        //TODO: Тут выключаем пищалку, проверить !!!
        
          #if BUZZER_DRIVE_MODE == DRIVE_DIRECT
          
            WORK_STATUS.PinWrite(BUZZER_DRIVE_PIN,BUZZER_OFF);
            
          #elif BUZZER_DRIVE_MODE == DRIVE_MCP23S17
          
            #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
            
              WORK_STATUS.MCP_SPI_PinWrite(BUZZER_MCP23S17_ADDRESS,BUZZER_DRIVE_PIN,BUZZER_OFF);
              
            #endif
            
          #elif BUZZER_DRIVE_MODE == DRIVE_MCP23017
          
            #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
            
              WORK_STATUS.MCP_I2C_PinWrite(BUZZER_MCP23017_ADDRESS,BUZZER_DRIVE_PIN,BUZZER_OFF);
              
            #endif
            
          #endif
              
      }
    }
  #endif // USE_BUZZER_ON_TOUCH  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::buzzer()
{
  #ifdef USE_BUZZER_ON_TOUCH
  
    flags.buzzerActive = true;
    buzzerTimer = millis();
    
    //TODO: тут включаем пищалку, проверить !!!
      
      #if BUZZER_DRIVE_MODE == DRIVE_DIRECT
      
        WORK_STATUS.PinWrite(BUZZER_DRIVE_PIN,BUZZER_OFF);
        
      #elif BUZZER_DRIVE_MODE == DRIVE_MCP23S17
      
        #if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
        
          WORK_STATUS.MCP_SPI_PinWrite(BUZZER_MCP23S17_ADDRESS,BUZZER_DRIVE_PIN,BUZZER_ON);
          
        #endif
        
      #elif BUZZER_DRIVE_MODE == DRIVE_MCP23017
      
        #if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
        
          WORK_STATUS.MCP_I2C_PinWrite(BUZZER_MCP23017_ADDRESS,BUZZER_DRIVE_PIN,BUZZER_ON);
          
        #endif
        
      #endif
       
    
  #endif  // USE_BUZZER_ON_TOUCH
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif

