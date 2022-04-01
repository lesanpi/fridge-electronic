#include <Arduino.h>
#include <ESP8266WiFi.h> //import for wifi functionality
#include <WebSocketsServer.h> //import for websocket
#include <OneWire.h>
#include <DallasTemperature.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ArduinoJson.h>

// Electronic
#define ledpin D2 //defining the OUTPUT pin for LED
#define dataDQ D5 // temperature

// Wifi Access Point
const char *ssid =  "ZonaElectronica";   //Wifi SSID (Name)   
const char *pass =  "12345678"; //wifi password

//Data 
String devices_connected[254];
int fridgesConnected = 0;
const size_t capacity = 2048;
DynamicJsonDocument doc(capacity);

// Server
WebSocketsServer webSocket = WebSocketsServer(81); //websocket init with port 81

// Instancia a las clases OneWire y DallasTemperature
OneWire oneWireObjeto(dataDQ);
DallasTemperature sensorDS18B20(&oneWireObjeto);

// Temporizador
unsigned long ultimaTemp = 0;
unsigned long tiempoTemp = 5000; // Cada 5 segundos
uint8_t lastNum = 0;

String jsonToString(DynamicJsonDocument doc){
  String buf;
  serializeJson(doc, buf);
  return buf;
}


void sendConnected(uint8_t num){
  DynamicJsonDocument response(1024);
  String responseText;
  response["action"] = "connected";
  serializeJson(response, responseText);
  webSocket.sendTXT(num, responseText);
}

void setMaxTemperature(JsonObject json){
  DynamicJsonDocument response(1024);
  String responseText;
  int num = doc["devices"][String(json["payload"]["id"])]["num"];

  response["action"] = "setMaxTemperature";
  response["payload"]["temperature"] = json["payload"]["temperature"];
  serializeJson(response, responseText);
  webSocket.sendTXT(num, responseText);
}


String getStringMessage(uint8_t * payload, size_t length){
  String message = "";
  for(int i = 0; i < length; i++) {
    message = message + (char) payload[i]; 
  }
  return message;
}


