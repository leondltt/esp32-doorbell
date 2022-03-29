#include <WiFiClientSecure.h>
#include <RCSwitch.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "vars.h"

#define LED 2

const char *ssid = "VIVOFIBRA-CA11";
const char *password = "xvxct22763";
const char *server = "maker.ifttt.com";
const char *url = "trigger/front_doorbell_pressed/json/with/key/egbFvH-fNRvQiAzis4zBkISLDWCYc-RHUrrYVYwoH3e";

uint64_t lastAction;

WiFiClientSecure client;
HTTPClient http;
RCSwitch mySwitch = RCSwitch();
WebServer httpserver(80);

void setupOtaServer() {
  httpserver.on("/serverIndex", HTTP_GET, []() {
    httpserver.sendHeader("Connection", "close");
    httpserver.send(200, "text/html", serverIndex);
  });
  httpserver.on("/update", HTTP_POST, []() {
    httpserver.sendHeader("Connection", "close");
    httpserver.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = httpserver.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  httpserver.begin();
}

bool checkConnection() {
  int status = WiFi.status();
  return status == WL_CONNECTED;
}

void sendGetRequest() {
  client.setInsecure();

  if (!client.connect(server, 443)) {
    Serial.println("Connection failed!");
  } else {
    Serial.println("Connected to server!");
    char myRequest[255];
    sprintf(myRequest, "GET https://%s/%s HTTP/1.0", server, url);
    client.println(myRequest);
    client.print("Host: ");
    client.println(server);
    client.println("Connection: close");
    client.println();

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        // Serial.println("headers received");
        break;
      }
    }

    while (client.available()) {
      char c = client.read();
      Serial.write(c);
    }

    client.stop();
  }
}

void sendEvent() {
  sendGetRequest();
}

void connectWifi() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED, HIGH);
}

void debugRf() {
  Serial.print("Received ");
  Serial.print(mySwitch.getReceivedValue());
  Serial.print(" / ");
  Serial.print(mySwitch.getReceivedBitlength());
  Serial.print("bit ");
  Serial.print("Protocol: ");
  Serial.println(mySwitch.getReceivedProtocol());
}

void debounce(void (*callback)(void)) {
  uint64_t currentTime = esp_timer_get_time();
  if (currentTime > lastAction + 5000 * 1000) {
    lastAction = currentTime;
    callback();
  }
}

void listenRf() {
  if (mySwitch.available()) {
    int received = mySwitch.getReceivedValue();

    if (received = 6953864) {
      debounce(dorbellAction);
    }

    //debugRf();
    mySwitch.resetAvailable();
  }
}
void dorbellAction() {
  Serial.println("Dorbell action.");
  digitalWrite(LED, LOW);
  sendEvent();
  digitalWrite(LED, HIGH);
}

void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  mySwitch.enableReceive(5);
  connectWifi();
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  setupOtaServer();
}

void loop() {
  listenRf();
  httpserver.handleClient();
  if (!checkConnection())
    ESP.restart();
}
