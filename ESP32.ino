#include <Keypad.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>

// WIFI CONFIG
const char *ssid = "ENTER_SSI_NAME"; // WiFi Name
const char *password = "ENTER_SSI_PASSWORD"; // WiFi Password

const char* CAM_IP = "http://ESP32_CAM_IP"; //ip address (ESP32 CAM)

// THINGSPEAK CONFIG
const char* TS_API_KEY = "THINGSPEAK_API_KEY"; // Write API KEY
const char* TS_URL     = "http://api.thingspeak.com/update";

unsigned long lastTS = 0;
const unsigned long TS_INTERVAL = 20000;  // 20 sec minimum

// PIN DEFINITIONS
const int PIR_PIN     = 13;
const int BUZZER_PIN  = 16;
const int SERVO_PIN   = 17;

// KEYPAD
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {18, 19, 21, 22};
byte colPins[COLS] = {23, 25, 26, 27};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// SYSTEM VARIABLES
Servo doorServo;
String inputCode = "";
String correctCode = "1234";

bool doorMode = false;
bool irTimerMode = false;

unsigned long irStart = 0;
unsigned long nextEvent = 30000; // 30s

int wrongCount = 0;

unsigned long noObjectStart = 0;
const unsigned long CLEAR_MS = 3000;

// IR logic (active low)
bool irActiveLow = true;

// WIFI CONNECT
void wifiConnect() {
  WiFi.begin(ssid, password);
  Serial.print("[WIFI] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\n[WIFI] Connected");
  Serial.print("[WIFI] IP: ");
  Serial.println(WiFi.localIP());
}

// HTTP CLIENT — TRIGGER CAMERA
void triggerCamera(String reason) {
  if (WiFi.status() != WL_CONNECTED) wifiConnect();

  String url = String(CAM_IP) + "/capture?reason=" + reason;
  Serial.print("[HTTP] Request: ");
  Serial.println(url);

  HTTPClient http;
  http.begin(url);

  int code = http.GET();
  if (code == 200) {
    Serial.println("[HTTP] Camera responded OK");

    WiFiClient* stream = http.getStreamPtr();
    int len = http.getSize();

    while (http.connected() && (len > 0 || len == -1)) {
      uint8_t buf[128];
      int c = stream->readBytes(buf, sizeof(buf));
      if (c > 0) len -= c;
    }
  }
  else {
    Serial.printf("[HTTP] FAILED code=%d\n", code);
  }

  http.end();
}

// SEND DATA TO THINGSPEAK
void sendToThingSpeak(int ir_val, int door_val, int wrong_val, int event_code) {

  if (millis() - lastTS < TS_INTERVAL) return;

  if (WiFi.status() != WL_CONNECTED) wifiConnect();

  String url = TS_URL;
  url += "?api_key=" + String(TS_API_KEY);
  url += "&field1=" + String(ir_val);
  url += "&field2=" + String(door_val);
  url += "&field3=" + String(wrong_val);
  url += "&field4=" + String(event_code);

  HTTPClient http;
  http.begin(url);
  int code = http.GET();

  Serial.print("[TS] HTTP code: ");
  Serial.println(code);

  http.end();
  lastTS = millis();
}

// MISC
bool irDetected() {
  int raw = digitalRead(PIR_PIN);
  return irActiveLow ? (raw == LOW) : (raw == HIGH);
}

void buzzerOn()  { digitalWrite(BUZZER_PIN, HIGH); }
void buzzerOff() { digitalWrite(BUZZER_PIN, LOW); }

void buzzPattern(int times, int onMs = 150, int offMs = 150) {
  for (int i = 0; i < times; i++) {
    buzzerOn();
    delay(onMs);
    buzzerOff();
    if (i < times - 1) delay(offMs);
  }
}

// SETUP
void setup() {
  Serial.begin(9600);

  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();

  pinMode(colPins[0], INPUT_PULLUP);
  pinMode(colPins[1], INPUT_PULLUP);
  pinMode(colPins[2], INPUT_PULLUP);
  pinMode(colPins[3], INPUT_PULLUP);

  doorServo.attach(SERVO_PIN);
  doorServo.write(0);

  wifiConnect();

  Serial.println("=== SMART DOOR LOCK READY ===");
}

