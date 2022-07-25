// #include <WebSocketClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "uMQTTBroker.h"
#include "StreamUtils.h"
#include <SoftwareSerial.h>
#include "EspMQTTClient.h"

#define CONFIGURATION_MODE_OUTPUT D5 

char path[] = "/";
char host[] = "192.168.0.1";

/// State of the Fridge
String id = "nevera-07-test";
String name = "nevera-07-test";
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

// My Wifi (Standalone)
String ssid     = id; // Nombre del wifi en modo standalone
String password = "12345678"; // Clave del wifi en modo standalone
// Coordinator Wifi  
String ssidCoordinator     = ""; // Wifi al que se debe conectar (coordinador)
String passwordCoordinator = "12345678"; // Clave del Wifi del coordinador
// Notify information: Publish when a new user is connected
bool notifyInformation = false;
bool notifyState = false;

/// Configuration mode
bool configurationMode = true;
bool configurationModeLightOn = false;
String configurationInfoStr = "";

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
  state["name"] = name;
  state["temperature"] = temperature;
  state["light"] = light;
  state["compressor"] = compressor;
  state["door"] = door;
  state["maxTemperature"] = maxTemperature;
  state["minTemperature"] = minTemperature;
  state["standalone"] = standalone;
  state["ssid"] = ssid;
  state["ssidCoordinator"] = ssidCoordinator;


  // SSID Coordinator just if standalone is false;
  // if (!standalone){
  // }

}

/// Initialize or update the JSON Info of the MQTT Connection (Standalone)
void setInformation(){
  information["id"] = id;
  information["name"] = name;
  information["ssid"] = ssid;
  information["standalone"] = standalone;
  information["configurationMode"] = configurationMode;
}
a
/// Obtener los datos en memoria
void getMemoryData(){
  DynamicJsonDocument doc(1024);
  JsonObject json;
  // deserializeJson(docInput, str);
  EepromStream eepromStream(0, 1024);
  deserializeJson(doc, eepromStream);
  json = doc.as<JsonObject>();


  /// Getting the memory data if the configuration mode is false
  Serial.println("Modo configuracion: "+ String(json["configurationMode"]));
  Serial.println("Modo configuracion: "+ bool(json["configurationMode"]));
  Serial.println(String(json["configurationMode"]) == "null");


  /// TODO: Cambiar para cuando el modo configuracion este listo
  if (String(json["configurationMode"]) == "null" || bool(json["configurationMode"]) == true){
  // if (false){    
    Serial.println("Activando modo de configuracion");
    configurationMode = true;
    
  } else {
    
    Serial.println("Obteniendo datos en memoria");
    configurationMode = false;
    minTemperature = json["minTemperature"];
    maxTemperature = json["maxTemperature"];
    ssid = String(json["ssid"]);
    name = String(json["name"]);
    String ssidCoordinatorMemory = String(json["ssidCoordinator"]);
    password = String(json["password"]);
    String passwordCoordinatorMemory = String(json["passwordCoordinator"]);

    Serial.print("ssidCoordinator obtenido en memoria: ");
    Serial.println(ssidCoordinatorMemory);
    Serial.println(ssidCoordinator);
    if(ssidCoordinatorMemory != ""){
      ssidCoordinator = ssidCoordinatorMemory;
    }

    Serial.print("passwordCoordinator obtenido en memoria: ");
    Serial.println(passwordCoordinatorMemory);
    Serial.println(passwordCoordinator);
    if(passwordCoordinatorMemory != ""){
      passwordCoordinator = passwordCoordinatorMemory;
    }
    
    standalone = bool(json["standalone"]);
  }

}

/// Guardar los datos en memoria
void setMemoryData(){
  memoryJson["id"] = id;
  memoryJson["name"] = name;
  memoryJson["maxTemperature"] = maxTemperature;
  memoryJson["minTemperature"] = minTemperature;
  memoryJson["standalone"] = standalone;
  memoryJson["ssid"] = ssid;
  memoryJson["ssidCoordinator"] = ssidCoordinator;
  memoryJson["password"] = password;
  memoryJson["passwordCoordinator"] = passwordCoordinator;
  memoryJson["configurationMode"] = configurationMode;

  EepromStream eepromStream(0, 2048);
  serializeJson(memoryJson, eepromStream);
  EEPROM.commit();

}

/// MQTT Broker 
/// Usado para crear el servidor MQTT en modo independiente
class FridgeMQTTBroker: public uMQTTBroker
{
public:
    virtual bool onConnect(IPAddress addr, uint16_t client_count) {
      Serial.println(addr.toString()+" connected");
      notifyInformation = true;
      notifyState = true;
      return true;
    }

    virtual void onDisconnect(IPAddress addr, String client_id) {
      Serial.println(addr.toString()+" ("+client_id+") disconnected");
    }

