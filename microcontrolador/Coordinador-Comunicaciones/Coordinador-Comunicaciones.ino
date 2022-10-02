#include <ESP8266WiFi.h>
#include "uMQTTBroker.h"
#include "StreamUtils.h"
#include <ArduinoJson.h>
#include "EspMQTTClient.h"
#include <ArduinoJWT.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
// Electronic
#define CONFIGURATION_MODE_OUTPUT D5
#define KEY "secretphrase"

// #define ledpin D2 //defining the OUTPUT pin for LED
// #define dataDQ D5 // temperature

String API_HOST = "https://zona-refri-api.herokuapp.com";
//========================================== datos del coordinador
//==========================================
/// Identifier
String id = "ZONA-REFRI";
/// Owner user id
String userId = "";
/// Name
String name = "";
//========================================== Wifi y MQTT
//==========================================
char path[] = "/";
char host[] = "192.168.0.1";
/// Wifi Name
String ssid = id; // Nombre del wifi en modo standalone
/// Wifi Password
String password = "12345678"; // wifi password
/// Internet Wifi
String ssidInternet = "";
/// Internet Wifi Password
String passwordInternet = "";
// MQTT
const char *mqtt_cloud_server = "b18bfec2abdc420f99565f02ebd1fa05.s2.eu.hivemq.cloud"; // replace with your broker url
// b18bfec2abdc420f99565f02ebd1fa05.s2.eu.hivemq.cloud
const char *mqtt_cloud_username = "testUser2";
const char *mqtt_cloud_password = "testUser2";
const int mqtt_cloud_port = 8883;
//========================================== Variables importantes
//==========================================
// Notify information: Publish when a new user is connected
bool notifyInformation = false;
bool notifyState = false;
bool notifyError = false;
/// Configuration mode
bool configurationMode = true;
bool configurationModeLightOn = false;
//========================================== json
//==========================================
const size_t capacity = 1024;
const size_t state_capacity = 360;
DynamicJsonDocument state(state_capacity);       // State, sensors, outputs...
DynamicJsonDocument information(state_capacity); // Information, name, id, ssid...
DynamicJsonDocument memoryJson(capacity);        // State, sensors, outputs...
DynamicJsonDocument error(128);                  // State, sensors, outputs...

// Convert to json
JsonObject toJson(String str)
{
  // Serial.println(str);
  DynamicJsonDocument docInput(1024);
  JsonObject json;
  deserializeJson(docInput, str);
  json = docInput.as<JsonObject>();
  return json;
}

/// Returns the JSON converted to String
String jsonToString(DynamicJsonDocument json)
{
  String buf;
  serializeJson(json, buf);

  return buf;
}

//========================================== timers
//==========================================
/// 1000 millisPerSecond * 60 secondPerMinutes * 30 minutes
const long interval = 1000 * 60 * 20;
unsigned long previousTemperatureNoticationMillis = 0;

const long notifyInformationInterval = 2500;
unsigned long previousNotifyInformation = 0;

const long retryWifiConnectionInterval = 180000;
unsigned long previousRetryWifiConnection = 0;

/// Initialize or update the JSON Info of the MQTT Connection (Standalone)
void setInformation()
{
  information["id"] = id;
  information["ssid"] = ssid;
  information["name"] = name;
  information["standalone"] = false;
  information["configurationMode"] = configurationMode;
  state["isConnectedToWifi"] = WiFi.status() == WL_CONNECTED;
}

//========================================== memoria
//==========================================

/// Guardar datos en memoria
void setMemoryData()
{
  memoryJson["id"] = id;
  memoryJson["name"] = name;
  memoryJson["ssid"] = ssid;
  memoryJson["password"] = password;
  memoryJson["ssidInternet"] = ssidInternet;
  memoryJson["passwordInternet"] = passwordInternet;
  memoryJson["configurationMode"] = configurationMode;
  memoryJson["userId"] = userId;

  EepromStream eepromStream(0, 1024);
  serializeJson(memoryJson, eepromStream);
  EEPROM.commit();
}

