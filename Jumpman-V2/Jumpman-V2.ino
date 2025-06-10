#include <Adafruit_GFX.h>
#include <Adafruit_TFTLCD.h>
#include <EEPROM.h>

// EEPROM address for high score
#define HS_ADDR 0

// Pin definitions
#define BUZZER_PIN 12
#define BUTTON_PIN A2

// Color definitions
#define BLACK    0x0000
#define WHITE    0xFFFF
#define GREEN    0x07E0
#define RED      0xF800
#define YELLOW   0xFFE0
#define CYAN     0x07FF

// TFT display identifier for ILI9488
uint16_t tft_id = 0x9488;
Adafruit_TFTLCD tft(40, 38, 39, 42, 41);

// Screen dimensions
const int SCREEN_W = 480;
const int SCREEN_H = 320;
const int GROUND_Y  = SCREEN_H - 20;

// Player settings
const int PLAYER_X    = 50;
const int PLAYER_SIZE = 20;
const int PLAYER_Y0   = GROUND_Y - PLAYER_SIZE;

// Jump settings
const unsigned long JUMP_DURATION = 1000;
const int JUMP_HEIGHT = 80;

// Obstacle settings
const int MAX_OBST       = 3;
const int OBST_WIDTH     = 20;
const int OBST_HEIGHTS[] = {20, 40};
unsigned long obst_interval = 2000;

// Frame timing (~60 FPS)
const unsigned long FRAME_TIME = 16;
const int OBST_STEP = 3;

// Game state
bool inStart         = true;
bool gameOver        = false;
unsigned long lastFrame    = 0;
unsigned long lastObstacle = 0;

int playerY     = PLAYER_Y0;
int prevPlayerY = PLAYER_Y0;
int score       = 0;
int highScore   = 0;

// Obstacles and previous
struct Obstacle { int x, h; bool active; };
Obstacle obst[MAX_OBST];
int prevObstX[MAX_OBST];
int prevObstH[MAX_OBST];

bool isButtonPressed() {
  return analogRead(BUTTON_PIN) > 500;
}

// Forward declarations
void drawStartScreen();
void drawGround();
void drawHUDStatic();
void updateHUD();
void handleJump();
void updateGame();
void triggerGameOver();
void resetGame();
void playScoreSound();
void playDeathSound();

// =================================================================
// Arduino setup
void setup() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.begin(115200);
  randomSeed(analogRead(0));

  // Load high score
  EEPROM.get(HS_ADDR, highScore);

  // Init display
  tft.reset();
  tft.begin(tft_id);
  tft.setRotation(1);
  tft.fillScreen(BLACK);

  // Show start screen
  drawStartScreen();
}

// =================================================================
// Main loop
void loop() {
  unsigned long now = millis();

  // Start screen logic
  if (inStart) {
    if (isButtonPressed()) {
      inStart = false;
      // Initialize game
      tft.fillScreen(CYAN);
      drawGround();
      drawHUDStatic();
      updateHUD();
      playerY = prevPlayerY = PLAYER_Y0;
      tft.fillRect(PLAYER_X, playerY, PLAYER_SIZE, PLAYER_SIZE, YELLOW);
      lastFrame = now;
      lastObstacle = now;
      score = 0;
      // Reset obstacles
      for (int i = 0; i < MAX_OBST; i++) {
        obst[i].active = false;
        prevObstH[i] = 0;
      }
    }
    return;
  }

  // Game over logic
  if (gameOver) {
    // show restart prompt
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    const char* pr = "Press to Restart";
    int16_t pw = strlen(pr) * 6 * 2;
    tft.setCursor((SCREEN_W - pw)/2, SCREEN_H/2 + 40);
    tft.print(pr);
    if (isButtonPressed()) {
      resetGame();
    }
    return;
  }

  // In-game
  handleJump();
  if (now - lastFrame >= FRAME_TIME) {
    lastFrame = now;
    updateGame();
  }
}

