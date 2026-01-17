#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TFT_eSPI.h>

// 디버그 설정
#define USE_MEASURE_MODE 0  // 1: /measure (센서 3개 혼잡도 측정), 0: /test (테스트용 d1 센서 거리만 보냄.)

// WiFi (아래 순서대로 접속시작.)
constexpr char* phoneHotspotSsid = "U+Net3145"; // 폰 핫스팟
constexpr char* phoneHotspotPassword = "23416785";

constexpr char* surfaceHotspotSsid = "iptime6678"; // 서피스
constexpr char* surfaceHotspotPassword = "23416785";

constexpr char* schoolSsid = "SenWiFi_Free"; // 학교
constexpr char* schoolPassword = "sen2024!Wi";

// 연결할 WiFi 
char* ssid = "None"; 
char* password = "None";

// 호스트 주소
#if USE_MEASURE_MODE
  constexpr char* serverUrl = "https://api.sdhcc.mooo.com/measure"; // 센서 3개로 측정
#else
  constexpr char* serverUrl = "https://api.sdhcc.mooo.com/measure"; // 테스트용 (센서 1개 거리 측정) <-통합
#endif

#define WIFI_CONNECT_TRY_TICK 1000 // 와이파이 연결 주기
#define OVERALL_WIFI_CONNECT_TRY_LIMIT 5
#define WIFI_CONNECT_TRY_LIMIT 20 

// 센서 핀
#define TRIG1 22 // 0 4
#define ECHO1 23 // 0 5

#define TRIG2 20 // 추가
#define ECHO2 21

#define TRIG3 5   // 추가
#define ECHO3 17

TFT_eSPI tft = TFT_eSPI(); // Display
WiFiClientSecure client;
HTTPClient https;
bool isSending = false; // 전송 중복 방지 플래그

// config
constexpr int TICK_RATE = 0; // 화면 업데이트 및 센서 측정 및 포스트 전송 주기
constexpr int TIME_WINDOW = 2000; // 한 사람이 통과하는 데 걸리는 최대 시간 (MAX 2000ms)

#if USE_MEASURE_MODE
constexpr unsigned long SEND_INTERVAL = 5000; // 5초마다 서버 전송
#else
constexpr unsigned long SEND_INTERVAL = 0; // 서버 테스트용. 매 루프마다 전송
#endif 

// value
bool isWifiConnected = false;
long baseDist1, baseDist2, baseDist3;
int triggerDist = -1; // 최종 평균 거리 기준.

int inCount = 0;  // 들어온 사람
int outCount = 0; // 나간 사람
bool s1_active = false, s2_active = false, s3_active = false;
unsigned long lastS1 = 0, lastS2 = 0, lastS3 = 0; // 각 센서 마지막 감지 시간

unsigned long lastSendTime = 0;

long getDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(2);
  digitalWrite(trig, LOW);
  return pulseIn(echo, HIGH) * 0.034 / 2;
}

void trackMovement(long d1, long d2, long d3) {
  unsigned long now = millis();

  // 1. 각 센서 활성화 시간 기록
  if (d1 < triggerDist && d1 > 2) lastS1 = now;
  if (d2 < triggerDist && d2 > 2) lastS2 = now;
  if (d3 < triggerDist && d3 > 2) lastS3 = now;

  // 2. 방향 판정 로직 (S2가 트리거된 시점을 기준으로 분석)
  // [입장] 1번 -> 2번 -> 3번 순서로 시간이 찍혀 있어야 함
  if (lastS1 > 0 && lastS2 > lastS1 && lastS3 > lastS2) {
    if (now - lastS1 < TIME_WINDOW) { // 너무 오래전 기록은 무시
      inCount++;
      Serial.println(">>> Person ENTERED");
      resetTracker(); // 판정 후 초기화
    }
  }
  // [퇴장] 3번 -> 2번 -> 1번 순서
  else if (lastS3 > 0 && lastS2 > lastS3 && lastS1 > lastS2) {
    if (now - lastS3 < TIME_WINDOW) {
      outCount++;
      Serial.println("<<< Person EXITED");
      resetTracker();
    }
  }

  // 오래된 기록 자동 삭제 (노이즈 방지)
  if (now - lastS1 > TIME_WINDOW) lastS1 = 0;
  if (now - lastS2 > TIME_WINDOW) lastS2 = 0;
  if (now - lastS3 > TIME_WINDOW) lastS3 = 0;
}

void resetTracker() {
  lastS1 = 0; lastS2 = 0; lastS3 = 0;
}

uint16_t getCongestionColor(int level) {
  if (level < 20)      return TFT_GREEN;       // 0~19: 여유
  else if (level < 50) return TFT_YELLOW;      // 20~49: 보통
  else if (level < 80) return TFT_RED;         // 50~79: 혼잡함
  else                 return TFT_DARKGREY;    // 80~100: 매우 혼잡
}

