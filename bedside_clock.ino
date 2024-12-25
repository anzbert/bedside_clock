// DFPlayer
// https://github.com/PowerBroker2/DFPlayerMini_Fast
// Classes:
// https://powerbroker2.github.io/DFPlayerMini_Fast/html/class_d_f_player_mini___fast.html
#include <DFPlayerMini_Fast.h>
#include <SoftwareSerial.h>
SoftwareSerial mySerial(15, 14); // TX and RX pin
DFPlayerMini_Fast mp3;
#define VOLMODIFIER 13 // add this to the selectable DEFVOL range of 0-9

#define RXTIME 5
#define MAXFOLDERS 20
int foldersAndFiles[MAXFOLDERS] = {};

// DISPLAY
// https://github.com/Seeed-Studio/Grove_4Digital_Display
#include "TM1637.h"
#define CLK 5
#define DIO 4
#define MAXBRIGHTNESS 3
TM1637 display(CLK, DIO);

int8_t displayArray[4] = {0, 0, 0, 0}; // 4 digits array for display
int brightness;                        // working variable (supported range is 0-7)
byte tempBrightCountdown = 0;          // brightness Countdown on snoozepress
boolean clockPoints;

// menu labels
char volMenu[] = "vo";
char sndMenu[] = "sd";
char sdfMenu[] = "sf";
char brtMenu[] = "brt";
char snzMenu[] = "sn";

byte timeOut = 0;
const byte timeOutDefault = 20; // default timeout time in seconds

boolean runOncePerSecond;
int prevSecond;

boolean runOncePerFractionSecond;
unsigned long lastFractionStart;
boolean swapEveryFractionSecond;

// RTC DS1307 (I2C)
// https://github.com/Seeed-Studio/RTC_DS1307
#include <Wire.h> // for I2C communication
#include <DS3231.h>
DS3231 rtc; // define an object of DS1307 class
bool h12Flag;
bool pmFlag;

// ENCODER
// https://github.com/mprograms/SimpleRotary
#include <SimpleRotary.h>
SimpleRotary rotary(0, 1, A0); // button A0 doesnt exist but the library requires a pin to be set?!?!

// DEBOUNCER and BUTTON reader
// https://thomasfredericks.github.io/Bounce2/files/index.html
#include <Bounce2.h>
#define DEBOUNCETIME 5 // debounce interval of buttons in ms

// FLIP SWITCH + SWITCH LED
#define SWITCHLED 8 // reverse wiring: HIGH = OFF, LOW = ON
#define SWITCHTGL 9 // External PullDown resistor: HIGH = ON, LOW = OFF
Bounce toggleSwitch = Bounce();
byte blinkSwitchLed = 0;             // SWITCHLED blinkSwitchLeding timer Countdown
const byte blinkSwitchLedTimes = 10; // default number of blinks

// SNOOZE BUTTON
#define SNOOZEBTN 7 // Internal PullUp: HIGH = OFF, LOW = ON
Bounce snoozeButton = Bounce();

// ENCODER BUTTON
#define ENCBTN 6 // Internal PullUp: HIGH = OFF, LOW = ON
Bounce encoderButton = Bounce();

// variables for long press function
boolean encoderButtonState;
unsigned long encoderButtonPressTimeStamp; // saves time when button was pressed down
const unsigned int longPressTime = 3000;   // time required for long press in ms

// ALARM + SOUND + DEFVOL + BRIGHTNESS - using EEPROM to store reset-persistent settings
#include <EEPROM.h>
#define EEPROMSIZE 8 // define size of used (from 0 to EEPROMSIZE)

// buffer arrays:
int clockSetting[EEPROMSIZE + 1] = {}; // array for storing of all alarm settings in RAM
int tempSetting[EEPROMSIZE + 1] = {};  // temporarily displayed settings, before saving to EEPROM, RTC and RAM

#define RANDOM 0 // when SOUND selection is 0, we can call it RANDOM

// @note case MENU, Arrays and EEPROM LOCATIONS
#define ALARMHOURS 0
#define ALARMMINUTES 1
#define SNZTIME 2
#define DEFVOL 3
#define DEFBRIGHTNESS 4
#define SOUNDFOLDER 5
#define SOUND 6
#define TRACKNUMBER 7
#define EQSELECT 8

// not in eeprom, but in RTC:
#define SETTIMEHOURS 9
#define SETTIMEMINUTES 10

// neither RTC, nor EEPROM.. just BASE menu value:
#define BASE 100
int mode = BASE; // set default starting mode to BASE

// ALARM FUNCTION
boolean alarmArmed;             // alarm on or off
boolean alarmTriggered = false; // not triggered to start off with
boolean snoozing = false;
unsigned int alarmTimeOutHH;
unsigned int alarmTimeOutMM;
const unsigned int alarmTimeout = 20; // alarm timeout time in minutes

