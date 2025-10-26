#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>
#include <LiquidCrystal.h>

#define SYNC_BYTE ((uint8_t)0xC8)

// NOTE: retype this line if you pasted from a doc; avoid non-breaking spaces.
const int rs = 8, en = 7, d4 = 5, d5 = 4, d6 = 3, d7 = 2;

volatile uint8_t rxBuf[32];
volatile uint8_t rxLen = 0;
volatile bool rxReady = false;

enum State : uint8_t {
  INITIALIZE,
  GAMESTART,
  POKE,
  GAME_END
};
State state = INITIALIZE;

LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Forward decls
void onI2CReceive(int n);
void initScreen();

void setup() {
  // I2C SLAVE at address 0x08
  Wire.begin(0x08);
  // Optional (supported on most cores): timeout for hung I2C transactions.
  // If unsupported on your board, just comment these out.
  #if defined(TWISRA) || defined(TWI0_SDA)
  Wire.setWireTimeout(20000 /* us */, true);
  #endif
  Wire.onReceive(onI2CReceive);

  // LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Howdy, Howdy!");
  lcd.setCursor(0, 1);
  lcd.print("Lets Get Rowdy!");
}

void loop() {
  // Simple state hook (expand as you build your game)
  if (rxReady) {
    noInterrupts();
    uint8_t len = rxLen;
    uint8_t local[32];
    for (uint8_t i = 0; i < len; ++i) local[i] = rxBuf[i];
    rxLen = 0;
    rxReady = false;
    interrupts();

    // Optional: require a leading SYNC_BYTE before accepting a payload
    uint8_t start = 0;
    if (len > 0 && local[0] == SYNC_BYTE) start = 1;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Rx n=");
    lcd.print(len - start);
    lcd.print(" 0x");

    // Show first up to 8 nibbles on second line
    lcd.setCursor(0, 1);
    for (uint8_t i = start; i < len && i < start + 8; ++i) {
      uint8_t b = local[i];
      const char hex[] = "0123456789ABCDEF";
      lcd.print(hex[b >> 4]);
      lcd.print(hex[b & 0x0F]);
      lcd.print(' ');
    }
  }

  // …add your game state machine here…
}

void initScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select # of Rounds");
  lcd.setCursor(0, 1);
  lcd.print(" on main module");
}

// I2C receive ISR-context callback (don’t do heavy work here)
void onI2CReceive(int n) {
  // Read up to our buffer size
  rxLen = 0;
  while (Wire.available() && rxLen < sizeof(rxBuf)) {
    rxBuf[rxLen++] = (uint8_t)Wire.read();
  }
  rxReady = true;
}
