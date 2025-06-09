#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Configurações fixas
#define LCD_ADDR 0x27
#define LCD_COLS 20
#define LCD_ROWS 4
#define PLAYER_COL 1
#define GROUND_ROW 3
#define OBSTACLE_MAX 3
#define BUTTON_PIN A2
#define BUZZER_PIN 12

// Duração dos pulos (ms)
#define JUMP_MEDIUM_DURATION 700
#define JUMP_LONG_DURATION 1000
#define EXTRA_JUMP_TIME 200
#define JUMP_QUEUE_WINDOW 150

// Limites de velocidade (ms)
#define MIN_OBSTACLE_INTERVAL 1200
#define MIN_UPDATE_INTERVAL 250
#define MIN_JUMP_MEDIUM 500
#define MIN_JUMP_LONG 800
#define MIN_JUMP_INTERVAL 400

// Velocidades iniciais (ms)
#define START_UPDATE_INTERVAL 350
#define START_OBSTACLE_INTERVAL 2800

// Textos de interface
#define TEXT_TITLE "Jumpman -- VIDA!"
#define TEXT_GAME_OVER " GAME OVER! "
#define TEXT_START "Hold to Start"
#define TEXT_RESTART "Hold to Restart!"
#define TEXT_HIGHEST "Highest: "
#define TEXT_NEW_HIGHEST "New Highest: "
#define TEXT_SCORE "Score: "

// Códigos Caracteres Personalizados
#define PLAYER_WALK_CHAR 0
#define PLAYER_JUMP_CHAR 1
#define BASE_OBSTACLE_CHAR 2
#define TOP_OBSTACLE_CHAR 3
#define SKULL_CHAR 4

// Músicas
#define STARTUP_TONE_DURATION 150

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Estado do jogador
int jumpHeight = 0;
int playerRow = GROUND_ROW;
bool jumping = false;
bool canExtendJump = false;
unsigned long jumpStartTime = 0;

// Obstáculos
int obstacleCols[OBSTACLE_MAX];
int obstacleHeights[OBSTACLE_MAX];
bool obstacleActives[OBSTACLE_MAX];

// Tempo e dificuldade
unsigned long lastUpdate = 0;
unsigned long updateInterval = START_UPDATE_INTERVAL;
unsigned long lastObstacleTime = 0;
unsigned long obstacleInterval = START_OBSTACLE_INTERVAL;
unsigned long jumpMediumDuration = JUMP_MEDIUM_DURATION;
unsigned long jumpLongDuration = JUMP_LONG_DURATION;
unsigned long lastJumpTime = 0;
unsigned long buttonPressTime = 0;

// Controle de jogo
int score = 0;
int highScore = 0;
int lastObstacleHeight = 1;
int sameHeightCount = 0;
bool gameOver = false;
bool lastButtonState = false;
bool jumpRequested = false;

// Caracteres personalizados
byte playerChar1[] = { B01110, B01110, B00100, B01110, B10101, B00100, B01010, B10001 };
byte playerChar2[] = { B01110, B01100, B00101, B01110, B10100, B00101, B01010, B10000 };
byte baseObstacleChar[] = { B11111, B10101, B11111, B10101, B10101, B11111, B10101, B11111 };
byte topObstacleChar[] = { B00100, B00100, B01010, B01110, B11011, B10101, B10101, B11111 };
byte skullChar[] = { B01110, B10101, B10101, B11111, B11011, B01110, B01110, B00000 };

