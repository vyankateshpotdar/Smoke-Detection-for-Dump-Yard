#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

// ---------------- PINS ----------------
#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ2_PIN A0
#define BUZZER_PIN 15

// ---------------- WIFI ----------------
const char* ssid = "";
const char* password = "";

// ---------------- TELEGRAM ----------------
const char* botToken = "";
const char* chatID   = "";

// ---------------- SENSOR ----------------
DHT dht(DHTPIN, DHTTYPE);

int gasValue = 0;
int prevGas = 0;

int airThreshold = 450;
int spikeThreshold = 50;

unsigned long lastTime = 0;
unsigned long lastAlertTime = 0;
int alertCooldown = 30000;

long lastUpdateID = 0;

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();

  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi Connected");

  Serial.println("Warming MQ2...");
  delay(20000);

  Serial.println("✅ System Ready");
}

// ---------------- LOOP ----------------
void loop() {

  checkTelegramCommands(); // 🔥 NEW

  if (millis() - lastTime > 5000) {

    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    gasValue = analogRead(MQ2_PIN);

    Serial.println("\n--- NEW READING ---");
    Serial.println("Temp: " + String(temp));
    Serial.println("Humidity: " + String(hum));
    Serial.println("Gas: " + String(gasValue));

    int diff = gasValue - prevGas;

    if ((gasValue > airThreshold || diff > spikeThreshold) &&
        (millis() - lastAlertTime > alertCooldown)) {

      lastAlertTime = millis();

      Serial.println("🔥🔥 SMOKE DETECTED 🔥🔥");
      digitalWrite(BUZZER_PIN, HIGH);

      sendTelegram(temp, hum, gasValue);

    } else {
      Serial.println("✅ Air Normal");
      digitalWrite(BUZZER_PIN, LOW);
    }

    prevGas = gasValue;
    lastTime = millis();
  }
}

// ---------------- SEND ALERT ----------------
void sendTelegram(float temp, float hum, int gas) {

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage";

  String keyboard = "{\"inline_keyboard\":["
                    "[{\"text\":\"📊 Status\",\"callback_data\":\"status\"}],"
                    "[{\"text\":\"🔕 Silence\",\"callback_data\":\"mute\"}],"
                    "[{\"text\":\"🔄 Reset\",\"callback_data\":\"reset\"}]"
                    "]}";

  String payload = "chat_id=" + String(chatID) +
                   "&text=🔥 FIRE ALERT!\nTemp: " + String(temp) +
                   "\nHumidity: " + String(hum) +
                   "\nGas: " + String(gas) +
                   "&reply_markup=" + keyboard;

  http.begin(client, url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpCode = http.POST(payload);

  Serial.print("HTTP Code: ");
  Serial.println(httpCode);

  http.end();
}

// ---------------- CHECK BUTTONS ----------------
void checkTelegramCommands() {

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = "https://api.telegram.org/bot" + String(botToken) +
               "/getUpdates?offset=" + String(lastUpdateID + 1);

  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String response = http.getString();

    // 🔥 Extract update_id (basic parsing)
    int idIndex = response.lastIndexOf("update_id");
    if (idIndex != -1) {
      int start = response.indexOf(":", idIndex) + 1;
      int end = response.indexOf(",", start);
      lastUpdateID = response.substring(start, end).toInt();
    }

    // 🔥 BUTTON ACTIONS
    if (response.indexOf("status") != -1) {
      Serial.println("📊 Status Pressed");
      sendStatus();
    }

    if (response.indexOf("mute") != -1) {
      Serial.println("🔕 Mute Pressed");
      digitalWrite(BUZZER_PIN, LOW);
    }

    if (response.indexOf("reset") != -1) {
      Serial.println("🔄 Reset Pressed");
      lastAlertTime = 0;
    }
  }

  http.end();
}

// ---------------- STATUS ----------------
void sendStatus() {

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = "https://api.telegram.org/bot" + String(botToken) +
               "/sendMessage?chat_id=" + String(chatID) +
               "&text=📊 System Running Normally";

  http.begin(client, url);
  http.GET();
  http.end();
}
