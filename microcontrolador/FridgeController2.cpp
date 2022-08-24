// #include <WebSocketClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "uMQTTBroker.h"
#include "StreamUtils.h"
#include <SoftwareSerial.h>
#include "DHT.h"
#include "EspMQTTClient.h"
#include <ArduinoJWT.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>

//========================================== pins
//==========================================
#define CONFIGURATION_MODE_OUTPUT D5 
#define LIGHT D2
#define DHTTYPE DHT11   // DHT 11
#define COMPRESOR D1
#define KEY "secretphrase"

String API_HOST = "https://zona-refri-api.herokuapp.com";
uint8_t DHTPin = D3; /// DHT1

//========================================== jwt
//==========================================

//========================================== datos de la nevera
//==========================================
String id = "nevera-07-test";
String userId = "";
String name = "nevera-07-test";
bool light = false; // Salida luz
bool compressor = false; // Salida compressor
bool door = false; // Sensor puerta abierta/cerrada
bool standalone = true;   // Quieres la nevera en modo independiente?
float temperature = 0; // Sensor temperature
float humidity = 70; // Sensor humidity
int maxTemperature = 20; // Parametro temperatura minima permitida.
int minTemperature = -10; // Parametro temperatura maxima permitida.
int temperaturaDeseada = 4; // Parametro temperatura recibida por el usuario.
unsigned long tiempoAnterior; //Para almacenar el tiempo en milisegundos.
int tiempoEspera = 4200000; // 7 minutos de espera de tiempo prudencial para volver a encender el compresor.
bool compresorFlag = false; //Bandera que indica que el compresor fue encendido.

//========================================== json
//==========================================
const size_t capacity = 1024; 
DynamicJsonDocument state(256); // State, sensors, outputs...
// DynamicJsonDocument information(256); // Information, name, id, ssid...
// DynamicJsonDocument error(256); // State, sensors, outputs...
DynamicJsonDocument memoryJson(2048); // State, sensors, outputs...

// String testConf = String("")
/// Returns the JSON converted to String
String jsonToString(DynamicJsonDocument json){
  String buf;
  serializeJson(json, buf);

  return buf;
}
//========================================== Variables importantes
//==========================================
// Notify information: Publish when a new user is connected
bool notifyInformation = false;
bool notifyState = false;
bool notifyError = false;
/// Configuration mode
bool configurationMode = true;
bool configurationModeLightOn = false;
String configurationInfoStr = "";

//========================================== Wifi y MQTT
//==========================================
char path[] = "/";
char host[] = "192.168.0.1";
// Modo Independiente
String ssid     = id; // Nombre del wifi en modo standalone
String password = "12345678"; // Clave del wifi en modo standalone
// Coordinator Wifi  
String ssidCoordinator     = ""; // Wifi al que se debe conectar (coordinador)
String passwordCoordinator = "12345678"; // Clave del Wifi del coordinador
// Internet Wifi
String ssidInternet = "Sanchez Fuentes 2";
String passwordInternet = "09305573";
// MQTT
const char* mqtt_cloud_server = "b18bfec2abdc420f99565f02ebd1fa05.s2.eu.hivemq.cloud"; // replace with your broker url
const char* mqtt_cloud_username = "testUser";
const char* mqtt_cloud_password = "testUser";
const int mqtt_cloud_port =8883;

//========================================== json information
//==========================================
/// Initilize or update the JSON State of the Fridge.
void setState(){
  

}

/// Initialize or update the JSON Info of the MQTT Connection (Standalone)
void setInformation(){
  // ! Remover luego de resolver problema.
  return;
  DynamicJsonDocument information(256); // Information, name, id, ssid...
  information["id"] = id;
  information["name"] = name;
  information["ssid"] = ssid;
  information["standalone"] = standalone;
  information["configurationMode"] = configurationMode;
}

// void setError(){
//   error["id"] = id;
//   error["error"] = error;
// }

//========================================== sensors
//==========================================
DHT dht(DHTPin, DHTTYPE);                

const long updateTempInterval = 1000 * 60 * 1;
unsigned long previousTemperaturePushMillis = 0;

//========================================== notifications
//==========================================
/// 1000 millisPerSecond * 60 secondPerMinutes * 20 minutes  
const long interval = 1000 * 60 * 20;  
unsigned long previousTemperatureNoticationMillis = 0;

