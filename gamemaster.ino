#include <Wire.h>
#include <stdint.h>

int makePacket(uint8_t *packet, uint8_t type, uint32_t *payload);


#define PLAYER1 (uint8_t)(0x08)
#define PLAYER2 (uint8_t)(0x16)

#define SYNC_BYTE (uint8_t)(0xC8)



  enum types{
  INITIALIZE,GAME_START,POKE,GAME_END
};


#define LED_PIN 1



void setup() {
  pinMode(LED_PIN, OUTPUT); 
  digitalWrite(LED_PIN, LOW);
  Wire.begin(); // Initialize I2C as master
  Wire.setTimeout(100);    // 100ms timeout
}


// 1. Initialize with both players, 2. Wait for a player to send a start game request with round count,
//3. Send confirm to other player, 4. Wait for ready, 5. Start game by pulling LED's high ,
//6. Poll both players for death stat (time till death), 7. Declare winner with difference, 8. Maybe data log
void loop() {
   size_t n_rx = 0;
  uint32_t start = 0,t_fb = 0,t_sb = 0;

  uint8_t host_buffer[8] = {0};

  /*
  INITIALIZE
  */
 uint8_t size = makePacket(host_buffer,INITIALIZE,NULL);
 if(size > 0){
  //player 1
  Wire.beginTransmission(PLAYER1); 
  Wire.write(host_buffer,size);          
  Wire.endTransmission();   

  //player2
   Wire.beginTransmission(PLAYER2); 
  Wire.write(host_buffer,size);          
  Wire.endTransmission();  
 }

 /*
 POLL BOTH PLAYERS FOR GAMESTART
 */

 uint8_t game_began = 0;
 uint8_t first_blood = 0;

 while(!game_began){
   n_rx = Wire.requestFrom(PLAYER1, (sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t)));
   
 
  if(n_rx > 0){
 Wire.readBytes(host_buffer, n_rx);   
 game_began = 1;
 first_blood = PLAYER1;
 break;
  }
  n_rx = Wire.requestFrom(PLAYER2, (sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t)));


   if(n_rx > 0){
 Wire.readBytes(host_buffer, n_rx);   
 game_began = 1;
 first_blood = PLAYER2;
 break;
  }

 }

 /*
 GAMESTART RECEIVED, PARSE FOR ROUND COUNT, AND TRANSMIT TO OTHER PLAYER
 */
 uint32_t n_rounds = host_buffer[3] | host_buffer[4] | host_buffer[5] | host_buffer[6];
uint8_t unaware_player = (first_blood == PLAYER1)?PLAYER2:PLAYER1;


 size = makePacket(host_buffer,GAME_START,&n_rounds);
   Wire.beginTransmission(unaware_player); 
  Wire.write(host_buffer,size);          
  Wire.endTransmission();  



//DELAY RANDOM TIME


  /*
  PULL LED HIGH, AND POKE BOTH!
  */
  digitalWrite(LED_PIN, HIGH);

start = micros();

size = makePacket(host_buffer,POKE,NULL);
    //player 1
  Wire.beginTransmission(PLAYER1); 
  Wire.write(host_buffer,size);          
  Wire.endTransmission();   

  //player2
   Wire.beginTransmission(PLAYER2); 
  Wire.write(host_buffer,size);          
  Wire.endTransmission();  


  /*
  AWAIT GAME END
  */
 while(game_began){
  n_rx = Wire.requestFrom(PLAYER1, (sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t)));
   
 
  if(n_rx > 0){
 Wire.readBytes(host_buffer, n_rx);   
 game_began = 0;
 first_blood = PLAYER1;
 t_fb = micros();
 break;
  }
  n_rx = Wire.requestFrom(PLAYER2, (sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t)));


   if(n_rx > 0){
 Wire.readBytes(host_buffer, n_rx);   
 game_began = 0;
 first_blood = PLAYER2;
 t_fb = micros();
 break;
  }

 }

 uint8_t winner = (first_blood == PLAYER1)?PLAYER2:PLAYER1;

 //wait for  second 
 n_rx = 0;

 while(n_rx = 0){
   n_rx = Wire.requestFrom(PLAYER2, (sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t)));
 }

 t_sb = micros();


 int32_t t_win = t_sb - t_fb; //pos

 int32_t t_loss = -1 * t_win; //neg


size = makePacket(host_buffer,GAME_END,&t_win);

  Wire.beginTransmission(winner); 
  Wire.write(host_buffer,size);          
  Wire.endTransmission();   

  size = makePacket(host_buffer,GAME_END,&t_loss);

  //player2
   Wire.beginTransmission(first_blood); 
  Wire.write(host_buffer,size);          
  Wire.endTransmission();  


  while(1){
    
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