void displayUpdate(long v1, long v2, long v3, int cgLevel, bool wifiOk, int loopTime) {
  // 최상단 타이틀 바 (0 ~ 30)
  tft.fillRect(0, 0, 240, 30, (wifiOk) ? TFT_BLUE : TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Cafeteria Traffic", 120, 5, 4);

  // 혼잡도 표시 및 바 그래프 (40 ~ 95)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.printf("Congestion: %d%% ", cgLevel);
  
  uint16_t barColor = getCongestionColor(cgLevel);
  tft.drawRect(10, 65, 220, 25, TFT_WHITE); // 바 테두리 조금 더 크게
  tft.fillRect(11, 66, 218, 23, TFT_BLACK); // 잔상 제거
  tft.fillRect(11, 66, map(cgLevel, 0, 100, 0, 218), 23, barColor);

  // IN / OUT 카운터 (105 ~ 135)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(15, 110);
  tft.printf("IN : %d ", inCount); 
  tft.setCursor(130, 110);
  tft.printf("OUT: %d ", outCount);

  // 개별 센서 정보 및 그래픽 (150 ~ 210)

  uint16_t s1Color = getCongestionColor(v1);
  uint16_t s2Color = getCongestionColor(v2);
  uint16_t s3Color = getCongestionColor(v3);
  
  tft.fillCircle(40, 165, 10, s1Color);
  tft.fillCircle(120, 165, 10, s2Color);
  tft.fillCircle(200, 165, 10, s3Color);
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("S1", 40, 180, 2);
  tft.drawCentreString("S2", 120, 180, 2);
  tft.drawCentreString("S3", 200, 180, 2);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 205);
  tft.printf("D1:%ld  D2:%ld  D3:%ld (cm)   ", v1, v2, v3);

  // 구분선 및 하단 WiFi/Ping 정보 (220 ~ 280)
  tft.fillRect(0, 225, 240, 2, TFT_DARKGREY); // 구분선
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(10, 240);
  tft.printf("WiFi: %s  |  Loop: %d ms   ", (wifiOk ? "OK" : "LOST"), loopTime);

  // 개발자 정보 표시
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setCursor(10, 260);
  tft.printf("20433 Choi Eun Woo");
}

void setup() {
  Serial.begin(115200);

  // 디스플레이 초기화
  tft.init();
  tft.setRotation(90); // 세로 모드
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Initializing...", 10, 10, 4);

  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);
  pinMode(TRIG3, OUTPUT); pinMode(ECHO3, INPUT);

  // 기준 거리 (벽까지의 거리)
  baseDist1 = getDistance(TRIG1, ECHO1);
  baseDist2 = getDistance(TRIG2, ECHO2);
  baseDist3 = getDistance(TRIG3, ECHO3);

  triggerDist = (baseDist1 + baseDist2 + baseDist3) / 3 - 20; // 기준 값 민감하면 조정하셈 

  // WiFi 연결
  bool isWifiConnected = false;
  int wifiConnectCount = 0;

  while (!isWifiConnected || wifiConnectCount <= OVERALL_WIFI_CONNECT_TRY_LIMIT) { 
    wifiConnectCount++;

    // 우선순위에 따라 SSID, 비번 설정
    for (int j = 0; j < 3; j++) {  //j는 wifi 아디/비번 개수
      switch (j) {
        case 0: 
          ssid = phoneHotspotSsid; password = phoneHotspotPassword;
          break;
        case 1: 
          ssid = surfaceHotspotSsid; password = surfaceHotspotPassword;
          break;
        case 2: 
          ssid = schoolSsid; password = schoolPassword;
          break;
      } 

      // WiFi 연결 시작
      Serial.printf("\n[WiFi] Connecting to %s", ssid);
      WiFi.begin(ssid, password);

      // WiFi 연결 대기 루프
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_TRY_LIMIT) {
        delay(500);
        Serial.print(".");
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected!");
        Serial.print("[WiFi] IP: "); Serial.println(WiFi.localIP());
        isWifiConnected = true;
        break;
      } else {
        Serial.println("\n[WiFi] Connection Failed.");
        isWifiConnected = false;
      }
    }
    if (isWifiConnected) { break; }
  }

  client.setInsecure();
  https.setReuse(true); // 연결 재사용 설정

  // WIFI 연결 후 화면 갱신
  tft.fillScreen(TFT_GREEN);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("WiFi Connected!", 120, 10, 2);
  delay(300);
  tft.fillScreen(TFT_YELLOW);
}


void loop() {
  unsigned long startPing = millis();

  // 센서 값 읽기 (상호 간섭 방지를 위해 딜레이 유지)
  long d1 = getDistance(TRIG1, ECHO1); delay(10);
  long d2 = getDistance(TRIG2, ECHO2); delay(10);
  long d3 = getDistance(TRIG3, ECHO3);

  // 혼잡도 계산
  trackMovement(d1, d2, d3);
  int currentInside = inCount - outCount;
  if (currentInside < 0) currentInside = 0; // 음수 방지
  int congestionPercent = constrain(map(currentInside, 0, 50, 0, 100), 0, 100);

  if (millis() - lastSendTime >= SEND_INTERVAL && !isSending) {
    if (WiFi.status() == WL_CONNECTED) {
      isSending = true;
      
      if (https.begin(client, serverUrl)) {
        https.addHeader("Content-Type", "application/json");
        String jsonPayload = "{\"congestion\":" + String(congestionPercent) + "}";
        
        // POST 실행
        int httpCode = https.POST(jsonPayload); 
        https.end();
      }
      lastSendTime = millis();
      isSending = false;
    }
  }

    unsigned long endPing = millis() - startPing; //화면에 출력할 핑
    #if USE_MEASURE_MODE
    displayUpdate(d1, d2, d3, congestionPercent, (WiFi.status() == WL_CONNECTED), int(endPing));
    #else
    displayUpdate(d1, d2, d3, d1, (WiFi.status() == WL_CONNECTED), int(endPing));
    #endif
    delay(TICK_RATE);
}