//========================================== memoria
//==========================================
/// Obtener datos en memoria


void getMemoryData(){
  DynamicJsonDocument doc(2048);
  JsonObject json;
  // deserializeJson(docInput, str);
  EepromStream eepromStream(0, 2048);
  deserializeJson(doc, eepromStream);
  json = doc.as<JsonObject>();


  /// Getting the memory data if the configuration mode is false
  Serial.println("[MEMORIA] Modo configuracion: "+ String(json["configurationMode"]));

  /// TODO: Cambiar para cuando el modo configuracion este listo
  if (String(json["configurationMode"]) == "null" || bool(json["configurationMode"]) == true){
  // if (false){    
    Serial.println("[MEMORIA] Activando modo de configuracion");
    configurationMode = true;
    
  } else {
    
    Serial.println("[MEMORIA] Obteniendo datos en memoria");
    configurationMode = false;
    id = String(json["id"]);
    userId = String(json["userId"]);
    temperaturaDeseada = json["desiredTemperature"];
    minTemperature = json["minTemperature"];
    maxTemperature = json["maxTemperature"];
    ssid = String(json["ssid"]);
    ssidCoordinator = String(json["ssidCoordinator"]);
    password = String(json["password"]);
    passwordCoordinator = String(json["passwordCoordinator"]);
    name = String(json["name"]);
    String ssidCoordinatorMemory = String(json["ssidCoordinator"]);
    password = String(json["password"]);
    String passwordCoordinatorMemory = String(json["passwordCoordinator"]);

    // Serial.print("ssidCoordinator obtenido en memoria: ");
    // Serial.println(ssidCoordinatorMemory);
    // Serial.println(ssidCoordinator);
    if(ssidCoordinatorMemory != ""){
      ssidCoordinator = ssidCoordinatorMemory;
    }

    // Serial.print("passwordCoordinator obtenido en memoria: ");
    // Serial.println(passwordCoordinatorMemory);
    // Serial.println(passwordCoordinator);
    if(passwordCoordinatorMemory != ""){
      passwordCoordinator = passwordCoordinatorMemory;
    }
    
    standalone = bool(json["standalone"]);
  }
  Serial.println("[JSON DEBUG][GET MEMORY] Is overflowed: " + String(state.overflowed()));
  Serial.println("[JSON DEBUG][GET MEMORY] Is memoryUsage: " + String(state.memoryUsage()));
  Serial.println("[JSON DEBUG][GET MEMORY] Is size: " + String(state.size()));
  // json.clear();

}

/// Guardar los datos en memoria
void setMemoryData(){

  Serial.println("[MEMORY] Guardando datos en memoria");
  memoryJson["id"] = id;
  memoryJson["userId"] = userId;
  memoryJson["name"] = name;
  memoryJson["desiredTemperature"] = temperaturaDeseada;
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

  // memoryJson.clear();

}