// =================================================================
// Draw start screen with smooth gradient, waves, sun
void drawStartScreen() {
  const int bands = 20;
  for (int i = 0; i < bands; i++) {
    float t = i / float(bands - 1);
    uint8_t r = 255;
    uint8_t g = uint8_t(100*(1-t) + 200*t);
    uint8_t b = uint8_t(0*(1-t) + 50*t);
    uint16_t color = tft.color565(r, g, b);
    int y = i * SCREEN_H / bands;
    tft.fillRect(0, y, SCREEN_W, SCREEN_H/bands + 1, color);
  }
  // waves
  for (int x = 0; x < SCREEN_W; x+=4) {
    float phase = 2*3.14159*x/60.0;
    int y = SCREEN_H*2/3 + 10 + int(8*sin(phase));
    tft.drawPixel(x, y, CYAN);
    tft.drawPixel(x, y+1, CYAN);
  }
  // sun
  int sunR = 60;
  tft.fillCircle(SCREEN_W/2, SCREEN_H/3+sunR/2, sunR, YELLOW);
  // title
  tft.setTextSize(4);
  tft.setTextColor(BLACK);
  const char* title = "Jumpman";
  int16_t tw = strlen(title)*6*4;
  tft.setCursor((SCREEN_W-tw)/2, SCREEN_H/2-40);
  tft.print(title);
  // prompt
  tft.setTextSize(2);
  const char* pr = "Press to Start";
  int16_t pw = strlen(pr)*6*2;
  tft.setCursor((SCREEN_W-pw)/2, SCREEN_H/2+20);
  tft.print(pr);
}

// =================================================================
// Draw ground line
void drawGround() {
  tft.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H-GROUND_Y, GREEN);
}

// =================================================================
// Update game: clear, update, draw
void updateGame() {
  // clear player
  tft.fillRect(PLAYER_X, prevPlayerY, PLAYER_SIZE, PLAYER_SIZE, CYAN);
  // clear obstacles
  for (int i = 0; i < MAX_OBST; i++) {
    if (prevObstH[i] > 0) {
      tft.fillRect(prevObstX[i], GROUND_Y-prevObstH[i], OBST_WIDTH, prevObstH[i], CYAN);
    }
  }
  unsigned long now = millis();
  // spawn new
  if (now - lastObstacle >= obst_interval) {
    lastObstacle = now;
    for (int i=0;i<MAX_OBST;i++) if (!obst[i].active) {
      obst[i].active=true;
      obst[i].x=SCREEN_W;
      obst[i].h=OBST_HEIGHTS[random(0,2)];
      break;
    }
    if(score>0 && score%5==0 && obst_interval>500) obst_interval-=100;
  }
  // move & draw obs
  for (int i=0;i<MAX_OBST;i++){
    if(obst[i].active){
      obst[i].x-=OBST_STEP;
      if(obst[i].x+OBST_WIDTH<0){obst[i].active=false;score++;playScoreSound();}
      else {
        tft.fillRect(obst[i].x, GROUND_Y-obst[i].h, OBST_WIDTH, obst[i].h, RED);
        prevObstX[i]=obst[i].x; prevObstH[i]=obst[i].h;
        // collision
        if(obst[i].x<=PLAYER_X+PLAYER_SIZE && obst[i].x+OBST_WIDTH>=PLAYER_X)
          if(playerY+PLAYER_SIZE>GROUND_Y-obst[i].h){triggerGameOver();return;}
      }
    } else prevObstH[i]=0;
  }
  // draw player
  tft.fillRect(PLAYER_X, playerY, PLAYER_SIZE, PLAYER_SIZE, YELLOW);
  prevPlayerY=playerY;
  // HUD
  updateHUD();
}

// =================================================================
// Handle jump
void handleJump() {
  static bool jumping=false; static unsigned long js=0;
  if(isButtonPressed() && !jumping && playerY==PLAYER_Y0){jumping=true;js=millis();}
  if(jumping){unsigned long jt=millis()-js;
    float t=jt<JUMP_DURATION?jt/(float)JUMP_DURATION:1.0;
    int h=JUMP_HEIGHT;
    float ht=-4*h*(t-0.5)*(t-0.5)+h;
    playerY=PLAYER_Y0-int(ht);
    if(jt>=JUMP_DURATION){jumping=false;playerY=PLAYER_Y0;}
  }
}

// =================================================================
// Static HUD
void drawHUDStatic(){
  tft.fillRect(0,0,SCREEN_W,20,BLACK);
  tft.setTextSize(2);tft.setTextColor(WHITE);
  tft.setCursor(10,3);tft.print("Score:");
  tft.setCursor(140,3);tft.print("Hi:");
}

// Update HUD
void updateHUD(){
  tft.fillRect(82,3,36,16,BLACK); tft.setCursor(82,3); tft.print(score);
  tft.fillRect(176,3,36,16,BLACK); tft.setCursor(176,3); tft.print(highScore);
}

