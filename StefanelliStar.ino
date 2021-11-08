
/* Christmas Star by Daniel Stefanelli 12/08/2019 */
/* Uses Neopixels, pushbutton rotary encoder and I2C LCD */

//NEOPIXEL library
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

// Libraries for LCD
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Libraries for Rotary Encoder knob
#include <ClickEncoder.h>
#include <TimerOne.h>

// Output PIN for LEDs
#define PIN            4

// Number of LEDs
#define NUMLEDS      80

// Properties of star
#define NUMPOINTS   5
#define LEDSPERPOINT  (NUMLEDS/NUMPOINTS)
#define LEDSPERLEG    (LEDSPERPOINT/2)

// Maximum delay time for animated effects (milliseconds)
#define MAXDELAYTIME  300

// Time to stay on one mode when in Automatic cycliing (milliseconds)
#define AUTOTIME  30000

// ENUM List of display modes
enum DisplayMode {
  SOLID,
  RAINBOW1,
  RAINBOW2,
  POINTS,
  RAZZLE,
  REDGREENFADE,
  TWINKLE,
  CHARLIE,
  JACK,
  BURST,
  BURSTSTACK,
  BURSTSTACK2,
  CANDYCANE,
  STRIPES,
  NUMMODES
};

// Character to display for each mode in the Enum list, need to keep these in sync
// Probably a better more elegant way to do this
char DisplayModeString[][16] = {
  "Solid",
  "Rainbow 1",
  "Rainbow 2",
  "Points",
  "Razzle",
  "Red Green Fade",
  "Twinkle",
  "Charlie",
  "Jack",
  "Burst",
  "Burst Stack",
  "Burst Stack 2",
  "Candy Cane",
  "Stripes"
};

// Global variables
// Store last time the mode switched for automatic
static long lastswitchtime = 0;
// Set brightness, Max is 255 but don't go above 20 without additional power supply
static int brightnessValue = 20;
// Integer used for Charlie's preset
static int CharlieJ = 0;
// Array of delaytimes for each mode (Not used for Solid mode)
static uint32_t delayTime[NUMMODES];
// Initialize AutoMode to true
static int AutoMode = 1;
// Array of selected color for each mode (really only used for Solid mode)
uint32_t selectedColor[NUMMODES];

// Include EEPROM I/O
#include <EEPROM.h>

// Create encoder object
ClickEncoder *encoder;

// Create pixels for LEDs
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMLEDS, PIN, NEO_GRB + NEO_KHZ800);

// Create lcd for LCD display
LiquidCrystal_I2C lcd(0x3F, 16, 2);

// List of predefined colors
uint32_t red      = pixels.Color(255, 0, 0);
uint32_t green    = pixels.Color(0, 255, 0);
uint32_t blue     = pixels.Color(0, 0, 255);
uint32_t yellow   = pixels.Color(255, 255, 0);
uint32_t magenta  = pixels.Color(255, 0, 255);
uint32_t cyan     = pixels.Color(0, 255, 255);
uint32_t white    = pixels.Color(255, 255, 255);
uint32_t black    = pixels.Color(0, 0, 0);
enum Colors { RED, GREEN, BLUE, YELLOW, MAGENTA, CYAN, WHITE, BLACK, NUMCOLORS };
char ColorNames[][16] = {"Red", "Green", "Blue", "Yellow", "Magenta", "Cyan", "White", "Black"};
uint32_t ColorArray[] = {red, green, blue, yellow, magenta, cyan, white, black};

// Displaymode, controlled by pushing rotary encoder button
static int mode = 0;
// Keep track of last mode, initialize to something invalid
static int lastmode = -99;

// Variables used for Rotary encoder
int16_t value;
static int last = 0;
static long lastmillis = 0;
static long startmillis = 0;
uint32_t stepsize = 10;
//uint16_t i, j, k, l;

// Timer routine
void timerIsr() {
  encoder->service();
}

