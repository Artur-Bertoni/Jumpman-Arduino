#include <Adafruit_GFX.h>
#include <Adafruit_TFTLCD.h>

#define BLACK            0x0000      /*   0,   0,   0 */
#define NAVY             0x000F      /*   0,   0, 128 */
#define DARKGREEN        0x03E0      /*   0, 128,   0 */
#define DARKCYAN         0x03EF      /*   0, 128, 128 */
#define MAROON           0x7800      /* 128,   0,   0 */
#define PURPLE           0x780F      /* 128,   0, 128 */
#define OLIVE            0x7BE0      /* 128, 128,   0 */
#define LIGHTGREY        0xC618      /* 192, 192, 192 */
#define DARKGREY         0x7BEF      /* 128, 128, 128 */
#define BLUE             0x001F      /*   0,   0, 255 */
#define GREEN            0x07E0      /*   0, 255,   0 */
#define CYAN             0x07FF      /*   0, 255, 255 */
#define RED              0xF800      /* 255,   0,   0 */
#define MAGENTA          0xF81F      /* 255,   0, 255 */
#define YELLOW           0xFFE0      /* 255, 255,   0 */
#define WHITE            0xFFFF      /* 255, 255, 255 */
#define ORANGE           0xFD20      /* 255, 165,   0 */
#define GREENYELLOW      0xAFE5      /* 173, 255,  47 */ 

uint16_t vg_identifier  = 0x9488;

Adafruit_TFTLCD monitor(40, 38, 39, 42, 41);

void setup() {
  Serial.begin(115200);
  preparaMonitor();
}

void preparaMonitor() 
{  
  monitor.reset();
  monitor.begin(vg_identifier);
  monitor.setRotation(1);

  int16_t vcolMin = 0;
  int16_t vlinMin = 0;
  int16_t vcolMax = monitor.width();  // 480
  int16_t vlinMax = monitor.height(); // 320
} 

void loop(void) {
  for(uint8_t rotation=0; rotation<4; rotation++) {
    monitor.setRotation(rotation);
    monitor.fillScreen(BLACK);
    delay(1000);
    monitor.fillScreen(GREEN);
  }
}
