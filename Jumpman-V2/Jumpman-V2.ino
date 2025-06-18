#include <Adafruit_GFX.h>
#include <Adafruit_TFTLCD.h>
#include <EEPROM.h>

#define HS_ADDR            0
#define BUZZER_PIN         12
#define BUTTON_PIN         A2
#define BLACK              0x0000
#define WHITE              0xFFFF
#define GREEN              0x07E0
#define RED                0xF800
#define YELLOW             0xFFE0
#define CYAN               0x07FF
#define PURPLE             0xA11D
#define BROWN              0x8A22

uint16_t tft_id = 0x9488;
Adafruit_TFTLCD tft(40,38,39,42,41);

const int SCREEN_W           = 480;
const int SCREEN_H           = 320;
const int GROUND_Y           = SCREEN_H - 20;
const int PLAYER_X           = 50;
const int PLAYER_SIZE        = 20;
const int PLAYER_Y0          = GROUND_Y - PLAYER_SIZE;
const unsigned long FRAME_TIME           = 16;
const unsigned long BASE_JUMP_DURATION   = 1000;
const int           BASE_JUMP_HEIGHT     = 80;
const int           BASE_OBST_STEP       = 4;
const unsigned long BASE_OBST_INTERVAL   = 2000;
const int MAX_OBST                   = 4;
const int OBST_WIDTH                 = 20;
const int OBST_HEIGHTS[]             = {20,40};

const int melody[]     = { 262, 294, 330, 349, 330, 294, 262 };
const int noteDur[]    = { 500, 500, 500, 500, 500, 500, 500 };  // em ms
const int melodyLen    = sizeof(melody)/sizeof(melody[0]);

struct Obstacle { int x, h; bool active; };
Obstacle obst[MAX_OBST];
int prevObstX[MAX_OBST], prevObstH[MAX_OBST];

bool     inStart      = true;
bool     gameOver     = false;
bool     inTransition = false;
bool     blinkState   = false;
unsigned long blinkT       = 0;
unsigned long lastFrame    = 0;
unsigned long lastObstacle = 0;
int      level           = 1;
float    speedMultiplier = 1.0;
int      playerY         = PLAYER_Y0;
int      prevPlayerY     = PLAYER_Y0;
int      score           = 0;
int      highScore       = 0;
unsigned long currJumpDuration;
int           currObstStep;
unsigned long currObstInterval;
int currentNote         = 0;
unsigned long noteStart = 0;

// ==== Display Helpers ====================================================

void drawGradientBackground() {
  const int bands = 20;
  for (int i = 0; i < bands; i++) {
    float t = i / float(bands - 1);
    uint16_t color = tft.color565(
      255,
      uint8_t(100*(1-t) + 200*t),
      uint8_t(0*(1-t)   + 50*t)
    );
    int y = i * SCREEN_H / bands;
    tft.fillRect(0, y, SCREEN_W, SCREEN_H/bands + 1, color);
  }
}

void showCentered(const char* txt, int textSize, uint16_t fg) {
  tft.setTextSize(textSize);
  tft.setTextColor(fg);
  int16_t w = strlen(txt) * 6 * textSize;
  tft.setCursor((SCREEN_W - w)/2, SCREEN_H/2 - (8*textSize)/2);
  tft.print(txt);
}

void drawStartScreen() {
  drawGradientBackground();
  for (int x = 0; x < SCREEN_W; x += 4) {
    float p = 2*3.14159 * x / 60.0;
    int y = SCREEN_H*2/3 + 10 + int(8*sin(p));
    tft.drawPixel(x, y, CYAN);
    tft.drawPixel(x, y+1, CYAN);
  }
  tft.fillCircle(SCREEN_W/2, SCREEN_H/3 + 30, 60, YELLOW);
  tft.setTextSize(4);
  tft.setTextColor(BLACK);
  const char* title = "JUMPMAN 2.0";
  int16_t tw = strlen(title) * 6 * 4;
  tft.setCursor((SCREEN_W - tw)/2, SCREEN_H/2 - 40);
  tft.print(title);
}

void drawGround() {
  tft.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, GREEN);
}

void drawHUDStatic() {
  tft.fillRect(0, 0, SCREEN_W, 20, BLACK);
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(10, 3);   tft.print("Score:");
  tft.setCursor(170, 3);  tft.print("Level:");
  tft.setCursor(310, 3);  tft.print("Highest:");
}

