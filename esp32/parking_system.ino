// ParkSmart ESP32 - Parking System
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "put your not mine";
const char* password = "put your ssid's pass";

WebServer server(80);
Servo gate;

// Gate sensors
const int entrySensor = 34;
const int exitSensor  = 35;

// Slot sensors
const int slotPins[4] = {32, 33, 25, 26};

// Slot LEDs
const int ledPins[4] = {19, 18, 17, 16};

// Booking state
bool slotBooked[4] = {false, false, false, false};
int  slotPassword[4] = {0, 0, 0, 0};

// Physical tracking
bool lastPhysicalState[4] = {false, false, false, false};
unsigned long slotParkStartMs[4] = {0, 0, 0, 0};

int pos = 90;
bool gateOpen = false;

// Gate close timing
unsigned long closeWaitTime = 0;
bool waitingToClose = false;

// Trigger protection
bool lastEntryState = false;
bool lastExitState = false;
unsigned long lastGateTriggerTime = 0;
const unsigned long gateCooldown = 1000;

// Demo rate
const float PARKING_RATE_PER_SEC = 0.001f;

// sms sending
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

void sendSMS(String number, String message) {

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  HTTPClient http;

  if (!http.begin(client, "https://api.fast2sms.com/dev/bulkV2")) {
    Serial.println("HTTP begin failed");
    return;
  }

  http.addHeader("authorization", "YOUR_API_KEY");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  message.replace(" ", "%20");

  String data = "route=q&message=" + message +
                "&language=english&numbers=" + number +
                "&flash=0";

  int response = http.POST(data);

  Serial.print("SMS Response: ");
  Serial.println(response);

  if (response == -1) {
    Serial.println("Connection failed");
  }

  if (response > 0) {
    Serial.println(http.getString());
  }

  http.end();
}

// ---------------------------------------------------------
// HELPERS
// ---------------------------------------------------------
String format4Digit(int n) {
  char buf[5];
  snprintf(buf, sizeof(buf), "%04d", n);
  return String(buf);
}

int countFreeSlots() {
  int freeSlots = 0;
  for (int i = 0; i < 4; i++) {
    bool physical = (digitalRead(slotPins[i]) == LOW);
    bool booked = slotBooked[i];
    if (!physical && !booked) freeSlots++;
  }
  return freeSlots;
}

bool anyActiveBooking() {
  for (int i = 0; i < 4; i++) {
    if (slotBooked[i]) return true;
  }
  return false;
}

bool passwordExists(int pass) {
  for (int i = 0; i < 4; i++) {
    if (slotBooked[i] && slotPassword[i] == pass) return true;
  }
  return false;
}

int generateUniquePassword() {
  int pass = 0;
  do {
    pass = random(1000, 10000);
  } while (passwordExists(pass));
  return pass;
}

void sendCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
}

void openGate() {
  for (; pos <= 180; pos++) {
    gate.write(pos);
    delay(15);
  }
  gateOpen = true;
}

void closeGate() {
  for (; pos >= 90; pos--) {
    if (digitalRead(entrySensor) == LOW || digitalRead(exitSensor) == LOW) {
      openGate();
      return;
    }
    gate.write(pos);
    delay(15);
  }
  gateOpen = false;
}

void refreshSlotStates() {
  for (int i = 0; i < 4; i++) {
    bool currentPhysical = (digitalRead(slotPins[i]) == LOW);

    // Vehicle just arrived in the slot
    if (currentPhysical && !lastPhysicalState[i]) {
      slotParkStartMs[i] = millis();
    }

    // Vehicle just left the slot -> booking expires immediately
    if (!currentPhysical && lastPhysicalState[i]) {
      slotBooked[i] = false;
      slotPassword[i] = 0;
      slotParkStartMs[i] = 0;
      Serial.println("Slot " + String(i + 1) + " auto-freed");
    }

    lastPhysicalState[i] = currentPhysical;

    bool slotOn = (currentPhysical || slotBooked[i]);
    digitalWrite(ledPins[i], slotOn ? HIGH : LOW);
  }
}

void appendSlotJson(String &json, int i) {
  bool physical = (digitalRead(slotPins[i]) == LOW);
  bool booked = slotBooked[i];

  unsigned long parkedSec = 0;
  float rent = 0.0f;

  if (physical && slotParkStartMs[i] > 0) {
    parkedSec = (millis() - slotParkStartMs[i]) / 1000;
    rent = parkedSec * PARKING_RATE_PER_SEC;
  }

  json += "\"slot" + String(i + 1) + "\":{";
  json += "\"occupied\":" + String(physical ? 1 : 0) + ",";
  json += "\"booked\":" + String(booked ? 1 : 0) + ",";
  json += "\"password\":\"" + (booked ? format4Digit(slotPassword[i]) : "") + "\",";
  json += "\"parkedSec\":" + String(parkedSec) + ",";
  json += "\"rent\":" + String(rent, 3);
  json += "}";

  if (i < 3) json += ",";
}

bool stableLow(int pin, unsigned long stableTime = 80) {
  if (digitalRead(pin) != LOW) return false;

  unsigned long start = millis();
  while (millis() - start < stableTime) {
    if (digitalRead(pin) != LOW) return false;
    delay(2);
  }
  return true;
}

