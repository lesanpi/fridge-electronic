// #include <WebSocketClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "uMQTTBroker.h"
#include "StreamUtils.h"

char path[] = "/";
char host[] = "192.168.0.1";

/// State of the Fridge
String id = "nevera-07-test";
bool light = false; // Salida luz
bool compressor = false; // Salida compressor
bool door = false; // Sensor puerta abierta/cerrada
bool standalone = true;   // Quieres la nevera en modo independiente?
int temperature = 0; // Sensor temperature
int maxTemperature = 20; // Parametro temperatura minima permitida.
int minTemperature = -10; // Parametro temperatura maxima permitida.
// State JSON
const size_t capacity = 1024; 
DynamicJsonDocument state(capacity); // State, sensors, outputs...
DynamicJsonDocument information(capacity); // Information, name, id, ssid...
DynamicJsonDocument memoryJson(capacity); // State, sensors, outputs...

/// WIFI Connection

/// TODO: Guardar informacion en memoria, para persistir datos.
// My Wifi (Standalone)
String ssid     = id; // Nombre del wifi en modo standalone
String password = "12345678"; // Clave del wifi en modo standalone
// Coordinator Wifi  
String ssidCoordinator     = id; // Wifi al que se debe conectar (coordinador)
String passwordCoordinator = "12345678"; // Clave del Wifi del coordinador
// Notify information: Publish when a new user is connected
bool notifyInformation = false;

/// JSON ///

// Convert to json
JsonObject toJson(String str){
  // Serial.println(str);
  DynamicJsonDocument docInput(1024);
  JsonObject json;
  deserializeJson(docInput, str);
  json = docInput.as<JsonObject>();
  return json;
}

/// Returns the JSON converted to String
String jsonToString(DynamicJsonDocument json){
  String buf;
  serializeJson(json, buf);
  return buf;
}

/// Initilize or update the JSON State of the Fridge.
void setState(){
  state["id"] = id;
  state["temperature"] = temperature;
  state["light"] = light;
  state["compressor"] = compressor;
  state["door"] = door;
  state["maxTemperature"] = maxTemperature;
  state["minTemperature"] = minTemperature;
  state["standalone"] = standalone;
  state["ssid"] = ssid;

  // SSID Coordinator just if standalone is false;
  if (!standalone){
    state["ssidCoordinator"] = ssidCoordinator;
  }

}

/// Initialize or update the JSON Info of the MQTT Connection (Standalone)
void setInformation(){
  information["id"] = id;
  information["ssid"] = ssid;
  information["standalone"] = standalone;
}

void getMemoryData(){
  Serial.println("Obteniendo datos en memoria");
  DynamicJsonDocument doc(1024);
  JsonObject json;
  // deserializeJson(docInput, str);
  EepromStream eepromStream(0, 1024);
  deserializeJson(doc, eepromStream);
  json = doc.as<JsonObject>();
  Serial.println(String(json["minTemperature"]));
  minTemperature = json["minTemperature"];
  maxTemperature = json["maxTemperature"];
  ssid = String(json["ssid"]);

}

void setMemoryData(){
  memoryJson["id"] = id;
  memoryJson["maxTemperature"] = maxTemperature;
  memoryJson["minTemperature"] = minTemperature;
  memoryJson["standalone"] = standalone;
  memoryJson["ssid"] = ssid;
  memoryJson["ssidCoordinator"] = ssidCoordinator;

  EepromStream eepromStream(0, 1024);
  serializeJson(memoryJson, eepromStream);
  EEPROM.commit();

}

/// MQTT Broker ///

class FridgeMQTTBroker: public uMQTTBroker
{
public:
    virtual bool onConnect(IPAddress addr, uint16_t client_count) {
      Serial.println(addr.toString()+" connected");
      return true;
    }

    virtual void onDisconnect(IPAddress addr, String client_id) {
      Serial.println(addr.toString()+" ("+client_id+") disconnected");
    }

    virtual bool onAuth(String username, String password, String client_id) {
      Serial.println("Username/Password/ClientId: "+username+"/"+password+"/"+client_id);
      notifyInformation = true;
      return true;
    }
    
    virtual void onData(String topic, const char *data, uint32_t length) {
      char data_str[length+1];
      os_memcpy(data_str, data, length);
      data_str[length] = '\0';
      Serial.println("received topic '"+topic+"' with data '"+(String)data_str+"'");

      // Convert to JSON.
      DynamicJsonDocument docInput(1024); 
      JsonObject json;
      deserializeJson(docInput, (String)data_str);
      json = docInput.as<JsonObject>();
      
      // if (topic == "state/" + id){
      //   Serial.println("Temperature: " + String(json["temperature"]));
      // }

      if(topic == "action/" + id){
        Serial.println("Action: " + String(json["action"]));
        onAction(json);
        // EJECUTAR LAS ACCIONES
        
        setState();
        publishState();
      }

      
      //printClients();
    }