void onMessage(JsonObject json, uint8_t num){
        String action = json[String("action")];
      
        if(action.equals("sendState")){
           Serial.println("Recibir y almacenar estado de la nevera");
           doc["devices"][String(json["payload"]["id"])]["id"] = json["payload"]["id"];
           doc["devices"][String(json["payload"]["id"])]["temperature"] = json["payload"]["temperature"];
           doc["devices"][String(json["payload"]["id"])]["light"] = json["payload"]["light"];
           doc["devices"][String(json["payload"]["id"])]["compressor"] = json["payload"]["compressor"];
           doc["devices"][String(json["payload"]["id"])]["status"] = json["payload"]["status"];
           doc["devices"][String(json["payload"]["id"])]["temperature_max"] = json["payload"]["temperature_max"];
           doc["devices"][String(json["payload"]["id"])]["temperature_min"] = json["payload"]["temperature_min"];
           doc["devices"][String(json["payload"]["id"])]["ip"] = webSocket.remoteIP(num).toString();
           doc["devices"][String(json["payload"]["id"])]["num"] = num;

           
        }
        if(action.equals("confirmConnection")){
          Serial.println("La conexion de la nevera fue verificada y se actualiza los datos");
        }
        if(action.equals("error")){
          Serial.println("Se recibio un error de la nevera");
        }
        if(action.equals("setTemperature")){
          Serial.println("Hay que indicarle a la nevera seleccionada la temperatura que se indico");
        }
        if(action.equals("setTemperatureForAll")){
          Serial.println("Indicarle a todas las neveras que deben ponerse a la temperatura indicada");
        }
        if(action.equals("toggleLight")){
          Serial.println("Indicarle a la nevera seleccionada que prenda la luz");
        }
        if(action.equals("setMaxTemperature")){
          Serial.println("Indicarle a la nevera seleccionada que cambie su nivel maximo de temperature");
          setMaxTemperature(json);
        }
        if(action.equals("setMinTemperature")){
          Serial.println("Indicarle a la nevera seleccionada que cambie su nivel minimo de temperature");
        }
        if(action.equals("setMaxTemperatureForAll")){
          Serial.println("Indicarle a todas las neveras que cambien su nivel maximo de temperature");
        }
        if(action.equals("setMinTemperatureForAll")){
          Serial.println("Indicarle a todas las neveras que cambien su nivel minimo de temperature");
        }
        if(action.equals("delete")){
          Serial.println("Eliminar la nevera indicada");
        }
        if(action.equals("deleteAll")){
          Serial.println("Eliminar todas las neveras");
        }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    String cmd;
    DynamicJsonDocument docInput(1024);
    JsonObject objReceived;
    
    if(type == WStype_TEXT){
      cmd = getStringMessage(payload, length);
      deserializeJson(docInput, cmd);
      objReceived = docInput.as<JsonObject>();
      String action = objReceived[String("action")];
      onMessage(objReceived, num);
      
    }
    
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("Websocket is disconnected");
            //case when Websocket is disconnected
            break;
        case WStype_CONNECTED:
            //wcase when websocket is connected
            Serial.println("Websocket is connected");
            Serial.println(webSocket.remoteIP(num).toString());
            sendConnected(num);
            break;
        case WStype_TEXT:
            cmd = "";
            for(int i = 0; i < length; i++) {
                cmd = cmd + (char) payload[i]; 
            } //merging payload to single string

            if(cmd == "poweron"){ //when command from app is "poweron"
                digitalWrite(ledpin, HIGH);
                webSocket.sendTXT(num, "poweron:success");
                //make ledpin output to HIGH  
            }else if(cmd == "poweroff"){
                digitalWrite(ledpin, LOW);
                webSocket.sendTXT(num, "poweroff:success");
                //make ledpin output to LOW on 'pweroff' command.
            }
            
             
//             doc["devices"][num]["temp"] = objReceived[String("temp")];
//             doc["devices"][num]["id"] = objReceived[String("id")]; 
//            
             //webSocket.sendTXT(num, ":success");

             //send response to mobile, if command is "poweron" then response will be "poweron:success"
             //this response can be used to track down the success of command in mobile app.
            break;
        case WStype_FRAGMENT_TEXT_START:
            break;
        case WStype_FRAGMENT_BIN_START:
            break;
        case WStype_BIN:
            hexdump(payload, length);
            break;
        default:
            break;
    }
}

void setup() {
   // Init Debug Tools
   pinMode(ledpin, OUTPUT); //set ledpin (D2) as OUTPUT pin
   Serial.begin(9600); //serial start

   // Init WiFi
   Serial.println("Connecting to wifi");
   IPAddress apIP(192, 168, 0, 1);   //Static IP for wifi gateway
   WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); //set Static IP gateway on NodeMCU
   WiFi.softAP(ssid, pass); //turn on WIFI

   // Init WebSocket
   webSocket.begin(); //websocket Begin
   webSocket.onEvent(webSocketEvent); //set Event for websocket
   Serial.println("Websocket is started");

    
   // Sensor 
   sensorDS18B20.begin();
   Serial.println("Sensor is started");

   // Inicio temporizador
   ultimaTemp = millis();
  
}

void loop() {
   webSocket.loop(); //keep this line on loop method

  // Protección overflow
  if (ultimaTemp > millis()) {
    ultimaTemp = millis();
  }

  // Comprobar si ha pasado el tiempo
  if (millis() - ultimaTemp > tiempoTemp) {
    // Marca de tiempo
    ultimaTemp = millis();
    
    // Mandamos comandos para toma de temperatura a los sensores
    sensorDS18B20.requestTemperatures();
    
    
    int temperature = int(sensorDS18B20.getTempCByIndex(0));
    // Leemos y mostramos los datos de los sensores DS18B20
//    Serial.print("Temperatura sensor: ");
//    Serial.print(temperature);
//    Serial.println(" C");

    String buf;
    serializeJson(doc, buf);
    Serial.println(buf);
    webSocket.broadcastTXT(buf);

   
    
  }
}
