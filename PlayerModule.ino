#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>
#include <LiquidCrystal.h>
#include <IRremote.h>

#define SYNC_BYTE (uint8_t)(0xC8)


#define PLAYER1 (uint8_t)(0x08)
#define PLAYER2 (uint8_t)(0x16)

#define period_us (uint32_t)(1000000U) //scroll period

int makePacket(uint8_t *packet, uint8_t type, uint32_t *payload);

 enum types{
  INITIALIZE,GAME_START,POKE,GAME_END
};

enum states{
  PRE_INIT,PRE_GAME_START,PRE_POKE,HIGH_NOON,UNALIVE
  };


// NOTE: retype this line if you pasted from a doc; avoid non-breaking spaces.
const int rs = 8, en = 7, d4 = 5, d5 = 4, d6 = 3, d7 = 2;

int xPin = A0;
int yPin = A1;
int buttonPin = 9;

const uint8_t IR_PIN = 6;           // <-- using D6 now

uint32_t last_decodedRawData = 0;

volatile uint8_t rx_buf[7];
volatile uint8_t rxLen = 0;
volatile bool rxReady = false;

uint32_t timestamp = micros();
volatile uint8_t state = PRE_INIT;
volatile uint8_t n_rounds = 0;
uint8_t n_display = 0;



 



LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Forward decls
void onI2CReceive(int n);
void onI2CRequest(void);




void setup() {
    Serial.begin(9600);
  pinMode(xPin, INPUT);
  pinMode(yPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  // I2C SLAVE at address 0x08
  Wire.begin(PLAYER1); // or 
 
  Wire.setTimeout(200);

//register ISR handler
  Wire.onReceive(onI2CReceive);
  Wire.onRequest(onI2CRequest);

    IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK); // start IR receiver


}

void loop() {
  // on RX
  if (rxReady) {
    rxReady = 0;
  noInterrupts();
  switch (rx_buf[2]){
    case INITIALIZE:
  //initialize block
  state = PRE_GAME_START;
    // LCD startup
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Howdy, Howdy!");
  lcd.setCursor(0, 1);
  lcd.print("Lets Get Rowdy!");

      break;
    case GAME_START:
  //note number of rounds, get ready for game
  state = PRE_POKE;

   lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Its high noon...");
  lcd.setCursor(0, 1);
  lcd.print("Ready your blaster...");

      break;
     case POKE:
  // prepare to take shot
   state = HIGH_NOON;


    lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("!FIRE! TIRALOOOO!");
  lcd.setCursor(0, 1);
  lcd.print("~~~~~~~~~~~~~~~~");


      break;
    case GAME_END:
  //process win or loss
  int32_t t_diff = 0;
  memcpy(&t_diff,&(rx_buf[3]),sizeof(int32_t));
  if(t_diff > 0){
     lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("We won!");
  lcd.setCursor(0, 1);
  lcd.print("Ayyyyyyyyy");
  }else{
      lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("We lost!");
  lcd.setCursor(0, 1);
  lcd.print("noooooooo");
  }
      break;
  }

   
  }
  interrupts();  


//when not RX, handle UI, IR INPUT
if(state == PRE_GAME_START){
  char * w_buf = 'a';
  sprintf(w_buf, "How many rounds? %su\r\n",n_display);
   lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(w_buf);


  int xVal = analogRead(xPin);
  int yVal = analogRead(yPin);
  int button = digitalRead(buttonPin);
  if((micros() - timestamp) > period_us){
    timestamp = micros();
    if(xVal > 600){
    n_display++;
  }

  if((xVal < 400) && (n_display > 0)){
  n_display--;
}
  }



if(button == 0){
  n_rounds = (n_display > 0)?n_display:1;
}
  
}


if(state == HIGH_NOON){
  if(IrReceiver.decode()){
    state = UNALIVE; 
  }

}

 
}



// I2C receive ISR-context callback (don’t do heavy work here)
void onI2CReceive(int n) {
  // Read up to our buffer size
  rxLen = 0;
  while (Wire.available() && rxLen < sizeof(rx_buf)) {
    rx_buf[rxLen++] = (uint8_t)Wire.read();
  }
  rxReady = true;
}

//we will always send a full packet e.g 7 bytes
void onI2CRequest(void){
switch (state){
  case PRE_INIT:
  //shouldnt happen
    break;
  case PRE_GAME_START:
    if(n_rounds > 0){
      uint8_t pkt[7] = {0};
      uint32_t pload = n_rounds;
      /***********error here, with location of round count*/
      size_t size_pkt = makePacket(pkt,GAME_START,&pload);
      Wire.write(pkt,size_pkt);
    }
    break;
  case PRE_POKE:
  //shouldnt happen
    break;
  case HIGH_NOON:
  //no response
    break;
  case UNALIVE:
  int32_t pload = 0; //dummy 
  uint8_t pkt[7] = {0};

  // payload unused, more of a nudge
   size_t size_pkt = makePacket(pkt,GAME_END,&pload);

   
   Wire.write(pkt,size_pkt);
}


}


//make packet : [SYNC][LEN][TYPE][PAYLOAD] -> max size is 7 bytes
int makePacket(uint8_t *packet, uint8_t type, uint32_t *payload){

int8_t len = ((type == INITIALIZE) || (type == POKE))?1:(type == GAME_START)?(sizeof(uint8_t) + 1):(type == GAME_END)?(sizeof(int32_t) + 1):-1;

if(len < 0){
  return -1;
}

packet[0] =  SYNC_BYTE;
packet[1] = len;
packet[2] = type;
memcpy(&(packet[3]),payload,len - 1);

//return size of packet
return len + 2;
  
}