    virtual bool onAuth(String username, String password, String client_id) {
      Serial.println("Username/Password/ClientId: "+username+"/"+password+"/"+client_id);
      notifyInformation = true;
      notifyState = true;
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

/// MQTT Servidor para Modo Independiente
FridgeMQTTBroker myBroker;
/// MQTT Cliente para Mode Coordinado
EspMQTTClient localClient(
  "192.168.0.1",
  1883,
  "MQTTUsername", 
  "MQTTPassword",
  "id"
);

/// Publish state. Used to initilize the topic and when the state changes
/// publish the state in the correct topic according if the fridge is working on standole mode or not.
void publishState(){
  String stateEncoded = jsonToString(state);
  if(standalone){
    /// Publish on Standalone Mode
    myBroker.publish("state/" + id, stateEncoded);
  }else{
    /// TODO: Publish on Coordinator Mode
    localClient.publish("state/" + id, stateEncoded);
  }
}

/// Publicar la información de la comunicación/conexión
void publishInformation (){
  String informationEncoded = jsonToString(information);
  if(standalone){
    // Publish on Standalone Mode
    Serial.println(informationEncoded);
    myBroker.publish("information", informationEncoded);
  }else{
    // TODO: Publish on Coordinator Mode
    
  }
}

/// Publicar errores
void publishError(String errorMessage){
  /// topic 'error/id'


}

///
/// WIFI ///
///

/// Conexión al Wifi como cliente
bool startWiFiClient()
{
  // Serial.println(ssidCoordinator);
  /// Desconexion por si acaso hubo una conexion previa
  WiFi.disconnect();
  /// Modo estacion, por si hubo un modo diferente previamente.
  WiFi.mode(WIFI_STA);
  
  /// Conexion al coordinador
  // WiFi.begin("coordinador-07-test", "12345678");
  WiFi.begin(ssidCoordinator, passwordCoordinator);
  Serial.print("Conectandome al coordinador...");
  Serial.println(ssidCoordinator);
  Serial.println(passwordCoordinator);
  Serial.print("Conectandome al coordinador...");
  delay(500);
  /// Contador de intentos
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    /// Esperar medio segundo entre intervalo
    // WiFi.begin(ssidCoordinator, passwordCoordinator);
    delay(500);

    /// Reconectarme
    Serial.print(".");
    tries = tries + 1;
    /// 
    if (tries > 30){
      
      standalone = true;
      return false;
    } 
    /// Reintentar conexion.
    // WiFi.begin(ssidCoordinator, passwordCoordinator);
  }
  Serial.println();

  Serial.print("WiFi connected ");
  Serial.println("IP address: " + WiFi.localIP().toString());

  /// Conectarse al servidor MQTT del Coordinador y suscribirse a los topicos.
  Serial.println("Conectarse al Servidor MQTT");
  localClient.setMqttServer("192.168.0.1", "MQTTUsername", "MQTTPassword", 1883);
  localClient.setOnConnectionEstablishedCallback(onConnectionEstablished); 
  return true;
}

/// Creación del punto de acceso (WiFI)
void startWiFiAP()
{
  /// Modo Punto de Acceso.
  WiFi.mode(WIFI_AP);
  IPAddress apIP(192, 168, 0, 1);   //Static IP for wifi gateway
  /// Inicializo Gateway, Ip y Mask
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); //set Static IP gateway on NodeMCU
  /// Nombre y clave del wifi
  WiFi.softAP(ssid, password);
  Serial.println("AP started");
  Serial.println("IP address: " + WiFi.softAPIP().toString());


  // Start the broker
  Serial.println("Starting MQTT Broker");
  // Inicializo el servidor MQTT
  myBroker.init();
  // Suscripcion a los topicos de interes
  myBroker.subscribe("state/" + id);
  myBroker.subscribe("action/" + id);
  /// TODO: suscribirse a tema de error, que comunica mensaje de error
}

/// Configurar el WIFI y MQTT
void setupWifi(){

  /// Standalone Mode, configurar punto de acceso (WiFI) y el servidor MQTT
  /// para el modo independiente
  if (standalone){
    // We start by connecting to a WiFi network or create the AP
    Serial.println("Creando Wifi Independiente y Servidor MQTT");
    startWiFiAP();
  }
  /// Coordinator Mode, conectarse al wifi del coordinador y conectarse al servidor MQTT del coordinador.
  else{
    Serial.println("Conectarse al Wifi del Coordinador y al servidor MQTT");
    bool connected = startWiFiClient();
    if(!connected){
      startWiFiAP();
    }
  }
}

/// Funcion llamada una vez que el cliente MQTT establece la conexión.
/// funciona para el modo independiente solamente
void onConnectionEstablished(){
  localClient.subscribe("state/" + id, [](const String & payload) {
    Serial.println(payload);

  });

  localClient.subscribe("action/" + id, [](const String & payload) {
    Serial.println(payload);

    // Convert to JSON.
    DynamicJsonDocument docInput(1024); 
    JsonObject json;
    deserializeJson(docInput, (String)payload);
    json = docInput.as<JsonObject>();
    // Ejecutar las acciones
    onAction(json);
    // Guardar el estado
    setState();
    publishState();

  });
}