// Number of rows of pixels, defined rows of pixels for striped candy cane effect
#define NUMROWS 26
const PROGMEM int LEDRows[NUMROWS][17] = {
  {39, 40, 99},
  {38, 41, 99},
  {37, 42, 99},
  {36, 43, 99},
  {35, 44, 99},
  {34, 45, 99},
  {33, 46, 99},
  {32, 47, 99},
  {55, 54, 53, 52, 51, 50, 49, 48, 31, 30, 29, 28, 27, 26, 25, 24, 99},
  {56, 23, 99},
  {57, 22, 99},
  {58, 21, 99},
  {59, 20, 99},
  {60, 19, 99},
  {61, 18, 99},
  {62, 17, 99},
  {63, 16, 99},
  {64, 15, 99},
  {65, 14, 99},
  {66, 13, 99},
  {67, 12, 0, 79, 99},
  {68, 77, 2, 78, 1, 11, 99},
  {69, 76, 3, 10, 99},
  {70, 74, 75, 5, 4, 9, 99},
  {71, 73, 6, 8, 99},
  {72, 7, 99}
};

void Stripes() {
  int backstep = 0;
  int RowStart = 0;
  int TotalIndex = 0;
  int i, j, k;

  for (i = 0; i < NUMROWS; i++) {
    for (j = 0; j < NUMROWS - backstep; j++) {
      pixels.fill(ColorArray[BLACK], 0, NUMLEDS);
      k = 0;
      while (pgm_read_word(&LEDRows[j][k]) < 99) {
        pixels.setPixelColor(pgm_read_word(&LEDRows[j][k]), ColorArray[GREEN]);
        k++;
      }
      pixels.show();
      if (CheckForClickWithDelay(delayTime[mode])) return;
    }
    backstep++;
  }

}

// Print the delay time to the 2nd line of the LCD
void printDelayTime() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(delayTime[mode]);
  lcd.print(" ms");
}

// Print the Color to the 2nd line of the LCD
void printSelectedColor() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(ColorNames[selectedColor[mode]]);
}

// Routine called to check for Encoder Action
int CheckForClick() {
  ClickEncoder::Button b = encoder->getButton();
  static int HeldCounter = 0;

  if (AutoMode) {
    if ((millis() - lastswitchtime) > AUTOTIME) {
      mode++;
      if (mode >= NUMMODES)
        mode = 0;
      lastswitchtime = millis();
      return 1;
    }
  }

  // Process encoder clicks to cycle through display options
  // Hold encoder button to enter calibrate menu
  if (b != ClickEncoder::Open) {
    switch (b) {
      case ClickEncoder::Held:
        HeldCounter++;
        if (HeldCounter > 5) {
          lcd.setCursor(0, 0);
          lcd.print("SAVING          ");
          lcd.setCursor(0, 1);
          lcd.print("SETTINGS!!      ");
          // Save settings starting with AutoMode followed by selectedColor and then delayTime
          EEPROM.put(0, mode);
          EEPROM.put(sizeof(int), AutoMode);
          EEPROM.put(sizeof(int) * 2, selectedColor);
          EEPROM.put(sizeof(int) * 2 + sizeof(selectedColor), delayTime);
          delay(1500);
          lastmode = -99;
          return (1);
        }
        break;
      case ClickEncoder::Released:
        break;
      case ClickEncoder::Clicked:
        mode++;
        if (mode >= NUMMODES)
          mode = 0;
        return 1;
        break;
      case ClickEncoder::DoubleClicked:
        if (AutoMode) {
          AutoMode = 0;
          lcd.setCursor(0, 0);
          lcd.print("STOPPING        ");
          lcd.setCursor(0, 1);
          lcd.print("AUTO MODE       ");
          delay(1500);
          lastmode = -99;
          lastswitchtime = millis();
          return (1);
        }
        else {
          AutoMode = 1;
          lcd.setCursor(0, 0);
          lcd.print("STARTING        ");
          lcd.setCursor(0, 1);
          lcd.print("AUTO MODE       ");
          delay(1500);
          lastmode = -99;
          lastswitchtime = millis();
          mode = 0;
          return (1);
        }
        break;
    }
  }

  value += encoder->getValue();
  if (mode == 0) {
    if (abs(value - last) > 2) {
      if (value > last) {
        if (selectedColor[mode] < (NUMCOLORS - 1))
          selectedColor[mode]++;
      }
      else {
        if (selectedColor[mode] != 0)
          selectedColor[mode]--;
      }
      printSelectedColor();
      last = value;
      return 1;
    }
  }
  else {
    if (abs(value - last) > 2) {
      if (value > last) {
        if (delayTime[mode] < MAXDELAYTIME)
          delayTime[mode] = delayTime[mode] + stepsize;
      }
      else {
        if ((delayTime[mode] > 0) && (delayTime[mode] >= stepsize))
          delayTime[mode] = delayTime[mode] - stepsize;
      }
      last = value;
      lastmillis = millis();
      printDelayTime();
      return 1;
    }
  }
  return 0;
}