// ---------------------------------------------------------
// WEB HANDLERS
// ---------------------------------------------------------
void handleStatus() {
  sendCORS();
  refreshSlotStates();

  String json = "{";
  json += "\"freeSlots\":" + String(countFreeSlots()) + ",";
  json += "\"hasBooking\":" + String(anyActiveBooking() ? 1 : 0) + ",";
  json += "\"slots\":{";

  for (int i = 0; i < 4; i++) {
    bool physical = (digitalRead(slotPins[i]) == LOW);
    bool booked = slotBooked[i];

    unsigned long parkedSec = 0;
    float rent = 0.0f;
    if (physical && slotParkStartMs[i] > 0) {
      parkedSec = (millis() - slotParkStartMs[i]) / 1000;
      rent = parkedSec * PARKING_RATE_PER_SEC;
    }

    json += "\"slot" + String(i + 1) + "\":{";
    json += "\"occupied\":" + String(physical ? 1 : 0) + ",";
    json += "\"booked\":" + String(booked ? 1 : 0) + ",";
    json += "\"password\":\"" + (booked ? format4Digit(slotPassword[i]) : "") + "\",";
    json += "\"parkedSec\":" + String(parkedSec) + ",";
    json += "\"rent\":" + String(rent, 3);
    json += "}";

    if (i < 3) json += ",";
  }

  json += "}}";
  server.send(200, "application/json", json);
}

void handleBook() {
  sendCORS();
  refreshSlotStates();

  if (!server.hasArg("slot")) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"Slot missing\"}");
    return;
  }

  int slot = server.arg("slot").toInt();
  if (slot < 1 || slot > 4) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"Invalid slot\"}");
    return;
  }

  int idx = slot - 1;

  bool physical = (digitalRead(slotPins[idx]) == LOW);
  if (physical || slotBooked[idx]) {
    server.send(409, "application/json", "{\"ok\":false,\"message\":\"Slot not available\"}");
    return;
  }

  slotBooked[idx] = true;
  slotPassword[idx] = generateUniquePassword();
  //sms send
  String msg = "Parking booked. Slot " + String(slot) +
             ". Password: " + format4Digit(slotPassword[idx]);

  sendSMS("your phone number not mine", msg);

  String json = "{";
  json += "\"ok\":true,";
  json += "\"slot\":" + String(slot) + ",";
  json += "\"password\":\"" + format4Digit(slotPassword[idx]) + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleFree() {
  sendCORS();
  refreshSlotStates();

  if (!server.hasArg("slot")) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"Slot missing\"}");
    return;
  }

  int slot = server.arg("slot").toInt();
  if (slot < 1 || slot > 4) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"Invalid slot\"}");
    return;
  }

  int idx = slot - 1;
  slotBooked[idx] = false;
  slotPassword[idx] = 0;
  slotParkStartMs[idx] = 0;

  String json = "{";
  json += "\"ok\":true,";
  json += "\"slot\":" + String(slot);
  json += "}";
  server.send(200, "application/json", json);
}

void handleEnter() {
  sendCORS();
  refreshSlotStates();

  if (!server.hasArg("password")) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"Password required\"}");
    return;
  }

  String passStr = server.arg("password");
  int pass = passStr.toInt();

  if (pass < 1000 || pass > 9999) {
    server.send(401, "application/json", "{\"ok\":false,\"message\":\"Invalid password\"}");
    return;
  }

  for (int i = 0; i < 4; i++) {
    if (slotBooked[i] && slotPassword[i] == pass) {
      openGate();

      String json = "{";
      json += "\"ok\":true,";
      json += "\"slot\":" + String(i + 1) + ",";
      json += "\"message\":\"Access granted\"";
      json += "}";
      server.send(200, "application/json", json);
      return;
    }
  }

  server.send(403, "application/json", "{\"ok\":false,\"message\":\"Wrong password or no booking\"}");
}

void handleOptions() {
  sendCORS();
  server.send(204);
}

// ---------------------------------------------------------
// SETUP
// ---------------------------------------------------------
void setup() {
  Serial.begin(115200);
  randomSeed(micros());

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("ESP IP Address: ");
  Serial.println(WiFi.localIP());

  gate.attach(23);

  pinMode(entrySensor, INPUT);
  pinMode(exitSensor, INPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(slotPins[i], INPUT);
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  gate.write(pos);

  server.on("/status", HTTP_GET, handleStatus);
  server.on("/book", HTTP_GET, handleBook);
  server.on("/free", HTTP_GET, handleFree);
  server.on("/enter", HTTP_GET, handleEnter);
  server.onNotFound(handleOptions);

  server.begin();
  Serial.println("Web Server Started!");
}

// ---------------------------------------------------------
// LOOP
// ---------------------------------------------------------
void loop() {
  server.handleClient();

  refreshSlotStates();

  int freeSlots = countFreeSlots();

  bool entryNow = stableLow(entrySensor);
  bool exitNow  = stableLow(exitSensor);

  // Normal entry only when free slots exist
  if (entryNow && !lastEntryState && !gateOpen &&
      millis() - lastGateTriggerTime > gateCooldown) {
    if (freeSlots > 0) {
      openGate();
      waitingToClose = false;
      lastGateTriggerTime = millis();
    }
  }

  // Exit is always allowed
  if (exitNow && !lastExitState && !gateOpen &&
      millis() - lastGateTriggerTime > gateCooldown) {
    openGate();
    waitingToClose = false;
    lastGateTriggerTime = millis();
  }

  lastEntryState = entryNow;
  lastExitState = exitNow;

  // Close after car clears
  if (gateOpen && !entryNow && !exitNow) {
    if (!waitingToClose) {
      waitingToClose = true;
      closeWaitTime = millis();
    } else if (millis() - closeWaitTime > 1500) {
      if (!entryNow && !exitNow) {
        closeGate();
      }
      waitingToClose = false;
    }
  } else {
    waitingToClose = false;
  }

  delay(10);
}