//========================================== Cliente MQTT y Cliente WiFi
//==========================================
/// MQTT Broker usado para crear el servidor MQTT en modo independiente
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
      Serial.println("[LOCAL BROKER][DISCONNECT] " + addr.toString()+" ("+client_id+") disconnected");
    }

    virtual bool onAuth(String username, String password, String client_id) {
      Serial.println("[LOCAL BROKER][AUTH] Username/Password/ClientId: "+username+"/"+password+"/"+client_id);
      notifyInformation = true;
      notifyState = true;
      return true;
    }
    
    virtual void onData(String topic, const char *data, uint32_t length) {
      char data_str[length+1];
      os_memcpy(data_str, data, length);
      data_str[length] = '\0';
      // Serial.println("Topico recibido: '"+topic+"', con los datos: '"+(String)data_str+"'");
      Serial.println("[LOCAL BROKER]["+String(topic)+"] Mensaje recibido> "+(String)data_str+" ");

      // Convert to JSON.
      DynamicJsonDocument docInput(512); 
      JsonObject json;
      deserializeJson(docInput, String(data_str));
      json = docInput.as<JsonObject>();
      Serial.println(json);
      int tries = 0;
      
      while (json == 1 && tries < 20){
        deserializeJson(docInput, String(data_str));
        json = docInput.as<JsonObject>();
        tries++;
      }

      if (json == 1) return;
      
      Serial.println(String(data_str));
      if(topic == "action/" + id){
        Serial.println("Action: " + String(json["action"]));
        onAction(json);
        // EJECUTAR LAS ACCIONES
        
        
      }

      notifyState = true;
      // setState();
      // publishState();
      
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

/// Cliente ESP
WiFiClientSecure espClient;  
/// Cliente MQTT en la nube
PubSubClient cloudClient(espClient);
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


//===================================== funciones en MQTT
//=====================================

/// Publish state. Used to initilize the topic and when the state changes
/// publish the state in the correct topic according if the fridge is working on standole mode or not.
void publishState(){
  // DynamicJsonDocument state(2048); // State, sensors, outputs...
  state["id"] = id;
  // state["userId"] = userId;
  state["name"] = name;
  state["temperature"] = temperature;
  state["light"] = light;
  state["compressor"] = compressor;
  state["door"] = door;
  state["desiredTemperature"] = 15;
  state["maxTemperature"] = maxTemperature;
  state["minTemperature"] = minTemperature;
  state["standalone"] = standalone;
  state["ssid"] = ssid;
  state["ssidCoordinator"] = ssidCoordinator;
  state["isConnectedToWifi"] = WiFi.status() == WL_CONNECTED;
  setState();
  String stateEncoded = jsonToString(state);
  setState();
  String stateEncoded2 = jsonToString(state);
  
  Serial.println("[PUBLISH] Publicando estado");
  if(standalone){
    /// Publish on Standalone Mode
    bool published = myBroker.publish("state/" + id, stateEncoded);
    Serial.println("[PUBLISH] Publicado en broker local: " + String(published));
    if (cloudClient.connected()){
      if (cloudClient.publish((("state/"+id)).c_str(), stateEncoded2.c_str(), true)){
        // Serial.print("[INTERNET] Estado publicado en: ");
        Serial.println(String(("state/"+id)).c_str());

      }
      // cloudClient.publish(String("state/62f90f52d8f2c401b58817e3").c_str(), stateEncoded2.c_str(), true);
    }

  }else{
    localClient.publish("state/" + id, stateEncoded);
    
  }
  Serial.println("[JSON DEBUG][STATE] Is overflowed: " + String(state.overflowed()));
  Serial.println("[JSON DEBUG][STATE] Is memoryUsage: " + String(state.memoryUsage()));
  Serial.println("[JSON DEBUG][STATE] Is size: " + String(state.size()));
  // state.clear();  

}

/// Publicar la información de la comunicación/conexión
void publishInformation (){
  DynamicJsonDocument information(256); // Information, name, id, ssid...
  information["id"] = id;
  information["name"] = name;
  information["ssid"] = ssid;
  information["standalone"] = standalone;
  information["configurationMode"] = configurationMode;
  String informationEncoded = jsonToString(information);
  if(standalone){
    // Publish on Standalone Mode
    Serial.println("[LOCAL BROKER] Publicando informacion: " + informationEncoded);
    myBroker.publish("information", informationEncoded);
  }else{
    // TODO: Publish on Coordinator Mode

  }
  Serial.println("[JSON DEBUG][INFORMATION] Is overflowed: " + String(state.overflowed()));
  Serial.println("[JSON DEBUG][INFORMATION] Is memoryUsage: " + String(state.memoryUsage()));
  Serial.println("[JSON DEBUG][INFORMATION] Is size: " + String(state.size()));
  // information.clear();
}

// void publishError (){
//   String errorEncoded = jsonToString(error);
//   if(standalone){
//     // Publish on Standalone Mode
//     Serial.println("Publicando error:" + errorEncoded);
//     myBroker.publish("error", errorEncoded);
//   }else{
//     // TODO: Publish on Coordinator Mode

//   }
// }

//===================================== reconnect cloud
/// Reconnect cloud connection
void reconnectCloud() {
  int tries = 0;
  // Loop until we're reconnected
  while (!cloudClient.connected() && tries <= 1) {
    Serial.print("[INTERNET] Intentando conexion MQTT...");
    
    // Attempt to connect
    if (cloudClient.connect(id.c_str(), mqtt_cloud_username, mqtt_cloud_password)) {
      Serial.println("conectado");

      // Subscribe to topics
      cloudClient.subscribe(("action/" + id).c_str());
      // cloudClient.subscribe(("state/" + id).c_str());

    } else {
      Serial.print("failed, rc=");
      Serial.print(cloudClient.state());
      Serial.println(" try again in 5 seconds");   // Wait 5 seconds before retrying
      tries += 1;
      delay(5000);
      
    }
  }
}

//===================================== configuracion wifi
//=====================================
/// Conexion al Wifi con internet
void startInternetClient()
{
  /// Conectarse al Wifi con Internet
  Serial.print("[WIFI INTERNET] Conectandose al Wifi con Internet: ");
  Serial.print(ssidInternet);
  if (ssidInternet != ""){
    // Configures static IP address
    // Set your Static IP address
    IPAddress local_IP(192, 168, 1, 200);
    // Set your Gateway IP address
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress primaryDNS(8, 8, 8, 8);   //optional
    IPAddress secondaryDNS(8, 8, 4, 4); //optional
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("[WIFI INTERNET] STA Fallo en la configuracion");
    }
    WiFi.begin(ssidInternet, passwordInternet);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 60)
    {
      tries = tries + 1;
      delay(100);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED){
      Serial.print("\n[WIFI INTERNET] Conectado. ");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      espClient.setInsecure();

      cloudClient.setServer(mqtt_cloud_server, mqtt_cloud_port);
      cloudClient.setCallback(cloud_callback);
      reconnectCloud();
    }



  }
}