boolean started = false; // for demo listening of selected DEFVOL and SOUND
// boolean loop = false; // maybe implement general loop function??

/////////////////////////////
/////////// @note FUNCTIONS

// reset EEPROM with default values
void resetEEPROM(byte a, byte z)
{
  for (int i = a; i <= z; i++)
  {
    switch (i)
    {
    case ALARMHOURS:
      EEPROM.write(ALARMHOURS, 0);
      break;
    case ALARMMINUTES:
      EEPROM.write(ALARMMINUTES, 0);
      break;
    case DEFBRIGHTNESS:
      EEPROM.write(DEFBRIGHTNESS, 3);
      break;
    case SOUNDFOLDER:
      EEPROM.write(SOUNDFOLDER, 1);
      break;
    case SOUND:
      EEPROM.write(SOUND, 1);
      break;
    case TRACKNUMBER:
      int x = getTrackNumber(1, 1);
      EEPROM.write(TRACKNUMBER, x);
      break;
    case EQSELECT:
      EEPROM.write(EQSELECT, 0); // set to standard EQ
      break;
    default: // ALL ELSE
      EEPROM.write(i, 5);
      break;
    }
  }
}

// update displayArray array, used to update 4 display digits
void updateDisplayArray(byte hour, byte minute)
{
  displayArray[0] = hour / 10;
  displayArray[1] = hour % 10;
  displayArray[2] = minute / 10;
  displayArray[3] = minute % 10;
}

// toggle clockPointss state
void togglePoints()
{
  clockPoints = !clockPoints;
  if (clockPoints == true)
  {
    display.point(POINT_ON);
  }
  else
  {
    display.point(POINT_OFF);
  }
}

// to get tracknumber on SD card
int getTrackNumber(int folder, int file)
{
  mp3.playFolder(folder, file);
  int trackNumber = mp3.currentSdTrack();
  mp3.stop();
  return trackNumber;
}

// rotate value with rollover
int rotateValue(int variable, int minimum, int maximum)
{
  int encChange = rotary.rotate();
  // bump timeout on rotation
  if (encChange != 0)
  {
    timeOut = timeOutDefault;
  }
  // clockwise
  if (encChange == 1)
  {
    variable = variable + 1;
    if (variable > maximum)
    {
      variable = minimum;
    }
  }
  // anti-clockwise
  if (encChange == 2)
  {
    variable = variable - 1;
    if (variable < minimum)
    {
      variable = maximum;
    }
  }
  return variable;
}

void getSettingsFromEEPROMtoArray(byte a, byte z)
{
  for (int i = a; i <= z; i++)
  {
    int value = EEPROM.read(i);
    clockSetting[i] = value;
    // Serial.print(i);
    // Serial.print(" = ");
    // Serial.print(value);
  }
}

void stopAndResetVolume()
{
  started = false;
  byte vol = clockSetting[DEFVOL] + VOLMODIFIER;
  mp3.volume(vol);
  delay(20);
  mp3.stop();
}

void updateFractionTimer(unsigned int MSinterval)
{
  // UPDATE fraction timer (for flashing of numbers, for example)
  if (millis() - lastFractionStart >= MSinterval)
  {
    runOncePerFractionSecond = true;
    swapEveryFractionSecond = !swapEveryFractionSecond;
    lastFractionStart = lastFractionStart + MSinterval;
  }
}

void blinkDisplay()
{
  // flash all digits:
  if (runOncePerFractionSecond == true)
  {
    if (swapEveryFractionSecond == true)
    {
      display.point(POINT_ON);
      display.display(displayArray);
    }
    else
    {
      display.point(POINT_OFF);
      display.clearDisplay();
    }
  }
}

void getfoldersAndFiles()
{
  // Get number of folders and files into Array ([0] is number of folders)
  int fold = mp3.numFolders(); // may be necessary?? investigate!
  delay(RXTIME);
  foldersAndFiles[0] = fold - 1; // for some reason -1 is correct ??
  Serial.print("# of Folders: ");
  Serial.println(foldersAndFiles[0]);

  for (int i = 1; i <= foldersAndFiles[0]; i++)
  {
    foldersAndFiles[i] = mp3.numTracksInFolder(i);
    delay(RXTIME);
    Serial.print("Folder  ");
    Serial.print(i);
    Serial.print(" contains: ");
    Serial.println(foldersAndFiles[i]);
  }
}

