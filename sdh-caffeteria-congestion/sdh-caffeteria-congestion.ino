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

constexpr int WIFI_CONNECT_TRY_TICK = 1000; // 와이파이 연결 주기
constexpr int WIFI_CONNECT_TRY_LIMIT = 10; // 각 와이파이당 연결 시도 횟수

// 센서 핀
constexpr int TRIG1 = 22; // 0 4
constexpr int ECHO1 = 23; // 0 5

constexpr int TRIG2 = 20; // 추가
constexpr int ECHO2 = 21;

constexpr int TRIG3 = 5;  // 추가
constexpr int ECHO3 = 17;

TFT_eSPI tft = TFT_eSPI(); // Display
WiFiClientSecure client;
HTTPClient https;

// config
constexpr int TICK_RATE = 10; // 화면 업데이트 및 센서 측정 및 포스트 전송 주기
constexpr int TIME_WINDOW = 2000; // 한 사람이 통과하는 데 걸리는 최대 시간 (MAX 2000ms)

#if USE_MEASURE_MODE
constexpr unsigned long SEND_INTERVAL = 5000; // 5초마다 서버 전송
#else
constexpr unsigned long SEND_INTERVAL = 5000; // 서버 테스트용. 
#endif 

// value
int httpCode = -1;
bool isWifiConnected = false;


long baseDist1, baseDist2, baseDist3;
int triggerDist = -1; // 최종 평균 거리 기준.

int inCount = 0;  // 들어온 사람
int outCount = 0; // 나간 사람
bool s1_active = false, s2_active = false, s3_active = false;
unsigned long lastS1 = 0, lastS2 = 0, lastS3 = 0; // 각 센서 마지막 감지 시간

unsigned long lastSendTime = 0;
unsigned long lastDisplayMillis = 0;


void connectWifi() {
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
      isWifiConnected = true;
      Serial.println("\n[WiFi] Connected!");
      Serial.print("[WiFi] IP: "); Serial.println(WiFi.localIP());
      break;
    } else {
      isWifiConnected = false;
      Serial.println("\n[WiFi] Connection Failed.");
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawCentreString("Please check network.", 120, 200, 4);
    }
  }
}

long getDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(2);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 25000);
  if (duration == 0) { return -1; } // 감지 실패
  return duration * 0.034 / 2;
}

void trackMovement(long d1, long d2, long d3) {
  unsigned long now = millis();

  // 각 센서 활성화 시간 기록
  if (d1 < triggerDist && d1 > 2) lastS1 = now;
  if (d2 < triggerDist && d2 > 2) lastS2 = now;
  if (d3 < triggerDist && d3 > 2) lastS3 = now;

  // 방향 판정 로직 (S2가 트리거된 시점을 기준으로 분석)
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
  else                 return TFT_DARKGREY;    // 80~: 매우 혼잡
}