void setup() {
  /// Memory
  EEPROM.begin(1024);
  /// Logs
  Serial.begin(115200);
  ///// Bluetooth module baudrate 
  // btSerial.begin(9600);     

  /// Configuration mode light output
  pinMode(CONFIGURATION_MODE_OUTPUT, OUTPUT);
  
  delay(5000);
  getMemoryData();
  /// Setup WiFi
  setupWifi();
  setInformation();
  publishInformation();
  
  if (!configurationMode){

    // Initilize JSON with the state of the Fridge and initialize the topic
    setState();
    publishState();

  }

  delay(1000);
}


void loop() {
  
  // Mantener activo el cliente MQTT (Modo Independietne)
  localClient.loop();
  // Leer temperature
  readTemperature();

  if (!configurationMode) {

    // Publish info
    if (notifyInformation){
      delay(500);
      setInformation();
      publishInformation();

      notifyInformation = false;
    }

    // Publish state
    if (notifyState){
      setState();
      publishState();
      notifyState = false;
    }

    if (!standalone){
      localClient.loop();
    }

  }
  else {
    
    Serial.println("Esperando configuración...");
    // readDataFromBluetooth();
    if (!configurationModeLightOn){
      Serial.println("Encendiendo luces de modo de configuración...");
      
      digitalWrite(CONFIGURATION_MODE_OUTPUT, HIGH);
      configurationModeLightOn = true;
    }

    // Publish info
    if (notifyInformation){
      delay(500);
      setInformation();
      publishInformation();

      notifyInformation = false;
    }

  }


  
  delay(1000);

}

/// Funcion llamada cada vez que se recibe una publicacion en el tópico 'actions/{id}'
/// la funcion descubre cual accion es la requerida usando el json, y le pasa los parametros 
/// a traves del propio json
void onAction(JsonObject json){
  String action = json["action"];

  if (configurationMode){
    
    if (action.equals("configureDevice")){
      Serial.println("Configurando dispositivo");
      String name = json["name"];
      int maxTemperature = json["maxTemperature"];
      int minTemperature = json["minTemperature"];
      String _ssid = json["ssid"];
      String _password = json["password"];
      bool _standalone = json["standalone"];
      String _ssidCoordinator = json["ssidCoordinator"];
      String _passwordCoordinator = json["passwordCoordinator"];

      configureDevice(name, _ssid, _password, _ssidCoordinator, _passwordCoordinator, _standalone, maxTemperature, minTemperature);
    }
    
    return;
  } 

  if(action.equals("changeName")){
    Serial.println("Cambiar el nombre a la nevera");
    String _newName = json["name"];
    changeName(_newName);
  }

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

  if(action.equals("setCoordinatorMode")){
    Serial.println("Cambiar a modo coordinado");
    String _newSsidCoordinator = json["ssid"];
    String _newPasswordCoordinator = json["password"];
    setCoordinatorMode(_newSsidCoordinator, _newPasswordCoordinator);
  }

}

///
/// FUNCIONES PARA ACCIONES ///
///         

/// Lectura de temperatura a través del sensor.
void readTemperature(){
  int temperatureRead = random(0, 2);
  if (temperatureRead != temperature){
    temperature = temperatureRead;
    notifyState = true;
  }

}

/// Cambiar nombre
void changeName(String newName){
  name = newName;
  notifyState = true;
  setMemoryData();
}

/// Encender o apagar la luz
void toggleLight(){
  light = !light;
  // TODO(calg): encender luz usando digitalWrite()
  notifyState = true;
}

/// Cambiar el parametro de temperatura máxima.
void setMaxTemperature(int newMaxTemperature){
  maxTemperature = newMaxTemperature;
  notifyState = true;
  setMemoryData();
}

/// Cambiar el parametro de temperatura mínima.
void setMinTemperature(int newMinTemperature){
  minTemperature = newMinTemperature;
  notifyState = true;
  setMemoryData();
}

/// Cambiar a modo independiente, nombre del wifi y contraseña.
// TODO(lesanpi): que reciba tambien la contraseña
void setStandaloneMode(String newSsid){
  standalone = true;
  WiFi.mode(WIFI_OFF);  
  ssid = newSsid;
  setMemoryData();
  setupWifi();

}

/// Cambia a modo coordinador, indicando el nombre y contraseña del Wifi
void setCoordinatorMode(String ssid, String password){
  standalone = false;
  ssidCoordinator = ssid;
  passwordCoordinator = password;
  setMemoryData();
  WiFi.mode(WIFI_OFF);  
  setupWifi();

}

/// Configurar dispositivo cuando esta en modo configuración
// TODO(lesanpi): Falta ssid y password del wifi con internet.
void configureDevice(
  String name,
  String ssid, 
  String password, 
  String coordinatorSsid, 
  String coordinatorPassword, 
  bool standalone,
  int maxTemperature,
  int minTemperature
  ){
  
  configurationMode = false;
  digitalWrite(CONFIGURATION_MODE_OUTPUT, LOW);
  configurationModeLightOn = false;
  changeName(name);
  setMaxTemperature(maxTemperature);
  setMinTemperature(minTemperature);
  setStandaloneMode(ssid);
  if (standalone){
    setCoordinatorMode(coordinatorSsid, coordinatorPassword);
  }

}