/////////////////////////////////////////
/////////////////////////////////////////
// @note SETUP
void setup()
{
  Serial.begin(57600);

  // resetEEPROM(0, EEPROMSIZE); // to reset to default
  // get Alarm settings array from EEPROM
  getSettingsFromEEPROMtoArray(0, EEPROMSIZE);

  // DFPLAYER
  mySerial.begin(9600);                 // initialize SoftwareSerial.h library:
  mp3.begin(mySerial);                  // start dfplayer, using Software Serial pins
  delay(1000);                          // delays allows DFPLAYER hardware to power up before querying SD card content
  stopAndResetVolume();                 // stop playback and set DEFVOL based on clockSetting[DEFVOL]
  getfoldersAndFiles();                 // get SD card content
  mp3.EQSelect(clockSetting[EQSELECT]); // 0-5 = 0 Standard / 1 Pop / 2 Rock / 3 Jazz / 4 Classic / 5 Bass

  // DISPLAY
  brightness = clockSetting[DEFBRIGHTNESS];
  display.set(brightness); // Brightness 0-7 (default 2)
  display.init();          // actually the same as display.clearDisplay();

  // INIT ALL BUTTONS
  pinMode(SWITCHTGL, INPUT);           // no PULLUP because using external 10K PULL DOWN resistor
  toggleSwitch.attach(SWITCHTGL);      // debounce library init
  toggleSwitch.interval(DEBOUNCETIME); // debounce interval in ms

  pinMode(SNOOZEBTN, INPUT_PULLUP);
  snoozeButton.attach(SNOOZEBTN);
  snoozeButton.interval(DEBOUNCETIME);

  pinMode(ENCBTN, INPUT_PULLUP);
  encoderButton.attach(ENCBTN);
  encoderButton.interval(DEBOUNCETIME);

  // INIT FLIPSWITCH LED // (only works when SWITCH TOGGLE is HIGH)
  pinMode(SWITCHLED, OUTPUT);
  toggleSwitch.update(); // get state when plugged in and blinkSwitchLed if HIGH
  if (toggleSwitch.read() == 1)
  {
    blinkSwitchLed = blinkSwitchLedTimes;
  }

  // ENCODER LIBRARY SETTINGS
  rotary.setDebounceDelay(2); // DEFAULT is 2ms
  rotary.setErrorDelay(0);    // Basic error correction function for high speeds - DEFAULT is 200ms // Off is 0ms

  // initialize randomizer
  randomSeed(A1);

  // SDCARD QUERY PROBLEM CHECK
  if (foldersAndFiles[0] == -1)
  {
    display.displayStr("sder"); // SD ERROR
    while (snoozeButton.read() == 1)
    {
      snoozeButton.update();
    }
  }

  // I2C Communication to RTC Clock
  Wire.begin();            // Start the I2C interface via the wire.h library
  rtc.setClockMode(false); // set to 24h
}

