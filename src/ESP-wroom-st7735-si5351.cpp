#include <Wire.h>
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <RotaryEncoder.h>
#include <SPI.h>
#include <Adafruit_SI5351.h>
#include "Button2.h"
#include <EEPROM.h>
// remember TFT_ESPI need the setup in the library
// in user_setup.h this is set for esp32 dev module
// #define TFT_MISO 12
// #define TFT_MOSI 13
// #define TFT_SCLK 18
// #define TFT_CS   15 // Chip select control pin
// #define TFT_DC   2  // Data Command control pin
// //#define TFT_RST   4  // Reset pin (could connect to RST pin)
// #define TFT_RST  4 // Set TFT_RST to -1 if display RESET is connected to ESP32 board RST

#define TFT_GREY 0x5AEB

#define PIN_IN1 19
#define PIN_IN2 23
#define RELAY_BAND 25
#define color1 TFT_SILVER // 0xC638
#define color2 TFT_SILVER // 0xC638
#define DELAY 1000

float freq = 0.00;

struct BandsType
{
  char band[8];
  int calibrate;
  int value;
  int minimal;
  int maximal;
  uint32_t start;
}; //

int bandindex = 0;
struct BandsType AmateurBand[] = {{"40-m", 239, 71000, 7000000, 7200000, 7100000},
                                  {"20-m", 490, 140000, 14000000, 14350000, 14100000}};

int radix = 100;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
RotaryEncoder encoder(PIN_IN1, PIN_IN2, RotaryEncoder::LatchMode::FOUR3);
Button2 button;

volatile uint32_t vfo = AmateurBand[bandindex].minimal + AmateurBand[bandindex].calibrate; // initial frequency in Hz Put your favourite frequency here
volatile uint32_t walkingFreq = AmateurBand[bandindex].start;
uint32_t pllFreq; // si5351 internal vco frequency

uint32_t si5351multiCount;       // Multicounter
uint32_t si5351multi_int;        // Multicounter integer
uint32_t si5351nomi;             // Multicounter nominator
uint32_t si5351denomi = 1000000; // Multicounter denominator one million
uint32_t si5351divider;          // VCO output divider integer part
Adafruit_SI5351 clockgen = Adafruit_SI5351();

void count_frequency()
{
  uint16_t f, g;

  f = vfo / 1000000;                       // variable is now vfo instead of 'frequency' vfo esim 145787500
  si5351divider = 900000000 / vfo;         // vfo divider integer
  pllFreq = si5351divider * vfo;           // counts pllFrequency
  si5351multiCount = pllFreq / 25;         // feedback divider
  si5351multi_int = pllFreq / 25000000;    // feedback divider integer
  si5351nomi = si5351multiCount % 1000000; // feedback divider integer
  si5351denomi = 1000000;                  // feedback divider fraktion

  Serial.println(f = vfo / 1000000); // printtaa f 145.XXX.XXX
}
void setFrequency()
{
  count_frequency();
  clockgen.setupPLL(SI5351_PLL_A, (si5351multi_int), (si5351nomi), (si5351denomi)); // write si5351ctl divider word
                                                                                    // clockgen.setupMultisynthInt(0, SI5351_PLL_A, SI5351_MULTISYNTH_DIV_6);
  clockgen.setupMultisynth(1, SI5351_PLL_A, (si5351divider), 0, 1);
  clockgen.enableOutputs(true);
}

void drawStep(int step, uint16_t bgcolor)
{
  char charptr[20];
  // draw magenta outline
  tft.drawLine(0, 50, 160, 50, TFT_MAGENTA);
  tft.drawLine(0, 50, 0, 60, TFT_MAGENTA);
  tft.drawLine(99, 50, 99, 70, TFT_MAGENTA);
  tft.drawLine(158, 50, 158, 70, TFT_MAGENTA);

  sprintf(charptr, "Step %4.1f KHz", (float)step / 1000.0);
  tft.setFreeFont(&FreeMono9pt7b);
  tft.setTextColor(bgcolor, TFT_BLACK);
  tft.drawString(charptr, 5, 53, 2);

  Serial.print("charptr:");
  Serial.println(charptr);
}

