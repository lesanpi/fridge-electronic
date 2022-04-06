#include <WebSocketClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

const char* ssid     = "ZonaElectronica";
const char* password = "12345678";
char path[] = "/";
char host[] = "192.168.0.1";
String id = "07";

// Websocket
WebSocketClient webSocketClient;
// Use WiFiClient class to create TCP connections
WiFiClient client;

String getStringMessage(uint8_t * payload, size_t length){
  String message = "";
  for(int i = 0; i < length; i++) {
    message = message + (char) payload[i]; 
  }
  return message;
}



void setup() {
  Serial.begin(115200);
  delay(10);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  delay(5000);

  if (client.connect(host, 81)) {
    Serial.println("Connected");
  } else {
    Serial.println("Connection failed.");
  }
  
  // Websocket connection
  webSocketClient.path = path;
  webSocketClient.host = host;
  if (webSocketClient.handshake(client)) {
    Serial.println("Handshake successful");
  } else {
    Serial.println("Handshake failed.");
  }

  

}


void loop() {
  

  // capture the value of analog 1, send it along
//  pinMode(1, INPUT);
  //    data = String(analogRead(1));

  const size_t capacity = 1024;
  DynamicJsonDocument doc(capacity);
  doc["action"] = "sendState";
  doc["payload"]["id"] = "fridge-test-casa-luis";
  doc["payload"]["temperature"] = 15;
  doc["payload"]["light"] = false;
  doc["payload"]["compressor"] = false;
  doc["payload"]["status"] = "disconnected";
  doc["payload"]["temperature_max"] = 20;
  doc["payload"]["temperature_min"] = -10;
  
  String buf;
  serializeJson(doc, buf);
  Serial.print("Data: ");
  Serial.println(buf);
  String data;
  if (client.connected()) {
 
    webSocketClient.sendData(buf);
 
    webSocketClient.getData(data);
    if (data.length() > 0) {
      Serial.print("Received data: ");
      Serial.println(data);
    }
 
  } else {
    Serial.println("Client disconnected.");
  }


  // wait to fully let the client disconnect
  delay(3000);

}