void cloud_callback(char* topic, byte* payload, unsigned int length) {
  String incommingMessage = "";
  for (int i = 0; i < length; i++) incommingMessage+=(char)payload[i];
  
  Serial.println("[INTERNET]["+String(topic)+"] Mensaje recibido> "+incommingMessage);
  
  // Convert to JSON.
  DynamicJsonDocument docInput(1024); 
  JsonObject json;
  deserializeJson(docInput, incommingMessage);
  json = docInput.as<JsonObject>();
  
  // if (topic == "state/" + id){
  //   Serial.println("Temperature: " + String(json["temperature"]));
  
  char topicBuf[50];
  id.toCharArray(topicBuf, 50);
  if(String(topic) == ("action/" + id)){
    Serial.println("[INTERNET][ACCION] Accion recibida: " + String(json["action"]));
    onAction(json);
  }

  notifyState = true;
  // setState();
  // publishState();
  // String stateEncoded = jsonToString(state);
  // if (cloudClient.connected()){
  //   cloudClient.publish(("state/" + id).c_str(), stateEncoded.c_str());
  // }

}

/// Conexión al Wifi como cliente
bool startWiFiClient()
{
  // Serial.println(ssidCoordinator);
  /// Desconexion por si acaso hubo una conexion previa
  WiFi.disconnect();
  /// Modo estacion, por si hubo un modo diferente previamente.
  // WiFi.mode(WIFI_STA);
  
  /// Conexion al coordinador
  // WiFi.begin("coordinador-07-test", "12345678");
  WiFi.begin(ssidCoordinator, passwordCoordinator);
  Serial.print("Conectandome al coordinador... ");
  Serial.print(ssidCoordinator);

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

  Serial.print("conectado ");
  Serial.println("IP address: " + WiFi.localIP().toString());

  /// Conectarse al servidor MQTT del Coordinador y suscribirse a los topicos.
  Serial.println("Conectarse al Servidor MQTT...");
  localClient.setMqttServer("192.168.0.1", "MQTTUsername", "MQTTPassword", 1883);
  localClient.setOnConnectionEstablishedCallback(onConnectionEstablished); 
  return true;
}

/// Creación del punto de acceso (WiFI)
void startWiFiAP()
{
  /// Modo Punto de Acceso.
  IPAddress apIP(192, 168, 0, 1);   //Static IP for wifi gateway
  /// Inicializo Gateway, Ip y Mask
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); //set Static IP gateway on NodeMCU
  /// Nombre y clave del wifi
  WiFi.softAP(ssid, password);
  Serial.print("[WIFI AP INDEPENDIENTE] Iniciando punto de acceso: " + ssid);
  Serial.print(". IP address: " + WiFi.softAPIP().toString());


  // Start the broker
  Serial.println("\n[MQTT LAN] Iniciando MQTT Broker...");
  // Inicializo el servidor MQTT
  myBroker.init();
  // Suscripcion a los topicos de interes
  // myBroker.subscribe("state/" + id);
  myBroker.subscribe("action/" + id);
  /// TODO: suscribirse a tema de error, que comunica mensaje de error
}