// Routine called to check for Encoder Action
int CheckForClickWithDelay(long tdelay)
{
  ClickEncoder::Button b = encoder->getButton();
  static int HeldCounter = 0;
  int TimeExpired = 0;

  startmillis = millis();

  while (true) {
    if ((millis() - startmillis) >= tdelay)
      return 0;
    else if (CheckForClick())
      return 1;
  }
}

//SETUP (run once)
void setup()
{
  int i;

  // Initialize encoder
  encoder = new ClickEncoder(A1, A0, A2);

  // Not sure what this does
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  // Begin serial output
  Serial.begin(9600);

  randomSeed(analogRead(0));

  // Initialize NeoPixel library
  pixels.begin();
  pixels.setBrightness(brightnessValue);
  pixels.show();

  // Initialize LCD, not sure if backlight does anything
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Stefanelli Star ");
  lcd.setCursor(0, 1);
  lcd.print("Version 20200102");
  delay(5000);

  // Read saved settings from EEPROM
  EEPROM.get(0, mode);
  EEPROM.get(sizeof(int), AutoMode);
  EEPROM.get(sizeof(int) * 2, selectedColor);
  EEPROM.get(sizeof(int) * 2 + sizeof(selectedColor), delayTime);

  // Range check values
  if (mode < 0) mode = 0;
  else if (mode >= NUMMODES) mode = NUMMODES - 1;

  if (AutoMode < 0) AutoMode = 0;
  else if (AutoMode > 1) AutoMode = 1;

  for ( i = 0; i < NUMMODES; i++) {
    if (delayTime[i] < 0 || delayTime[i] > MAXDELAYTIME)
      delayTime[i] = 100;
    if (selectedColor[i] < 0 || selectedColor[i] >= NUMCOLORS)
      selectedColor[i] = 0;
  }

  // Initial test, run through base colors
  for ( i = 0; i < NUMCOLORS; i++) {
    Solid(ColorArray[i]);
    delay(100);
  }
  lastmode = -99;
}

// Main Loop
void loop()
{
  // If AutoMode is active then check if it is time to increment the mode
  if (AutoMode) {
    if ((millis() - lastswitchtime) > AUTOTIME) {
      mode++;
      if (mode >= NUMMODES)
        mode = 0;
      lastswitchtime = millis();
    }
  }

  // Periodic check for encoder action
  CheckForClick();

  // Main switch statement to display selected mode
  switch (mode) {
    case SOLID:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printSelectedColor();
      }
      Solid(ColorArray[selectedColor[mode]]);
      break;
    case RAINBOW1:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      rainbow1(10);
      break;
    case RAINBOW2:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Rainbow 2");
        lastmode = mode;
        printDelayTime();
      }
      rainbow2(10);
      break;
    case POINTS:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      Points();
      break;
    case RAZZLE:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      Razzle();
      break;
    case REDGREENFADE:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      RedGreenFade();
      break;
    case TWINKLE:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      Twinkle();
      break;
    case CHARLIE:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      Charlie();
      break;
    case JACK:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      Jack();
      break;
    case BURST:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      Burst();
      break;
    case BURSTSTACK:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      BurstStack();
      break;
    case BURSTSTACK2:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      BurstStack2();
      break;
    case CANDYCANE:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      Candycane();
      break;
    case STRIPES:
      if (mode != lastmode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(DisplayModeString[mode]);
        lastmode = mode;
        printDelayTime();
      }
      Stripes();
      break;
  }
}