void drawSprite()
{

  Serial.print("radix:");
  Serial.println(radix);

  freq = (float)walkingFreq / 1000.0;
  AmateurBand[bandindex].value = walkingFreq / 100;
  Serial.print("value:");
  Serial.println(AmateurBand[bandindex].value);

  vfo = walkingFreq + AmateurBand[bandindex].calibrate;
  EEPROM.writeLong64(8, walkingFreq);
  EEPROM.commit();
  Serial.print("write walkingFreq:");
  Serial.println(walkingFreq);
  setFrequency();

  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.setFreeFont(&FreeMonoBold12pt7b);
  tft.drawFloat(freq, 1, 2, 15);
  tft.setFreeFont(&FreeMono9pt7b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("KHz", 130, 25);

  drawStep(radix, TFT_WHITE);

  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);

  int temp = AmateurBand[bandindex].value - 20;
  int TickWidthInPx = 4;
  Serial.print("temp:");
  Serial.println(temp);
  spr.drawLine(0, 0, 161, 0, TFT_MAGENTA);

  for (int i = 0; i < 40; i++)
  {

    if ((temp % 10) == 0)
    {
      spr.drawLine(i * TickWidthInPx, 10, i * TickWidthInPx, 50, color1);
      spr.drawLine((i * TickWidthInPx) + 1, 10, (i * TickWidthInPx) + 1, 50, color1);
      spr.drawFloat(temp / 10.0, 0, i * TickWidthInPx, 10, 1);
    }
    else if ((temp % 5) == 0 && (temp % 10) != 0)
    {
      spr.drawLine(i * TickWidthInPx, 30, i * TickWidthInPx, 50, color1);
      spr.drawLine((i * TickWidthInPx) + 1, 30, (i * TickWidthInPx) + 1, 50, color1);
    }
    else
    {
      spr.drawLine(i * TickWidthInPx, 40, i * TickWidthInPx, 50, color2);
    }

    temp = temp + 1;
  }
  spr.drawLine(81, 5, 81, 45, TFT_RED);
  spr.drawLine(80, 5, 80, 45, TFT_RED);
  spr.pushSprite(-5, 70);
}

void StepClick()
{
  drawStep(radix, TFT_BLACK);
  switch (radix)
  {
  case 10000:
    radix = 1000;
    break;
  case 1000:
    radix = 500;
    break;
  case 500:
    radix = 100;
    break;
  case 100:
    radix = 10000;
    break;
  default:
    radix = 1000;
    break;
  }
  drawStep(radix, TFT_WHITE);

  EEPROM.writeInt(0, radix);
  EEPROM.commit();
  Serial.print("write radix:");
  Serial.println(radix);
}

void setup()
{

  Serial.begin(115200);
  Serial.println("start");

  EEPROM.begin(56);
  walkingFreq = EEPROM.readLong64(8);
  Serial.print("read walkingFreq:");
  Serial.println(walkingFreq);

  radix = EEPROM.readInt(0);
  Serial.print("read radix:");
  Serial.println(radix);

  tft.begin();
  tft.writecommand(0x11);
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(0);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  spr.createSprite(163, 55);
  spr.setTextColor(color1, TFT_BLACK);

  drawSprite();
  /* Initialise the sensor */
  if (clockgen.begin() != ERROR_NONE)
  {
    /* There was a problem detecting the IC ... check your connections */
    Serial.print("Ooops, no Si5351 detected ... Check your wiring or I2C ADDR!");
    while (1)
      ;
  }

  if (radix != 10000 && radix != 1000 && radix != 500 && radix != 100)
    radix = 1000;
  setFrequency();

  button.begin(12);
  tft.drawString(AmateurBand[bandindex].band, 115, 53, 2);

  pinMode(RELAY_BAND, OUTPUT);
  digitalWrite(RELAY_BAND, LOW);
}

void readEncoder()
{
  static int pos = 0;
  encoder.tick();

  int newPos = encoder.getPosition();
  if (pos != newPos)
  {
    if (newPos > pos)
      walkingFreq = walkingFreq - radix;
    if (walkingFreq < AmateurBand[bandindex].minimal)
      walkingFreq = AmateurBand[bandindex].minimal;
    if (newPos < pos)
      walkingFreq = walkingFreq + radix;
    if (walkingFreq > AmateurBand[bandindex].maximal)
      walkingFreq = AmateurBand[bandindex].maximal;
    pos = newPos;
    drawSprite();
  }
}

void loop(void)
{
  readEncoder();
  button.loop();
  if (button.wasPressed())
  {
    Serial.println("click ");
    switch (button.read())
    {
    case single_click:
      Serial.println("single");
      StepClick();
      break;
    case double_click:
      Serial.println("double");
      break;
    case triple_click:
      Serial.println("triple");
      break;
    case long_click:
      Serial.println("looong");
      if (bandindex == 0)
      {
        AmateurBand[bandindex].start = walkingFreq;
        bandindex = 1;
        walkingFreq = AmateurBand[bandindex].start;
        tft.drawString(AmateurBand[bandindex].band, 115, 53, 2);
        digitalWrite(RELAY_BAND, HIGH);
      }
      else if (bandindex == 1)
      {
        AmateurBand[bandindex].start = walkingFreq;
        bandindex = 0;
        walkingFreq = AmateurBand[bandindex].start;
        tft.drawString(AmateurBand[bandindex].band, 115, 53, 2);
        digitalWrite(RELAY_BAND, LOW);
      }
      Serial.print("bandindex=");
      Serial.println(bandindex);
      drawSprite();
      break;
    }
  }
}
