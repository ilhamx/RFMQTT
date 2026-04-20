#include <WiFi.h>
#include <PubSubClient.h>

#define PIN_EN 1

unsigned long lastPublish = 0;
#define PUBLISH_INTERVAL 500  // ms (atur sesuai kebutuhan)

// ======================
// CONFIG
// ======================
const char *ssid = "LPG_3KG";
const char *password = "gasoragratis";
const char *mqtt_server = "10.10.10.1";

// UART RFID
HardwareSerial RFID(1);

// MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// ======================
// GLOBAL
// ======================
bool scanningEnabled = false;

// anti duplicate buffer
#define MAX_TAGS 20
String tagBuffer[MAX_TAGS];
unsigned long tagTime[MAX_TAGS];

// parser buffer
uint8_t buffer[128];
int indexBuf = 0;

enum State {
  WAIT_HEADER,
  READ_DATA
};
State state = WAIT_HEADER;

// ======================
// CRC FUNCTION
// ======================
uint8_t calcCRC(uint8_t *data, int len) {
  uint16_t sum = 0;
  for (int i = 0; i < len; i++) sum += data[i];
  return sum & 0xFF;
}

// ======================
// SEND COMMAND
// ======================
void sendCommand(uint8_t cmd, uint8_t *param, uint8_t paramLen) {
  uint8_t frame[64];
  int i = 0;

  frame[i++] = 0xBB;
  frame[i++] = 0x00;
  frame[i++] = cmd;

  frame[i++] = 0x00;
  frame[i++] = paramLen;

  for (int j = 0; j < paramLen; j++) {
    frame[i++] = param[j];
  }

  uint8_t crc = calcCRC(&frame[1], 4 + paramLen);
  frame[i++] = crc;

  frame[i++] = 0x7E;

  Serial.print("TX: ");
  for (int k = 0; k < i; k++) {
    Serial.printf("%02X ", frame[k]);
  }
  Serial.println();

  RFID.write(frame, i);
}

// ======================
// MULTIPLE INVENTORY
// ======================
void startInventory() {
  uint8_t param[3] = { 0x22, 0xFF, 0xFF };
  sendCommand(0x27, param, 3);
  Serial.println("START INVENTORY");
}

void stopInventory() {
  sendCommand(0x28, NULL, 0);
  Serial.println("STOP INVENTORY");
}

// ======================
// ANTI DUPLICATE
// ======================
bool isNewTag(String epc) {
  unsigned long now = millis();

  for (int i = 0; i < MAX_TAGS; i++) {
    if (tagBuffer[i] == epc) {
      if (now - tagTime[i] < 3000) return false;
      tagTime[i] = now;
      return true;
    }
  }

  for (int i = 0; i < MAX_TAGS; i++) {
    if (tagBuffer[i] == "") {
      tagBuffer[i] = epc;
      tagTime[i] = now;
      return true;
    }
  }

  tagBuffer[0] = epc;
  tagTime[0] = now;
  return true;
}

// ======================
// PROCESS FRAME
// ======================
void processFrame(uint8_t *frame, int len) {


  uint8_t type = frame[1];
  uint8_t cmd = frame[2];

  if (type != 0x02 || cmd != 0x22) return;

  uint16_t paramLen = (frame[3] << 8) | frame[4];

  if (paramLen < 5) return;  // safety

  int idx = 5;

  int8_t rssi = (int8_t)frame[idx++];

  uint8_t pc1 = frame[idx++];
  uint8_t pc2 = frame[idx++];

  int epcLen = paramLen - 5;

  // 🔥 SAFETY CHECK
  if (epcLen <= 0 || epcLen > 32) return;

  char epcStr[80] = { 0 };
  int epcIndex = 0;

  for (int i = 0; i < epcLen; i++) {
    sprintf(&epcStr[epcIndex], "%02X", frame[idx++]);
    epcIndex += 2;
  }

  String epc = String(epcStr);

  Serial.println("EPC DETECTED: " + epc);
  Serial.println("RSSI: " + String(rssi));

  // 🔥 TEST MQTT
  char json[128];
  sprintf(json, "{\"epc\":\"%s\",\"rssi\":%d}", epcStr, rssi);

  Serial.println("MQTT SEND: " + String(json));

    Serial.print("FRAME OK: ");
  for (int i = 0; i < len; i++) {
    Serial.printf("%02X ", frame[i]);
  }
  Serial.println();

  if (millis() - lastPublish < PUBLISH_INTERVAL) {
    return;  // 🔥 skip publish
  }

  lastPublish = millis();

  client.publish("rfid/data", json);
}