// Fill the dots with solid color
void Solid(uint32_t c)
{
  for (uint16_t i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, c);
  pixels.show();
}

// Sets each point to a different color and then moves them around
void Points()
{
  uint32_t colorSeq[] = {RED, GREEN, BLUE, YELLOW, MAGENTA};
  int i, j, k, colortimes, totaltimes, spinoffset;

  spinoffset = 0;
  totaltimes = 10;
  colortimes = 3;

  for (i = 0; i < 5; i++) {
    for (j = (i * 16); j < ((i + 1) * 16); j++) {
      pixels.setPixelColor(j, ColorArray[colorSeq[i]]);
    }
  }
  pixels.show();
  if (CheckForClickWithDelay(delayTime[mode])) return;

  for (k = 0; k < 1000; k++) {
    for (i = 0; i < 5; i++) {
      for (j = (i * 16) + spinoffset; j < ((i + 1) * 16) + spinoffset; j++) {
        pixels.setPixelColor(j % 80, ColorArray[colorSeq[i]]);
        if (CheckForClick()) return;
      }
    }
    spinoffset++;
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;

  }
}

// Bounce colored pixel around
void Bounce(uint32_t color)
{
  int i;

  for (i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, ColorArray[BLACK]);

  pixels.show();
  if (CheckForClick()) return;

  for (i = 0; i < pixels.numPixels(); i++) {
    if (i == 0)
      pixels.setPixelColor(i, color);
    else {
      pixels.setPixelColor(i - 1, pixels.Color(0, 0, 0));
      pixels.setPixelColor(i, color);
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;

  }
  for (i = pixels.numPixels(); i >= 0; i--) {
    if (i == pixels.numPixels())
      pixels.setPixelColor(i, color);
    else {
      pixels.setPixelColor(i + 1, pixels.Color(0, 0, 0));
      pixels.setPixelColor(i, color);
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;

  }
}

// Make colors go round and round
void RoundAndRound()
{
  int i, j, k, colortimes, totaltimes;
  totaltimes = 10;
  colortimes = 3;

  for (j = 0; j < pixels.numPixels(); j++)
    pixels.setPixelColor(j, ColorArray[BLACK]);
  pixels.show();
  if (CheckForClick()) return;

  for (j = 0; j < pixels.numPixels(); j++) {
    if (j == 0)
      pixels.setPixelColor(i, ColorArray[selectedColor[mode]]);
    else {
      pixels.setPixelColor(j - 1, pixels.Color(0, 0, 0));
      pixels.setPixelColor(j, ColorArray[selectedColor[mode]]);
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;
  }

}

// Fill the dots one after the other with a color
void Experimental()
{

  for (uint16_t i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  pixels.show();
  if (CheckForClick()) return;

  // White
  for (uint16_t i = 0; i < pixels.numPixels(); i++)
  {
    if (i == 0)
      pixels.setPixelColor(i, pixels.Color(255, 255, 255));
    else {
      pixels.setPixelColor(i - 1, pixels.Color(0, 0, 0));
      pixels.setPixelColor(i, pixels.Color(255, 255, 255));
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;
  }
  for (uint16_t i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));
  pixels.show();
  if (CheckForClickWithDelay(delayTime[mode])) return;
  // Red
  for (uint16_t i = 0; i < pixels.numPixels(); i++) {
    if (i == 0)
      pixels.setPixelColor(i, pixels.Color(255, 0, 0));
    else {
      pixels.setPixelColor(i - 1, pixels.Color(255, 255, 255));
      pixels.setPixelColor(i, pixels.Color(255, 0, 0));
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;
  }
  for (uint16_t i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, pixels.Color(255, 0, 0));
  pixels.show();
  if (CheckForClickWithDelay(delayTime[mode])) return;

  // Green
  for (uint16_t i = 0; i < pixels.numPixels(); i++) {
    if (i == 0)
      pixels.setPixelColor(i, pixels.Color(0, 255, 0));
    else {
      pixels.setPixelColor(i - 1, pixels.Color(255, 0, 0));
      pixels.setPixelColor(i, pixels.Color(0, 255, 0));
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;
  }

  for (uint16_t i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, pixels.Color(0, 255, 0));
  pixels.show();
  if (CheckForClickWithDelay(delayTime[mode])) return;

  // Blue
  for (uint16_t i = 0; i < pixels.numPixels(); i++) {
    if (i == 0)
      pixels.setPixelColor(i, pixels.Color(0, 0, 255));
    else {
      pixels.setPixelColor(i - 1, pixels.Color(0, 255, 0));
      pixels.setPixelColor(i, pixels.Color(0, 0, 255));
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;
  }
  for (uint16_t i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, pixels.Color(0, 0, 255));
  pixels.show();
  if (CheckForClickWithDelay(delayTime[mode])) return;
}

// Fill the dots one after the other with a color
void Razzle() {
  int i, j;
  
  // Green
  for (i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, ColorArray[GREEN]);
  pixels.show();
  if (CheckForClickWithDelay(500)) return;

  for (i = 0; i < LEDSPERLEG; i++) {
    for (j = 0; j < NUMPOINTS; j++) {
      pixels.setPixelColor((j * LEDSPERPOINT) + i, ColorArray[RED]);
      pixels.setPixelColor((j * LEDSPERPOINT + LEDSPERPOINT - 1) - i, ColorArray[RED]);
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;
  }

  if (CheckForClickWithDelay(500)) return;
  for (i = 0; i < LEDSPERLEG; i++) {
    for (j = 0; j < NUMPOINTS; j++) {
      pixels.setPixelColor((j * LEDSPERPOINT) + i, ColorArray[GREEN]);
      pixels.setPixelColor((j * LEDSPERPOINT + LEDSPERPOINT - 1) - i, ColorArray[GREEN]);
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;
  }
}

void Burst() {
  int i, j;
  
  // Green
  for (i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, ColorArray[GREEN]);
  pixels.show();
  if (CheckForClickWithDelay(500)) return;

  for (i = 0; i < LEDSPERLEG; i++) {
    for (j = 0; j < NUMPOINTS; j++) {
      pixels.setPixelColor((j * LEDSPERPOINT) + i, ColorArray[RED]);
      pixels.setPixelColor((j * LEDSPERPOINT + LEDSPERPOINT - 1) - i, ColorArray[RED]);
      if (i > 0) {
        pixels.setPixelColor((j * LEDSPERPOINT) + i - 1, ColorArray[GREEN]);
        pixels.setPixelColor((j * LEDSPERPOINT + LEDSPERPOINT - 1) - i + 1, ColorArray[GREEN]);
      }
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;
  }
}

void BurstStack() {
  int backstep = 0;
  int i, j, k;

  for (i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, ColorArray[GREEN]);
  pixels.show();
  if (CheckForClickWithDelay(500)) return;

  for (k = 0; k < LEDSPERLEG; k++) {
    for (i = 0; i < (LEDSPERLEG - backstep); i++) {
      for (j = 0; j < NUMPOINTS; j++) {
        pixels.setPixelColor((j * LEDSPERPOINT) + i, ColorArray[RED]);
        pixels.setPixelColor((j * LEDSPERPOINT + LEDSPERPOINT - 1) - i, ColorArray[RED]);
        if (i > 0 ) {
          pixels.setPixelColor((j * LEDSPERPOINT) + i - 1, ColorArray[GREEN]);
          pixels.setPixelColor((j * LEDSPERPOINT + LEDSPERPOINT - 1) - i + 1, ColorArray[GREEN]);
        }
      }
      pixels.show();
      if (CheckForClickWithDelay(delayTime[mode])) return;
    }
    backstep++;
    if (backstep > LEDSPERLEG)
      backstep = 0;
  }

  if (CheckForClickWithDelay(500)) return;
}

void BurstStack2() {
  int backstep = 0;
  int i, j, k;

  for (i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, ColorArray[GREEN]);
  pixels.show();
  if (CheckForClickWithDelay(500)) return;

  for (k = 0; k < LEDSPERLEG; k++) {
    for (i = 0; i < (LEDSPERLEG - backstep); i++) {
      for (j = 0; j < NUMPOINTS; j++) {
        pixels.setPixelColor((j * LEDSPERPOINT) + i, ColorArray[RED]);
        pixels.setPixelColor((j * LEDSPERPOINT + LEDSPERPOINT - 1) - i, ColorArray[RED]);
        if (i > 0 ) {
          pixels.setPixelColor((j * LEDSPERPOINT) + i - 1, ColorArray[GREEN]);
          pixels.setPixelColor((j * LEDSPERPOINT + LEDSPERPOINT - 1) - i + 1, ColorArray[GREEN]);
        }
      }
      pixels.show();
      if (CheckForClickWithDelay(delayTime[mode])) return;
    }
    backstep++;
    if (backstep > LEDSPERLEG)
      backstep = 0;
  }
  if (CheckForClickWithDelay(500)) return;

  for (k = 0; k < LEDSPERLEG; k++) {
    for (i = 0; i < (LEDSPERLEG - backstep); i++) {
      for (j = 0; j < NUMPOINTS; j++) {
        pixels.setPixelColor((j * LEDSPERPOINT) + i, ColorArray[GREEN]);
        pixels.setPixelColor((j * LEDSPERPOINT + LEDSPERPOINT - 1) - i, ColorArray[GREEN]);
        if (i > 0 ) {
          pixels.setPixelColor((j * LEDSPERPOINT) + i - 1, ColorArray[RED]);
          pixels.setPixelColor((j * LEDSPERPOINT + LEDSPERPOINT - 1) - i + 1, ColorArray[RED]);
        }
      }
      pixels.show();
      if (CheckForClickWithDelay(delayTime[mode])) return;
    }
    backstep++;
    if (backstep > LEDSPERLEG)
      backstep = 0;
  }
}


void Candycane() {

  int redstep = 0;
  int i;
  
  for (i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, ColorArray[WHITE]);

  if (CheckForClickWithDelay(500)) return;

  for (i = 0; i < NUMLEDS / 2; i++) {
    if ((i % 4) < 2) {
      pixels.setPixelColor(40 + i, ColorArray[RED]);
      pixels.setPixelColor(39 - i, ColorArray[RED]);
    }
  }
  pixels.show();
  if (CheckForClickWithDelay(delayTime[mode])) return;
}

void RedGreenFade() {
  int i;

  for (i = 0; i < pixels.numPixels(); i++) {
    pixels.setPixelColor(i, ColorArray[GREEN]);
    if (CheckForClick()) return;
  }
  pixels.show();

  for (i = 0; i < 255; i++) {
    Solid(pixels.Color(i, 255 - i, 0));
    if (CheckForClick()) return;
    pixels.show();
  }

  CheckForClickWithDelay(delayTime[mode] * 10);

  for (i = 0; i < 255; i++) {
    Solid(pixels.Color(255 - i, i, 0));
    if (CheckForClick()) return;
    pixels.show();
  }

  CheckForClickWithDelay(delayTime[mode] * 10);
}

// Color all green and have randomly have red dots pop in and out
void Twinkle()
{
  int NumReds = 20;
  int Reds[20];
  int FT = 1;
  int PixelStatus[NUMLEDS];
  int randomCandidate;
  int i;

  for (i = 0; i < pixels.numPixels(); i++) {
    pixels.setPixelColor(i, ColorArray[GREEN]);
    PixelStatus[i] = 0;
  }
  pixels.show();

  if (CheckForClick()) return;

  while (true) {
    for (i = 0; i < NumReds; i++) {
      randomCandidate = random(1, NUMLEDS - 2);
      while (PixelStatus[randomCandidate]) {
        randomCandidate = random(1, NUMLEDS - 2);
      }
      if (!FT) {
        PixelStatus[Reds[i]] = 0;
        if (PixelStatus[Reds[i] + 2] != 2)
          PixelStatus[Reds[i] + 1] = 0;
        if (PixelStatus[Reds[i] - 2] != 2)
          PixelStatus[Reds[i] - 1] = 0;
        pixels.setPixelColor(Reds[i], ColorArray[GREEN]);
      }
      Reds[i] = randomCandidate;
      PixelStatus[randomCandidate] = 2;
      PixelStatus[randomCandidate + 1] = 1;
      PixelStatus[randomCandidate - 1] = 1;
      pixels.setPixelColor(Reds[i], ColorArray[RED]);
      pixels.show();
      if (CheckForClickWithDelay(delayTime[mode])) return;
    }
    FT = 0;
  }
}

//cycle all the brightdots together
void rainbow2(uint8_t wait)
{
  uint16_t i, j;

  for (j = 0; j < 256; j++)
  {
    for (i = 0; i < pixels.numPixels(); i++)
    {
      pixels.setPixelColor(i, Wheel((i + j) & 255));
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbow1(uint8_t wait)
{
  uint16_t i, j;

  for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < pixels.numPixels(); i++)
    {
      pixels.setPixelColor(i, Wheel(((i * 256 / pixels.numPixels()) + j) & 255));
    }
    pixels.show();
    if (CheckForClickWithDelay(delayTime[mode])) return;
  }
}

void SetPixel(uint8_t pixelnum, uint32_t color) {
  int i;

  if (CharlieJ >= 256)
    CharlieJ = 0;

  for (i = 0; i < pixels.numPixels(); i++)
  {
    pixels.setPixelColor(i, Wheel(((i * 256 / pixels.numPixels()) + CharlieJ) & 255));

    //        pixels.setPixelColor(i, Wheel(((i * 256 / pixels.numPixels())) & 255));
  }
  pixels.setPixelColor(pixelnum, color);
  pixels.show();
  CharlieJ++;
}

// Slightly different, this makes the rainbow equally distributed throughout
void Charlie()
{
  uint16_t i, j;
  int pixelSeq[] = {39, 38, 37, 36, 35, 34, 33, 32, 0, 79, 47, 46, 45, 44, 43, 42, 41, 40};

  while (true) {
    for (i = 0; i < sizeof(pixelSeq) / 2; i++) {
      SetPixel(pixelSeq[i], ColorArray[RED]);
      if (CheckForClickWithDelay(delayTime[mode])) return;
    }
    for (i = 0; i < sizeof(pixelSeq) / 2; i++) {
      SetPixel(pixelSeq[i], ColorArray[GREEN]);
      if (CheckForClickWithDelay(delayTime[mode])) return;
    }
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void Jack()
{
  pixels.setPixelColor(0, ColorArray[GREEN]);
  pixels.setPixelColor(1, ColorArray[GREEN]);
  pixels.setPixelColor(2, ColorArray[MAGENTA]);
  pixels.setPixelColor(3, ColorArray[MAGENTA]);
  pixels.setPixelColor(4, ColorArray[GREEN]);
  pixels.setPixelColor(5, ColorArray[MAGENTA]);
  pixels.setPixelColor(6, ColorArray[GREEN]);
  pixels.setPixelColor(7, ColorArray[MAGENTA]);

  pixels.setPixelColor(8, ColorArray[GREEN]);
  pixels.setPixelColor(9, ColorArray[MAGENTA]);
  pixels.setPixelColor(10, ColorArray[MAGENTA]);
  pixels.setPixelColor(11, ColorArray[GREEN]);
  pixels.setPixelColor(12, ColorArray[MAGENTA]);
  pixels.setPixelColor(13, ColorArray[MAGENTA]);
  pixels.setPixelColor(14, ColorArray[GREEN]);
  pixels.setPixelColor(15, ColorArray[MAGENTA]);

  pixels.setPixelColor(16, ColorArray[GREEN]);
  pixels.setPixelColor(17, ColorArray[MAGENTA]);
  pixels.setPixelColor(18, ColorArray[GREEN]);
  pixels.setPixelColor(19, ColorArray[MAGENTA]);
  pixels.setPixelColor(20, ColorArray[GREEN]);
  pixels.setPixelColor(21, ColorArray[MAGENTA]);
  pixels.setPixelColor(22, ColorArray[GREEN]);
  pixels.setPixelColor(23, ColorArray[MAGENTA]);

  pixels.setPixelColor(24, ColorArray[GREEN]);
  pixels.setPixelColor(25, ColorArray[MAGENTA]);
  pixels.setPixelColor(26, ColorArray[GREEN]);
  pixels.setPixelColor(27, ColorArray[MAGENTA]);
  pixels.setPixelColor(28, ColorArray[GREEN]);
  pixels.setPixelColor(29, ColorArray[MAGENTA]);
  pixels.setPixelColor(30, ColorArray[GREEN]);
  pixels.setPixelColor(31, ColorArray[MAGENTA]);

  pixels.setPixelColor(32, ColorArray[GREEN]);
  pixels.setPixelColor(33, ColorArray[MAGENTA]);
  pixels.setPixelColor(34, ColorArray[GREEN]);
  pixels.setPixelColor(35, ColorArray[MAGENTA]);
  pixels.setPixelColor(36, ColorArray[GREEN]);
  pixels.setPixelColor(37, ColorArray[MAGENTA]);
  pixels.setPixelColor(38, ColorArray[GREEN]);
  pixels.setPixelColor(39, ColorArray[MAGENTA]);

  pixels.setPixelColor(40, ColorArray[GREEN]);
  pixels.setPixelColor(41, ColorArray[MAGENTA]);
  pixels.setPixelColor(42, ColorArray[GREEN]);
  pixels.setPixelColor(43, ColorArray[MAGENTA]);
  pixels.setPixelColor(44, ColorArray[GREEN]);
  pixels.setPixelColor(45, ColorArray[MAGENTA]);
  pixels.setPixelColor(46, ColorArray[GREEN]);
  pixels.setPixelColor(47, ColorArray[MAGENTA]);

  pixels.setPixelColor(48, ColorArray[GREEN]);
  pixels.setPixelColor(49, ColorArray[MAGENTA]);
  pixels.setPixelColor(50, ColorArray[GREEN]);
  pixels.setPixelColor(51, ColorArray[MAGENTA]);
  pixels.setPixelColor(52, ColorArray[GREEN]);
  pixels.setPixelColor(53, ColorArray[MAGENTA]);
  pixels.setPixelColor(54, ColorArray[GREEN]);
  pixels.setPixelColor(55, ColorArray[MAGENTA]);

  pixels.setPixelColor(56, ColorArray[GREEN]);
  pixels.setPixelColor(57, ColorArray[MAGENTA]);
  pixels.setPixelColor(58, ColorArray[GREEN]);
  pixels.setPixelColor(59, ColorArray[MAGENTA]);
  pixels.setPixelColor(60, ColorArray[GREEN]);
  pixels.setPixelColor(61, ColorArray[MAGENTA]);
  pixels.setPixelColor(62, ColorArray[GREEN]);
  pixels.setPixelColor(63, ColorArray[MAGENTA])
  ;
  pixels.setPixelColor(64, ColorArray[GREEN]);
  pixels.setPixelColor(65, ColorArray[MAGENTA]);
  pixels.setPixelColor(66, ColorArray[GREEN]);
  pixels.setPixelColor(67, ColorArray[MAGENTA]);
  pixels.setPixelColor(68, ColorArray[GREEN]);
  pixels.setPixelColor(69, ColorArray[MAGENTA]);
  pixels.setPixelColor(70, ColorArray[GREEN]);
  pixels.setPixelColor(71, ColorArray[MAGENTA]);

  pixels.setPixelColor(72, ColorArray[GREEN]);
  pixels.setPixelColor(73, ColorArray[MAGENTA]);
  pixels.setPixelColor(74, ColorArray[GREEN]);
  pixels.setPixelColor(75, ColorArray[GREEN]);
  pixels.setPixelColor(76, ColorArray[MAGENTA]);
  pixels.setPixelColor(77, ColorArray[GREEN]);
  pixels.setPixelColor(78, ColorArray[MAGENTA]);
  pixels.setPixelColor(79, ColorArray[GREEN]);
  pixels.show();
  if (CheckForClick()) return;
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos)
{
  if (WheelPos < 85)
  {
    return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if (WheelPos < 170)
  {
    WheelPos -= 85;
    return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  else
  {
    WheelPos -= 170;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}