void displayUpdate(long v1, long v2, long v3, int cgLevel, bool wifiOk, int httpCode, int loopTime) {
  int safeCg = constrain(cgLevel, 0, 100); // 테스트모드에서 측정값이 너무 튀었을 때 강제 CPU 리셋 방지 
  int barWidth = map(safeCg, 0, 100, 0, 218);

  // 상단 바
  uint16_t cgColor = getCongestionColor(safeCg);
  tft.fillRect(0, 0, 240, 30, cgColor);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Cafeteria Traffic", 120, 5, 4);

  // 혼잡도 수치
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 40);
  tft.setTextSize(2);
  tft.printf("Congestion: %d%%  ", safeCg); // 뒤에 공백을 넣어 잔상 제거
  
  // 게이지 바
  tft.drawRect(10, 65, 220, 25, TFT_WHITE);
  tft.fillRect(11, 66, barWidth, 23, cgColor);
  if (barWidth < 218) {
    tft.fillRect(11 + barWidth, 66, 218 - barWidth, 23, TFT_BLACK); // 남은 부분만 검은색으로
  }

  // 카운터
  tft.setCursor(15, 110);
  tft.printf("IN:%d  OUT:%d    ", inCount, outCount);

  // 센서 원형 표시
  uint16_t s1Color = (v1 < triggerDist && v1 > 2) ? TFT_RED : TFT_GREEN;
  uint16_t s2Color = (v2 < triggerDist && v2 > 2) ? TFT_RED : TFT_GREEN;
  uint16_t s3Color = (v3 < triggerDist && v3 > 2) ? TFT_RED : TFT_GREEN;
  
  tft.fillCircle(40, 165, 10, s1Color);
  tft.fillCircle(120, 165, 10, s2Color);
  tft.fillCircle(200, 165, 10, s3Color);
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 205);
  tft.printf("D:%ld, %ld, %ld cm    ", v1, v2, v3);

  // 네트워크 정보
  tft.fillRect(0, 225, 240, 2, TFT_DARKGREY); 
  tft.setTextColor(wifiOk ? TFT_CYAN : TFT_RED, TFT_BLACK);
  tft.setCursor(10, 240);
  tft.printf("W:%s | H:%d | %dms   ", (wifiOk ? "OK" : "NO"), httpCode, loopTime);

  // 8. 개발자 정보
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawCentreString("20433 Choi Eun Woo", 120, 260, 2);
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);
  pinMode(TRIG3, OUTPUT); pinMode(ECHO3, INPUT);

  // 기준 거리 (벽까지의 거리)
  baseDist1 = getDistance(TRIG1, ECHO1);
  baseDist2 = getDistance(TRIG2, ECHO2);
  baseDist3 = getDistance(TRIG3, ECHO3);
  triggerDist = (baseDist1 + baseDist2 + baseDist3) / 3 - 20; // 기준 값 민감하면 조정하셈 
  
  // 디스플레이 초기화
  tft.init();
  tft.setRotation(90); // 세로 모드
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Initializing...", 10, 10, 4);

  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);
  pinMode(TRIG3, OUTPUT); pinMode(ECHO3, INPUT);

  // WiFi 연결
  while (!isWifiConnected) { 
    connectWifi();
    
    if (isWifiConnected) { break; }
  }

  client.setInsecure();
  https.setReuse(true); // 연결 재사용 설정

  // WIFI 연결 후 화면 갱신
  tft.fillScreen(TFT_GREEN);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawCentreString("WiFi Connected!", 120, 5, 4);
  delay(500);
  tft.fillScreen(TFT_YELLOW);
}


void loop() {
  unsigned long now = millis();
  
  // 센서 측정 및 트래킹
  long d1 = getDistance(TRIG1, ECHO1); delay(10);
  long d2 = getDistance(TRIG2, ECHO2); delay(10);
  long d3 = getDistance(TRIG3, ECHO3);
  trackMovement(d1, d2, d3);

  // HTTP 전송 (SEND_INTERVAL 주기)
  if (now - lastSendTime >= SEND_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
    httpCode = -1;

      if (https.begin(client, serverUrl)) {
        https.addHeader("Content-Type", "application/json");
        https.setTimeout(1000); 

        #if USE_MEASURE_MODE
        String jsonPayload = "{\"congestion\":" + String(congestionPercent) + "}";
        #else
        String jsonPayload = "{\"congestion\":" + String(d1) + "}";
        #endif

        // POST 실행
        httpCode = https.POST(jsonPayload); 
        https.end();
      }
    }
    lastSendTime = now;
  }

  // 디스플레이 업데이트 (약 200ms~500ms 주기로 제한)
  // 매 루프마다 그리면 CPU가 버티지 못하고 리셋됨
  if (now - lastDisplayMillis >= 300) { 
    int currentInside = inCount - outCount;
    if (currentInside < 0) { currentInside = 0; }
    int congestionPercent = constrain(map(currentInside, 0, 50, 0, 100), 0, 100);

    // 디스플레이 업데이트 호출
    #if USE_MEASURE_MODE
    displayUpdate(d1, d2, d3, congestionPercent, (WiFi.status() == WL_CONNECTED), httpCode, int(millis() - now));
    #else
    displayUpdate(d1, d2, d3, d1, (WiFi.status() == WL_CONNECTED), httpCode, int(millis() - now));
    #endif
    lastDisplayMillis = now;
  }

  yield(); // 시스템 OS에게 제어권을 잠시 넘김 (WDT 방지)
}