/// Obtener datos de memoria
void getMemoryData()
{
  DynamicJsonDocument doc(1024);
  JsonObject json;
  // deserializeJson(docInput, str);
  EepromStream eepromStream(0, 1024);
  deserializeJson(doc, eepromStream);
  json = doc.as<JsonObject>();

  /// Getting the memory data if the configuration mode is false
  Serial.print("\n[MEMORIA] Modo configuracion: ");
  Serial.print(String(json["configurationMode"]));
  Serial.print(String(json));
  String _userId = String(json["userId"]);
  Serial.print("\n[MEMORIA] UserID: ");
  Serial.print(_userId);

  userId = String(json["userId"]);
  configurationMode = String(json["configurationMode"]) == "null" || bool(json["configurationMode"]) == true;
  name = String(json["name"]);

  if (String(json["configurationMode"]) == "null" || bool(json["configurationMode"]) == true)
  {
    // if (false){
    Serial.println("\n[MEMORIA] Activando modo de configuracion");
    configurationMode = true;
    if (configurationMode && !_userId.equals("") && !_userId.equals("null"))
    {
      Serial.println("\n[MEMORIA] Hay un usuario dueño pero sigo en modo configuracion");

      Serial.println("\n[MEMORIA] Obteniendo datos del wifi con internet");
      ssidInternet = String(json["ssidInternet"]);
      passwordInternet = String(json["passwordInternet"]);

      Serial.println("\n[MEMORIA] Solicitando id");
      WiFi.mode(WIFI_AP_STA);
      startInternetClient();
      if (crearCoordinador(_userId))
      {
        configurationMode = false;
        ssid = String(json["ssid"]);
        password = String(json["password"]);
      }
      else
      {
        ESP.reset();
      }
    }
    else
    {
      Serial.println("\n[MEMORIA] No hay usuario dueño");
      configurationMode = true;
    }
  }
  else
  {
    Serial.println("\n[MEMORIA] Desactivando modo de configuracion");
    configurationMode = false;

    Serial.println("[MEMORIA] Obteniendo datos en memoria");
    Serial.print("Id del coordinador: ");
    Serial.println(String(json["id"]));

    id = String(json["id"]);
    ssid = String(json["ssid"]);
    password = String(json["password"]);
    Serial.print("password: ");
    Serial.println(password);
    ssidInternet = String(json["ssidInternet"]);
    passwordInternet = String(json["passwordInternet"]);
  }

  if (!configurationMode)
  {

    Serial.print("\n[MEMORIA] Configurando Wifi y guardando datos");
    setupWifi();
    setMemoryData();
  }
}

/// Cliente ESP
WiFiClientSecure espClient;
/// Cliente MQTT en la nube
PubSubClient cloudClient(espClient);
// EspMQTTClient internetClient(
//     mqtt_cloud_server,
//     8883,
//     "testUser2",
//     "testUser2",
//     "idCoordinador");

/// MQTT Broker ///
class CoordinatorMQTTBroker : public uMQTTBroker
{
public:
  virtual bool onConnect(IPAddress addr, uint16_t client_count)
  {
    Serial.print(addr.toString());
    Serial.println(" connected");
    return true;
  }

  virtual void onDisconnect(IPAddress addr, String client_id)
  {
    Serial.println(addr.toString() + " (" + client_id + ") disconnected");
  }

  virtual bool onAuth(String username, String password, String client_id)
  {
    Serial.println("Username/Password/ClientId: " + username + "/" + password + "/" + client_id);
    // TODO: Verificar que el token es legitimo

    cloudClient.subscribe(String("action/" + client_id).c_str());

    // while (!cloudClient.subscribe(String("action/" + client_id).c_str()))
    // {
    //   boolean result = cloudClient.subscribe(String("action/" + client_id).c_str());
    //   Serial.print("Subscribe result " + client_id + " : ");
    //   Serial.println(result);
    // }
    // boolean result2 = cloudClient.subscribe(String("action/#").c_str());
    // Serial.print("Subscribe result: ");
    // Serial.println(result2);
    // cloudClient.setCallback(cloud_callback);
    // reconnectCloud();

    // TODO: Verificar que el dueño sea el mismo que se configuro en
    // el modo configuracion, si es que no estan en modo configuracion
    notifyInformation = true;
    return true;
  }