/*
  | ▓▓▓ |  | ▓▓▓ |  |▓▓▓▓▓|  |  ▓  |  | ▓▓▓ |
  | ▓▓▓ |  | ▓▓  |  |▓ ▓ ▓|  |  ▓  |  |▓ ▓ ▓|
  |  ▓  |  |  ▓ ▓|  |▓▓▓▓▓|  | ▓ ▓ |  |▓ ▓ ▓|
  | ▓▓▓ |  | ▓▓▓ |  |▓ ▓ ▓|  | ▓▓▓ |  |▓▓▓▓▓|
  |▓ ▓ ▓|  |▓ ▓  |  |▓ ▓ ▓|  |▓▓ ▓▓|  |▓▓ ▓▓|
  |  ▓  |  |  ▓ ▓|  |▓▓▓▓▓|  |▓ ▓ ▓|  | ▓▓▓ |
  | ▓ ▓ |  | ▓ ▓ |  |▓ ▓ ▓|  |▓ ▓ ▓|  | ▓▓▓ |
  |▓   ▓|  |▓    |  |▓▓▓▓▓|  |▓▓▓▓▓|  |     |
  */

void setup() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  noTone(BUZZER_PIN);
  lcd.init();
  lcd.backlight();
  randomSeed(analogRead(0));

  lcd.createChar(PLAYER_WALK_CHAR, playerChar1);
  lcd.createChar(PLAYER_JUMP_CHAR, playerChar2);
  lcd.createChar(BASE_OBSTACLE_CHAR, baseObstacleChar);
  lcd.createChar(TOP_OBSTACLE_CHAR, topObstacleChar);
  lcd.createChar(SKULL_CHAR, skullChar);

  Serial.begin(9600);

  showStartScreen();
}

void loop() {
  if (gameOver) {
    showGameOver();
    waitForRestart();
    return;
  }

  handleJump();

  if (millis() - lastUpdate > updateInterval) {
    lastUpdate = millis();
    updateGame();
  }
}

// ======= CONTROLE DO JOGADOR =======
void handleJump() {
  bool buttonState = isButtonPressed();
  unsigned long currentTime = millis();

  // Detecta pressionamento inicial do botão
  if (buttonState && !lastButtonState) {
    buttonPressTime = currentTime;
    if (!jumping && playerRow == GROUND_ROW && 
        (currentTime - lastJumpTime) > MIN_JUMP_INTERVAL) {
      jumpRequested = true;
    }
  }

  // Finaliza o pulo quando o tempo acabar
  if (jumping && (currentTime - jumpStartTime) >= 
      (jumpHeight == 2 ? jumpLongDuration : jumpMediumDuration)) {
    jumping = false;
    jumpHeight = 0;
    playerRow = GROUND_ROW;
    lastJumpTime = currentTime;
  }

  // Inicia novo pulo se todas as condições forem atendidas
  if (!jumping && jumpRequested && playerRow == GROUND_ROW &&
      (currentTime - lastJumpTime) > MIN_JUMP_INTERVAL) {
    jumping = true;
    jumpHeight = 1;
    playerRow = 2;
    jumpStartTime = currentTime;
    canExtendJump = true;
    jumpRequested = false;
    lastJumpTime = currentTime;
  }

  // Extensão do pulo
  if (jumping && buttonState && jumpHeight == 1 && canExtendJump &&
      (currentTime - jumpStartTime) > EXTRA_JUMP_TIME) {
    jumpHeight = 2;
    playerRow = 1;
    canExtendJump = false;
  }

  // Sistema de fila de pulo
  if (!jumping && buttonState && (currentTime - buttonPressTime) < JUMP_QUEUE_WINDOW &&
      playerRow == GROUND_ROW && (currentTime - lastJumpTime) > MIN_JUMP_INTERVAL) {
    jumpRequested = true;
  }

  lastButtonState = buttonState;
}

// ======= MECÂNICA DO JOGO =======
void updateGame() {
  lcd.clear();
  updateObstacles();
  drawObstacles();
  drawPlayer();
  checkCollision();
  drawScore();
}