void updateHUD() {
  unsigned long nextThreshold = 10UL * level * (level+1) / 2;
  char buf[16];
  sprintf(buf, "%d/%lu", score, nextThreshold);
  tft.fillRect(82, 3, 60, 16, BLACK);
  tft.setCursor(82, 3);  tft.print(buf);
  tft.fillRect(240, 3, 40, 16, BLACK);
  tft.setCursor(240, 3); tft.print(level);
  tft.fillRect(410, 3, 40, 16, BLACK);
  tft.setCursor(410, 3); tft.print(highScore);
}

void drawGradientBand(int y0, int h) {
  const int bands = 20;
  int bandH = SCREEN_H / bands + 1;
  int startBand = y0 / bandH;
  int endBand   = (y0 + h) / bandH;
  if (startBand < 0) startBand = 0;
  if (endBand > bands-1) endBand = bands-1;
  for (int b = startBand; b <= endBand; b++) {
    float t = b / float(bands - 1);
    uint16_t color = tft.color565(
      255,
      uint8_t(100*(1-t) + 200*t),
      uint8_t(0*(1-t)   + 50*t)
    );
    int y = b * bandH;
    tft.fillRect(0, y, SCREEN_W, bandH, color);
  }
}

// ==== Input & Player ====================================================

bool isButtonPressed() {
  return analogRead(BUTTON_PIN) > 500;
}

void handleJump() {
  static bool jumping = false;
  static unsigned long js = 0;
  if (isButtonPressed() && !jumping && playerY == PLAYER_Y0) {
    jumping = true;
    js = millis();
  }
  if (jumping) {
    unsigned long jt = millis() - js;
    float t = jt < currJumpDuration ? jt / float(currJumpDuration) : 1.0;
    float h = -4 * BASE_JUMP_HEIGHT * (t - 0.5)*(t - 0.5) + BASE_JUMP_HEIGHT;
    playerY = PLAYER_Y0 - int(h);
    if (jt >= currJumpDuration) {
      jumping = false;
      playerY = PLAYER_Y0;
    }
  }
}

// ==== Game Loop ==========================================================

void updateGame() {
  //updateMusic();
  tft.fillCircle(PLAYER_X + PLAYER_SIZE/2,
                 prevPlayerY + PLAYER_SIZE/2,
                 PLAYER_SIZE/2,
                 CYAN);
  for (int i = 0; i < MAX_OBST; i++) {
    if (prevObstH[i] > 0) {
      tft.fillRect(prevObstX[i],
                   GROUND_Y - prevObstH[i],
                   OBST_WIDTH,
                   prevObstH[i],
                   CYAN);
    }
  }
  unsigned long now = millis();
  if (now - lastObstacle >= currObstInterval) {
    lastObstacle = now;
    for (int i = 0; i < MAX_OBST; i++) {
      if (!obst[i].active) {
        obst[i].active = true;
        obst[i].x = SCREEN_W;
        obst[i].h = OBST_HEIGHTS[random(0,2)];
        break;
      }
    }
  }
  for (int i = 0; i < MAX_OBST; i++) {
    if (obst[i].active) {
      obst[i].x -= currObstStep;
      if (obst[i].x + OBST_WIDTH < 0) {
        obst[i].active = false;
        score++;
        playScoreSound();
      } else {
        tft.fillRect(obst[i].x,
                     GROUND_Y - obst[i].h,
                     OBST_WIDTH,
                     obst[i].h,
                     BROWN);
        prevObstX[i] = obst[i].x;
        prevObstH[i] = obst[i].h;
        if (obst[i].x <= PLAYER_X + PLAYER_SIZE &&
            obst[i].x + OBST_WIDTH >= PLAYER_X &&
            playerY + PLAYER_SIZE > GROUND_Y - obst[i].h) {
          triggerGameOver();
          return;
        }
      }
    } else {
      prevObstH[i] = 0;
    }
  }
  tft.fillCircle(PLAYER_X + PLAYER_SIZE/2,
                 playerY + PLAYER_SIZE/2,
                 PLAYER_SIZE/2,
                 PURPLE);
  prevPlayerY = playerY;
  updateHUD();
}

// ==== Level Transition ==================================================