  virtual void onData(String topic, const char *data, uint32_t length)
  {
    char data_str[length + 1];
    os_memcpy(data_str, data, length);
    data_str[length] = '\0';
    // Serial.println("received topic '" + topic + "' with data '" + (String)data_str + "'");

    // Convert to JSON.
    DynamicJsonDocument docInput(512);
    JsonObject json;
    deserializeJson(docInput, (String)data_str);
    json = docInput.as<JsonObject>();

    if (topic.startsWith("action"))
    {
      Serial.println("Action: " + String(json["action"]));
      // onAction(json);
      // EJECUTAR LAS ACCIONES
      json.clear();
    }

    if (topic.startsWith("state"))
    {
      if (cloudClient.connected())
      {
        Serial.print("[PUBLISH][CLOUD] Publicando estado: " + String(data_str));
        if (cloudClient.publish(topic.c_str(), String(data_str).c_str(), false))
        {
          Serial.print("...Publicado con exito\n");
          // Serial.println(topic);
        }
        // cloudClient.publish(String("state/62f90f52d8f2c401b58817e3").c_str(), stateEncoded2.c_str(), true);
      }
    }

    if (topic.startsWith("temp/"))
    {
      String deviceId = topic.substring(5);
      Serial.print("[TEMP] Publicando temperatura del id: ");
      Serial.print(deviceId);
      Serial.print(" Temp: ");
      Serial.print(String(data_str));
      Serial.print("\n");

      pushTemperature(String(data_str).toFloat(), deviceId);
    }

    if (topic.startsWith("notification/"))
    {
      String deviceId = topic.substring(13);
      Serial.print("[NOTIFICATION] Publicando notificacion del id: ");
      Serial.print(deviceId);
      sendNotification((String)data_str, deviceId);
    }

    // printClients();
  }

  // Sample for the usage of the client info methods
  virtual void printClients()
  {
    for (int i = 0; i < getClientCount(); i++)
    {
      IPAddress addr;
      String client_id;

      getClientAddr(i, addr);
      getClientId(i, client_id);
      Serial.println("Client " + client_id + " on addr: " + addr.toString());
    }
  }
};

/// MQTT Servidor del coordinador
CoordinatorMQTTBroker myBroker;

/// MQTT Cliente para Mode Coordinado
// EspMQTTClient localClient(
//     "192.168.0.1",
//     1883,
//     "MQTTUsername",
//     "MQTTPassword",
//     "id");
void publishInformation()
{
  // Serial.print("[PUBLISH][INFORMATION] Publicano informacion");
  String informationEncoded = jsonToString(information);
  // Serial.println(informationEncoded);
  myBroker.publish("information", informationEncoded);
}

void onAction(JsonObject json)
{
  String action = json[String("action")];
  if (configurationMode)
  {

    if (action.equals("configureCoordinator"))
    {
      Serial.println("Configurando dispositivo");
      String _userId = json["userId"];
      String name = json["name"];
      String _ssid = json["ssid"];
      String _password = json["password"];
      String _ssidInternet = json["ssidInternet"];
      String _passwordInternet = json["passwordInternet"];
      Serial.print("[CONFIGURATION] SSID Internet ");
      Serial.print(_ssidInternet);
      Serial.print(" Password Internet ");
      Serial.print(_passwordInternet);

      configureDevice(_userId, name, _ssid, _password, _ssidInternet, _passwordInternet);
    }

    return;
  }
}

//============================= Setup WiFi
/// Configurar el WIFI y MQTT
void setupWifi()
{

  WiFi.mode(WIFI_AP_STA);
  /// Standalone Mode, configurar punto de acceso (WiFI) y el servidor MQTT
  /// para el modo independiente
  // We start by connecting to a WiFi network or create the AP
  startWiFiAP();
  /// Primero se crea el AP y despues el del Internet, por que el de internet puede tardar mas
  /// Tambien por que es propenso a fallos como: WiFi no encontrado, señal baja, contraseña in correcta, etc...
  startInternetClient();
}