/// Configurar el WIFI y MQTT
void setupWifi(){

  WiFi.mode(WIFI_AP_STA);
  /// Standalone Mode, configurar punto de acceso (WiFI) y el servidor MQTT
  /// para el modo independiente
  if (standalone){
    startInternetClient();
    // We start by connecting to a WiFi network or create the AP
    Serial.println("[LOCAL] Creando Wifi Independiente y Servidor MQTT...");
    startWiFiAP();
  }
  /// Coordinator Mode, conectarse al wifi del coordinador y conectarse al servidor MQTT del coordinador.
  else{
    Serial.println("[LOCAL] Conectandose al Wifi del Coordinador y al servidor MQTT...");
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
    // setState();
    // publishState();

  });
}

//================================================ setup
//================================================
void setup() {
  /// Memory
  EEPROM.begin(2048);
  /// Logs
  Serial.begin(115200);
  /// Dht Begin
  ///// Bluetooth module baudrate 
  // btSerial.begin(9600);     

  /// Configuration mode light output
  pinMode(CONFIGURATION_MODE_OUTPUT, OUTPUT);
  pinMode(LIGHT, OUTPUT);
  pinMode(COMPRESOR, OUTPUT);
  
  delay(5000);
  getMemoryData();
  /// Setup WiFi
  setupWifi();
  setInformation();
  publishInformation();
  cloudClient.setBufferSize(2048);

  // Se apaga la luz en primer lugar
  digitalWrite(LIGHT, LOW); 
  // Se apaga el compresor en primer lugar
  digitalWrite(COMPRESOR, LOW); 
  
  dht.begin();
  delay(1000);
}

//================================================ loop
//================================================
void loop() {
  

  
  // Mantener activo el cliente MQTT (Modo Independietne)
  localClient.loop();
  if (!cloudClient.connected()) reconnectCloud();
  cloudClient.loop();
  // Leer temperature

  if (!configurationMode) {
    
    //Se obtienen los datos de la temperatura
    readTemperature(); 
    // Controlo el compresor
    controlCompresor();


    unsigned long currentMillis = millis();
    bool canSendTemperatureNotification = currentMillis - previousTemperatureNoticationMillis >= interval;
    // Serial.println("[TIEMPO] Current millis: " + String(currentMillis));
    // Serial.println("[TIEMPO] Ultima notificacion de temperatura en millis: " + String(previousTemperatureNoticationMillis));

    if (temperature > maxTemperature){

      digitalWrite(COMPRESOR, HIGH); //Prender compresor
      compressor = true;
      notifyState = true;

      Serial.println("[TEMPERATURA] Temperatura máxima alcanzada");
      if (canSendTemperatureNotification){
        previousTemperatureNoticationMillis = currentMillis;
        Serial.println("[NOTIFICACION] Notificando temperatura máxima alcanzada");
        sendNotification("Se ha alcanzado la temperatura máxima.");
      }else {
        // Serial.println("[NOTIFICACION] No se puede notificar al usuario aun");

      }
      
    }else if (temperature < minTemperature){
      
      digitalWrite(COMPRESOR, LOW); //Apagar compresor
      compressor = false;
      notifyState = true;
      Serial.println("[TEMPERATURA] Temperatura mínima alcanzada");
      if (canSendTemperatureNotification){
        previousTemperatureNoticationMillis = currentMillis;
        Serial.println("[NOTIFICACION] Notificando temperatura minima alcanzada");
        sendNotification("Se ha alcanzado la temperatura mínima.");
      }{
        Serial.println("[NOTIFICACION] No se puede notificar al usuario aun");

      }

    }
    // Publish info
    // setInformation();
    // publishInformation();

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


    // if (notifyError){
    //   Serial.println("Notificando error");
    //   setError();
    //   publishError();

    //   notifyError = false;
    // }
    
    if (!standalone){
      localClient.loop();
    }

  }
  else {
    
    // readDataFromBluetooth();
    if (!configurationModeLightOn){
      Serial.println("[SETUP] Encendiendo luces de modo de configuración...");
      
      digitalWrite(CONFIGURATION_MODE_OUTPUT, HIGH);
      Serial.println("[SETUP] Modo de configuración activado");

      configurationModeLightOn = true;
    }

    // Publish info
    if (notifyInformation){
      Serial.println("Notificando informacion");
      delay(500);
      setInformation();
      publishInformation();

      notifyInformation = false;
    }

  //  if (notifyError){
  //     Serial.println("Notificando error");
  //     setError();
  //     publishError();

  //     notifyError = false;
  //   }
  }


  
  delay(1000);

}