void beginLevelTransition() {
  inTransition = true;

  drawGradientBackground();
  char bufLevel[16];
  sprintf(bufLevel, "LEVEL %d", level+1);
  showCentered(bufLevel, 3, BLACK);
  playTone(550,750);
  delay(1000);

  drawGradientBackground();
  showCentered("READY?", 3, BLACK);
  playTone(550,750);
  delay(1000);

  drawGradientBackground();
  showCentered("GO!", 4, BLACK);
  playTone(550,750);
  delay(1000);

  speedMultiplier *= 1.15;
  currObstStep     = int(BASE_OBST_STEP * speedMultiplier);
  currObstInterval = (unsigned long)(BASE_OBST_INTERVAL / speedMultiplier);
  currJumpDuration = (unsigned long)(BASE_JUMP_DURATION / speedMultiplier);

  tft.fillScreen(CYAN);
  drawGround();
  drawHUDStatic();
  updateHUD();
  lastFrame    = millis();
  lastObstacle = millis();
  for (int i = 0; i < MAX_OBST; i++) obst[i].active = false;

  playerY = prevPlayerY = PLAYER_Y0;
  tft.fillCircle(
    PLAYER_X + PLAYER_SIZE/2,
    playerY   + PLAYER_SIZE/2,
    PLAYER_SIZE/2,
    PURPLE
  );

  level++;
  inTransition = false;
  playTone(1000,1000);
}

// ==== Game Over =========================================================

void triggerGameOver() {
  tft.fillCircle(
    PLAYER_X + PLAYER_SIZE/2,
    playerY   + PLAYER_SIZE/2,
    PLAYER_SIZE/2,
    PURPLE
  );
  playDeathSound();
  gameOver = true;
  if (score > highScore) {
    highScore = score;
    EEPROM.put(HS_ADDR, highScore);
  }
  gameOverBloodAnimation();
  tft.fillScreen(RED);
  drawSkull(10 + 30, 10 + 30, 30);
  drawSkull(SCREEN_W - 10 - 30, 10 + 30, 30);
  tft.setTextSize(4);
  tft.setTextColor(WHITE);
  const char* line1 = "GAME OVER!";
  int16_t w1 = strlen(line1)*6*4;
  tft.setCursor((SCREEN_W - w1)/2, 40);
  tft.print(line1);
  unsigned long nextThreshold = 10UL * level * (level+1) / 2;
  char buf1[32];
  sprintf(buf1, "Level: %d   Score: %d/%lu", level, score, nextThreshold);
  tft.setTextSize(3);
  int16_t w2 = strlen(buf1)*6*3;
  tft.setCursor((SCREEN_W - w2)/2, 120);
  tft.print(buf1);
  char buf2[32];
  sprintf(buf2, "Highest: %d", highScore);
  tft.setTextSize(2);
  int16_t w3 = strlen(buf2)*6*2;
  tft.setCursor((SCREEN_W - w3)/2, 180);
  tft.print(buf2);
}

void gameOverBloodAnimation() {
  const int DRIPS = 40, LEVELS = 2, steps = 50;
  const int W[2] = {6,12};
  for (int p = 0; p < LEVELS; p++) {
    int w = W[p], dripX[DRIPS], dripY[DRIPS];
    for (int i = 0; i < DRIPS; i++) { dripX[i] = random(0,SCREEN_W); dripY[i] = 0; }
    for (int s = 0; s < steps; s++) {
      for (int i = 0; i < DRIPS; i++) {
        if (dripY[i] < SCREEN_H) {
          int ln = random(4,12);
          tft.fillRect(dripX[i], dripY[i], w, ln, RED);
          dripY[i] += ln;
        }
      }
      delay(10);
    }
  }
  for (int y = 0; y < SCREEN_H; y += 4) {
    tft.fillRect(0, y, SCREEN_W, 4, RED);
    delay(10);
  }
}

void drawSkull(int16_t cx, int16_t cy, int16_t r) {
  tft.fillCircle(cx, cy, r, WHITE);
  int16_t eyeR = r/4;
  int16_t eyeY = cy - r/4;
  int16_t eyeXoff = r/2 - eyeR;
  tft.fillCircle(cx - eyeXoff, eyeY, eyeR, RED);
  tft.fillCircle(cx + eyeXoff, eyeY, eyeR, RED);
  int16_t noseH = eyeR;
  tft.fillTriangle(cx, cy,
                   cx - eyeR/2, cy + noseH,
                   cx + eyeR/2, cy + noseH,
                   RED);
  int16_t teethW = r/3, teethH = r/6;
  int16_t startX = cx - teethW * 1.5;
  int16_t teethY = cy + r/2;
  for (int i = 0; i < 3; i++) {
    tft.fillRect(startX + i * (teethW + 2),
                 teethY,
                 teethW, teethH,
                 RED);
  }
}

// ==== Reset & Sounds ====================================================