    // Sample for the usage of the client info methods

    virtual void printClients() {
      for (int i = 0; i < getClientCount(); i++) {
        IPAddress addr;
        String client_id;
         
        getClientAddr(i, addr);
        getClientId(i, client_id);
        Serial.println("Client "+client_id+" on addr: "+addr.toString());
      }
    }
};

FridgeMQTTBroker myBroker;

/// Publish state. Used to initilize the topic and when the state changes
/// publish the state in the correct topic according if the fridge is working on standole mode or not.
void publishState(){
  String stateEncoded = jsonToString(state);
  if(standalone){
    /// Publish on Standalone Mode
    myBroker.publish("state/" + id, stateEncoded);
  }else{
    /// TODO: Publish on Coordinator Mode

  }
}

void publishInformation (){
  String informationEncoded = jsonToString(state);
  if(standalone){
    // Publish on Standalone Mode
    myBroker.publish("information", informationEncoded);
  }else{
    // TODO: Publish on Coordinator Mode

  }
}

/// WIFI ///

void startWiFiClient()
{
  Serial.println("Connecting to "+(String)ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tries = tries + 1;
    if (tries > 60){
      startWiFiAP();
      standalone = true;
      return;
    }
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: " + WiFi.localIP().toString());
}

void startWiFiAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  Serial.println("AP started");
  Serial.println("IP address: " + WiFi.softAPIP().toString());
}

void setupWifi(){
  /// Standalone Mode
  if (standalone){
    // We start by connecting to a WiFi network or create the AP
    startWiFiAP();
    // Start the broker
    Serial.println("Starting MQTT Broker");
    myBroker.init();
    myBroker.subscribe("state/" + id);
    myBroker.subscribe("action/" + id);
  }
  /// Coordinator Mode
  else{
    startWiFiClient();
    /// TODO: connect to local MQTT Broker and subscribe to topics
  }
}


void setup() {
  EEPROM.begin(1024);
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  getMemoryData();
  /// Setup WiFi
  setupWifi();

  // Initilize JSON with the state of the Fridge and initialize the topic
  setState();
  setInformation();
  publishState();
  publishInformation();
  delay(1000);
  
}


void loop() {
  
  // Publish state
  readTemperature();
  
  if (notifyInformation){
    publishInformation();
    setState();
    publishState();
    notifyInformation = false;
  }

  
  delay(1000);

}

/// ACTIONS ///

void onAction(JsonObject json){
  String action = json["action"];

  if(action.equals("toggleLight")){
    Serial.println("Indicarle a la nevera seleccionada que prenda la luz");
     toggleLight();
  }
  if(action.equals("setMaxTemperature")){
    Serial.println("Indicarle a la nevera seleccionada que cambie su nivel maximo de temperature");
    int maxTemperature = json["maxTemperature"];
    setMaxTemperature(maxTemperature);
  }
  if(action.equals("setMinTemperature")){
    Serial.println("Indicarle a la nevera seleccionada que cambie su nivel minimo de temperature");
    int minTemperature = json["minTemperature"];
    setMinTemperature(minTemperature);
  }

  if(action.equals("setStandaloneMode")){
    Serial.println("Cambiar a modo independiente");
    String _newSsid = json["ssid"];
    setStandaloneMode(_newSsid);
  }

}

/// Read temperature
void readTemperature(){
  int temperatureRead = random(-20, 5);
  if (temperatureRead != temperature){
    temperature = temperatureRead;
    notifyInformation = true;
  }

}

/// Turn on/off the light
void toggleLight(){
  light = !light;
  /// TODO: turn on the light using digital output.
  notifyInformation = true;
}

/// Set max temperature
void setMaxTemperature(int newMaxTemperature){
  maxTemperature = newMaxTemperature;
  notifyInformation = true;
  setMemoryData();
}

/// Set min temperature
void setMinTemperature(int newMinTemperature){
  minTemperature = newMinTemperature;
  notifyInformation = true;
  setMemoryData();
}

/// Set standalone mode.
void setStandaloneMode(String newSsid){
  standalone = true;
  WiFi.mode(WIFI_OFF);  
  ssid = newSsid;
  setupWifi();
  setMemoryData();

}