////////////////////////////////////////////
////////////////////////////////////////////
// @note LOOP
void loop()
{

  ///////////////////////////////
  // mode independant code:

  // UPDATE RTC TIMER
  if (rtc.getSecond() != prevSecond)
  {
    runOncePerSecond = true;      // to let other routines know that a second has passed
    prevSecond = rtc.getSecond(); // set up for next second check
  }

  // UPDATE FRACTION TIMER
  updateFractionTimer(150);

  // get latest states from all inputs
  encoderButton.update();
  snoozeButton.update();
  toggleSwitch.update();

  // query playback every 5 seconds (12 times per minute)
  // in RANDOM mode, fire next track, if necessary
  if (runOncePerSecond == true)
  {
    if ((rtc.getSecond() % 5) == 0 && alarmTriggered == true && clockSetting[SOUND] == RANDOM)
    {
      int queryPlay = mp3.isPlaying();
      if (queryPlay != 1)
      {
        mp3.playFolder(clockSetting[SOUNDFOLDER], random(1, foldersAndFiles[clockSetting[SOUNDFOLDER]]));
      }
    }
  }

  // // TIMEOUT timer... only does something when mode is not BASE already
  if (runOncePerSecond == true)
  {
    if (mode == BASE)
    {
      timeOut = 0;
    }
    if (timeOut > 1)
    {
      timeOut--;
    }
    if (timeOut == 1)
    {
      stopAndResetVolume();
      mode = BASE;
    }
  }

  // CHECK IF ALARM NEEDS TO BE TRIGGERED once per minute
  if (alarmArmed == 1 && runOncePerSecond == true)
  {
    if (rtc.getSecond() == 0) // which is true once per minute
    {
      if (rtc.getHour(h12Flag, pmFlag) == clockSetting[ALARMHOURS] && rtc.getMinute() == clockSetting[ALARMMINUTES])
      {
        mode = BASE;          // back to BASE when alarm is Triggered
        stopAndResetVolume(); // reset for playback

        if (clockSetting[SOUND] == RANDOM)
        {
          mp3.playFolder(clockSetting[SOUNDFOLDER], random(1, foldersAndFiles[clockSetting[SOUNDFOLDER]]));
        }
        if (clockSetting[SOUND] != RANDOM)
        {
          mp3.loop(clockSetting[TRACKNUMBER]);
        }

        alarmTriggered = true;

        // set alarm timeout in 'alarmTimeout' minutes:
        alarmTimeOutHH = rtc.getHour(h12Flag, pmFlag) + ((rtc.getMinute() + alarmTimeout) / 60);
        if (alarmTimeOutHH > 23)
        {
          alarmTimeOutHH = (alarmTimeOutHH % 24);
        }
        alarmTimeOutMM = rtc.getMinute() + alarmTimeout;
        if (alarmTimeOutMM > 59)
        {
          alarmTimeOutMM = (alarmTimeOutMM % 60);
        }

        if (brightness == -1)
        {
          brightness = clockSetting[DEFBRIGHTNESS]; // turn display back on, if off
        }
      }
    }
  }

  // ALARM TIMEOUT
  if (alarmTriggered == true && runOncePerSecond == true)
  {
    if (rtc.getSecond() == 0 && rtc.getHour(h12Flag, pmFlag) == alarmTimeOutHH && rtc.getMinute() == alarmTimeOutMM)
    {
      alarmTriggered = false;
      snoozing = false;
      mp3.stop();
      getSettingsFromEEPROMtoArray(0, 1); // restore alarm time HH and MM
    }
  }

  // ARM ALARM and blink LED
  if (toggleSwitch.rose() == 1)
  {
    blinkSwitchLed = blinkSwitchLedTimes;
  }
  if (toggleSwitch.read() == 1)
  {
    alarmArmed = 1;
  }
  if (toggleSwitch.read() == 0)
  {
    alarmArmed = 0;
    if (alarmTriggered == true || snoozing == true)
    {
      alarmTriggered = false;
      snoozing = false;
      mp3.stop();
      getSettingsFromEEPROMtoArray(0, 1); // restore alarm time HH and MM
    }
    blinkSwitchLed = 0;
    digitalWrite(SWITCHLED, HIGH); // turn power to LED OFF
  }

  // SWITCHLED blinking until blinkSwitchLed back to zero
  if (runOncePerFractionSecond == true)
  {
    if (blinkSwitchLed > 0)
    {
      digitalWrite(SWITCHLED, blinkSwitchLed % 2);
      blinkSwitchLed--;
    }
  }

  //////////////////////////////////////////////////////////////////////////////////
  // MODE dependant code //////////////////////////////////////////////////////////

  switch (mode)
  {
    //////////////////////////////////////////
    /////////////////////// @note BASE MODE
  case BASE:

    ////////////////////// START OF ONCE-PER-SECOND DISPLAY CODE
    if (runOncePerSecond == true)
    {
      // UPDATE TIME ARRAY AND DISPLAY TIME
      updateDisplayArray(rtc.getHour(h12Flag, pmFlag), rtc.getMinute());

      // tempBrightCountdown of more than 0 overrides any brightness setting:
      if (tempBrightCountdown > 0)
      {
        tempBrightCountdown--;
        display.set(clockSetting[DEFBRIGHTNESS]);
        display.display(displayArray);
        togglePoints(); // flip points on and off each second
      }
      else
      {
        // update display if brightness is not -1 with normal values:
        if (brightness >= 0)
        {
          display.set(brightness);
          display.display(displayArray);
          togglePoints(); // flip points on and off each second
        }
        // turn display off if brightness is -1
        if (brightness == -1)
        {
          display.point(POINT_OFF);
          display.clearDisplay();
        }
      }
    }
    ///////////////////////////// END OF ONCE-PER-SECOND CODE
    ///////////////////////////////////////////////////////////

    // ENCODER to control brightness in BASE mode (no rollover)
    if (tempBrightCountdown == 0) // if currently not on temp. brightness tempBrightCountdown
    {
      int encChange = rotary.rotate();
      if (encChange == 1 && brightness < MAXBRIGHTNESS)
      {
        brightness = brightness + 1;
        display.set(brightness);
        display.display(displayArray);
      }
      if (encChange == 2 && brightness >= 0)
      {
        brightness = brightness - 1;
        display.set(brightness);
        display.display(displayArray);
      }
    }

    // snoozeButton temporary BRIGHTNESS function in BASE mode, if dark:
    if (brightness == -1) // turn brightness to max for a number of seconds when display is off and snoozeButton is pressed
    {
      if (snoozeButton.fell() == true)
      {
        tempBrightCountdown = 2; // turn display on for 2 seconds and do it immediately, instead of waiting for display refresh
        display.set(clockSetting[DEFBRIGHTNESS]);
        display.display(displayArray);
      }
    }

    // SNOOZING FUNCTION in BASE mode:
    if (alarmTriggered == true)
    {
      if (snoozeButton.fell() == true)
      {
        mp3.stop();
        alarmTriggered = false;
        snoozing = true;

        // set new alarm hours:
        clockSetting[ALARMHOURS] = rtc.getHour(h12Flag, pmFlag) + ((rtc.getMinute() + clockSetting[SNZTIME]) / 60);
        if (clockSetting[ALARMHOURS] > 23)
        {
          clockSetting[ALARMHOURS] = (clockSetting[ALARMHOURS] % 24);
        }

        // set new alarm minutes:
        clockSetting[ALARMMINUTES] = rtc.getMinute() + clockSetting[SNZTIME];
        if (clockSetting[ALARMMINUTES] > 59)
        {
          clockSetting[ALARMMINUTES] = (clockSetting[ALARMMINUTES] % 60);
        }

        blinkSwitchLed = blinkSwitchLedTimes;
      }
    }

    // ENCODERBUTTON
    // when encbutton is pressed:
    if (encoderButton.fell() == true)
    {
      if (brightness == -1)
      {
        brightness = clockSetting[DEFBRIGHTNESS]; // display will stay back on after using menu
        display.set(clockSetting[DEFBRIGHTNESS]); // turn on display, if off
      }
      encoderButtonState = 1;
      encoderButtonPressTimeStamp = millis();

      // disarm any active alarm when pressed and reload alarm time HH:MM from EEPROM
      if (alarmTriggered == true || snoozing == true)
      {
        mp3.stop();
        alarmTriggered = false;
        snoozing = false;
        getSettingsFromEEPROMtoArray(0, 1); // restore alarm time HH and MM
      }
    }
    // when encbutton is released:
    if (encoderButton.rose() == true)
    {
      encoderButtonState = 0;

      mode = ALARMHOURS;                                  // switch to alarm settings
      tempSetting[ALARMHOURS] = clockSetting[ALARMHOURS]; // grab setting to be adjusted in that mode
      timeOut = timeOutDefault;                           // restart timeOUT timer
    }

    if (encoderButtonState == 1)
    {
      if (millis() - encoderButtonPressTimeStamp >= longPressTime)
      {
        encoderButtonState = 0; // not to repeat the longpress function

        // start blinking and hold in while loop when long press time reached
        while (encoderButton.read() == 0)
        {
          encoderButton.update();
          updateFractionTimer(150);
          blinkDisplay();

          // reset EEPROM when kept pressed for 3x as long
          if (millis() - encoderButtonPressTimeStamp >= (longPressTime * 3))
          {
            // start blinking and hold when long press time reached
            while (encoderButton.read() == 0)
            {
              encoderButton.update();
              updateFractionTimer(50); // blink faster
              blinkDisplay();
            }

            if (encoderButton.rose() == true)
            {
              display.clearDisplay();
              resetEEPROM(0, EEPROMSIZE);
              getSettingsFromEEPROMtoArray(0, EEPROMSIZE);
              brightness = clockSetting[DEFBRIGHTNESS];
              delay(3000);
              mode = BASE;
              return;
            }
          }
        }

        // set RTC TIME
        if (encoderButton.rose() == true)
        {
          mode = SETTIMEHOURS;
          tempSetting[ALARMHOURS] = rtc.getHour(h12Flag, pmFlag); // grab setting to be adjusted in that mode
          timeOut = timeOutDefault;                               // restart timeOUT timer
        }
      }
    }

    break;

    /////////// @note ALARM TIME
  case ALARMHOURS:

    // adjust hours
    tempSetting[ALARMHOURS] = rotateValue(tempSetting[ALARMHOURS], 0, 23);
    updateDisplayArray(tempSetting[ALARMHOURS], clockSetting[ALARMMINUTES]); // update time display data array with latest time
    display.point(POINT_ON);

    // flash 2 digits:
    if (runOncePerFractionSecond == true)
    {
      if (swapEveryFractionSecond == true)
      {
        display.display(displayArray);
      }
      else
      {
        display.display(0x00, 0x7f);
        display.display(0x01, 0x7f);
      }
    }

    if (snoozeButton.fell() == true)
    {
      mode = SOUNDFOLDER;                                   // rotate to sound folder selection menu
      tempSetting[SOUNDFOLDER] = clockSetting[SOUNDFOLDER]; // grab sound folder setting to temp variable
      timeOut = timeOutDefault;
    }

    if (encoderButton.rose() == true)
    {
      mode = ALARMMINUTES;
      tempSetting[ALARMMINUTES] = clockSetting[ALARMMINUTES]; // get minute setting onto temp variable 2
      timeOut = timeOutDefault;
    }
    break;

  case ALARMMINUTES:
    // adjust minutes
    tempSetting[ALARMMINUTES] = rotateValue(tempSetting[ALARMMINUTES], 0, 59);
    updateDisplayArray(tempSetting[ALARMHOURS], tempSetting[ALARMMINUTES]); // update time display data array with latest time
    display.point(POINT_ON);

    // flash 2 digits:
    if (runOncePerFractionSecond == true)
    {
      if (swapEveryFractionSecond == true)
      {
        display.display(displayArray);
      }
      else
      {
        display.display(0x02, 0x7f);
        display.display(0x03, 0x7f);
      }
    }

    if (snoozeButton.fell() == true)
    {
      mode = ALARMHOURS;        // switch to alarm settings
      timeOut = timeOutDefault; // restart timeOUT timer
    }

    if (encoderButton.rose() == true)
    {
      // save alarmTime (byte 0 and 1) to AlarmSetting array and EEPROM, only if changed to save flash memory rewrites:
      for (int i = ALARMHOURS; i <= ALARMMINUTES; i++)
      {
        if (clockSetting[i] != tempSetting[i])
        {
          clockSetting[i] = tempSetting[i];
          EEPROM.write(i, clockSetting[i]);
        }
      }

      mode = BASE;                          // back to BASE
      blinkSwitchLed = blinkSwitchLedTimes; // blink alarm LED
    }
    break;

    //////////////// @note SNOOZE TIME
  case SNZTIME:

    tempSetting[SNZTIME] = rotateValue(tempSetting[SNZTIME], 1, 15);
    display.point(POINT_OFF);

    // flash 2 digits:
    if (runOncePerFractionSecond == true)
    {
      if (swapEveryFractionSecond == true)
      {
        display.displayStr(snzMenu);
        display.display(0x02, tempSetting[SNZTIME] / 10);
        display.display(0x03, tempSetting[SNZTIME] % 10);
      }
      else
      {
        display.display(0x02, 0x7f);
        display.display(0x03, 0x7f);
      }
    }

    if (snoozeButton.fell() == true)
    {
      mode = EQSELECT;                                // rotate to EQSELECT menu
      started = false;                                // EQSELECTL demo playback has not been started
      tempSetting[EQSELECT] = clockSetting[EQSELECT]; // grab EQSELECT setting to variable
      timeOut = timeOutDefault;
    }

    if (encoderButton.rose() == true)
    {
      // save default snooze time selection to Array and EEPROM:
      if (clockSetting[SNZTIME] != tempSetting[SNZTIME])
      {
        clockSetting[SNZTIME] = tempSetting[SNZTIME];
        EEPROM.write(SNZTIME, clockSetting[SNZTIME]);
      }

      mode = BASE;
    }
    break;

    ////////////////// @note DEFVOL
  case DEFVOL:
    static int prevVol;
    prevVol = tempSetting[DEFVOL];

    tempSetting[DEFVOL] = rotateValue(tempSetting[DEFVOL], 1, 15);
    display.point(POINT_OFF);

    // flash digits:
    if (runOncePerFractionSecond == true)
    {
      display.displayStr(volMenu);
      if (swapEveryFractionSecond == true)
      {
        display.display(0x02, tempSetting[DEFVOL] / 10);
        display.display(0x03, tempSetting[DEFVOL] % 10);
      }
      else
      {
        display.display(0x02, 0x7f);
        display.display(0x03, 0x7f);
      }
    }

    // preview DEFVOL when changing
    if (prevVol != tempSetting[DEFVOL])
    {
      byte vol = tempSetting[DEFVOL] + VOLMODIFIER;
      mp3.volume(vol);

      if (started == false) // only start playback if it has not been started already
      {
        if (clockSetting[SOUND] != RANDOM)
        {
          mp3.loop(clockSetting[TRACKNUMBER]);
        }
        if (clockSetting[SOUND] == RANDOM)
        {
          int range = random(1, foldersAndFiles[clockSetting[SOUNDFOLDER]]);
          // Serial.println(range);
          mp3.playFolder(clockSetting[SOUNDFOLDER], range);
          // loop = true;
        }

        started = true;
      }
    }

    if (snoozeButton.fell() == true)
    {
      stopAndResetVolume();
      mode = BASE;
    }

    if (encoderButton.rose() == true)
    {
      // save alarmSound selection to Array and EEPROM:
      if (clockSetting[DEFVOL] != tempSetting[DEFVOL])
      {
        clockSetting[DEFVOL] = tempSetting[DEFVOL];
        EEPROM.write(DEFVOL, clockSetting[DEFVOL]);
      }

      stopAndResetVolume();
      mode = BASE;
    }
    break;

    ///////////////// @note DEF BRIGHTNESS
  case DEFBRIGHTNESS:
    static int prevBrt;
    prevBrt = tempSetting[DEFBRIGHTNESS];

    tempSetting[DEFBRIGHTNESS] = rotateValue(tempSetting[DEFBRIGHTNESS], 0, MAXBRIGHTNESS);
    display.point(POINT_OFF);

    // flash digits:
    if (runOncePerFractionSecond == true)
    {
      display.displayStr(brtMenu);
      if (swapEveryFractionSecond == true)
      {
        display.display(0x03, tempSetting[DEFBRIGHTNESS]);
      }
      else
      {
        display.display(0x03, 0x7f);
      }
    }

    // preview brightness when changing
    if (prevBrt != tempSetting[DEFBRIGHTNESS])
    {
      display.set(tempSetting[DEFBRIGHTNESS]);
    }

    // button presses:
    if (snoozeButton.fell() == true)
    {
      mode = SNZTIME;
      tempSetting[SNZTIME] = clockSetting[SNZTIME];
      timeOut = timeOutDefault;
    }

    if (encoderButton.rose() == true)
    {
      // save default brightness selection to Array and EEPROM:
      if (clockSetting[DEFBRIGHTNESS] != tempSetting[DEFBRIGHTNESS])
      {
        clockSetting[DEFBRIGHTNESS] = tempSetting[DEFBRIGHTNESS];
        brightness = clockSetting[DEFBRIGHTNESS];
        EEPROM.write(DEFBRIGHTNESS, clockSetting[DEFBRIGHTNESS]);
      }

      mode = BASE;
    }

    break;

  /////////////////// @note SDFOLDER ( clockSetting[SOUNDFOLDER] )
  case SOUNDFOLDER:
    tempSetting[SOUNDFOLDER] = rotateValue(tempSetting[SOUNDFOLDER], 1, foldersAndFiles[0]);
    display.point(POINT_OFF);

    // flash digits
    if (runOncePerFractionSecond == true)
    {
      display.displayStr(sdfMenu);
      if (swapEveryFractionSecond == true)
      {
        display.display(0x02, tempSetting[SOUNDFOLDER] / 10);
        display.display(0x03, tempSetting[SOUNDFOLDER] % 10);
      }
      else
      {
        display.display(0x02, 0x7f);
        display.display(0x03, 0x7f);
      }
    }

    if (snoozeButton.fell() == true)
    {
      mode = DEFVOL;                              // rotate to DEFVOL menu
      started = false;                            // DEFVOL demo playback has not been started
      tempSetting[DEFVOL] = clockSetting[DEFVOL]; // grab DEFVOL setting to variable
      timeOut = timeOutDefault;
    }

    if (encoderButton.rose() == true)
    {
      mode = SOUND; // rotate to sound selection menu
      if (tempSetting[SOUNDFOLDER] == clockSetting[SOUNDFOLDER])
      {
        tempSetting[SOUND] = clockSetting[SOUND]; // grab sound setting to temp variable
      }
      else
      {
        tempSetting[SOUND] = 1;
      }
      timeOut = timeOutDefault;
      started = false;
    }

    break;

    ////////////////// @note SOUND ( clockSetting[SOUND] )
  case SOUND:
    static int prevSound;
    prevSound = tempSetting[SOUND];

    tempSetting[SOUND] = rotateValue(tempSetting[SOUND], 0, foldersAndFiles[tempSetting[SOUNDFOLDER]]);
    display.point(POINT_OFF);

    switch (tempSetting[SOUND])
    {

      // RANDOM MODE:
    case RANDOM: // is value of 0
      // flash digits
      if (runOncePerFractionSecond == true)
      {
        if (swapEveryFractionSecond == true)
        {
          display.displayStr("sd");
        }
        else
        {
          display.displayStr("sdrd");
        }
      }

      // preview Sounds when changing selection
      if (prevSound != tempSetting[SOUND])
      {
        mp3.playFolder(tempSetting[SOUNDFOLDER], random(1, foldersAndFiles[tempSetting[SOUNDFOLDER]]));
        started = true;
      }

      if (encoderButton.rose() == true)
      {
        mp3.stop();

        // SOUND of 0 will save as random mode on SOUNDFOLDER and SOUND... ignoring TRACKNUMBER for now
        for (int i = SOUNDFOLDER; i <= SOUND; i++)
        {
          if (clockSetting[i] != tempSetting[i])
          {
            clockSetting[i] = tempSetting[i];
            EEPROM.write(i, clockSetting[i]);
          }
        }

        mode = BASE; // back to BASE
      }

      break;

      // not RANDOM:
    default:
      // flash digits
      if (runOncePerFractionSecond == true)
      {
        display.displayStr(sndMenu);
        if (swapEveryFractionSecond == true)
        {
          display.display(0x02, tempSetting[SOUND] / 10);
          display.display(0x03, tempSetting[SOUND] % 10);
        }
        else
        {
          display.display(0x02, 0x7f);
          display.display(0x03, 0x7f);
        }
      }

      // preview Sounds when changing selection
      if (prevSound != tempSetting[SOUND])
      {
        mp3.playFolder(tempSetting[SOUNDFOLDER], tempSetting[SOUND]);
        started = true;
      }

      if (encoderButton.rose() == true)
      {
        // work-around to get loopable track number
        tempSetting[TRACKNUMBER] = mp3.currentSdTrack();
        mp3.stop();

        // save alarm sound(6), folder(5) and track number(7) selection to Array and EEPROM:
        for (int i = SOUNDFOLDER; i <= TRACKNUMBER; i++)
        {
          if (clockSetting[i] != tempSetting[i])
          {
            clockSetting[i] = tempSetting[i];
            EEPROM.write(i, clockSetting[i]);
          }
        }

        mode = BASE; // back to BASE
      }
      break;
    }

    // increase timeout if playback started
    if (started == true)
    {
      timeOut = 1800;
    }

    // cancel back to folder selection
    if (snoozeButton.fell() == true)
    {
      mp3.stop();
      mode = SOUNDFOLDER; // rotate to sound folder selection menu
      timeOut = timeOutDefault;
    }
    break;

    ////////////////// @note SET TIME
  case SETTIMEHOURS:
    tempSetting[ALARMHOURS] = rotateValue(tempSetting[ALARMHOURS], 0, 23);
    updateDisplayArray(tempSetting[ALARMHOURS], rtc.getMinute()); // update time data array with latest time
    display.point(POINT_ON);

    // flash 2 digits and points:
    if (runOncePerFractionSecond == true)
    {
      if (swapEveryFractionSecond == true)
      {
        display.display(displayArray);
      }
      else
      {
        display.display(0x00, 0x7f);
        display.display(0x01, 0x7f);
      }
    }

    if (snoozeButton.fell() == true)
    {
      tempSetting[DEFBRIGHTNESS] = clockSetting[DEFBRIGHTNESS];
      mode = DEFBRIGHTNESS;
      timeOut = timeOutDefault;
    }

    if (encoderButton.rose() == true)
    {
      mode = SETTIMEMINUTES;
      tempSetting[ALARMMINUTES] = rtc.getMinute();
      timeOut = timeOutDefault;
    }
    break;

  case SETTIMEMINUTES:
    // adjust minutes
    tempSetting[ALARMMINUTES] = rotateValue(tempSetting[ALARMMINUTES], 0, 59);
    updateDisplayArray(tempSetting[ALARMHOURS], tempSetting[ALARMMINUTES]); // update time data array with latest time
    display.point(POINT_ON);

    // flash 2 digits and points:
    if (runOncePerFractionSecond == true)
    {
      if (swapEveryFractionSecond == true)
      {
        display.display(displayArray);
      }
      else
      {
        display.display(0x02, 0x7f);
        display.display(0x03, 0x7f);
      }
    }

    if (snoozeButton.fell() == true)
    {
      mode = SETTIMEHOURS;
      timeOut = timeOutDefault; // restart timeOUT timer
    }

    if (encoderButton.rose() == true)
    {
      // save time to RTC:
      rtc.setHour(tempSetting[ALARMHOURS]);
      rtc.setMinute(tempSetting[ALARMMINUTES]);
      rtc.setSecond(0);

      mode = BASE;
    }

    break;

    ////////////////// @note EQSELECT
  case EQSELECT:
    static int prevEQ;
    prevEQ = tempSetting[EQSELECT];

    tempSetting[EQSELECT] = rotateValue(tempSetting[EQSELECT], 0, 5);
    display.point(POINT_OFF);

    // flash digits :
    if (runOncePerFractionSecond == true)
    {
      if (swapEveryFractionSecond == true)
      {
        switch (tempSetting[EQSELECT])
        {
        case 0:
          display.displayStr("Eqst");
          break;
        case 1:
          display.displayStr("Eqpo");
          break;
        case 2:
          display.displayStr("Eqro");
          break;
        case 3:
          display.displayStr("EqJA");
          break;
        case 4:
          display.displayStr("EqCl");
          break;
        case 5:
          display.displayStr("EqbA");
          break;
        default:
          // error
          break;
        }
      }
      else
      {
        display.displayStr("Eq");
      }
    }

    // preview DEFVOL when changing
    if (prevEQ != tempSetting[EQSELECT])
    {
      mp3.EQSelect(tempSetting[EQSELECT]);
      delay(10);

      if (started == false) // only start playback if it has not been started already
      {
        if (clockSetting[SOUND] != RANDOM)
        {
          mp3.loop(clockSetting[TRACKNUMBER]);
        }
        if (clockSetting[SOUND] == RANDOM)
        {
          int range = random(1, foldersAndFiles[clockSetting[SOUNDFOLDER]]);
          mp3.playFolder(clockSetting[SOUNDFOLDER], range);
        }
        started = true;
      }
    }

    if (snoozeButton.fell() == true)
    {
      stopAndResetVolume();
      mode = BASE;
    }

    if (encoderButton.rose() == true)
    {
      // save alarmSound selection to Array and EEPROM:
      if (clockSetting[EQSELECT] != tempSetting[EQSELECT])
      {
        clockSetting[EQSELECT] = tempSetting[EQSELECT];
        EEPROM.write(EQSELECT, clockSetting[EQSELECT]);

        mp3.EQSelect(clockSetting[EQSELECT]);
      }

      stopAndResetVolume();
      mode = BASE;
    }
    break;
  }
  ////////////////////////////////
  // END OF LOOP
  // Reset timer variables:
  runOncePerSecond = false;
  runOncePerFractionSecond = false;
}