void resetGame() {
  drawGradientBackground();
  showCentered("LEVEL 1", 3, BLACK);
  playTone(550,750);
  delay(1000);
  drawGradientBackground();
  showCentered("READY?", 3, BLACK);
  playTone(550,750);
  delay(1000);
  drawGradientBackground();
  showCentered("GO!", 4, BLACK);
  playTone(550,750);
  delay(1000);
  score = 0;
  gameOver = false;
  inTransition = false;
  lastFrame = millis();
  lastObstacle = millis();
  currJumpDuration = BASE_JUMP_DURATION;
  currObstStep     = BASE_OBST_STEP;
  currObstInterval = BASE_OBST_INTERVAL;
  level = 1;
  tft.fillScreen(CYAN);
  drawGround();
  drawHUDStatic();
  updateHUD();
  for (int i = 0; i < MAX_OBST; i++) obst[i].active = false;
  playerY = prevPlayerY = PLAYER_Y0;
  tft.fillCircle(PLAYER_X + PLAYER_SIZE/2,
                 playerY + PLAYER_SIZE/2,
                 PLAYER_SIZE/2,
                 PURPLE);
  playTone(1000,1000);
}

void playTone(int pTone, int pTime) { tone(BUZZER_PIN,pTone,pTime); }
void playScoreSound() { playTone(523,80); }
void playDeathSound() { 
  playTone(300,500);
  delay(750);
  playTone(250,500);
  delay(750);
  playTone(200,500);
  delay(750);
  playTone(150,500);
  delay(750);

  playTone(100,750);
  delay(200);
  playTone(100,750);
  delay(200);
  playTone(100,750);
  delay(200);
  playTone(100,750);
  delay(200);
  playTone(100,750);
  delay(200);
  playTone(100,750);
  delay(200);
  playTone(100,750);
  delay(200);
  playTone(100,750);
  delay(200);
}
void playTransitionMusic(unsigned long duration) {
  unsigned long start = millis();
  while (millis() - start < duration) {
    tone(BUZZER_PIN, 440, 180);
    delay(250);
    tone(BUZZER_PIN, 660, 180);
    delay(250);
    tone(BUZZER_PIN, 880, 180);
    delay(250);
    tone(BUZZER_PIN, 660, 180);
    delay(250);
    tone(BUZZER_PIN, 440, 180);
    delay(250);
  }
  noTone(BUZZER_PIN);
}
void updateMusic() {
  unsigned long now = millis();
  if (now - noteStart >= noteDur[currentNote]) {
    noteStart = now;
    currentNote = (currentNote + 1) % melodyLen;
    tone(BUZZER_PIN, melody[currentNote]);
  }
}

// ==== Arduino Core ======================================================

void setup() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.begin(115200);
  randomSeed(analogRead(0));
  EEPROM.get(HS_ADDR, highScore);
  currJumpDuration = BASE_JUMP_DURATION;
  currObstStep     = BASE_OBST_STEP;
  currObstInterval = BASE_OBST_INTERVAL;
  tft.reset();
  tft.begin(tft_id);
  tft.setRotation(1);
  tft.fillScreen(BLACK);
  drawStartScreen();
}

void loop() {
  unsigned long now = millis();

  if (inStart) {
    if (now - blinkT > 1000) {
      blinkT = now;
      blinkState = !blinkState;

      const int ty = SCREEN_H/2 + 60;
      const int th = 16;
      drawGradientBand(ty - 2, th + 4);

      if (blinkState) {
        tft.setTextSize(2);
        tft.setTextColor(BLACK);
        const char* pr = "Press to Start";
        int16_t pw = strlen(pr)*6*2;
        tft.setCursor((SCREEN_W - pw)/2, ty);
        tft.print(pr);
      }
    }

    if (isButtonPressed()) {
      inStart = false;
      resetGame();
    }
    return;
  }

  if (gameOver) {
    if (now - blinkT > 1000) {
      blinkT = now;
      blinkState = !blinkState;
      tft.fillRect(0, 240, SCREEN_W, 16, RED);
      if (blinkState) {
        tft.setTextSize(2);
        tft.setTextColor(WHITE);
        const char* pr = "Press to Restart";
        int16_t pw = strlen(pr)*6*2;
        tft.setCursor((SCREEN_W-pw)/2, 240);
        tft.print(pr);
      }
    }
    if (isButtonPressed()) {
      level = 1;
      speedMultiplier = 1.0;
      currJumpDuration = BASE_JUMP_DURATION;
      currObstStep     = BASE_OBST_STEP;
      currObstInterval = BASE_OBST_INTERVAL;
      inStart = true;
      gameOver = false;
      tft.fillScreen(BLACK);
      drawStartScreen();
      blinkState = false;
      blinkT = millis();
    }
    return;
  }

  if (inTransition) return;

  handleJump();
  if (now - lastFrame >= FRAME_TIME) {
    lastFrame = now;
    updateGame();
    unsigned long nextThreshold = 10UL * level * (level+1) / 2;
    if (score >= nextThreshold) beginLevelTransition();
  }
}