// ======================
// PARSER
// ======================
void parseRFID() {
  while (RFID.available()) {
    uint8_t b = RFID.read();

    switch (state) {

      case WAIT_HEADER:
        if (b == 0xBB) {
          indexBuf = 0;
          buffer[indexBuf++] = b;
          state = READ_DATA;
        }
        break;

      case READ_DATA:
        buffer[indexBuf++] = b;

        if (b == 0x7E) {
          processFrame(buffer, indexBuf);
          state = WAIT_HEADER;
        }

        if (indexBuf >= sizeof(buffer)) {
          state = WAIT_HEADER;
        }
        break;
    }
  }
}

// ======================
// MQTT CALLBACK
// ======================
void callback(char* topic, byte* payload, unsigned int length) {

  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  String topicStr = String(topic);

  Serial.println("MQTT [" + topicStr + "]: " + msg);

  // ======================
  // CONTROL START/STOP
  // ======================
  if (topicStr == "rfid/cmd") {
    if (msg == "start") {
      scanningEnabled = true;
      startInventory();
    } 
    else if (msg == "stop") {
      scanningEnabled = false;
      stopInventory();
    }
    else {
      Serial.println("Wrong Command.");
    }
  }

  // ======================
  // RAW FRAME SEND 🔥
  // ======================
  else if (topicStr == "rfid/send") {

    uint8_t buffer[64];
    int len = 0;

    if (hexStringToBytes(msg, buffer, len)) {
      sendRawFrame(buffer, len);
    } else {
      Serial.println("Invalid HEX format");
    }
  }
}

// ======================
// WIFI
// ======================
void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
}

// ======================
// TASK MQTT
// ======================
void TaskMQTT(void *pvParameters) {

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  for (;;) {

    if (!client.connected()) {
      while (!client.connect("ESP32Client")) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
      }
      client.subscribe("rfid/cmd");
      client.subscribe("rfid/send");
    }

    client.loop();

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void debugRawRFID() {
  while (RFID.available()) {
    uint8_t b = RFID.read();
    Serial.printf("%02X ", b);
  }
}

bool hexStringToBytes(String hex, uint8_t *out, int &len) {
  String clean = "";

  // 🔥 filter hanya karakter HEX
  for (int i = 0; i < hex.length(); i++) {
    char c = hex[i];

    if (isxdigit(c)) {
      clean += c;
    }
  }

  // panjang harus genap
  if (clean.length() % 2 != 0) {
    Serial.println("HEX length not even");
    return false;
  }

  len = clean.length() / 2;

  for (int i = 0; i < len; i++) {
    String byteString = clean.substring(i * 2, i * 2 + 2);
    out[i] = (uint8_t) strtol(byteString.c_str(), NULL, 16);
  }

  return true;
}

void sendRawFrame(uint8_t *data, int len) {
  Serial.print("SEND RAW: ");
  for (int i = 0; i < len; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();

  // kirim byte per byte (lebih stabil)
  for (int i = 0; i < len; i++) {
    RFID.write(data[i]);
    delay(5);
  }
}

// ======================
// TASK RFID
// ======================
void TaskRFID(void *pvParameters) {
  for (;;) {
    // debugRawRFID();
    parseRFID();

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ======================
// SETUP
// ======================
void setup() {
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, HIGH);
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");

  RFID.begin(115200, SERIAL_8N1, 3, 2);
  delay(500);
  // set region china2
  uint8_t param[1] = {0x01};
  sendCommand(0x07, param, 1);
  Serial.println("Set Region Freq to China2");
  delay(1000);
  startInventory();

  setup_wifi();

  xTaskCreatePinnedToCore(TaskRFID, "RFID", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(TaskMQTT, "MQTT", 4096, NULL, 1, NULL, 0);
}

void loop() {}