void updateObstacles() {
  if (millis() - lastObstacleTime > obstacleInterval) {
    for (int i = 0; i < OBSTACLE_MAX; i++) {
      if (!obstacleActives[i]) {
        if (sameHeightCount == 3) {
          obstacleHeights[i] = (lastObstacleHeight == 1) ? 2 : 1;
          sameHeightCount = 0;
        } 
        else {
          obstacleHeights[i] = random(1, 3);
          
          if (obstacleHeights[i] == lastObstacleHeight) {
            sameHeightCount++;
          } else {
            sameHeightCount = 0;
          }
        }
        
        lastObstacleHeight = obstacleHeights[i];
        obstacleCols[i] = LCD_COLS - 1;
        obstacleActives[i] = true;
        lastObstacleTime = millis();
        break;
      }
    }

    if ((score != 0) && (score % 5 == 0)) {
      if (obstacleInterval > MIN_OBSTACLE_INTERVAL) obstacleInterval -= 50;
      if (updateInterval > MIN_UPDATE_INTERVAL) updateInterval -= 10;
      if (jumpMediumDuration > MIN_JUMP_MEDIUM) jumpMediumDuration -= 20;
      if (jumpLongDuration > MIN_JUMP_LONG) jumpLongDuration -= 30;
    }
  }

  for (int i = 0; i < OBSTACLE_MAX; i++) {
    if (obstacleActives[i]) {
      obstacleCols[i]--;
      if (obstacleCols[i] < 0) {
        obstacleActives[i] = false;
        score++;
        playScoreSound();
      }
    }
  }
}

// ======= DESENHO NA TELA =======
void drawPlayer() {
  lcd.setCursor(PLAYER_COL, playerRow);
  if (!jumping && playerRow == GROUND_ROW) {
    lcd.write(PLAYER_WALK_CHAR);
  } else {
    lcd.write(PLAYER_JUMP_CHAR);
  }
}

void drawObstacles() {
  for (int i = 0; i < OBSTACLE_MAX; i++) {
    if (!obstacleActives[i]) continue;
    
    lcd.setCursor(obstacleCols[i], GROUND_ROW - obstacleHeights[i] + 1);
    lcd.write(TOP_OBSTACLE_CHAR);
    
    if (obstacleHeights[i] == 2) {
      lcd.setCursor(obstacleCols[i], GROUND_ROW);
      lcd.write(BASE_OBSTACLE_CHAR);
    }
  }
}

void drawScore() {
  lcd.setCursor(10, 0);
  lcd.print(TEXT_SCORE);
  lcd.print(score);
}

// ======= COLISÃO =======
void checkCollision() {
  for (int i = 0; i < OBSTACLE_MAX; i++) {
    if (obstacleActives[i] && obstacleCols[i] == PLAYER_COL) {
      if (playerRow == GROUND_ROW - obstacleHeights[i] + 1) {
        triggerGameOver();
        return;
      }
      if (obstacleHeights[i] == 2 && playerRow == GROUND_ROW) {
        triggerGameOver();
        return;
      }
    }
  }
}

void triggerGameOver() {
  playDeathSound();

  lcd.clear();
  drawObstacles();
  drawPlayer();
  gameOver = true;
  showFrozenGameOver();
}

// ======= TELAS =======
void showStartScreen() {
  lcd.clear();
  drawCenteredText(1, TEXT_TITLE);

  playStartupSound();

  int dotCount = 0;
  while (!isButtonPressed()) {
    drawTextWithDots(2, TEXT_START, 7, dotCount);
    dotCount = (dotCount + 1) % 6;
    delay(300);
  }

  waitForPress();
}

void showFrozenGameOver() {
  lcd.setCursor(3, 0);
  lcd.write(SKULL_CHAR); lcd.print(TEXT_GAME_OVER); lcd.write(SKULL_CHAR);
  lcd.setCursor(6, 1);
  lcd.print(TEXT_SCORE);
  lcd.print(score);
  delay(2000);
}

void showGameOver() {
  if (score > highScore) highScore = score;
  int frame = 0;

  while (!isButtonPressed()) {
    drawSkullsAnimation(frame++);
    delay(400);
  }

  waitForPress();
}

