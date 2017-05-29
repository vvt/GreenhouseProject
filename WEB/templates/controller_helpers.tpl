{* Smarty *}

<script type="text/javascript" src="js/controller_view.js"></script>

<script type='text/javascript'>
//-----------------------------------------------------------------------------------------------------
// наш контроллер
var controller = new Controller({$selected_controller.controller_id},'{$selected_controller.controller_name}','{$selected_controller.controller_address}');
// представление
var view = new View(controller);
{literal}
//-----------------------------------------------------------------------------------------------------
var tempAndMotorDataRequested = false; // флаг, что мы запросили настройки температур и моторов
//-----------------------------------------------------------------------------------------------------
// обрабатываем запрос на редактирование имени
view.OnEditSensorName = function(mnemonic, sensor, row)
{
 $("#edit_sensor_name").val(mnemonic ? mnemonic.DisplayName : ""); 
  
  $("#sensor_name_dialog").dialog({modal:true, buttons: [{text: "Изменить", click: function(){
  
    var new_sensor_name = $("#edit_sensor_name").val();
    
    if(new_sensor_name != '')
    {
    
        if(!mnemonic)
        {
          // добавить
          mnemonic = new SensorMnemonicName(sensor.Index,sensor.ModuleName,"");
          controller.SensorsNames.Add(mnemonic);
          mnemonic.setName(new_sensor_name);
        }
        else
        {
          // обновить
          mnemonic.setName(new_sensor_name);
        }  
        
        // обновляем строку в таблице
        row.children('#index').html(new_sensor_name);  
    }
    
    $(this).dialog("close");

  
  } }
  
  , {text: "Отмена", click: function(){$(this).dialog("close");} }
  ] });   
  
  
}
//-----------------------------------------------------------------------------------------------------
function resetFlowData()
{
  promptMessage("Вы уверены, что хотите сбросить показания счётчиков?",function(){
  
    controller.queryCommand(false,'FLOW|RST',function(obj,answer){
    
      controller.FlowIncrementalLitres = 0;
      controller.FlowIncrementalLitres2 = 0;
      
      $('#flow_incremental').html(controller.FlowIncrementalLitres);
      $('#flow_incremental2').html(controller.FlowIncrementalLitres2);
    
      showMessage("Данные успешно сброшены!");
    
    });
  
  });
}
//-----------------------------------------------------------------------------------------------------
function showMessage(message)
{
  $('#message_dialog_message').html(message);

  $("#message_dialog").dialog({modal:true, buttons: [
    {text: "ОК", click: function(){$(this).dialog("close");} }
  ] });     
}
//-----------------------------------------------------------------------------------------------------
function promptMessage(message, yesFunc, cancelFunc)
{
  $('#prompt_dialog_message').html(message);

  $("#prompt_dialog").dialog({modal:true, buttons: [
    {text: "ОК", click: function(){$(this).dialog("close"); if(yesFunc) yesFunc(); } },
    {text: "Отмена", click: function(){$(this).dialog("close");  if(cancelFunc) cancelFunc(); } }
  ] });     
}
//-----------------------------------------------------------------------------------------------------
function setWaterChannelButtonState(button, channelNumber, channelEnabled)
{
  //button.removeAttr('disabled').removeClass('water-intermediate');

  var buttonCaption = "Канал #" + channelNumber;
  buttonCaption += channelEnabled ? " включен. Выключить!" : " выключен. Включить!";
  if(channelEnabled)
    button.addClass('water-on').removeClass('water-off');
  else
    button.addClass('water-off').removeClass('water-on');
    
   button.html(buttonCaption);

}
//-----------------------------------------------------------------------------------------------------
var lastIsOnline = controller.IsOnline();
// обработчик онлайн-статуса контроллера
controller.OnStatus = function(obj)
{
  var is_online = controller.IsOnline();
    
  $('#reset_controller_link').toggle(is_online);
  
  if(!lastIsOnline && is_online)
  {
    controller.querySensorNames(); // запрашиваем список имён датчиков из БД
    controller.queryModules(); // запрашиваем модули у контроллера
  }
  
  lastIsOnline = is_online; 
  
  if(is_online)
  {
    if(!tempAndMotorDataRequested)
    {
      tempAndMotorDataRequested = true;
      controller.queryMotorInterval();
      controller.queryTemperatureSettings();
    }
  
    $('#wait_block').hide();
    $('#offline_block').hide();
    $('#online_block').show();
  }
  else
  {
    tempAndMotorDataRequested = false;
    tempAndMotorSettingsCounter = 0;
    
    $('#wait_block').hide();
    $('#offline_block').show();
    $('#online_block').hide();
  }
  
  controller.queryCommand(true,'WATER|STATEMASK',function(obj,answer){
  
     var list = $('#waterChannelsState');
  
     if(!answer.IsOK || answer.Params.length < 4)
     {
      list.toggle(false);
      return;
     }
     
      list.toggle(true);
      
      var channelsCount = parseInt(answer.Params[2]);
      
      var statemask = answer.Params[3];
      
      var buttons = list.children('button');
      var countButtons = buttons.length;
      
      if(countButtons > channelsCount)
      {
        list.empty();
        countButtons = 0;
      }
      
      while(countButtons < channelsCount)
      {
        var button = $("<button/>",{'data-channel-id' : countButtons, 'class' : 'water-channel-button'});
        button.appendTo(list);
        countButtons++;
      }
      
      // теперь назначаем кнопкам надписи
      for(var i=0;i<channelsCount;i++)
      {
        var button = list.find("[data-channel-id=" + i + "]");
        button.button();
        //button.removeAttr('disabled').removeClass('water-intermediate');

        var channelEnabled = statemask.substring(i,i+1) == '1' ? true : false;        
        button.attr('data-channel-enabled', channelEnabled ? 1 : 0);
        setWaterChannelButtonState(button,(i+1),channelEnabled);
              
        button.off('click').click(function(){
        
          var btn = $(this);
          var channelId = parseInt(btn.attr('data-channel-id'));
          var isEnabled = parseInt(btn.attr('data-channel-enabled')) == 1;
          
          //btn.attr('disabled','disabled'); 
          //btn.removeClass('water-on').removeClass('water-off').addClass('water-intermediate');
          
          btn.attr('data-channel-enabled', !isEnabled ? 1 : 0);
          
          setWaterChannelButtonState(btn,(channelId+1),!isEnabled);
          
          var command = "WATER|";
          if(isEnabled)
            command += "OFF|";
          else
            command += "ON|";
            
            command += channelId;
            
            controller.queryCommand(false,command,function(obj,answer){});
        
        });
        
      } // for
      

  });
  
  
  controller.queryCommand(true,'STATE|WINDOW|STATEMASK',function(obj,answer){
    
        if(!answer.IsOK)
         {
          $('#windowsChannelsState').toggle(false);
          return;
         }
         
          $('#windowsChannelsState').toggle(true);
         
          var totalWindows = parseInt(answer.Params[3]);
          var byteHolder = answer.Params[4];
         
          var numBits = totalWindows * 2;
          
          var numBytes = numBits / 8;
          if (numBits % 8 > 0)
              numBytes++;
              
           
           var optionsCount = $('#windowsChannelsState').find("option").length;
           
           while(optionsCount < totalWindows)
           {
              $("<option/>", { 'value' : optionsCount, 'style' : 'padding:2px;padding-left:8px;padding-right:8px;'}).appendTo('#windowsChannelsState');
              optionsCount++;
           }
           
           var windowIndex = 0; // текущая позиция записи в массив состояний окон
           
            for (var i = 0; i < numBytes; i++)
            {
                var strByte = '0x' + byteHolder.substring(0, 2);
                byteHolder = byteHolder.substring(2);
                
                var bState = parseInt(strByte);
                // сконвертировали строку в байт, теперь смотрим
                
                // в одном байте у нас - состояние 4-х окон
                for (var bitPos = 0; bitPos < 8; bitPos += 2)
                {
                    // получаем младший бит
                    var bLow = BitIsSet(bState, bitPos);
                    // получаем старший бит
                    var bHigh = BitIsSet(bState, (bitPos+1));

                    var wCapt = "Окно номер #" + (windowIndex+1) + ': ';
                    var ws = "<нет данных>";
                    
                    var optionClass = '';
                    
                    // проверяем комбинации
                    if (!bLow && !bHigh)
                    {
                        ws = "закрыто";
                    }
                    else
                    if (!bHigh && bLow)
                    {
                        ws = "открывается";
                    }
                    else
                    if (bHigh && !bLow)
                    {
                        ws = "закрывается";
                    }
                    else
                    if (bHigh && bLow)
                    {
                        ws = "открыто";
                        optionClass = 'auto_mode';
                    }
                    
                    // сохраняем состояние
                    var opt = $('#windowsChannelsState').find("option")[windowIndex];
                    $(opt).text(wCapt + ws).removeClass();
                    
                    if(optionClass != '')
                      $(opt).addClass(optionClass);
                    
                    windowIndex++; // переходим на следующее окно
                } // for

            } // for
          
         
        });
  
};
//-----------------------------------------------------------------------------------------------------
// ждём получения настроек температур и моторов и только тогла показываем ссылку.
// поскольку это две разные команды - надо дождаться результата отработки обеих.
var tempAndMotorSettingsCounter = 0;
function waitForTempAndMotorData(obj)
{
  tempAndMotorSettingsCounter++;
  if(tempAndMotorSettingsCounter > 1)
  {
     $('#temp_motors_settings').show();
  }
}
//-----------------------------------------------------------------------------------------------------
// событие "Получен интервал работы моторов"
controller.OnQueryMotorWorkInterval = waitForTempAndMotorData;
// событие "Получены настройки температур"
controller.OnQueryTemperatureSettings = waitForTempAndMotorData;
//-----------------------------------------------------------------------------------------------------
// событие "Получен список модулей в прошивке"
controller.OnGetModulesList = function(obj)
{
  
};
//-----------------------------------------------------------------------------------------------------
// событие "Получен и разобран слепок состояния контроллера", приходит после вызова controller.queryState()
controller.OnUpdate = function(obj, answer)
{
  if(answer.IsOK)
  {
  
    // обновляем статус надписей и кнопок для окон, полива и досветки
    updateWindowsState();
    updateWaterState();
    updateLightState();
    updatePHState();
      
    //$('#WELCOME_MENU').show();
  
    $('#STATUS_MENU').show();
    
    if(!lastVisibleContent)
      content($('#STATUS_MENU'));
    
    if(controller.TemperatureList.List.length > 0)
    {
      $('#TEMPERATURE_MENU').show();
      view.fillTemperatureList('#TEMPERATURE_LIST'); // заполняем список показаний температур
    }
    else
      $('#TEMPERATURE_MENU').hide();
    
    if(controller.HumidityList.List.length > 0)
    {
      view.fillHumidityList('#HUMIDITY_LIST'); // заполняем список показаний влажности
      $('#HUMIDITY_MENU').show();
    }
    else
      $('#HUMIDITY_MENU').hide();
    
    if(controller.LuminosityList.List.length > 0)
    {
      view.fillLuminosityList('#LUMINOSITY_LIST'); // заполняем список показаний освещенности
      $('#LIGHT_MENU').show();
    }
    else
      $('#LIGHT_MENU').hide();

    if(controller.SoilMoistureList.List.length > 0)
    {
      view.fillSoilMoistureList('#SOIL_LIST'); // заполняем список показаний датчиков влажности почвы
      $('#SOIL_MENU').show();
    }
    else
      $('#SOIL_MENU').hide();
      
    if(controller.PHList.List.length > 0)
    {
      view.fillPHList('#PH_LIST'); // заполняем список показаний датчиков pH
      $('#PH_MENU').show();
    }
    else
      $('#PH_MENU').hide();      


    if(controller.Modules.includes('FLOW'))
    {
      $('#flow_instant').html(controller.FlowInstantLitres);
      $('#flow_incremental').html(controller.FlowIncrementalLitres);

      $('#flow_instant2').html(controller.FlowInstantLitres2);
      $('#flow_incremental2').html(controller.FlowIncrementalLitres2);
      
      $('#flow1_box').toggle(controller.Flow1Present);
      $('#flow2_box').toggle(controller.Flow2Present);
      
      $('#FLOW_MENU').show(controller.Flow1Present || controller.Flow2Present);
    }
    else
      $('#FLOW_MENU').hide();  


  } // is ok
};
//-----------------------------------------------------------------------------------------------------
function updatePHState()
{
  $('#ph_controller_status').toggle(controller.Modules.includes('PH'));
  $('#ph_flow_add').html( controller.IsPHFlowAddOn ? $('#ph_state_on').html() : $('#ph_state_off').html());
  $('#ph_mix_pump').html( controller.IsPHMixPumpOn ? $('#ph_state_on').html() : $('#ph_state_off').html());
  $('#ph_plus_pump').html( controller.IsPHPlusPumpOn ? $('#ph_state_on').html() : $('#ph_state_off').html());
  $('#ph_minus_pump').html( controller.IsPHMinusPumpOn ? $('#ph_state_on').html() : $('#ph_state_off').html());
  
}
//-----------------------------------------------------------------------------------------------------
function updateWindowsState()
{
  $('#windows_controller_status').toggle(controller.Modules.includes('STATE'));

    $('#window_state').html( controller.IsWindowsOpen ? $('#window_state_on').html() : $('#window_state_off').html());
    $('#window_mode').html( controller.IsWindowsAutoMode ? $('#mode_auto').html() : $('#mode_manual').html());
    $('#toggler_windows_mode').button({ label: !controller.IsWindowsAutoMode ? $('#mode_auto_switch').html() : $('#mode_manual_switch').html() });
    $('#toggler_windows').button({ label: controller.IsWindowsOpen ? $('#toggle_close').html() : $('#toggle_open').html() } );
}
//-----------------------------------------------------------------------------------------------------
function updateWaterState()
{
  $('#water_controller_status').toggle(controller.Modules.includes('WATER'));

    $('#water_state').html( controller.IsWaterOn ? $('#water_state_on').html() : $('#water_state_off').html());
    $('#water_mode').html( controller.IsWaterAutoMode ? $('#mode_auto').html() : $('#mode_manual').html());
    $('#toggler_water_mode').button({ label: !controller.IsWaterAutoMode ? $('#mode_auto_switch').html() : $('#mode_manual_switch').html() });
    $('#toggler_water').button({ label:controller.IsWaterOn ? $('#toggle_off').html() : $('#toggle_on').html() });

}
//-----------------------------------------------------------------------------------------------------
function updateLightState()
{
 $('#light_controller_status').toggle(controller.Modules.includes('LIGHT'));

    $('#light_state').html( controller.IsLightOn ? $('#light_state_on').html() : $('#light_state_off').html());
    $('#light_mode').html( controller.IsLightAutoMode ? $('#mode_auto').html() : $('#mode_manual').html());
    $('#toggler_light_mode').button({ label: !controller.IsLightAutoMode ? $('#mode_auto_switch').html() : $('#mode_manual_switch').html() });
    $('#toggler_light').button({ label:controller.IsLightOn ? $('#toggle_off').html() : $('#toggle_on').html() } );
}
//-----------------------------------------------------------------------------------------------------
// обновляем данные с контроллера
function updateControllerData()
{
  //if(controller.IsOnline())
    controller.queryState();
}
//-----------------------------------------------------------------------------------------------------
function addNewWidget()
{
  alert('Not implemented!');
}
//-----------------------------------------------------------------------------------------------------
$(document).ready(function(){

lastVisibleContent = null;//$('#WELCOME_MENU_CONTENT');

$('#edit_motor_time').forceNumericOnly();
$('#edit_t_open').forceNumericOnly();
$('#edit_t_close').forceNumericOnly();


controller.querySensorNames(); // запрашиваем список имён датчиков из БД
controller.queryModules(); // запрашиваем модули у контроллера

updateControllerData();
window.setInterval(updateControllerData,5000); // повторяем опрос состояния каждые 5 секунд

  $('#add_widget_button').button({
      icons: {
        primary: "ui-icon-plusthick"
      }
    }).off('click').click(function() {
    
      addNewWidget();
    
    });


  $( "#toggler_windows, #toggler_light, #toggler_water, #temp_motors_settings" ).button({
      icons: {
        primary: "ui-icon-gear"
      }
    });
    
    $('#temp_motors_settings').css('width','100%');

  $( "#toggler_windows_mode, #toggler_light_mode, #toggler_water_mode" ).button({
      icons: {
        primary: "ui-icon-refresh"
      }
    });
    
    $('#reset_flow_btn').button();
    

});
//-----------------------------------------------------------------------------------------------------
// редактируем настройки температур и времени открытия моторов
function editTempSettings()
{
  
  $("#edit_t_open").val(controller.OpenTemperature); 
  $("#edit_t_close").val(controller.CloseTemperature); 
  $("#edit_motor_time").val(parseInt(controller.MotorWorkInterval/1000)); 
  
  $("#temp_settings_dialog").dialog({modal:true, buttons: [{text: "Изменить", click: function(){
  
    var openTemp = $("#edit_t_open").val();
    var closeTemp =  $("#edit_t_close").val();
    var motorWorkInterval =  parseInt($("#edit_motor_time").val());
    
    controller.saveTemperatureSettings(openTemp,closeTemp);
    controller.saveMotorInterval(motorWorkInterval);
    
    $(this).dialog("close");

  
  } }
  
  , {text: "Отмена", click: function(){$(this).dialog("close");} }
  ] });   

  
}
//-----------------------------------------------------------------------------------------------------
</script>
{/literal}