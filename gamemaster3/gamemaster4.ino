#include <Wire.h>
#include <stdint.h>
#include <string.h>

#define PLAYER1  ((uint8_t)0x08)
#define PLAYER2  ((uint8_t)0x16)
#define SYNC_BYTE ((uint8_t)0xC8)

// Packet: [SYNC][LEN][TYPE][PAYLOAD...], LEN counts [TYPE+PAYLOAD], so min LEN=1, max LEN=5
enum types : uint8_t { INITIALIZE, GAME_START, POKE, GAME_END };


#define LED_PIN 7

static inline void put_u32_be(uint8_t* p, uint32_t v){
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v);
}
static inline uint32_t get_u32_be(const uint8_t* p){
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

int makePacket(uint8_t *pkt, uint8_t type, const void* payload, uint8_t payload_len){
  if (payload_len > 4) return -1;            // protocol max payload = 4
  if (type == INITIALIZE || type == POKE) payload_len = 0;
  if (type == GAME_START && payload_len != 4) return -1; // rounds as u32
  if (type == GAME_END   && payload_len != 4) return -1; // signed delta as i32

  pkt[0] = SYNC_BYTE;
  pkt[1] = (uint8_t)(1 + payload_len); // TYPE + payload
  pkt[2] = type;
  if (payload_len && payload) memcpy(&pkt[3], payload, payload_len);
  return (int)(pkt[1] + 2); // total bytes
}

bool readFrame(uint8_t addr, uint8_t* buf, size_t buflen, uint8_t* out_type){
  // Request up to 7 bytes (max frame)
  size_t got = Wire.requestFrom(addr, (size_t)7);
  if (got < 3 || got > 7) return false;   // need at least SYNC/LEN/TYPE

  // Read exactly 'got' bytes
  size_t rd = Wire.readBytes(buf, got);
  if (rd != got) return false;

  // Basic validation
  if (buf[0] != SYNC_BYTE) return false;
  uint8_t len = buf[1];
  if (len < 1 || len > 5) return false;
  if ((size_t)(len + 2) != got) return false; // total must match
  *out_type = buf[2];
  return true;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Wire.begin();           // master
  Wire.setTimeout(100);   // 100 ms for readBytes()
}

// Flow:
// 1) Send INITIALIZE to both
// 2) Poll for GAME_START from either; parse u32 rounds; forward to the other
// 3) Random (optional) delay
// 4) LED HIGH, send POKE to both
// 5) Wait for first GAME_END (first blood) -> mark t_fb, who lost
// 6) Poll the other until GAME_END -> t_sb
// 7) Compute signed delta and send GAME_END to both: +delta to winner, -delta to loser
void loop() {
  uint8_t pkt[8] = {0};

  // --- 1) INIT both
  int sz = makePacket(pkt, INITIALIZE, nullptr, 0);
  if (sz > 0) {
    Wire.beginTransmission(PLAYER1); Wire.write(pkt, sz); Wire.endTransmission();
    Wire.beginTransmission(PLAYER2); Wire.write(pkt, sz); Wire.endTransmission();
  }

  // --- 2) Wait for GAME_START
  uint8_t first_sender = 0;
  uint8_t frame_type = 0;
  uint8_t buf[8];

  uint32_t n_rounds = 0;

  for(;;){
    if (readFrame(PLAYER1, buf, sizeof(buf), &frame_type) && frame_type == GAME_START) {
      n_rounds = get_u32_be(&buf[3]);
      first_sender = PLAYER1;
      break;
    }
    if (readFrame(PLAYER2, buf, sizeof(buf), &frame_type) && frame_type == GAME_START) {
      n_rounds = get_u32_be(&buf[3]);
      first_sender = PLAYER2;
      break;
    }
    // Optional: add a small delay to avoid hammering the bus
    // delay(1);
  }

  // Forward GAME_START to the other player
  uint8_t other = (first_sender == PLAYER1) ? PLAYER2 : PLAYER1;
  uint8_t rounds_be[4]; put_u32_be(rounds_be, n_rounds);
  sz = makePacket(pkt, GAME_START, rounds_be, 4);
  Wire.beginTransmission(other); Wire.write(pkt, sz); Wire.endTransmission();

  // --- 3) Optional randomized delay before starting
  delay(random(3000, 10000));

  // --- 4) LED HIGH + POKE both
  digitalWrite(LED_PIN, HIGH);
  uint32_t t_poke = micros();

  sz = makePacket(pkt, POKE, nullptr, 0);
  Wire.beginTransmission(PLAYER1); Wire.write(pkt, sz); Wire.endTransmission();
  Wire.beginTransmission(PLAYER2); Wire.write(pkt, sz); Wire.endTransmission();

  // --- 5) Wait for first GAME_END
  uint8_t first_blood = 0;
  uint32_t t_fb = 0;
  for(;;){
    if (readFrame(PLAYER1, buf, sizeof(buf), &frame_type) && frame_type == GAME_END) {
      first_blood = PLAYER1; t_fb = micros(); break;
    }
    if (readFrame(PLAYER2, buf, sizeof(buf), &frame_type) && frame_type == GAME_END) {
      first_blood = PLAYER2; t_fb = micros(); break;
    }
    // delay(1);
  }

  // --- 6) Wait for second GAME_END from the other player
  other = (first_blood == PLAYER1) ? PLAYER2 : PLAYER1;
  size_t got = 0;
  uint32_t t_sb = 0;
  for(;;){
    if (readFrame(other, buf, sizeof(buf), &frame_type) && frame_type == GAME_END) {
      t_sb = micros(); break;
    }
    // delay(1);
  }

  // --- 7) Compute signed delta and notify both
  int32_t delta = (int32_t)(t_sb - t_fb);      // positive for winner’s lead
  int32_t delta_win  =  delta;
  int32_t delta_lose = -delta;

  uint8_t dwin_be[4];  put_u32_be(dwin_be,  (uint32_t)delta_win);
  uint8_t dlose_be[4]; put_u32_be(dlose_be, (uint32_t)delta_lose);

  uint8_t winner = (first_blood == PLAYER1) ? PLAYER2 : PLAYER1;

  sz = makePacket(pkt, GAME_END, dwin_be, 4);
  Wire.beginTransmission(winner); Wire.write(pkt, sz); Wire.endTransmission();

  sz = makePacket(pkt, GAME_END, dlose_be, 4);
  Wire.beginTransmission(first_blood); Wire.write(pkt, sz); Wire.endTransmission();

  // Hold state
  for(;;) { /* done */ }
}
