// 컴파일 오류 해결용
#define ARDUINO_ARCH_ESP32
// 4. [중요] C6 컴파일 에러 해결을 위한 강제 설정
// 라이브러리가 최적화 코드를 건너뛰고 표준 SPI 방식을 쓰게 유도합니다.
#if defined (ESP32C6) || defined (ARDUINO_ARCH_ESP32)
  #undef ESP32
#endif


#define ST7789_DRIVER     // 드라이버 설정
#define TFT_RGB_ORDER TFT_BGR  // TFT_RGB TFT_BGR (색상이 이상하면 변경)
#define TFT_WIDTH  240
#define TFT_HEIGHT 280

#define TFT_DRIVER       0x7789

#define TFT_SCLK  8 // 1 5
#define TFT_MOSI 10 // 1 3
#define TFT_MISO -1 // 사용 안 함
#define TFT_CS   1 // 0 1
#define TFT_DC   2 // 0 2
#define TFT_RST  7 // 1 6
#define BL_PIN   -1 // 3.3v로 대체 

#define LOAD_GLCD   // 폰트 설정
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT
#define SPI_FREQUENCY 20000000
