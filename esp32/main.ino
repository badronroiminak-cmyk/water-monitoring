// =====================================================
// KODE FINAL - OPTIMASI UNTUK GRAFIK BLYNK
// MONITORING PESISIR + BLYNK + WIFIMANAGER
// =====================================================

#define BLYNK_TEMPLATE_ID "TMPL5f7ipGN7a"
#define BLYNK_TEMPLATE_NAME "Monitoring Air Laut"
#define BLYNK_AUTH_TOKEN "soYvnfBg4ZZMvK52VmuE2-98cx1CaXaa"

// Aktifkan debug Blynk
#define BLYNK_PRINT Serial
#define BLYNK_DEBUG

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>

// =====================================================
// KONFIGURASI PIN
// =====================================================
#define SDA_PIN 21
#define SCL_PIN 22
#define TRIG_PIN 13
#define ECHO_PIN 12
#define ANEMO_PIN 4
#define BUZZER_PIN 5

// =====================================================
// KONFIGURASI BLYNK VIRTUAL PINS
// =====================================================
#define V0 0  // Ketinggian Air (Double - cm)
#define V1 1  // Kecepatan Angin (Double - km/h)
#define V2 2  // Status Air (String)
#define V3 3  // Status Angin (String)
#define V4 4  // Status Sistem (String)

// =====================================================
// KONSTANTA
// =====================================================
const float AIR_BAHAYA = 10.0;
const float AIR_WASPADA = 20.0;
const float ANGIN_WASPADA = 10.0;
const float ANGIN_BAHAYA = 20.0;

// =====================================================
// INISIALISASI
// =====================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
BlynkTimer timer;
WiFiManager wifiManager;

// =====================================================
// VARIABEL
// =====================================================
float waterDistance = 0;
float windSpeed = 0;
String waterStatus = "AMAN";
String windStatus = "AMAN";
String overallStatus = "AMAN";

volatile unsigned int windCounter = 0;
unsigned long lastWindTime = 0;
const float WIND_FACTOR = 2.4;

bool blynkConnected = false;
bool wifiConnected = false;

// Counter untuk debugging
int sendCount = 0;

// =====================================================
// INTERRUPT
// =====================================================
void IRAM_ATTR windISR() {
  windCounter++;
}

// =====================================================
// BACA SENSOR
// =====================================================
float readWaterDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  if (duration == 0) {
    return -1;
  }
  
  float distance = duration * 0.034 / 2;
  return distance;
}

float readWindSpeed() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastWindTime >= 2000) {
    float rotations = windCounter;
    windCounter = 0;
    lastWindTime = currentTime;
    
    float speed = (rotations / 2.0) * WIND_FACTOR;
    return speed;
  }
  
  return -1;
}

String determineWaterStatus(float distance) {
  if (distance < 0) return "ERROR";
  if (distance <= AIR_BAHAYA) return "BAHAYA";
  if (distance <= AIR_WASPADA) return "WASPADA";
  return "AMAN";
}

String determineWindStatus(float speed) {
  if (speed < 0) return "ERROR";
  if (speed >= ANGIN_BAHAYA) return "BAHAYA";
  if (speed >= ANGIN_WASPADA) return "WASPADA";
  return "AMAN";
}