void drawSkullsAnimation(int frame) {
  lcd.clear();

  lcd.setCursor(3, 0);
  if (frame % 2 == 0) {
    lcd.write(SKULL_CHAR); lcd.print(TEXT_GAME_OVER); lcd.write(SKULL_CHAR);
  } else {
    lcd.print(String(" ") + TEXT_GAME_OVER + "   ");
  }

  lcd.setCursor(4, 1);
  lcd.print(score == highScore ? TEXT_NEW_HIGHEST : TEXT_HIGHEST);
  lcd.print(highScore);

  lcd.setCursor(6, 2);
  lcd.print(TEXT_SCORE);
  lcd.print(score);

  lcd.setCursor(2, 3);
  lcd.print(TEXT_RESTART);
}

// ======= UTILITÁRIAS =======
void waitForRestart() {
  waitForPress();
  resetGame();
}

void waitForPress() {
  while (!isButtonPressed()) delay(10);
}

bool isButtonPressed() {
  return analogRead(BUTTON_PIN) > 500;
}

void drawTextWithDots(int row, const char* text, int dotStart, int dotCount) {
  lcd.setCursor(3, row);
  lcd.print(text);

  for (int i = 0; i < 6; i++) {
    lcd.setCursor(dotStart + i, 3);
    lcd.print(i < dotCount ? "." : " ");
  }
}

void drawCenteredText(int row, const char* text) {
  int len = strlen(text);
  int pos = max(0, (LCD_COLS - len) / 2);
  lcd.setCursor(pos, row);
  lcd.print(text);
}

void resetGame() {
  gameOver = false;
  jumping = false;
  jumpHeight = 0;
  canExtendJump = false;
  playerRow = GROUND_ROW;
  score = 0;
  updateInterval = START_UPDATE_INTERVAL;
  obstacleInterval = START_OBSTACLE_INTERVAL;
  lastUpdate = millis();

  for (int i = 0; i < OBSTACLE_MAX; i++) {
    obstacleActives[i] = false;
    obstacleCols[i] = LCD_COLS - 1;
    obstacleHeights[i] = 1;
  }

  lcd.clear();
}

// ======= FUNÇÕES DE ÁUDIO =======

void playStartupSound() {
  // Melodia heróica de inicialização (notas: C5, G5, E5, C6, G5, E6)
  /*tone(BUZZER_PIN, 523, 200);  // C5
  delay(200);
  tone(BUZZER_PIN, 784, 150);  // G5
  delay(150);
  tone(BUZZER_PIN, 659, 150);  // E5
  delay(150);
  tone(BUZZER_PIN, 1046, 300); // C6
  delay(300);
  tone(BUZZER_PIN, 784, 200);  // G5
  delay(200);
  tone(BUZZER_PIN, 1318, 500); // E6 (mais agudo para final heróico)
  delay(500);
  noTone(BUZZER_PIN);
  
  // Pequena pausa e repete mais suave
  delay(100);
  tone(BUZZER_PIN, 523, 150);
  delay(150);
  tone(BUZZER_PIN, 784, 100);
  delay(100);
  tone(BUZZER_PIN, 1046, 300);
  delay(300);
  noTone(BUZZER_PIN);*/
}

void playDeathSound() {
  // Som de morte grave e longo (notas: A3, G3, E3, D3)
  /*tone(BUZZER_PIN, 220, 400);  // A3 grave
  delay(400);
  tone(BUZZER_PIN, 196, 300);  // G3
  delay(300);
  tone(BUZZER_PIN, 165, 600);  // E3 muito grave
  delay(600);
  
  // Efeito descendente final
  for (int freq = 165; freq >= 110; freq -= 5) {
    tone(BUZZER_PIN, freq, 30);
    delay(30);
  }
  noTone(BUZZER_PIN);
  
  // Pausa dramática e último tom
  delay(200);
  tone(BUZZER_PIN, 110, 800);  // A2 muito grave e longo
  delay(800);
  noTone(BUZZER_PIN);*/
}

void playScoreSound() {
  tone(BUZZER_PIN, 523, 80);
  delay(80);
  tone(BUZZER_PIN, 659, 80);
  delay(80);
  noTone(BUZZER_PIN);
}