/// Creación del punto de acceso (WiFI)
void startWiFiAP()
{
  Serial.println("[LOCAL] Creando Wifi Independiente y Servidor MQTT...");

  /// Modo Punto de Acceso.
  IPAddress apIP(192, 168, 0, 1); // Static IP for wifi gateway
  /// Inicializo Gateway, Ip y Mask
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); // set Static IP gateway on NodeMCU
  /// Nombre y clave del wifi
  WiFi.softAP(ssid, password);
  Serial.print("[WIFI AP] Iniciando punto de acceso: " + ssid);
  Serial.print(". IP address: " + WiFi.softAPIP().toString());

  // Start the broker
  Serial.println("\n[MQTT LAN] Iniciando MQTT Broker...");
  // Inicializo el servidor MQTT
  myBroker.init();
  // Suscripcion a los topicos de interes
  myBroker.subscribe("state/#");
  myBroker.subscribe("notification/#");
  myBroker.subscribe("temp/#");
  /// TODO: suscribirse a tema de error, que comunica mensaje de error
}

/// Conexion al Wifi con internet
void startInternetClient()
{
  /// No hacer return si esta en modo configuracion, quitaria el internet.

  Serial.print("[WIFI INTERNET] Wifi con Internet: ");
  Serial.print(ssidInternet);
  Serial.print("\n");

  /// Conectarse al Wifi con Internet
  if (!ssidInternet.equals("") && !ssidInternet.equals("null"))
  {
    Serial.print("[WIFI INTERNET] Conectandose al Wifi con Internet: ");
    Serial.print(ssidInternet);
    // Configures static IP address
    // Set your Static IP address
    IPAddress local_IP(192, 168, 1, 200);
    // Set your Gateway IP address
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress primaryDNS(8, 8, 8, 8);   // optional
    IPAddress secondaryDNS(8, 8, 4, 4); // optional
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
    {
      Serial.println("[WIFI INTERNET] STA Fallo en la configuracion");
    }
    WiFi.begin(ssidInternet, passwordInternet);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries <= 45)
    {
      // shouldPublish();
      tries = tries + 1;
      // delay(100);
      Serial.print(".");
      delay(100);
    }

    Serial.print("\n[WIFI INTERNET] Salida del loop de conectarse a wifi con internet ");

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.print("\n[WIFI INTERNET] Conectado. IP address: ");
      Serial.println(WiFi.localIP());

      if (configurationMode)
        return;

      espClient.setInsecure();
      cloudClient.setServer(mqtt_cloud_server, mqtt_cloud_port);
      cloudClient.setCallback(cloud_callback);
      // cloudClient.
      reconnectCloud();
    }
    else
    {
      Serial.print("\n[WIFI INTERNET] NO conectado. ");
    }
  }
}

//===================================== reconnect cloud
/// Reconnect cloud connection
void reconnectCloud()
{
  // if (userLocalConnected) return;

  if (configurationMode)
    return;

  if (WiFi.status() != WL_CONNECTED)
    return;

  int tries = 0;
  // Loop until we're reconnected

  cloudClient.setServer(mqtt_cloud_server, mqtt_cloud_port);
  cloudClient.setCallback(cloud_callback);

  while (!cloudClient.connected() && tries <= 20)
  {

    Serial.print("[INTERNET] Intentando conexion MQTT...");
    Serial.print(id);
    // Attempt to connect
    if (cloudClient.connect(id.c_str(), mqtt_cloud_username, mqtt_cloud_password))
    {

      Serial.println(" conectado");

      // Subscribe to topics
      // boolean result = cloudClient.subscribe(("action/" + String("631cc81b7cdd106307fd5ffe")).c_str());
      // Serial.print("Subscribe result: ");
      // Serial.println(result);
      // cloudClient.subscribe(("state/" + id).c_str());
    }
    else
    {
      Serial.print(" failed, rc=");
      Serial.print(cloudClient.state());
      Serial.println(" try again in 5 seconds"); // Wait 5 seconds before retrying
      tries += 1;
      shouldPublish();

      // delay(500);
    }
  }
}

