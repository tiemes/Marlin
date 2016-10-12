#include "ultralcd.h"
 #if ENABLED(ULTRA_LCD)
 #include "Marlin.h"
 #include "language.h"
 #include "cardreader.h"
 #include "temperature.h"
 #include "stepper.h"
 #include "configuration_store.h"
 
 int8_t encoderDiff; // updated from interrupt context and added to encoderPosition every LCD update
 
 bool encoderRateMultiplierEnabled;
 int32_t lastEncoderMovementMillis;
 
 int plaPreheatHotendTemp;
 int plaPreheatHPBTemp;
 int plaPreheatFanSpeed;
 
 int absPreheatHotendTemp;
 int absPreheatHPBTemp;
 int absPreheatFanSpeed;
 
 #if ENABLED(FILAMENT_LCD_DISPLAY)
   millis_t previous_lcd_status_ms = 0;
 #endif
 
 // Function pointer to menu functions.
 typedef void (*menuFunc_t)();
 
 uint8_t lcd_status_message_level;
 char lcd_status_message[3 * LCD_WIDTH + 1] = WELCOME_MSG; // worst case is kana with up to 3*LCD_WIDTH+1
 
 #if ENABLED(DOGLCD)
   #include "dogm_lcd_implementation.h"
 #else
   #include "ultralcd_implementation_hitachi_HD44780.h"
 #endif
 
 // The main status screen
 static void lcd_status_screen();
 
 #if ENABLED(ULTIPANEL)
 
   #if HAS_POWER_SWITCH
     extern bool powersupply;
   #endif
   const float manual_feedrate[] = MANUAL_FEEDRATE;
   static void lcd_main_menu();
   static void lcd_tune_menu();
   static void lcd_prepare_menu();
   static void lcd_move_menu();
   static void lcd_control_menu();
   static void lcd_control_temperature_menu();
   static void lcd_control_temperature_preheat_pla_settings_menu();
   static void lcd_control_temperature_preheat_abs_settings_menu();
   static void lcd_control_motion_menu();
   static void lcd_control_volumetric_menu();
 
   #if ENABLED(HAS_LCD_CONTRAST)
     static void lcd_set_contrast();
   #endif
 
   #if ENABLED(FWRETRACT)
     static void lcd_control_retract_menu();
   #endif
 
   #if ENABLED(DELTA_CALIBRATION_MENU)
     static void lcd_delta_calibrate_menu();
   #endif
 
   #if ENABLED(MANUAL_BED_LEVELING)
     #include "mesh_bed_leveling.h"
     static void _lcd_level_bed();
     static void _lcd_level_bed_homing();
     static void lcd_level_bed();
   #endif
 
   /* Different types of actions that can be used in menu items. */
   static void menu_action_back(menuFunc_t data);
   static void menu_action_submenu(menuFunc_t data);
   static void menu_action_gcode(const char* pgcode);
   static void menu_action_function(menuFunc_t data);
   static void menu_action_setting_edit_bool(const char* pstr, bool* ptr);
   static void menu_action_setting_edit_int3(const char* pstr, int* ptr, int minValue, int maxValue);
   static void menu_action_setting_edit_float3(const char* pstr, float* ptr, float minValue, float maxValue);
   static void menu_action_setting_edit_float32(const char* pstr, float* ptr, float minValue, float maxValue);
   static void menu_action_setting_edit_float43(const char* pstr, float* ptr, float minValue, float maxValue);
   static void menu_action_setting_edit_float5(const char* pstr, float* ptr, float minValue, float maxValue);
   static void menu_action_setting_edit_float51(const char* pstr, float* ptr, float minValue, float maxValue);
   static void menu_action_setting_edit_float52(const char* pstr, float* ptr, float minValue, float maxValue);
   static void menu_action_setting_edit_long5(const char* pstr, unsigned long* ptr, unsigned long minValue, unsigned long maxValue);
   static void menu_action_setting_edit_callback_bool(const char* pstr, bool* ptr, menuFunc_t callbackFunc);
   static void menu_action_setting_edit_callback_int3(const char* pstr, int* ptr, int minValue, int maxValue, menuFunc_t callbackFunc);
   static void menu_action_setting_edit_callback_float3(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
   static void menu_action_setting_edit_callback_float32(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
   static void menu_action_setting_edit_callback_float43(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
   static void menu_action_setting_edit_callback_float5(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
   static void menu_action_setting_edit_callback_float51(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
   static void menu_action_setting_edit_callback_float52(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
   static void menu_action_setting_edit_callback_long5(const char* pstr, unsigned long* ptr, unsigned long minValue, unsigned long maxValue, menuFunc_t callbackFunc);
 
   #if ENABLED(SDSUPPORT)
     static void lcd_sdcard_menu();
     static void menu_action_sdfile(const char* filename, char* longFilename);
     static void menu_action_sddirectory(const char* filename, char* longFilename);
   #endif
 
   #define ENCODER_FEEDRATE_DEADZONE 10
 
   #if DISABLED(LCD_I2C_VIKI)
     #ifndef ENCODER_STEPS_PER_MENU_ITEM
       #define ENCODER_STEPS_PER_MENU_ITEM 5
     #endif
     #ifndef ENCODER_PULSES_PER_STEP
       #define ENCODER_PULSES_PER_STEP 1
     #endif
   #else
     #ifndef ENCODER_STEPS_PER_MENU_ITEM
       #define ENCODER_STEPS_PER_MENU_ITEM 2 // VIKI LCD rotary encoder uses a different number of steps per rotation
     #endif
     #ifndef ENCODER_PULSES_PER_STEP
       #define ENCODER_PULSES_PER_STEP 1
     #endif
   #endif
 
 
   /* Helper macros for menus */
 
   /**
    * START_MENU generates the init code for a menu function
    */
   #define START_MENU() do { \
     encoderRateMultiplierEnabled = false; \
     if (encoderPosition > 0x8000) encoderPosition = 0; \
     uint8_t encoderLine = encoderPosition / ENCODER_STEPS_PER_MENU_ITEM; \
     if (encoderLine < currentMenuViewOffset) currentMenuViewOffset = encoderLine; \
     uint8_t _lineNr = currentMenuViewOffset, _menuItemNr; \
     bool wasClicked = LCD_CLICKED, itemSelected; \
     for (uint8_t _drawLineNr = 0; _drawLineNr < LCD_HEIGHT; _drawLineNr++, _lineNr++) { \
       _menuItemNr = 0;
 
   /**
    * MENU_ITEM generates draw & handler code for a menu item, potentially calling:
    *
    *   lcd_implementation_drawmenu_[type](sel, row, label, arg3...)
    *   menu_action_[type](arg3...)
    *
    * Examples:
    *   MENU_ITEM(back, MSG_WATCH, lcd_status_screen)
    *     lcd_implementation_drawmenu_back(sel, row, PSTR(MSG_WATCH), lcd_status_screen)
    *     menu_action_back(lcd_status_screen)
    *
    *   MENU_ITEM(function, MSG_PAUSE_PRINT, lcd_sdcard_pause)
    *     lcd_implementation_drawmenu_function(sel, row, PSTR(MSG_PAUSE_PRINT), lcd_sdcard_pause)
    *     menu_action_function(lcd_sdcard_pause)
    *
    *   MENU_ITEM_EDIT(int3, MSG_SPEED, &feedrate_multiplier, 10, 999)
    *   MENU_ITEM(setting_edit_int3, MSG_SPEED, PSTR(MSG_SPEED), &feedrate_multiplier, 10, 999)
    *     lcd_implementation_drawmenu_setting_edit_int3(sel, row, PSTR(MSG_SPEED), PSTR(MSG_SPEED), &feedrate_multiplier, 10, 999)
    *     menu_action_setting_edit_int3(PSTR(MSG_SPEED), &feedrate_multiplier, 10, 999)
    *
    */
   #define MENU_ITEM(type, label, args...) do { \
     if (_menuItemNr == _lineNr) { \
       itemSelected = encoderLine == _menuItemNr; \
       if (lcdDrawUpdate) \
         lcd_implementation_drawmenu_ ## type(itemSelected, _drawLineNr, PSTR(label), ## args); \
       if (wasClicked && itemSelected) { \
         lcd_quick_feedback(); \
         menu_action_ ## type(args); \
         return; \
       } \
     } \
     _menuItemNr++; \
   } while(0)
 
   #if ENABLED(ENCODER_RATE_MULTIPLIER)
 
     //#define ENCODER_RATE_MULTIPLIER_DEBUG  // If defined, output the encoder steps per second value
 
     /**
      * MENU_MULTIPLIER_ITEM generates drawing and handling code for a multiplier menu item
      */
     #define MENU_MULTIPLIER_ITEM(type, label, args...) do { \
       if (_menuItemNr == _lineNr) { \
         itemSelected = encoderLine == _menuItemNr; \
         if (lcdDrawUpdate) \
           lcd_implementation_drawmenu_ ## type(itemSelected, _drawLineNr, PSTR(label), ## args); \
         if (wasClicked && itemSelected) { \
           lcd_quick_feedback(); \
           encoderRateMultiplierEnabled = true; \
           lastEncoderMovementMillis = 0; \
           menu_action_ ## type(args); \
           return; \
         } \
       } \
       _menuItemNr++; \
     } while(0)
   #endif //ENCODER_RATE_MULTIPLIER
 
   #define MENU_ITEM_DUMMY() do { _menuItemNr++; } while(0)
   #define MENU_ITEM_EDIT(type, label, args...) MENU_ITEM(setting_edit_ ## type, label, PSTR(label), ## args)
   #define MENU_ITEM_EDIT_CALLBACK(type, label, args...) MENU_ITEM(setting_edit_callback_ ## type, label, PSTR(label), ## args)
   #if ENABLED(ENCODER_RATE_MULTIPLIER)
     #define MENU_MULTIPLIER_ITEM_EDIT(type, label, args...) MENU_MULTIPLIER_ITEM(setting_edit_ ## type, label, PSTR(label), ## args)
     #define MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(type, label, args...) MENU_MULTIPLIER_ITEM(setting_edit_callback_ ## type, label, PSTR(label), ## args)
   #else //!ENCODER_RATE_MULTIPLIER
     #define MENU_MULTIPLIER_ITEM_EDIT(type, label, args...) MENU_ITEM(setting_edit_ ## type, label, PSTR(label), ## args)
     #define MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(type, label, args...) MENU_ITEM(setting_edit_callback_ ## type, label, PSTR(label), ## args)
   #endif //!ENCODER_RATE_MULTIPLIER
   #define END_MENU() \
       if (encoderLine >= _menuItemNr) { encoderPosition = _menuItemNr * ENCODER_STEPS_PER_MENU_ITEM - 1; encoderLine = encoderPosition / ENCODER_STEPS_PER_MENU_ITEM; }\
       if (encoderLine >= currentMenuViewOffset + LCD_HEIGHT) { currentMenuViewOffset = encoderLine - LCD_HEIGHT + 1; lcdDrawUpdate = 1; _lineNr = currentMenuViewOffset - 1; _drawLineNr = -1; } \
       } } while(0)
 
   /** Used variables to keep track of the menu */
   volatile uint8_t buttons;  //the last checked buttons in a bit array.
   #if ENABLED(REPRAPWORLD_KEYPAD)
     volatile uint8_t buttons_reprapworld_keypad; // to store the keypad shift register values
   #endif
 
   #if ENABLED(LCD_HAS_SLOW_BUTTONS)
     volatile uint8_t slow_buttons; // Bits of the pressed buttons.
   #endif
   uint8_t currentMenuViewOffset;              /* scroll offset in the current menu */
   millis_t next_button_update_ms;
   uint8_t lastEncoderBits;
   uint32_t encoderPosition;
   #if PIN_EXISTS(SD_DETECT)
     uint8_t lcd_sd_status;
   #endif
 
 #endif // ULTIPANEL
 
 menuFunc_t currentMenu = lcd_status_screen; /* function pointer to the currently active menu */
 millis_t next_lcd_update_ms;
 uint8_t lcd_status_update_delay;
 bool ignore_click = false;
 bool wait_for_unclick;
 uint8_t lcdDrawUpdate = 2;                  /* Set to none-zero when the LCD needs to draw, decreased after every draw. Set to 2 in LCD routines so the LCD gets at least 1 full redraw (first redraw is partial) */
 
 //prevMenu and prevEncoderPosition are used to store the previous menu location when editing settings.
 menuFunc_t prevMenu = NULL;
 uint16_t prevEncoderPosition;
 //Variables used when editing values.
 const char* editLabel;
 void* editValue;
 int32_t minEditValue, maxEditValue;
 menuFunc_t callbackFunc;
 
 // place-holders for Ki and Kd edits
 float raw_Ki, raw_Kd;
 
 /**
  * General function to go directly to a menu
  */
 static void lcd_goto_menu(menuFunc_t menu, const bool feedback = false, const uint32_t encoder = 0) {
   if (currentMenu != menu) {
     currentMenu = menu;
     #if ENABLED(NEWPANEL)
       encoderPosition = encoder;
       if (feedback) lcd_quick_feedback();
     #endif
     // For LCD_PROGRESS_BAR re-initialize the custom characters
     #if ENABLED(LCD_PROGRESS_BAR)
       lcd_set_custom_characters(menu == lcd_status_screen);
     #endif
   }
 }
 
 /**
  *
  * "Info Screen"
  *
  * This is very display-dependent, so the lcd implementation draws this.
  */
 
 static void lcd_status_screen() {
   encoderRateMultiplierEnabled = false;
 
   #if ENABLED(LCD_PROGRESS_BAR)
     millis_t ms = millis();
     #if DISABLED(PROGRESS_MSG_ONCE)
       if (ms > progress_bar_ms + PROGRESS_BAR_MSG_TIME + PROGRESS_BAR_BAR_TIME) {
         progress_bar_ms = ms;
       }
     #endif
     #if PROGRESS_MSG_EXPIRE > 0
       // Handle message expire
       if (expire_status_ms > 0) {
         #if ENABLED(SDSUPPORT)
           if (card.isFileOpen()) {
             // Expire the message when printing is active
             if (IS_SD_PRINTING) {
               if (ms >= expire_status_ms) {
                 lcd_status_message[0] = '\0';
                 expire_status_ms = 0;
               }
             }
             else {
               expire_status_ms += LCD_UPDATE_INTERVAL;
             }
           }
           else {
             expire_status_ms = 0;
           }
         #else
           expire_status_ms = 0;
         #endif //SDSUPPORT
       }
     #endif
   #endif //LCD_PROGRESS_BAR
 
   lcd_implementation_status_screen();
 
   #if ENABLED(ULTIPANEL)
 
     bool current_click = LCD_CLICKED;
 
     if (ignore_click) {
       if (wait_for_unclick) {
         if (!current_click)
           ignore_click = wait_for_unclick = false;
         else
           current_click = false;
       }
       else if (current_click) {
         lcd_quick_feedback();
         wait_for_unclick = true;
         current_click = false;
       }
     }
 
     if (current_click) {
       lcd_goto_menu(lcd_main_menu, true);
       lcd_implementation_init( // to maybe revive the LCD if static electricity killed it.
         #if ENABLED(LCD_PROGRESS_BAR)
           currentMenu == lcd_status_screen
         #endif
       );
       #if ENABLED(FILAMENT_LCD_DISPLAY)
         previous_lcd_status_ms = millis();  // get status message to show up for a while
       #endif
     }
 
     #if ENABLED(ULTIPANEL_FEEDMULTIPLY)
       // Dead zone at 100% feedrate
       if ((feedrate_multiplier < 100 && (feedrate_multiplier + int(encoderPosition)) > 100) ||
           (feedrate_multiplier > 100 && (feedrate_multiplier + int(encoderPosition)) < 100)) {
         encoderPosition = 0;
         feedrate_multiplier = 100;
       }
       if (feedrate_multiplier == 100) {
         if (int(encoderPosition) > ENCODER_FEEDRATE_DEADZONE) {
           feedrate_multiplier += int(encoderPosition) - ENCODER_FEEDRATE_DEADZONE;
           encoderPosition = 0;
         }
         else if (int(encoderPosition) < -ENCODER_FEEDRATE_DEADZONE) {
           feedrate_multiplier += int(encoderPosition) + ENCODER_FEEDRATE_DEADZONE;
           encoderPosition = 0;
         }
       }
       else {
         feedrate_multiplier += int(encoderPosition);
         encoderPosition = 0;
       }
     #endif // ULTIPANEL_FEEDMULTIPLY
 
     feedrate_multiplier = constrain(feedrate_multiplier, 10, 999);
 
   #endif //ULTIPANEL
 }
 
 #if ENABLED(ULTIPANEL)
 
 static void lcd_return_to_status() { lcd_goto_menu(lcd_status_screen); }
 
 #if ENABLED(SDSUPPORT)
 
   static void lcd_sdcard_pause() { card.pauseSDPrint(); }
 
   static void lcd_sdcard_resume() { card.startFileprint(); }
 
   static void lcd_sdcard_stop() {
     quickStop();
     card.sdprinting = false;
     card.closefile();
     autotempShutdown();
     cancel_heatup = true;
     lcd_setstatus(MSG_PRINT_ABORTED, true);
   }
 
 #endif //SDSUPPORT
 