// =================================================================
void gameOverBloodAnimation() {
  const int DRIPS = 40;
  const int PHASES = 2;
  // larguras: fino, médio e grosso
  const int widths[PHASES] = {6, 12};
  // número de passos em cada fase
  const int stepsPerPhase = 50;

  for (int p = 0; p < PHASES; p++) {
    int w = widths[p];
    int dripX[DRIPS];
    int dripY[DRIPS];
    // inicializa drips em X aleatório e Y=0
    for (int i = 0; i < DRIPS; i++) {
      dripX[i] = random(0, SCREEN_W);
      dripY[i] = 0;
    }
    // em cada passo, cada drip desce um pouco
    for (int s = 0; s < stepsPerPhase; s++) {
      for (int i = 0; i < DRIPS; i++) {
        if (dripY[i] < SCREEN_H) {
          int len = random(4, 12);
          // desenha uma faixa vertical de largura w
          tft.fillRect(dripX[i], dripY[i], w, len, RED);
          dripY[i] += len;
        }
      }
    }
  }
  // depois de todas as fases, preenche toda a tela
  for (int y = 0; y < SCREEN_H; y += 4) {
    tft.fillRect(0, y, SCREEN_W, 4, RED);
    delay(10);
  }
}

// Desenha um crânio centrado em (cx,cy) com raio 'r'
void drawSkull(int16_t cx, int16_t cy, int16_t r) {
  // 1) Cabeça
  tft.fillCircle(cx, cy, r, WHITE);

  // 2) Olhos (um pouco acima do centro)
  int16_t eyeR    = r/4;
  int16_t eyeY    = cy - r/4;
  int16_t eyeXoff = r/2 - eyeR;
  tft.fillCircle(cx - eyeXoff, eyeY, eyeR, RED);  // buraco do olho esquerdo
  tft.fillCircle(cx + eyeXoff, eyeY, eyeR, RED);  // buraco do olho direito

  // 3) Nariz (triângulo invertido)
  int16_t noseH = eyeR;
  tft.fillTriangle(
    cx,              cy,
    cx - eyeR/2,     cy + noseH,
    cx + eyeR/2,     cy + noseH,
    RED
  );

  // 4) Dentes (pequenos retângulos abaixo)
  int16_t teethW = r/3;
  int16_t teethH = r/6;
  int16_t startX = cx - teethW * 1.5;
  int16_t teethY = cy + r/2;
  for (int i = 0; i < 3; i++) {
    tft.fillRect(
      startX + i * (teethW + 2),
      teethY,
      teethW, teethH,
      RED
    );
  }
}

// Game over screen
void triggerGameOver() {
  playDeathSound();
  gameOver = true;
  if (score > highScore) {
    highScore = score;
    EEPROM.put(HS_ADDR, highScore);
  }

  // animação de sangue e preenche tela
  gameOverBloodAnimation();  // já faz fillScreen(RED) ao final

  // ——— Desenha duas caveiras grandes e fixas nos cantos superiores ———
  const int skullR = 30;      // raio do crânio (pode ajustar)
  const int margin = 10;
  // canto superior esquerdo
  drawSkull(margin + skullR, margin + skullR, skullR);
  // canto superior direito
  drawSkull(SCREEN_W - margin - skullR, margin + skullR, skullR);

  // ——— Texto centralizado ———
  tft.setTextSize(4);
  tft.setTextColor(WHITE);
  const char* line1 = "GAME OVER";
  int16_t w1 = strlen(line1) * 6 * 4;
  tft.setCursor((SCREEN_W - w1) / 2, SCREEN_H / 2 - 40);
  tft.print(line1);

  tft.setTextSize(2);
  char buf[32];
  sprintf(buf, "Score: %d  Hi: %d", score, highScore);
  int16_t w2 = strlen(buf) * 6 * 2;
  tft.setCursor((SCREEN_W - w2) / 2, SCREEN_H / 2 + 10);
  tft.print(buf);

  const char* prompt = "Press to Restart";
  int16_t w3 = strlen(prompt) * 6 * 2;
  tft.setCursor((SCREEN_W - w3) / 2, SCREEN_H / 2 + 40);
  tft.print(prompt);
}


// Reset game
void resetGame(){
  gameOver=false;inStart=false;score=0;obst_interval=2000;
  playerY=prevPlayerY=PLAYER_Y0;lastFrame=millis();lastObstacle=millis();
  for(int i=0;i<MAX_OBST;i++)obst[i].active=false;
  tft.fillScreen(CYAN);
  drawGround();drawHUDStatic();updateHUD();
  tft.fillRect(PLAYER_X,playerY,PLAYER_SIZE,PLAYER_SIZE,YELLOW);
}

// =================================================================
// Sounds
void playScoreSound(){tone(BUZZER_PIN,523,80);}
void playDeathSound(){tone(BUZZER_PIN,220,200);}