void cloud_callback(char *topic, byte *payload, unsigned int length)
{
  String incommingMessage = "";
  for (int i = 0; i < length; i++)
    incommingMessage += (char)payload[i];

  Serial.println("[INTERNET][");
  Serial.print(String(topic));
  Serial.print("] Mensaje recibido> ");
  Serial.print(incommingMessage);
  Serial.print("\n");
  // Convert to JSON.
  DynamicJsonDocument docInput(1024);
  JsonObject json;
  deserializeJson(docInput, incommingMessage);
  json = docInput.as<JsonObject>();

  myBroker.publish(String(topic), incommingMessage);

  // if (topic == "state/" + id){
  //   Serial.println("Temperature: " + String(json["temperature"]));

  char topicBuf[50];
  id.toCharArray(topicBuf, 50);
  if (String(topic).startsWith("action"))
  {
    Serial.println("[INTERNET][ACCION] Accion recibida: " + String(json["action"]));
    // onAction(json);
  }

  // publishStateCloud();
}
//============================= publish
//=============================
void shouldPublish()
{
  // Publish info
  boolean canPublish = millis() - previousNotifyInformation >= notifyInformationInterval;
  if (canPublish)
  {
    // Serial.println("[PUBLISH INFORMATION]");
    previousNotifyInformation = millis();
    setInformation();
    publishInformation();
  }
}

/// Publish state to cloiud server
void publishStateCloud(String id, String message)
{
  // if (cloudClient.connected()){
  //   Serial.print("[PUBLISH][CLOUD] Publicando estado: ");
  //   Serial.println(message);
  //   if (cloudClient.publish((("state/"+id)).c_str(), message.c_str(), true)){
  //     Serial.print("[INTERNET] Estado publicado en: ");
  //     Serial.println(String(("state/"+id)).c_str());

  //   }
  //     // cloudClient.publish(String("state/62f90f52d8f2c401b58817e3").c_str(), stateEncoded2.c_str(), true);
  // }
}

/// Notificar al usuario
void sendNotification(String message, String deviceId)
{
  DynamicJsonDocument payload(512);
  payload["id"] = deviceId;
  payload["user"] = userId;
  payload["type"] = 0;
  String tokenEncoded = jsonToString(payload);

  ArduinoJWT jwt = ArduinoJWT(KEY);
  String token = jwt.encodeJWT(tokenEncoded);
  Serial.println("[NOTIFICATION] Enviando notificacion al usuario");

  HTTPClient http;
  http.begin(espClient, API_HOST + "/api/fridges/alert");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + token);

  String body = "";
  StaticJsonDocument<300> jsonDoc;
  jsonDoc["message"] = message;
  serializeJson(jsonDoc, body);

  // yield();
  int httpCode = http.POST(body);

  Serial.println("[NOTIFICACION] Estatus code de la respuesta a la notificacion: ");
  Serial.print(String(httpCode));
  http.end();
  // processResponse(httpCode, http);

  // yield();
}

/// Publicar temperatura
void pushTemperature(float temp, String deviceId)
{
  DynamicJsonDocument payload(512);
  payload["id"] = deviceId;
  payload["user"] = userId;
  payload["type"] = 0;
  String tokenEncoded = jsonToString(payload);

  ArduinoJWT jwt = ArduinoJWT(KEY);
  String token = jwt.encodeJWT(tokenEncoded);
  Serial.print("[TEMPERATURA] Publicando temperatura ");
  Serial.print(String(temp));
  Serial.print(" id: ");
  Serial.print(deviceId);
  Serial.print("\n");
  Serial.print("UserId: ");
  Serial.print(userId);
  Serial.print("\n");

  espClient.setInsecure();
  HTTPClient http;
  http.begin(espClient, API_HOST + "/api/fridges/push");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + token);
  http.addHeader("Host", "<calculated when request is sent>");
  http.addHeader("Content-Length", "<calculated when request is sent>");

  String body = "";
  StaticJsonDocument<300> jsonDoc;
  jsonDoc["temp"] = temp;
  serializeJson(jsonDoc, body);

  // yield();
  int httpCode = http.POST(body);
  Serial.print("[TEMPERATURA] Estatus code de la respuesta: ");
  Serial.println(String(httpCode));
  String payloadResponse = http.getString();
  Serial.println(payloadResponse);
  http.end();

  // processResponse(httpCode, http);

  // yield();

  // payload.clear();
}