// LOOP
void loop() {
  unsigned long now = millis();
  // MODE 1 — IR TIMER MODE
  if (!doorMode) {

    if (irDetected()) {

      if (!irTimerMode) {
        irTimerMode = true;
        irStart = now;
        nextEvent = 30000;
        Serial.println("[IR] Detected -> Timer ON");
      }

      unsigned long elapsed = now - irStart;

      if (elapsed >= nextEvent) {
        Serial.print("[IR] Standing ");
        Serial.print(nextEvent / 1000);
        Serial.println(" s -> EVENT");

        triggerCamera("standing");

        sendToThingSpeak(
          irDetected() ? 1 : 0,
          doorMode ? 1 : 0,
          wrongCount,
          1 // event standing
        );

        nextEvent += 30000;
      }
    }

    else {
      if (irTimerMode) {
        Serial.println("[IR] Object gone -> Timer RESET");
      }
      irTimerMode = false;
      irStart = 0;
      nextEvent = 30000;
    }
  }

  // KEYPAD PROCESSING
  char key = keypad.getKey();
  if (key) {

    if (irTimerMode) {
      irTimerMode = false;
      irStart = 0;
      nextEvent = 30000;
    }

    Serial.print("[KEY] Pressed: ");
    Serial.println(key);

    if (key == '#') {

      if (inputCode == correctCode) {

        Serial.println("[AUTH] Correct password");
        wrongCount = 0;

        buzzerOn();
        delay(500);
        buzzerOff();
        delay(100);
        buzzerOn();
        delay(500);
        buzzerOff();
        delay(100);

        triggerCamera("authorized");

        sendToThingSpeak(
          irDetected() ? 1 : 0,
          1,               // door open
          wrongCount,
          2                // event authorized
        );

        doorMode = true;
        noObjectStart = 0;
        doorServo.write(90);
      }

      else {
        Serial.println("[AUTH] Wrong password");
        wrongCount++;
        buzzPattern(2);

        if (wrongCount >= 3) {
          buzzerOn(); delay(5000);
          buzzerOff(); delay(200);
          Serial.println("[ALARM] x3 wrong");

          triggerCamera("alarm");

          // unsigned long alarmStart = millis();
          // while (millis() - alarmStart < 10000) {
          //   buzzerOn(); delay(200);
          //   buzzerOff(); delay(200);
          // }
          // OPTIONAL FOR BUZZER AFTER CORRECT PIN

          sendToThingSpeak(
            irDetected() ? 1 : 0,
            doorMode ? 1 : 0,
            wrongCount,
            3     // alarm
          );

          wrongCount = 0;
        }
      }

      inputCode = "";
    }

    else if (key == '*') {
      inputCode = "";
      Serial.println("[KEY] Clear");
    }

    else {
      inputCode += key;
      Serial.print("[KEY] Input: ");
      Serial.println(inputCode);
    }
  }

  // MODE 2 — DOOR OPEN MODE
  if (doorMode) {

    if (irDetected()) {
      noObjectStart = 0;
    }
    else {
      if (noObjectStart == 0) noObjectStart = now;

      unsigned long elapsed = now - noObjectStart;
      if (elapsed >= CLEAR_MS) {
        doorServo.write(0);
        doorMode = false;

        irTimerMode = false;
        irStart = 0;
        nextEvent = 30000;

        sendToThingSpeak(
          irDetected() ? 1 : 0,
          0,   // door closed
          wrongCount,
          4    // heartbeat / closure event
        );
      }
    }
  }

  // PERIODIC HEARTBEAT
  if (millis() - lastTS > 60000) {
    sendToThingSpeak(
      irDetected() ? 1 : 0,
      doorMode ? 1 : 0,
      wrongCount,
      4  // heartbeat
    );
  }

  delay(20);
}