//===================================== al recibir una accion
//=====================================

/// Funcion llamada cada vez que se recibe una publicacion en el tópico 'actions/{id}'
/// la funcion descubre cual accion es la requerida usando el json, y le pasa los parametros 
/// a traves del propio json
void onAction(JsonObject json){
  String action = json["action"];
  Serial.println("[ACTION] " + action);
  if(action == "null") return;

  if (configurationMode){
    
    if (action.equals("configureDevice")){
      Serial.println("Configurando dispositivo");
      String name = json["name"];
      int _temperatureDeseada = json["desiredTemperature"];
      // int temperatureDeseada = 6;
      int maxTemperature = json["maxTemperature"];
      int minTemperature = json["minTemperature"];
      String _ssid = json["ssid"];
      String _password = json["password"];
      bool _standalone = json["standalone"];
      String _ssidCoordinator = json["ssidCoordinator"];
      String _passwordCoordinator = json["passwordCoordinator"];
      String _id = json["id"];
      String _userId = json["userId"];
      
      configureDevice(_id, _userId, name, _ssid, _password, 
      _ssidCoordinator, _passwordCoordinator, _standalone, maxTemperature, minTemperature, _temperatureDeseada);
      // TODO:use json["temperatureDeseada"]
    }
    
    return;
  } 

  if(action.equals("setDesiredTemperature")){
    int newDesiredTemperature = json["temperature"];
    Serial.println("Indicarle a la nevera seleccionada que cambie su temperatura");
    setTemperature(newDesiredTemperature);
  }

  if(action.equals("factoryRestore")){
    Serial.println("Restaurar de fabrica la nevera");
    factoryRestore();
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

}


//===================================== acciones
//=====================================
        
/// Lectura de temperatura a través del sensor.
void readTemperature(){
  float temperatureRead = dht.readTemperature();

  if (int(temperatureRead) != temperature){
    temperature = int(temperatureRead);
    // temperature = 10;
    Serial.println("[NOTIFY STATE] True");
    notifyState = true;
  }

  unsigned long currentMillis = millis();
  bool canPushTemperature = currentMillis - previousTemperaturePushMillis >= updateTempInterval;
  // Serial.println("[TIEMPO] Ultima publicacion de temperatura en millis: " + String(previousTemperaturePushMillis));

  if (canPushTemperature){
    return;
    previousTemperaturePushMillis = currentMillis;
    pushTemperature(temperatureRead);
  }

}

/// Turn on/off the light
void toggleLight(){
  light = !light;

  if(light){
    digitalWrite(LIGHT, HIGH); // envia señal alta al relay
    Serial.println("Enciende la luz");
  }
  else{
    digitalWrite(LIGHT, LOW); // envia señal alta al relay
    Serial.println("Apaga la luz");
  }
  
  notifyState = true;
}

/// Set max temperature
void setMaxTemperature(float newMaxTemperature){

  if((newMaxTemperature > -22 || newMaxTemperature < 17) && newMaxTemperature > minTemperature){ //Grados Centigrados
    maxTemperature = newMaxTemperature;
  notifyState = true;
  setMemoryData();
  
  }else{
    sendError("Limite de temperatura maxima inválida");
  }
  
}

/// Set min temperature
void setMinTemperature(int newMinTemperature){
  if((newMinTemperature > -22 || newMinTemperature < 17) && newMinTemperature < maxTemperature){ //Grados Centigrados
    
    minTemperature = newMinTemperature;
    notifyState = true;
    setMemoryData();
  
  }else{
    sendError("Limite de temperatura minima inválida");
  }
  

}

/// Cambiar nombre
void changeName(String newName){
  name = newName;
  notifyState = true;
  setMemoryData();
}

/// Enviar error
void sendError(String newError){
  // error = newError;
  notifyError = true;
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
  String _id,
  String _userId,
  String _name,
  String _ssid, 
  String _password, 
  String _coordinatorSsid, 
  String _coordinatorPassword, 
  bool _standalone,
  int _maxTemperature,
  int _minTemperature,
  int newTemperatureDeseada
  ){
  
  id = _id;
  userId = _userId;
  configurationMode = false;
  temperaturaDeseada = newTemperatureDeseada;
  digitalWrite(CONFIGURATION_MODE_OUTPUT, LOW);
  configurationModeLightOn = false;
  name = _name;
  minTemperature = minTemperature;
  maxTemperature = maxTemperature;
  standalone = _standalone;
  ssid = _ssid;
  ssidCoordinator = _coordinatorSsid;
  passwordCoordinator = _coordinatorPassword;
  setMemoryData();
  ESP.restart();
  
  // changeName(name);
  // setMaxTemperature(maxTemperature);
  // setMinTemperature(minTemperature);
  // setTemperature(newTemperatureDeseada);
  setStandaloneMode(ssid);
  if (standalone){
    // setCoordinatorMode(coordinatorSsid, coordinatorPassword);
  }

}

/// Notificar al usuario
void sendNotification(String message)
{ 
  return;
  DynamicJsonDocument payload(512);
  payload["id"] = id;
  payload["user"] = userId;
  payload["type"] = 0;
  String tokenEncoded = jsonToString(payload);

  ArduinoJWT jwt = ArduinoJWT(KEY);
  String token = jwt.encodeJWT(tokenEncoded);
  Serial.println("[NOTIFICATION] Enviando notificacion al usuario");
  if (standalone){
    HTTPClient http;
    http.begin(espClient, API_HOST + "/api/fridges/alert");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + token);
    
    String body = "";
    StaticJsonDocument<300> jsonDoc;
    jsonDoc["message"] = message;
    serializeJson(jsonDoc, body);
    
    int httpCode = http.POST(body);
    Serial.println("[NOTIFICACION] Estatus code de la respuesta a la notificacion: " + String(httpCode));
    // processResponse(httpCode, http);
    http.end();
  }

  // payload.clear();

   
}