// =====================================================
// KIRIM DATA KE BLYNK - VERSI OPTIMAL
// =====================================================
void sendToBlynk() {
  sendCount++;
  
  // Cek koneksi Blynk
  if (!Blynk.connected()) {
    Serial.println("⚠️ BLYNK TIDAK TERHUBUNG! Mencoba reconnect...");
    if (Blynk.connect(5000)) {
      Serial.println("✅ Blynk berhasil reconnect!");
    } else {
      Serial.println("❌ Blynk gagal reconnect!");
      return;
    }
  }
  
  // --- BACA SENSOR ---
  waterDistance = readWaterDistance();
  waterStatus = determineWaterStatus(waterDistance);
  
  windSpeed = readWindSpeed();
  if (windSpeed >= 0) {
    windStatus = determineWindStatus(windSpeed);
  }
  
  // --- KIRIM KE BLYNK ---
  Serial.println("========================================");
  Serial.print("📤 Pengiriman ke-#");
  Serial.println(sendCount);
  
  // KIRIM V0 - Ketinggian Air (HANYA jika valid)
  if (waterDistance >= 0) {
    Blynk.virtualWrite(V0, waterDistance);
    Serial.print("✅ V0 (Ketinggian Air): ");
    Serial.print(waterDistance);
    Serial.println(" cm");
  } else {
    Serial.println("❌ V0 SKIP - Data air invalid (-1)");
  }
  
  // KIRIM V1 - Kecepatan Angin (HANYA jika valid)
  if (windSpeed >= 0) {
    Blynk.virtualWrite(V1, windSpeed);
    Serial.print("✅ V1 (Kecepatan Angin): ");
    Serial.print(windSpeed);
    Serial.println(" km/h");
  } else {
    Serial.println("❌ V1 SKIP - Data angin invalid (-1)");
  }
  
  // KIRIM V2 - Status Air (String - SELALU dikirim)
  Blynk.virtualWrite(V2, waterStatus);
  Serial.print("✅ V2 (Status Air): ");
  Serial.println(waterStatus);
  
  // KIRIM V3 - Status Angin (String - SELALU dikirim)
  Blynk.virtualWrite(V3, windStatus);
  Serial.print("✅ V3 (Status Angin): ");
  Serial.println(windStatus);
  
  // KIRIM V4 - Status Sistem
  if (waterStatus == "BAHAYA" || windStatus == "BAHAYA") {
    overallStatus = "BAHAYA";
  } else if (waterStatus == "WASPADA" || windStatus == "WASPADA") {
    overallStatus = "WASPADA";
  } else {
    overallStatus = "AMAN";
  }
  Blynk.virtualWrite(V4, overallStatus);
  Serial.print("✅ V4 (Status Sistem): ");
  Serial.println(overallStatus);
  
  Serial.println("========================================");
  
  // Update LCD
  updateLCD(waterDistance, windSpeed, waterStatus, windStatus, overallStatus);
  
  // Buzzer
  if (overallStatus == "BAHAYA") {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  } else if (overallStatus == "WASPADA") {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(50);
    digitalWrite(BUZZER_PIN, LOW);
    delay(450);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// =====================================================
// UPDATE LCD
// =====================================================
void updateLCD(float distance, float speed, String wStatus, String wWind, String overall) {
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print("J:");
  if (distance >= 0) {
    lcd.print(distance, 1);
  } else {
    lcd.print("ERR");
  }
  lcd.print("cm K:");
  if (speed >= 0) {
    lcd.print(speed, 1);
  } else {
    lcd.print("---");
  }
  lcd.print("km/h");
  
  lcd.setCursor(0, 1);
  lcd.print("A:");
  lcd.print(wStatus.substring(0, 3));
  lcd.print(" G:");
  lcd.print(wWind.substring(0, 3));
  lcd.print(" S:");
  lcd.print(overall.substring(0, 3));
  
  lcd.setCursor(15, 1);
  if (Blynk.connected()) {
    lcd.print("B");
  } else if (wifiConnected) {
    lcd.print("W");
  } else {
    lcd.print(" ");
  }
}

// =====================================================
// CALLBACK BLYNK UNTUK DEBUG
// =====================================================
BLYNK_WRITE(V0) {
  double value = param.asDouble();
  Serial.print("🔔 CALLBACK V0 (Air): ");
  Serial.println(value);
}

BLYNK_WRITE(V1) {
  double value = param.asDouble();
  Serial.print("🔔 CALLBACK V1 (Angin): ");
  Serial.println(value);
}

BLYNK_WRITE(V2) {
  String value = param.asStr();
  Serial.print("🔔 CALLBACK V2 (Status Air): ");
  Serial.println(value);
}

BLYNK_WRITE(V3) {
  String value = param.asStr();
  Serial.print("🔔 CALLBACK V3 (Status Angin): ");
  Serial.println(value);
}

BLYNK_WRITE(V4) {
  String value = param.asStr();
  Serial.print("🔔 CALLBACK V4 (Status Sistem): ");
  Serial.println(value);
}

// =====================================================
// KONEKSI WIFI
// =====================================================
void connectWiFi() {
  Serial.println("\n📶 WiFiManager - Setup WiFi");
  
  lcd.clear();
  lcd.print("WiFi Setup");
  lcd.setCursor(0, 1);
  lcd.print("AP: ESP32-Monitor");
  
  wifiManager.setConfigPortalTimeout(120);
  
  if (!wifiManager.autoConnect("ESP32-Monitoring", "12345678")) {
    Serial.println("❌ Gagal konek WiFi");
    ESP.restart();
  }
  
  wifiConnected = true;
  Serial.println("✅ WiFi TERHUBUNG!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  lcd.clear();
  lcd.print("WiFi OK!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
  delay(2000);
  
  connectBlynk();
}

// =====================================================
// KONEKSI BLYNK
// =====================================================
void connectBlynk() {
  Serial.println("🔗 Menghubungkan ke Blynk...");
  
  lcd.clear();
  lcd.print("Blynk Connect");
  
  Blynk.config(BLYNK_AUTH_TOKEN);
  
  if (Blynk.connect(10000)) {
    Serial.println("✅ BLYNK TERHUBUNG!");
    Serial.print("📊 Template: ");
    Serial.println(BLYNK_TEMPLATE_NAME);
    Serial.print("🔑 Auth: ");
    Serial.println(BLYNK_AUTH_TOKEN);
    
    blynkConnected = true;
    lcd.setCursor(0, 1);
    lcd.print("Connected!  ");
    delay(1000);
  } else {
    Serial.println("❌ BLYNK GAGAL TERHUBUNG!");
    blynkConnected = false;
    lcd.setCursor(0, 1);
    lcd.print("Blynk Failed");
    delay(2000);
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== 🌊 MONITORING PESISIR ===");
  Serial.println("=== OPTIMASI GRAFIK BLYNK ===");
  
  // I2C & LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Monitoring");
  lcd.setCursor(0, 1);
  lcd.print("Air Laut v3.2");
  delay(2000);
  
  // Pin
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(ANEMO_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Interrupt Anemometer
  attachInterrupt(digitalPinToInterrupt(ANEMO_PIN), windISR, FALLING);
  
  // WiFi & Blynk
  connectWiFi();
  
  // TIMER: Kirim data setiap 2 detik
  timer.setInterval(2000L, sendToBlynk);
  
  // TIMER: Cek koneksi Blynk setiap 10 detik
  timer.setInterval(10000L, []() {
    if (!Blynk.connected() && wifiConnected) {
      Serial.println("⚠️ Blynk reconnect...");
      Blynk.connect(5000);
    }
  });
  
  Serial.println("✅ Sistem siap!");
  lcd.clear();
  lcd.print("Sistem Siap");
  lcd.setCursor(0, 1);
  lcd.print("PIN4: Anemometer");
  delay(2000);
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  Blynk.run();
  timer.run();
}