void setup()
{
  /// Memory
  EEPROM.begin(1024);
  /// Init Debug Tools
  Serial.begin(115200); // serial start
  /// Configuration mode light output
  while (!Serial)
  {
  }
  espClient.setInsecure();

  pinMode(CONFIGURATION_MODE_OUTPUT, OUTPUT);

  delay(5000);
  getMemoryData();
  setupWifi();
  /// Buffer size
  cloudClient.setBufferSize(512);

  setInformation();
  publishInformation();
}

void loop()
{
  if (!configurationMode)
  {

    // Publish info
    if (notifyInformation)
    {
      // delay(1000);

      setInformation();
      publishInformation();
      notifyInformation = false;
    }

    // myBroker.printClients();
    if (!cloudClient.connected())
      reconnectCloud();
    cloudClient.loop();
    shouldPublish();
  }
  else
  {

    // if (cloudClient.connected())

    if (configurationModeLightOn)
    {
      Serial.println("[SETUP] Apagando luces de modo de configuración...");
      digitalWrite(CONFIGURATION_MODE_OUTPUT, LOW);
      Serial.println("[SETUP] Modo de configuración desactivado");
      configurationModeLightOn = false;
    }

    shouldPublish();
  }
}

/// Cambiar nombre
void changeName(String newName)
{
  name = newName;
  notifyInformation = true;
  setMemoryData();
}

/// Cambia el nombre y contraseña del Wifi
void setWifi(String newSsid, String newPpassword)
{
  ssid = newSsid;
  password = newPpassword;
  setMemoryData();
  startWiFiAP();
}

/// Configurar dispositivo cuando esta en modo configuración
// TODO(lesanpi): Falta ssid y password del wifi con internet.
void configureDevice(
    String _userId,
    String _name,
    String _ssid,
    String _password,
    String _ssidInternet,
    String _passwordInternet)
{

  digitalWrite(CONFIGURATION_MODE_OUTPUT, LOW);
  configurationModeLightOn = false;
  name = _name;
  ssid = _ssid;

  if (!_ssidInternet.equals("") && !_ssidInternet.equals("null"))
  {
    ssidInternet = _ssidInternet;
  }
  if (!_passwordInternet.equals("") && !_passwordInternet.equals("null"))
  {
    passwordInternet = _passwordInternet;
  }

  changeName(_name);
  setMemoryData();
  setWifi(_ssid, _password);
  configurationMode = false;
  setMemoryData();
  WiFi.mode(WIFI_AP_STA);
  // delay(1000);
  startInternetClient();
  crearCoordinador(_userId);
  startWiFiAP();
}

//====================================== setup
/// Crear nuevo coordinador
bool crearCoordinador(String userId)
{

  Serial.println("[CONFIGURACION] Enviando peticion de crear nevera");
  // delay(500);
  espClient.setInsecure();
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin(espClient, API_HOST + "/api/fridges");
    http.addHeader("Content-Type", "application/json");
    // http.addHeader("Content-Length", "<calculated when request is sent>");
    // http.addHeader("Host", "<calculated when request is sent>");

    String body = "";
    StaticJsonDocument<300> jsonDoc;
    jsonDoc["userId"] = userId;
    jsonDoc["type"] = 1;
    serializeJson(jsonDoc, body);

    // yield();
    Serial.print(body);
    int httpCode = http.POST(body);
    if (httpCode > 0)
    {
      Serial.print("HTTP Response code: ");
      Serial.println(httpCode);
      String payload = http.getString();
      Serial.println(payload);

      // Convert to JSON.
      if (httpCode == 200)
      {

        DynamicJsonDocument docInput(512);
        JsonObject json;
        deserializeJson(docInput, payload);
        json = docInput.as<JsonObject>();
        // TODO: Al parecer no se guarda este ID
        id = String(json["id"]);
        Serial.println("\n[CONFIG] Nuevo id: ");
        Serial.print(id);
        configurationMode = false;
        return true;
      }
    }
    else
    {
      Serial.print("Error code: ");
      Serial.println(httpCode);
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      return false;
    }

    http.end();

    // processResponse(httpCode, http);
  }
  else
  {
    Serial.println("WiFi Disconnected");
    return false;
  }
}