/// Publicar temperatura
void pushTemperature(float temp)
{ 
  DynamicJsonDocument payload(512);
  payload["id"] = id;
  payload["user"] = userId;
  payload["type"] = 0;
  String tokenEncoded = jsonToString(payload);

  ArduinoJWT jwt = ArduinoJWT(KEY);
  String token = jwt.encodeJWT(tokenEncoded);
  Serial.println("[TEMPERATURA] Publicando temperatura " + String(temp));

  if (standalone){
    HTTPClient http;
    http.begin(espClient, API_HOST + "/api/fridges/push");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + token);
    
    String body = "";
    StaticJsonDocument<300> jsonDoc;
    jsonDoc["temp"] = temp;
    serializeJson(jsonDoc, body);
    
    int httpCode = http.POST(body);
    Serial.println("[TEMPERATURA] Estatus code de la respuesta: " + String(httpCode));
    String payload = http.getString();
    Serial.println(payload);
    http.end();

    // processResponse(httpCode, http);
  }


  // payload.clear();
   
}

/// Reiniciar valores de fabrica
void factoryRestore(){
	for (int i = 0; i < EEPROM.length(); i++) { 
	  EEPROM.write(i, 0); 
	}
  Serial.println("Controlador reiniciado de fabrica");
  Serial.println("Reiniciando...");
  ESP.restart();
}

//Control del compresor
void controlCompresor(){
  if (temperature > temperaturaDeseada){
      if((millis() - tiempoAnterior >= tiempoEspera)){
        digitalWrite(COMPRESOR, HIGH); //Prender compresor
        compressor = true;
        notifyState = true;
        compresorFlag = true;
      }
    }
    if (temperature < temperaturaDeseada){
      if(compresorFlag){
        compresorFlag = false;
        tiempoAnterior = millis();
      }
      digitalWrite(COMPRESOR, LOW); //Apagar compresor
      compressor = false;
      notifyState = true;
    }
}

/// Set temperatura deseada
void setTemperature(int newTemperaturaDeseada){

  temperaturaDeseada = newTemperaturaDeseada;
  notifyState = true;
  Serial.println("Temperatura deseada cambiada");
  setMemoryData();
}