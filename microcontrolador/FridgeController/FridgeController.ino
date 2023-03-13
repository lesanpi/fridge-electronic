#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESP8266Ping.h>
#include "uMQTTBroker.h"
#include "StreamUtils.h"
#include "DHT.h"
#include "EspMQTTClient.h"
#include <ArduinoJWT.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//========================================== display
//==========================================
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define NUMFLAKES 10 // Number of snowflakes in the animation example

//========================================== pins
//==========================================

#define KEY "secretphrase"
#define DHTTYPE DHT11 // DHT 11

#define FACTORYREST D0
uint8_t DHTPin = D4; /// DHT1
#define COMPRESOR D6
#define ELECTRICIDAD D7
#define BUZZER D5
#define BAJARTEMP D6
#define SUBIRTEMP D0
#define LIGHT D8
#define ANALOGPILA A0
#define ONE_WIRE_BUS D3

// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);

// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);

/// Dirección del sensor 1
DeviceAddress address1 = {0x28, 0xFF, 0xCA, 0x4A, 0x5, 0x16, 0x3, 0xBD};

// String API_HOST = "https://zona-refri-api.herokuapp.com";
String API_HOST = "https://zona-refri.onrender.com";

//========================================== jwt
//==========================================

//========================================== datos de la nevera
//==========================================
bool userLocalConnected = false;
String id = "ZONA-REFRI";
String userId = "";
String name = "";
bool light = false;            // Salida luz
bool compressor = false;       // Salida compressor
bool door = false;             // Sensor puerta abierta/cerrada
bool standalone = true;        // Quieres la nevera en modo independiente?
float temperature = 0;         // Sensor temperature
float externalTemperature = 0; // Sensor temperature
float humidity = 70;           // Sensor humidity
int maxTemperature = 20;       // Parametro temperatura minima permitida.
int minTemperature = -10;      // Parametro temperatura maxima permitida.
int temperaturaDeseada = 4;    // Parametro temperatura recibida por el usuario.
bool softApEnabled = false;
int batteryPorcentage = 100; // Battery level porcentage %
int compresorMinutesToWait = 7;
bool internetConnectionOk = true;
int serialCount = 0; // Variable para usar analogRead()
// Para almacenar el tiempo en milisegundos.
unsigned long tiempoAnterior = 0;
// 7 minutos de espera de tiempo prudencial para volver a encender el compresor.
int tiempoEspera = 1000 * 60 * 7; // 1000 * 60 * 7
// int tiempoEspera = 120000; // 1000 * 60 * 7
// Bandera que indica que el compresor fue encendido.
bool compresorFlag = false;

bool muted = false;

bool upTempFlag = false;               // Bandera de boton de subida de temperatura
bool downTempFlag = false;             // Bandera de boton de bajada de temperatura
bool restoreFactoryFlag = false;       // Bandera de boton de restoreFactory
bool restoreFactoryFlagFinish = false; // Bandera de restoreFactory para aplicar funcion cuando se deje de presionar el boton
unsigned long tiempoAnteriorTe;        //
unsigned long tiempoAnteriorRf;

unsigned long tiempo1LecturaFallaElectrica = 0;
unsigned long tiempo2LecturaFallaElectrica = 0;
unsigned long intervaloFallaElectrica = 1000 * 60 * 5;

//========================================== BUZZER
unsigned long timeAlarm1 = 0;
unsigned long timeAlarm2 = 0;
bool alarm = false;

bool stopAlarmBattery = false;
bool stopAlarmCompressor = false;

//========================================== json
//==========================================
const size_t capacity = 1024;
const size_t state_capacity = 512;

/// Returns the JSON converted to String
String jsonToString(DynamicJsonDocument json)
{
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
bool retryCoordinatorConnection = false;
// bool retryInternetConnection = false;
/// Configuration mode
bool configurationMode = true;
bool configurationModeLightOn = false;
String configurationInfoStr = "";

//========================================== Wifi y MQTT
//==========================================
char path[] = "/";
char host[] = "192.168.0.1";
// Modo Independiente
String ssid = id;             // Nombre del wifi en modo standalone
String password = "12345678"; // Clave del wifi en modo standalone
// Coordinator Wifi
String ssidCoordinator = "";     // Wifi al que se debe conectar (coordinador)
String passwordCoordinator = ""; // Clave del Wifi del coordinador
// Internet Wifi
String ssidInternet = "";
String passwordInternet = "";
// MQTT
const char *mqtt_cloud_server = "b18bfec2abdc420f99565f02ebd1fa05.s2.eu.hivemq.cloud"; // replace with your broker url
const char *mqtt_coordinator_server = "192.168.0.1:1883";                              // replace with your broker url
// b18bfec2abdc420f99565f02ebd1fa05.s2.eu.hivemq.cloud
const char *mqtt_cloud_username = "testUser2";
const char *mqtt_coordinador_username = "testUser2";
const char *mqtt_cloud_password = "testUser2";
const char *mqtt_coordinador_password = "testUser2";
const int mqtt_cloud_port = 8883;
const int mqtt_coordinator_port = 1883;

//=========================== mqtt clients
/// Cliente ESP
WiFiClientSecure espClient;
/// Cliente MQTT en la nube
PubSubClient cloudClient(espClient);
/// MQTT Cliente para Mode Coordinado
PubSubClient coordinatorClient(espClient);
EspMQTTClient localClient(
    "192.168.0.1",
    1883,
    "MQTTUsername",
    "MQTTPassword",
    "id");
//========================================== json information
//==========================================
/// Initilize or update the JSON State of the Fridge.

bool internetConnected()
{
  bool ret = Ping.ping("www.google.com", 1);
  return ret;
}

//========================================== sensors
//==========================================
DHT dht(DHTPin, DHTTYPE);
const long updateTempInterval = 1000 * 60 * 5;
unsigned long previousTemperaturePushMillis = 0;

//========================================== timers
//==========================================
/// 1000 millisPerSecond * 60 secondPerMinutes * 30 minutes
const long interval = 1000 * 60 * 20;
unsigned long previousTemperatureNoticationMillis = 0;

const long intervalBatteryNotification = 1000 * 60 * 10;
unsigned long previousBatteryNoticationMillis = 0;

const long notifyInformationInterval = 1000;
unsigned long previousNotifyInformation = 0;

const long retryWifiConnectionInterval = 1000 * 60 * 5;
unsigned long previousRetryWifiConnection = 0;

unsigned long debugMemoryTime;

const long intervalInternet = 1000 * 60 * 5;
unsigned long previousInternetRevision = 0;

//========================================== memoria
//==========================================
/// Obtener datos en memoria

void getMemoryData()
{
  // delay(5000);
  DynamicJsonDocument doc(1024);
  JsonObject json;
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
      // WiFi.mode(WIFI_STA);
      startInternetClient();
      if (crearNevera(_userId))
      {
        configurationMode = false;

        ssid = String(json["ssid"]);
        password = String(json["password"]);

        // ssidCoordinator = String(json["ssidCoordinator"]);
        // passwordCoordinator = String(json["passwordCoordinator"]);
        temperaturaDeseada = json["desiredTemperature"];
        minTemperature = json["minTemperature"];
        maxTemperature = json["maxTemperature"];
        light = bool(json["light"]);
        muted = bool(json["muted"]);
        compresorMinutesToWait = json["compresorMinutesToWait"];
        // standalone = bool(json["standalone"]) || String(json["standalone"]).equals("null");
        // WiFi.disconnect(true);
        // WiFi.softAPdisconnect(true);

        // ESP.eraseConfig();
      }
      else
      {
        userId = "";
        setMemoryData();
        ESP.restart();
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
    id = String(json["id"]);
    ssid = String(json["ssid"]);
    password = String(json["password"]);
    // ssidCoordinator = String(json["ssidCoordinator"]);
    // passwordCoordinator = String(json["passwordCoordinator"]);
    ssidInternet = String(json["ssidInternet"]);
    passwordInternet = String(json["passwordInternet"]);
    temperaturaDeseada = json["desiredTemperature"];
    minTemperature = json["minTemperature"];
    maxTemperature = json["maxTemperature"];
    compresorMinutesToWait = json["compresorMinutesToWait"];
    light = bool(json["light"]);
    muted = bool(json["muted"]);

    // standalone = bool(json["standalone"]) || String(json["standalone"]).equals("null");
  }

  if (!configurationMode)
  {

    Serial.print("\n[MEMORIA] Standalone: ");
    Serial.println(String(json["standalone"]));
    if (minTemperature == maxTemperature)
    {
      minTemperature = maxTemperature - 1;
    }

    if (String(json["standalone"]).equals("null"))
    {
      standalone = true;
    }
    localClient.setMqttClientName(id.c_str());

    setupWifi();
    setMemoryData();
  }
}

/// Guardar los datos en memoria
void setMemoryData()
{
  DynamicJsonDocument memoryJson(capacity); // State, sensors, outputs...
  memoryJson["id"] = id;
  memoryJson["compresorMinutesToWait"] = compresorMinutesToWait;
  memoryJson["userId"] = userId;
  memoryJson["name"] = name;
  memoryJson["desiredTemperature"] = temperaturaDeseada;
  memoryJson["light"] = light;
  memoryJson["muted"] = muted;
  memoryJson["maxTemperature"] = maxTemperature;
  memoryJson["minTemperature"] = minTemperature;
  // memoryJson["standalone"] = standalone;
  memoryJson["ssid"] = ssid;
  // memoryJson["ssidCoordinator"] = ssidCoordinator;
  memoryJson["ssidInternet"] = ssidInternet;
  memoryJson["password"] = password;
  // memoryJson["passwordCoordinator"] = passwordCoordinator;
  memoryJson["passwordInternet"] = passwordInternet;
  memoryJson["configurationMode"] = configurationMode;

  EepromStream eepromStream(0, 2048);
  serializeJson(memoryJson, eepromStream);
  EEPROM.commit();
  memoryJson.clear();
}

//========================================== Cliente MQTT y Cliente WiFi
//==========================================
/// MQTT Broker usado para crear el servidor MQTT en modo independiente
class FridgeMQTTBroker : public uMQTTBroker
{
public:
  virtual bool onConnect(IPAddress addr, uint16_t client_count)
  {
    Serial.println(addr.toString() + " connected");
    notifyInformation = true;

    publishInformation();
    return true;
  }

  virtual void onDisconnect(IPAddress addr, String client_id)
  {
    Serial.println("[LOCAL BROKER][DISCONNECT] " + addr.toString() + " (" + client_id + ") disconnected");
    userLocalConnected = false;
  }

  virtual bool onAuth(String username, String password, String client_id)
  {
    Serial.println("[LOCAL BROKER][AUTH] Username/Password/ClientId: " + username + "/" + password + "/" + client_id);
    Serial.println("Dueño Id: " + userId);
    // return true;
    if (configurationMode)
    {
      if (getClientCount() > 1)
      {

        Serial.println("[LOCAL BROKER][AUTH] Modo configuracion con usuario ya activo, rechazando ingreso");
        return false;
      }

      Serial.println("[LOCAL BROKER][AUTH] Modo configuracion permitiendo ingreso");

      notifyInformation = true;
      publishInformation();
      userLocalConnected = true;
      return true;
    }

    // ArduinoJWT jwt = ArduinoJWT(KEY);
    // jwt.decodeJWT(password);
    Serial.println("[LOCAL BROKER][AUTH] Verificando usuario");
    Serial.println(userId.equals(client_id));
    if (userId.equals(client_id))
    {
      Serial.println("[LOCAL BROKER][AUTH] Ingreso valido");

      notifyInformation = true;
      publishInformation();
      userLocalConnected = true;
      return true;
    }

    Serial.println("[LOCAL BROKER][AUTH] Ingreso invalido");

    return false;
  }

  virtual void onData(String topic, const char *data, uint32_t length)
  {
    char data_str[length + 1];
    os_memcpy(data_str, data, length);
    data_str[length] = '\0';
    // Serial.println("Topico recibido: '"+topic+"', con los datos: '"+(String)data_str+"'");
    Serial.println("[LOCAL BROKER][" + String(topic) + "] Mensaje recibido> " + (String)data_str + "'");

    // Convert to JSON.
    DynamicJsonDocument docInput(1024);
    JsonObject json;
    deserializeJson(docInput, (String)data_str);
    json = docInput.as<JsonObject>();

    if (topic == "action/" + id)
    {
      Serial.println("Action: " + String(json["action"]));
      if (!json.isNull())
      {
        onAction(json);
      }
      // EJECUTAR LAS ACCIONES
      // json.clear();
    }

    // publishState();

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

/// MQTT Servidor para Modo Independiente
FridgeMQTTBroker myBroker;

//===================================== funciones en MQTT
//=====================================

/// Publish state. Used to initilize the topic and when the state changes
/// publish the state in the correct topic according if the fridge is working on standole mode or not.
void publishState()
{
  DynamicJsonDocument state(state_capacity); // State, sensors, outputs...
  state["id"] = id;
  state["compresorMinutesToWait"] = compresorMinutesToWait;
  state["batteryPorcentage"] = batteryPorcentage;
  // state["userId"] = userId;
  state["name"] = name;
  state["temperature"] = temperature;
  state["externalTemperature"] = externalTemperature;
  state["light"] = light;
  state["muted"] = muted;
  state["compressor"] = compressor;
  state["door"] = door;
  state["desiredTemperature"] = temperaturaDeseada;
  state["maxTemperature"] = maxTemperature;
  state["minTemperature"] = minTemperature;
  state["standalone"] = standalone;
  state["ssid"] = ssid;
  state["ssidCoordinator"] = ssidCoordinator;
  state["ssidInternet"] = ssidInternet;
  state["isConnectedToWifi"] = WiFi.status() == WL_CONNECTED;
  // state["internet"] = internetConnected();
  state["internet"] = internetConnectionOk;
  state["battery"] = fallaElectrica();
  String stateEncoded = jsonToString(state);
  Serial.println("[PUBLISH] Publicando estado: " + stateEncoded);
  // String stateEncoded2 = jsonToString(state);

  if (standalone)
  {
    /// Publish on Standalone Mode
    if (userLocalConnected || myBroker.getClientCount() > 0)
    {
      myBroker.publish("state/" + id, stateEncoded);
      return;
    }
    if (cloudClient.connected())
    {
      if (cloudClient.publish((("state/" + id)).c_str(), stateEncoded.c_str(), true))
      {
        Serial.print("... publicado con exito\n");
        // Serial.println(String(("state/" + id)).c_str());
      }
      // cloudClient.publish(String("state/62f90f52d8f2c401b58817e3").c_str(), stateEncoded2.c_str(), true);
    }
  }
  else
  {
    // if (coordinatorClient.connected())
    // {
    //   if (coordinatorClient.publish((("state/" + id)).c_str(), stateEncoded.c_str(), false))
    //   {
    //     Serial.print("[COORDINADOR] Estado publicado en: ");
    //     Serial.println(String(("state/" + id)).c_str());
    //   }
    // }
    localClient.publish("state/" + id, stateEncoded);
  }
}

/// Publish to local server
void publishStateLocalBroker()
{
  DynamicJsonDocument state(state_capacity); // State, sensors, outputs...
  state["id"] = id;
  state["compresorMinutesToWait"] = compresorMinutesToWait;
  state["batteryPorcentage"] = batteryPorcentage;

  // state["userId"] = userId;
  state["name"] = name;
  state["temperature"] = temperature;
  state["externalTemperature"] = externalTemperature;
  state["light"] = light;
  state["muted"] = muted;
  state["compressor"] = compressor;
  state["door"] = door;
  state["desiredTemperature"] = temperaturaDeseada;
  state["maxTemperature"] = maxTemperature;
  state["minTemperature"] = minTemperature;
  state["standalone"] = standalone;
  state["ssid"] = ssid;
  state["ssidCoordinator"] = ssidCoordinator;
  state["ssidInternet"] = ssidInternet;
  state["isConnectedToWifi"] = WiFi.status() == WL_CONNECTED;
  // state["internet"] = internetConnected();
  state["internet"] = internetConnectionOk;
  state["battery"] = fallaElectrica();
  String stateEncoded = jsonToString(state);
  Serial.println("[PUBLISH][LOCAL BROKER] Publicando estado: " + stateEncoded);
  myBroker.publish("state/" + id, stateEncoded);
}

/// Publish to local server
void publishStateLocalCoordinator()
{
  DynamicJsonDocument state(state_capacity); // State, sensors, outputs...
  state["id"] = id;
  state["compresorMinutesToWait"] = compresorMinutesToWait;
  state["batteryPorcentage"] = batteryPorcentage;

  // state["userId"] = userId;
  state["name"] = name;
  state["temperature"] = temperature;
  state["externalTemperature"] = externalTemperature;
  state["light"] = light;
  state["muted"] = muted;
  state["compressor"] = compressor;
  state["door"] = door;
  state["desiredTemperature"] = temperaturaDeseada;
  state["maxTemperature"] = maxTemperature;
  state["minTemperature"] = minTemperature;
  state["standalone"] = standalone;
  state["ssid"] = ssid;
  state["ssidCoordinator"] = ssidCoordinator;
  state["ssidInternet"] = ssidInternet;
  state["isConnectedToWifi"] = WiFi.status() == WL_CONNECTED;
  // state["internet"] = internetConnected();
  state["internet"] = internetConnectionOk;

  state["battery"] = fallaElectrica();
  String stateEncoded = jsonToString(state);
  Serial.print("[PUBLISH][LOCAL COORDINATOR] Publicando estado: " + stateEncoded);
  // if (coordinatorClient.connected())
  // {
  //   if (coordinatorClient.publish((("state/" + id)).c_str(), stateEncoded.c_str(), true))
  //   {
  //     Serial.print("[LOCAL COORDINATOR] Estado publicado en: ");
  //     Serial.println(String(("state/" + id)).c_str());
  //   }
  //   // cloudClient.publish(String("state/62f90f52d8f2c401b58817e3").c_str(), stateEncoded2.c_str(), true);
  // }
  boolean result = localClient.publish("state/" + id, stateEncoded);

  if (result)
  {
    Serial.print("...estado publicando exitosamente");
    // Serial.print();
  }
  Serial.print('\n');
}

void publishStateCloud()
{
  // Serial.print("[time#0.6.3.1] time: ");
  // Serial.print(millis());
  // Serial.println();
  DynamicJsonDocument state(state_capacity); // State, sensors, outputs...
  state["id"] = id;
  state["compresorMinutesToWait"] = compresorMinutesToWait;
  state["batteryPorcentage"] = batteryPorcentage;
  // state["userId"] = userId;
  state["name"] = name;
  state["temperature"] = temperature;
  state["externalTemperature"] = externalTemperature;
  state["light"] = light;
  state["muted"] = muted;
  state["compressor"] = compressor;
  state["door"] = door;
  state["desiredTemperature"] = temperaturaDeseada;
  state["maxTemperature"] = maxTemperature;
  state["minTemperature"] = minTemperature;
  state["standalone"] = standalone;
  state["ssid"] = ssid;
  state["ssidCoordinator"] = ssidCoordinator;
  state["ssidInternet"] = ssidInternet;
  state["isConnectedToWifi"] = WiFi.status() == WL_CONNECTED;
  // state["internet"] = internetConnected();
  state["internet"] = internetConnectionOk;

  state["battery"] = fallaElectrica();
  // Serial.print("[time#0.6.4.1] time: ");
  // Serial.print(millis());
  // Serial.println();
  String stateEncoded = jsonToString(state);
  if (cloudClient.connected())
  {
    // Serial.println("[PUBLISH][CLOUD] Publicando estado: " + stateEncoded);
    if (cloudClient.publish((("state/" + id)).c_str(), stateEncoded.c_str(), true))
    {
      // Serial.print("[time#0.6.4.2] time: ");
      // Serial.print(millis());
      // Serial.println();
      // Serial.print("[INTERNET] Estado publicado en: ");
      // Serial.println(String(("state/" + id)).c_str());
    }
    // cloudClient.publish(String("state/62f90f52d8f2c401b58817e3").c_str(), stateEncoded2.c_str(), true);
  }
}

/// Publicar la información de la comunicación/conexión
void publishInformation()
{
  DynamicJsonDocument information(state_capacity); // Information, name, id, ssid...
  information["id"] = id;
  information["name"] = name;
  information["ssid"] = ssid;
  information["standalone"] = standalone;
  information["configurationMode"] = configurationMode;
  String informationEncoded = jsonToString(information);
  if (standalone)
  {
    // Publish on Standalone Mode
    // Serial.println("[LOCAL BROKER] Publicando informacion: " + informationEncoded);
    myBroker.publish("information", informationEncoded);
  }
  else
  {
    // TODO: Publish on Coordinator Mode
  }

  information.clear();
}

void publishErrorMessage(String title, String message)
{
  DynamicJsonDocument errorMessage(state_capacity);
  errorMessage["title"] = title;
  errorMessage["message"] = message;
  String errorMessageEncoded = jsonToString(errorMessage);

  if (standalone)
  {

    if (userLocalConnected || myBroker.getClientCount() > 0)
    {
      myBroker.publish("error/" + id, errorMessageEncoded);
      return;
    }
    if (cloudClient.connected())
    {
      if (cloudClient.publish((("error/" + id)).c_str(), errorMessageEncoded.c_str(), true))
      {
        Serial.print("... publicado mensaje de error\n");
      }
    }
  }
  else
  {
    // TODO: Publish on Coordinator Mode
  }
}

void publishMessage(String title, String message)
{
  DynamicJsonDocument jsonMessage(state_capacity);
  jsonMessage["title"] = title;
  jsonMessage["message"] = message;
  String messageEncoded = jsonToString(jsonMessage);

  if (standalone)
  {

    if (userLocalConnected || myBroker.getClientCount() > 0)
    {
      myBroker.publish("message/" + id, messageEncoded);
      return;
    }
    if (cloudClient.connected())
    {
      if (cloudClient.publish((("message/" + id)).c_str(), messageEncoded.c_str(), true))
      {
        Serial.print("... publicado mensaje\n");
      }
    }
  }
  else
  {
    // TODO: Publish on Coordinator Mode
  }
}

//===================================== reconnect
/// Reconnect cloud connection
void reconnectCloud()
{
  // if (userLocalConnected)
  //   return;

  if (configurationMode)
    return;

  if (WiFi.status() != WL_CONNECTED)
    return;

  if (!standalone)
    return;

  int tries = 0;
  // Loop until we're reconnected

  cloudClient.setServer(mqtt_cloud_server, mqtt_cloud_port);
  cloudClient.setCallback(cloud_callback);

  yield();
  logMemory();
  while (!cloudClient.connected() && tries <= 5)
  {

    Serial.print("[INTERNET] Intentando conexion MQTT...");
    Serial.print(id);
    // Attempt to connect
    if (cloudClient.connect(id.c_str(), mqtt_cloud_username, mqtt_cloud_password))
    {

      Serial.println(" conectado");

      // Subscribe to topics
      cloudClient.subscribe(("action/" + id).c_str());
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

//===================================== configuracion wifi
//=====================================
/// Conexion al Wifi con internet
void startInternetClient()
{
  // if (fallaElectrica() && !configurationMode)
  // {
  //   Serial.print("[WIFI INTERNET] No se puede iniciar configuracion, falla electrica");
  //   return;
  // }
  // WiFi.mode(WIFI_STA);

  /// No hacer return si esta en modo configuracion, quitaria el internet.
  Serial.print("[WIFI INTERNET] Wifi con Internet: ");
  Serial.print(ssidInternet);
  // if (WiFi.status() == WL_CONNECTED){
  //   Serial.print("\n[WIFI INTERNET] Ya conectado ");
  //   return;
  // }

  /// Conectarse al Wifi con Internet
  if (!ssidInternet.equals("") && !ssidInternet.equals("null"))
  {
    Serial.print("[WIFI INTERNET] Conectandose al Wifi con Internet: ");
    Serial.print(ssidInternet);

    WiFi.begin(ssidInternet, passwordInternet);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries <= 200)
    {
      shouldPublish();
      delay(200);
      tries = tries + 1;
      // delay(100);
      Serial.print(".");
      yield();
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
      reconnectCloud();

      // retryInternetConnection = false;
    }
    else
    {
      Serial.print("\n[WIFI INTERNET] NO conectado. ");
      // retryInternetConnection = true;
      // TODO: Verify when to use
      if (configurationMode)
      {
        factoryRestore();
      }
      startWiFiAP();
    }
  }
}

void cloud_callback(char *topic, byte *payload, unsigned int length)
{
  String incommingMessage = "";
  for (int i = 0; i < length; i++)
    incommingMessage += (char)payload[i];

  Serial.println("[INTERNET][" + String(topic) + "] Mensaje recibido> " + incommingMessage);

  // Convert to JSON.
  DynamicJsonDocument docInput(1024);
  JsonObject json;
  deserializeJson(docInput, incommingMessage);
  json = docInput.as<JsonObject>();

  // if (topic == "state/" + id){
  //   Serial.println("Temperature: " + String(json["temperature"]));

  char topicBuf[50];
  id.toCharArray(topicBuf, 50);
  if (String(topic) == ("action/" + id))
  {
    Serial.println("[INTERNET][ACCION] Accion recibida: " + String(json["action"]));
    onAction(json);
  }

  publishStateCloud();
}

/// Conexión al Wifi como cliente
bool startWiFiClient()
{
  // Serial.println(ssidCoordinator);
  /// Desconexion por si acaso hubo una conexion previa

  if (ssidCoordinator.equals("") || ssidCoordinator.equals("null"))
    return false;
  WiFi.disconnect();
  /// Conexion al coordinador
  WiFi.begin(ssidCoordinator, passwordCoordinator);
  Serial.print("Conectandome al coordinador... ");
  Serial.print(ssidCoordinator);
  Serial.print(passwordCoordinator);

  // delay(500);
  /// Contador de intentos
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    /// Esperar medio segundo entre intervalo
    // WiFi.begin(ssidCoordinator, passwordCoordinator);

    /// Reconectarme
    Serial.print(".");
    tries = tries + 1;

    ///
    if (tries > 40)
    {

      // standalone = true;
      return false;
    }
    delay(500);

    /// Reintentar conexion.
    // WiFi.begin(ssidCoordinator, passwordCoordinator);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("conectado ");
    Serial.println("IP address: " + WiFi.localIP().toString());

    Serial.println("Conectarse al Servidor MQTT...");
    localClient.setMqttServer("192.168.0.1", "MQTTUsername", "MQTTPassword", 1883);
    localClient.setOnConnectionEstablishedCallback(onConnectionEstablished);
    // localClient.connect();
    // espClient.setInsecure();
    // coordinatorClient.setServer(mqtt_coordinator_server, mqtt_coordinator_port);
    // coordinatorClient.setCallback(coordinator_callback);
    // reconnectCoordinator();
  }

  /// Conectarse al servidor MQTT del Coordinador y suscribirse a los topicos.
  // Serial.println("Conectarse al Servidor MQTT...");
  return true;
}

/// Creación del punto de acceso (WiFI)
void startWiFiAP()
{
  // WiFi.mode(WIFI_AP);
  Serial.println("[LOCAL] Creando Wifi Independiente y Servidor MQTT...");

  /// Modo Punto de Acceso.
  IPAddress apIP(192, 168, 0, 1); // Static IP for wifi gateway
  /// Inicializo Gateway, Ip y Mask
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); // set Static IP gateway on NodeMCU
  /// Nombre y clave del wifi
  WiFi.softAP(ssid, password);
  Serial.print("[WIFI AP INDEPENDIENTE] Iniciando punto de acceso: " + ssid);
  Serial.print(". IP address: " + WiFi.softAPIP().toString());
  Serial.print(". Password: " + password);
  softApEnabled = true;


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
void setupWifi()
{

  // if (configurationMode) return;

  WiFi.mode(WIFI_OFF);
  /// Standalone Mode, configurar punto de acceso (WiFI) y el servidor MQTT
  /// para el modo independiente

  /// Setup WiFi AP to configure
  if (configurationMode)
  {
    Serial.println("[LOCAL AP] Soft AP Iniciado. Modo configuracion activado.");
    startWiFiAP();
    return;
  }

  /// Electrical issue, emergency mode
  if (fallaElectrica())
  {
    // Start WiFi AP because the system have an electrical issue
    // Serial.println("[BATTERY] Iniciando WiFi AP. Modo ahorro activado.");
    // startWiFiAP();
    // if (WiFi.disconnect())
    // {

    //   cloudClient.disconnect();
    //   startWiFiAP();
    //   softApEnabled = true;
    Serial.println("[LOCAL AP] Soft AP Iniciado. Modo ahorro activado.");
    // }

    return;
  }

  if (standalone)
  {
    /// Primero se crea el AP y despues el del Internet, por que el de internet puede tardar mas
    /// Tambien por que es propenso a fallos como: WiFi no encontrado, señal baja, contraseña in correcta, etc...
    startInternetClient();
  }
  /// Coordinator Mode, conectarse al wifi del coordinador y conectarse al servidor MQTT del coordinador.
  else
  {
    Serial.println("[LOCAL] Conectandose al Wifi del Coordinador y al servidor MQTT...");
    bool connected = startWiFiClient();
    if (!connected)
    {
      Serial.println("[LOCAL] No se pudo conectase al coordinador, iniciando indepente...");
      standalone = true;
      retryCoordinatorConnection = true;
      // startWiFiAP();
      startInternetClient();
    }
    else
    {
      retryCoordinatorConnection = false;
    }
  }
}

/// Funcion llamada una vez que el cliente MQTT establece la conexión.
/// funciona para el modo independiente solamente
void onConnectionEstablished()
{
  // localClient.subscribe("state/" + id, [](const String &payload)
  //                       { Serial.println(payload); });

  localClient.subscribe("action/" + id, [](const String &payload)
                        {
                          Serial.println(payload);

                          // Convert to JSON.
                          DynamicJsonDocument docInput(1024);
                          JsonObject json;
                          deserializeJson(docInput, (String)payload);
                          json = docInput.as<JsonObject>();
                          // Ejecutar las acciones
                          onAction(json);
                          // Guardar el estado
                          publishStateLocalCoordinator(); });
}

//================================================ setup
//================================================
void setup()
{

  pinMode(COMPRESOR, OUTPUT);
  /// Apago el compresor
  digitalWrite(COMPRESOR, HIGH);
  /// Logs
  Serial.begin(115200);
  /// Memory
  EEPROM.begin(1024);

  // while (!EEPROM) {}
  while (!Serial)
  {
  }

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  else
  {
    Serial.println("[Display] OK");
  }

  display.clearDisplay();
  display.display();

  // Clear the buffer
  display.clearDisplay();
  display.display();

  displayLoading();

  // btSerial.begin(9600);
  espClient.setInsecure();

  /// Configuration mode light output
  // pinMode(CONFIGURATION_MODE_OUTPUT, OUTPUT);
  pinMode(LIGHT, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // pinMode(BAJARTEMP, INPUT);
  // pinMode(SUBIRTEMP, INPUT);
  pinMode(FACTORYREST, INPUT);
  pinMode(ELECTRICIDAD, INPUT);
  /// Apago el compresor
  digitalWrite(COMPRESOR, HIGH);

  /// Setup WiFi
  // setupWifi();
  // delay(5000);
  getMemoryData();
  setupWifi();
  publishInformation();
  cloudClient.setBufferSize(512);

  // Se apaga la luz en primer lugar
  digitalWrite(LIGHT, LOW);

  /// Dht Begin
  dht.begin();
  sensors.begin();

  debugMemoryTime = millis();
}

//================================================ loop
//================================================

void stopAlarm()
{

  if (stopAlarmBattery && stopAlarmCompressor)
  {
    noTone(BUZZER);
  }
}

void alarmState(int frequency, int interval)
{

  timeAlarm2 = millis();
  if (timeAlarm2 > timeAlarm1 + interval)
  {
    timeAlarm1 = timeAlarm2;

    if (alarm)
    {

      tone(BUZZER, frequency);
      alarm = false;
      // Serial.println("[ALARMA] SONANDO");
    }
    else
    {
      noTone(BUZZER);
      alarm = true;
      // Serial.println("[ALARMA] MUTE");
    }
  }
}

void verifyInternetConnection()
{
  boolean canPublish = millis() - previousInternetRevision >= intervalInternet;
  if (canPublish)
  {
    previousInternetRevision = millis();
    if (WiFi.status() != WL_CONNECTED)
    {
      internetConnectionOk = false;
      return;
    }
    else
    {
      internetConnectionOk = internetConnected();
    }
    Serial.print("[Internet] Connected: ");
    Serial.println(internetConnectionOk);
  }
}

void shouldPublish()
{
  // Publish info
  /// Control de botontes
  // controlBotones();
  // Serial.print("[shouldPublish] time: ");
  // Serial.print(millis());
  // Serial.println();
  // Serial.print("[time#0.6.1] time: ");
  // Serial.print(millis());
  // Serial.println();

  boolean canPublish = millis() - previousNotifyInformation >= notifyInformationInterval * 5;
  if (canPublish)
  {
    readTemperature();
    readExternalTemperature();
    // Serial.println("[PUBLISH]");
    publishInformation();
    previousNotifyInformation = millis();
    // Serial.print("[time#0.6.2] time: ");
    // Serial.print(millis());
    // Serial.println();
    if (!configurationMode)
    {

      // if (softApEnabled && myBroker.getClientCount() > 0)
      if (softApEnabled)
      {
        publishStateLocalBroker();
      }

      if (!standalone)
      {

        publishStateLocalCoordinator();
      }
      else
      {
        // Serial.print("[time#0.6.3] time: ");
        // Serial.print(millis());
        // Serial.println();
        if (softApEnabled)
          return;
        publishStateCloud();
      }

      // Serial.print("[time#0.6.4] time: ");
      // Serial.print(millis());
      // Serial.println();
    }
  }
}

unsigned long freeRam;
void logMemory()
{
  if (millis() - debugMemoryTime <= 1000)
    return;
  if (ESP.getFreeHeap() == freeRam)
    return;
  debugMemoryTime = millis();
  freeRam = ESP.getFreeHeap();
  Serial.print("[RAM] RAM: ");
  Serial.println(freeRam);
  Serial.printf("Stations connected = %d\n", WiFi.softAPgetStationNum());
}

bool fallaTemperatura()
{
  if (fallaElectrica())
  {
    return (temperature < minTemperature || temperature > maxTemperature);
  }
  return (temperature < minTemperature || temperature > maxTemperature) && !muted;
}

bool fallaBateria()
{
  return batteryPorcentage < 20 && fallaElectrica();
}

void loop()
{

  // Serial.print("[time#0] time: ");
  // Serial.print(millis());
  // Serial.println();
  logMemory();

  // Serial.print("[time#0.1] time: ");
  // Serial.print(millis());
  // Serial.println();
  if (!configurationMode)
  {

    stopAlarm();
    if (fallaBateria() && fallaTemperatura())
    {
      alarmState(880, 500);
    }
    else
    {
      if (fallaBateria())
      {
        // Serial.println("BATERIA ALARMA");
        alarmState(523, 1000);
        stopAlarmBattery = false;
      }
      else
      {
        stopAlarmBattery = true;
      }

      if (fallaTemperatura())
      {
        // Serial.println("COMPRESOR ALARMA");
        alarmState(880, 1000);
        stopAlarmCompressor = false;
      }
      else
      {
        stopAlarmCompressor = true;
      }
    }
    lightLoop();

    levelBattery();

    // Serial.print("[time#0.2] time: ");
    // Serial.print(millis());
    // Serial.println();

    wifiAPLoop();

    // Serial.print("[time#0.3] time: ");
    // Serial.print(millis());
    // Serial.println();

    // Se obtienen los datos de la temperatura
    // readTemperature();

    // Serial.print("[time#0.4] time: ");
    // Serial.print(millis());
    // Serial.println();

    // Controlo el compresor
    controlCompresor();

    // Serial.print("[time#0.5] time: ");
    // Serial.print(millis());
    // Serial.println();

    /// Control de botontes
    // controlBotones();

    // Serial.print("[time#0.6] time: ");
    // Serial.print(millis());
    // Serial.println();

    /// verificando si deberia publicar datos
    shouldPublish();

    // Serial.print("[time#0.7] time: ");
    // Serial.print(millis());
    // Serial.println();

    /// verifyInternet
    verifyInternetConnection();

    // Serial.print("[time#0.8] time: ");
    // Serial.print(millis());
    // Serial.println();

    /// verificando si deberia publicar datos
    // shouldPublish();

    // Serial.print("[time#1] time: ");
    // Serial.print(millis());
    // Serial.println();
    if (fallaElectrica())
    {
      tiempo2LecturaFallaElectrica = millis();
      if (tiempo2LecturaFallaElectrica > tiempo1LecturaFallaElectrica + intervaloFallaElectrica || tiempo1LecturaFallaElectrica < intervaloFallaElectrica)
      {
        tiempo1LecturaFallaElectrica = millis();
        if (millis() < intervaloFallaElectrica)
        {
          tiempo1LecturaFallaElectrica = intervaloFallaElectrica;
        }
        Serial.println("[ELECTRICIDAD] Falla electrica... Enviando notificacion");
        sendNotification("Ha ocurrido una falla eléctrica, se esta usando la bateria de respaldo");
        String title = "Error ocurrido en " + name;
        publishErrorMessage(title, "Ha ocurrido una falla eléctrica");
      }
    }

    // Serial.print("[time#0.9] time: ");
    // Serial.print(millis());
    // Serial.println();

    // if (retryCoordinatorConnection)
    // {
    //   boolean canReconnection = millis() - previousRetryWifiConnection >= retryWifiConnectionInterval;
    //   if (canReconnection)
    //   {
    //     previousRetryWifiConnection = millis();

    //     Serial.println("\n[LOCAL] Conectandose al Wifi del Coordinador y al servidor MQTT...");
    //     bool connected = startWiFiClient();
    //     if (!connected)
    //     {
    //       Serial.println("[LOCAL] No se pudo conectase al coordinador, iniciando indepente...");
    //       startWiFiAP();
    //       startInternetClient();
    //     }
    //   }
    // }

    // /// Reintentar conexion al WiFi
    if (WiFi.status() != WL_CONNECTED)
    {
      boolean canReconnection = millis() - previousRetryWifiConnection >= retryWifiConnectionInterval;
      if (millis() < 1000 * 60 * 2)
      {
        return;
      }

      Serial.println("[RETRY] Reintentando conexion WiFi");

      if (canReconnection)
      {
        previousRetryWifiConnection = millis();
        setupWifi();
      }
    }

    //   // userLocalConnected = WiFi.status() != WL_CONNECTED;
    // }
    // else
    // {
    //   // userLocalConnected = false;
    // }

    // if (retryCoordinatorConnection)
    // {
    //   boolean canReconnection = millis() - previousRetryWifiConnection >= retryWifiConnectionInterval;
    //   if (canReconnection)
    //   {
    //     previousRetryWifiConnection = millis();

    //     Serial.println("\n[LOCAL] Conectandose al Wifi del Coordinador y al servidor MQTT...");
    //     bool connected = startWiFiClient();
    //     if (!connected)
    //     {
    //       Serial.println("[LOCAL] No se pudo conectase al coordinador, iniciando indepente...");
    //       startWiFiAP();
    //       startInternetClient();
    //     }
    //   }
    // }

    if (!cloudClient.connected() && standalone && !fallaElectrica())
    {
      reconnectCloud();
    }
    // Serial.print("[time#3] time: ");
    // Serial.print(millis());
    // Serial.println();
    if (cloudClient.connected() && standalone && !fallaElectrica())
    {
      cloudClient.loop();
    }

    // Serial.print("[time#4] time: ");
    // Serial.print(millis());
    // Serial.println();

    // // Mantener activo el cliente MQTT (Modo Independietne)
    // if (!standalone)
    // {
    //   localClient.loop();
    // }

    /// verificando si deberia enviar push notification
    shouldPushTempNotification();
    // Serial.print("[time#5] time: ");
    // Serial.print(millis());
    // Serial.println();
  }
  else
  {

    if (!userId.equals("") && !userId.equals("null"))
    {
      Serial.println("\n[MEMORIA] Hay un usuario dueño pero sigo en modo configuracion. Reinicio!");
      ESP.restart();
      // Serial.println("\n[MEMORIA] Solicitando id");

      startInternetClient();
      yield();
      // setupWifi();
      Serial.println("[CONFIG] Se configurara el id");
      bool configurado = crearNevera(userId);

      if (configurado)
      {
        Serial.println("[CONFIG] Se termino de configurar completamente");
        configurationMode = false;
        setMemoryData();
        // WiFi.softAPdisconnect(true);
        if (WiFi.softAPdisconnect())
        {
          Serial.println("[CONFIG] Removiendo WiFi AP. Configuracion completada");
        }
      }
      else
      {
        Serial.println("[CONFIG] No se pudo configurar compeltamente");
        // ESP.restart();
      }
      // if (crearNevera(userId)){

      //   configurationMode = false;
      //   ssid = String(json["ssid"]);
      //   password = String(json["password"]);
      //   ssidCoordinator = String(json["ssidCoordinator"]);
      //   passwordCoordinator = String(json["passwordCoordinator"]);
      //   temperaturaDeseada = json["desiredTemperature"];
      //   minTemperature = json["minTemperature"];
      //   maxTemperature = json["maxTemperature"];
      //   standalone = bool(json["standalone"]) || String(json["standalone"]).equals("null");
      // }
    }

    // readDataFromBluetooth();
    // if (!configurationModeLightOn)
    // {
    //   Serial.println("[SETUP] Encendiendo luces de modo de configuración...");

    //   digitalWrite(CONFIGURATION_MODE_OUTPUT, HIGH);
    //   Serial.println("[SETUP] Modo de configuración activado");

    //   configurationModeLightOn = true;
    // }

    shouldPublish();

    if (notifyError)
    {
      Serial.println("Notificando error");

      notifyError = false;
    }
  }

  // delay(4000);

  // Serial.println("Entrando a delay");

  // Serial.println("Saliendo de delay");
}

//===================================== al recibir una accion
//=====================================

/// Funcion llamada cada vez que se recibe una publicacion en el tópico 'actions/{id}'
/// la funcion descubre cual accion es la requerida usando el json, y le pasa los parametros
/// a traves del propio json
void onAction(JsonObject json)
{
  String action = json["action"];

  if (configurationMode)
  {

    if (action.equals("configureDevice"))
    {
      Serial.println("Configurando dispositivo");
      String _id = json["id"];
      String _userId = json["userId"];
      String _name = json["name"];
      int _maxTemperature = json["maxTemperature"];
      int _temperature = json["desiredTemperature"];
      int _minTemperature = json["minTemperature"];
      int _timeToWait = json["minutes"];
      String _ssid = json["ssid"];
      String _password = json["password"];
      boolean _standalone = !boolean(json["startOnCoordinatorMode"]);
      // String _ssidCoordinator = json["ssidCoordinator"];
      // String _passwordCoordinator = json["passwordCoordinator"];
      String _ssidInternet = json["ssidInternet"];
      String _passwordInternet = json["passwordInternet"];
      Serial.print("Ids recibidos");
      Serial.println(_id);
      Serial.println(_userId);
      Serial.print("[CONFIGURATION] SSID Internet ");
      Serial.print(_ssidInternet);
      Serial.print(" Password Internet ");
      Serial.print(_passwordInternet);
      Serial.print(" Modo independiente: ");
      Serial.print(_standalone);

      configureDevice(_id, _userId, _name, _ssid, _password, _ssidInternet, _passwordInternet, _standalone, _temperature, _maxTemperature, _minTemperature, _timeToWait);
    }

    return;
  }

  if (action.equals("setDesiredTemperature"))
  {
    int newDesiredTemperature = json["temperature"];
    Serial.println("Indicarle a la nevera seleccionada que cambie su temperatura");
    setTemperature(newDesiredTemperature);
  }

  if (action.equals("factoryRestore"))
  {
    Serial.println("Restaurar de fabrica la nevera");
    factoryRestore();
  }

  if (action.equals("changeName"))
  {
    Serial.println("Cambiar el nombre a la nevera");
    String _newName = json["name"];
    changeName(_newName);
  }

  if (action.equals("changeMinutesToWait"))
  {
    Serial.println("Cambiar los minutos de espera del compresor");
    int _newMinutes = json["minutes"];
    changeMinutesToWait(_newMinutes);
  }

  if (action.equals("muteAlerts"))
  {
    Serial.println("Indicarle a la nevera seleccionada que mutee/desmutee las alertas");
    muteAlerts();
  }

  if (action.equals("toggleLight"))
  {
    Serial.println("Indicarle a la nevera seleccionada que prenda la luz");
    toggleLight();
  }

  if (action.equals("setMaxTemperature"))
  {
    Serial.println("Indicarle a la nevera seleccionada que cambie su nivel maximo de temperature");
    int maxTemperature = json["maxTemperature"];
    setMaxTemperature(maxTemperature);
  }
  if (action.equals("setMinTemperature"))
  {
    Serial.println("Indicarle a la nevera seleccionada que cambie su nivel minimo de temperature");
    int minTemperature = json["minTemperature"];
    setMinTemperature(minTemperature);
  }

  if (action.equals("setStandaloneMode"))
  {
    Serial.println("Cambiar a modo independiente");
    String _newSsid = json["ssid"];
    setStandaloneMode(_newSsid);
  }

  if (action.equals("setCoordinatorMode"))
  {
    Serial.println("Cambiar a modo coordinado");
    String _newSsidCoordinator = json["ssid"];
    String _newPasswordCoordinator = json["password"];
    setCoordinatorMode(_newSsidCoordinator, _newPasswordCoordinator);
  }

  if (action.equals("setInternet"))
  {
    Serial.println("Cambiar punto de acceso con internet");
    String _newSsidInternet = json["ssid"];
    String _newPasswordInternet = json["password"];
    setInternet(_newSsidInternet, _newPasswordInternet);
  }
}

void changeMinutesToWait(int newMinutesToWait)
{
  compresorMinutesToWait = newMinutesToWait;
  setMemoryData();
}

//===================================== acciones
//=====================================

/// Lectura de temperatura a través del sensor.
void readTemperature()
{
  // Serial.print("[time#1] ");
  // Serial.println(millis());

  sensors.requestTemperaturesByIndex(0);
  // Serial.print("[time#2] ");f
  // Serial.println(millis());
  float temperatureRead = sensors.getTempCByIndex(0);
  // float temperatureRead = sensors.getTempC(address1);

  // Serial.print("[time#3] ");
  // Serial.println(millis());

  // float temperatureRead = sensors.getTempCByIndex(0);
  // float temperatureRead = dht.readTemperature();

  // Serial.print("[Temperatura] Temperatura: ");
  // Serial.println(temperatureRead);

  displayTemperature(temperatureRead);
  if (int(temperatureRead) != temperature)
  {
    if (!std::isnan(temperatureRead) && temperatureRead != -127.0)
    {
      temperature = int(temperatureRead);
    }
    // temperature = 10;
    // Serial.println("[NOTIFY STATE] Cambio de temperatura");
  }

  boolean canPushTemperature = millis() - previousTemperaturePushMillis >= updateTempInterval;
  // Serial.println("[TIEMPO] Ultima publicacion de temperatura en millis: " + String(previousTemperaturePushMillis));

  if (canPushTemperature || millis() < 10000)
  {
    if (WiFi.status() != WL_CONNECTED)
      return;
    previousTemperaturePushMillis = millis();
    pushTemperature(temperatureRead);
  }
}

void readExternalTemperature()
{
  float temperatureRead = dht.readTemperature();

  if (temperatureRead != externalTemperature)
  {
    externalTemperature = int(temperatureRead);
    // temperature = 10;
    notifyState = true;
  }

  // Serial.println("[Temperatura Externa] Temperatura: ");
  // Serial.print(externalTemperature);
}

/// Turn on/off the light
void toggleLight()
{
  light = !light;
  String title = "Mensaje de " + name;
  if (light)
  {
    publishMessage(
        title,
        "Se ha encendido la luz del equipo");
  }
  else
  {
    publishMessage(
        title,
        "Se ha apagado la luz del equipo");
  }
  publishState();

  lightLoop();
}

/// Turn on/off the light
void muteAlerts()
{
  muted = !muted;
  String title = "Mensaje de " + name;
  if (muted)
  {
    publishMessage(
        title,
        "Se han apagado las alertas del equipo");
  }
  else
  {
    publishMessage(
        title,
        "Se han encendido las alertas del equipo");
  }
  publishState();
}

void lightLoop()
{
  if (light)
  {
    digitalWrite(LIGHT, LOW); // envia señal alta al relay
    // Serial.println("Enciende la luz");
  }
  else
  {
    digitalWrite(LIGHT, HIGH); // envia señal alta al relay
    // Serial.println("Apaga la luz");
  }
}

/// Set max temperature
void setMaxTemperature(int newMaxTemperature)
{
  Serial.print("\n[CONFIG] Cambiando a newMaxTemperature: ");
  Serial.print(newMaxTemperature);
  if ((newMaxTemperature > -22 || newMaxTemperature < 17) && newMaxTemperature > minTemperature)
  { // Grados Centigrados
    maxTemperature = newMaxTemperature;

    setMemoryData();
    String title = "Mensaje de " + name;
    // publishMessage(
    //   title,
    //   "Se ha cambiado la temperatura máxima de alerta con éxito"
    // );
  }
  else
  {
    String title = "Error ocurrido en " + name;
    publishErrorMessage(title, "Limite de temperatura maxima inválida");
    sendError("Limite de temperatura máxima inválida");
  }
}

/// Set min temperature
void setMinTemperature(int newMinTemperature)
{
  Serial.print("\n[CONFIG] Cambiando a newMinTemperature: ");
  Serial.print(newMinTemperature);
  if ((newMinTemperature > -22 || newMinTemperature < 17) && newMinTemperature < maxTemperature)
  { // Grados Centigrados

    minTemperature = newMinTemperature;

    setMemoryData();
    String title = "Mensaje de " + name;
    // publishMessage(
    //   title,
    //   "Se ha cambiado la temperatura mínima de alerta con éxito"
    // );
  }
  else
  {
    sendError("Limite de temperatura minima inválida");
    String title = "Error ocurrido en " + name;
    publishErrorMessage(title, "Limite de temperatura mínima inválida");
  }
}

/// Cambiar nombre
void changeName(String newName)
{
  Serial.print("\n[CONFIG] Cambiando a nombre: ");
  Serial.print(newName);
  name = newName;

  String title = "Mensaje de " + name;
  // publishMessage(
  //   title,
  //   "Se ha cambiado el nombre del dispositivo con éxito"
  // );
  setMemoryData();
}

/// Enviar error
void sendError(String newError)
{
  // error = newError;
  notifyError = true;
}

/// Cambiar a modo independiente, nombre del wifi y contraseña.
// TODO(lesanpi): que reciba tambien la contraseña
void setStandaloneMode(String newSsid)
{
  Serial.print("\n[CONFIG] Cambiando a modo independiente");
  WiFi.mode(WIFI_OFF);
  standalone = true;
  ssid = newSsid;
  setMemoryData();
  setupWifi();
}

/// Cambia a modo coordinador, indicando el nombre y contraseña del Wifi
void setCoordinatorMode(String newSsidCoordinator, String newPasswordCoordinator)
{
  Serial.print("\n[setCoordinatorMode] ");
  Serial.print(newSsidCoordinator);
  Serial.print(" ");
  Serial.print(newPasswordCoordinator);
  if (!newSsidCoordinator.equals("") && !newSsidCoordinator.equals("null"))
  {
    ssidCoordinator = newSsidCoordinator;
  }
  else
  {
    return;
  }
  if (!newPasswordCoordinator.equals("") && !newPasswordCoordinator.equals("null"))
  {
    Serial.print("\n[CONFIG] Cambiando a modo coordinado");
    passwordCoordinator = newPasswordCoordinator;
    standalone = false;
    setMemoryData();
    WiFi.mode(WIFI_OFF);
    setupWifi();
  }
  else
  {
    return;
  }
}

/// Cambia a modo coordinador, indicando el nombre y contraseña del Wifi
void setInternet(String newSsidInternet, String newPasswordInternet)
{
  if (newPasswordInternet.length() < 8)
    return;
  if (!newSsidInternet.equals("") && !newSsidInternet.equals("null"))
  {
    ssidInternet = newSsidInternet;
  }
  else
  {
    return;
  }
  if (!newPasswordInternet.equals("") && !newPasswordInternet.equals("null"))
  {
    Serial.print("\n[CONFIG] Cambiando internet");
    passwordInternet = newPasswordInternet;
    // standalone = false;
    setMemoryData();
    String title = "Mensaje de " + name;
    // publishMessage(
    //   title,
    //   "Se ha cambiado el punto de acceso con internet con éxito"
    // );
    if (standalone)
    {
      startInternetClient();
    }
    // setupWifi();
  }
  else
  {
    return;
  }
}

/// Configurar dispositivo cuando esta en modo configuración
// TODO(lesanpi): Falta ssid y password del wifi con internet.
void configureDevice(
    String _id,
    String _userId,
    String _name,
    String _ssid,
    String _password,
    // String coordinatorSsid,
    // String coordinatorPassword,
    String _ssidInternet,
    String _passwordInternet,
    bool _standalone,
    int _temperature,
    int _maxTemperature,
    int _minTemperature,
    int _minutesToWait)
{

  // id = _id;
  userId = _userId;
  // configurationMode = false;
  // ssid = _ssid;
  // password = _password;
  if (!_password.equals("") && !_password.equals("null"))
  {
    ssid = _ssid;
    password = _password;
  }
  else
  {
    return;
  }

  Serial.println("New ssid: " + ssid);
  Serial.println("New password: " + password);
  setMemoryData();
  name = _name;
  maxTemperature = _maxTemperature;
  minTemperature = _minTemperature;
  temperaturaDeseada = _temperature;
  compresorMinutesToWait = _minutesToWait;
  if (!_ssidInternet.equals("") && !_ssidInternet.equals("null"))
  {
    ssidInternet = _ssidInternet;
  }
  if (!_passwordInternet.equals("") && !_passwordInternet.equals("null"))
  {
    passwordInternet = _passwordInternet;
  }
  standalone = _standalone;
  changeName(_name);
  setMaxTemperature(_maxTemperature);
  setMinTemperature(_minTemperature);
  setTemperature(_temperature);
  changeMinutesToWait(_minutesToWait);

  setMemoryData();
  ESP.restart();
  return;

  Serial.println("[CONFIG] Se guardo los datos en memoria");
  Serial.println("[CONFIG] Se configurara el wifi nuevamente");
  // WiFi.mode(WIFI_OFF);
  // delay(1000);
  WiFi.mode(WIFI_AP_STA);
  // Configure Internet
  // return;
  // startWiFiAP();
  startInternetClient();
  // setupWifi();
  Serial.println("[CONFIG] Se configurara el id");
  bool configurado = crearNevera(userId);

  if (configurado)
  {
    Serial.println("[CONFIG] Se termino de configurar completamente");
    configurationMode = false;
    // WiFi.softAPdisconnect(true);
    if (WiFi.softAPdisconnect())
    {
      Serial.println("[CONFIG] Removiendo WiFi AP. Configuracion completada");
    }
  }
  else
  {
    Serial.println("[CONFIG] No se pudo configurar compeltamente");
    ESP.restart();
  }
  // TODO(lesanpi): Save internetSsid and internetPassword
  // setStandaloneMode(_ssid);
  // if (!standalone){
  //   standalone = false;
  //   ssidCoordinator = coordinatorSsid;
  //   passwordCoordinator = coordinatorPassword;
  //   setCoordinatorMode(coordinatorSsid, coordinatorPassword);
  // }

  // ESP.reset();
}

/// Notificar al usuario
void sendNotification(String message)
{
  DynamicJsonDocument payload(512);
  payload["id"] = id;
  payload["user"] = userId;
  payload["type"] = 0;
  String tokenEncoded = jsonToString(payload);

  ArduinoJWT jwt = ArduinoJWT(KEY);
  String token = jwt.encodeJWT(tokenEncoded);
  Serial.println("[NOTIFICATION] Enviando notificacion al usuario");
  if (standalone)
  {
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

    Serial.print("[NOTIFICACION] Estatus code de la respuesta a la notificacion: ");
    Serial.println(String(httpCode));
    String payload = http.getString();
    Serial.println(payload);
    http.end();
    // processResponse(httpCode, http);
  }
  else
  {
    localClient.publish("notification/" + id, message);
  }
  // yield();
}

/// Publicar temperatura
void pushTemperature(float temp)
{
  if (std::isnan(temp))
    return;

  DynamicJsonDocument payload(512);
  payload["id"] = id;
  payload["user"] = userId;
  payload["type"] = 0;
  String tokenEncoded = jsonToString(payload);

  ArduinoJWT jwt = ArduinoJWT(KEY);
  String token = jwt.encodeJWT(tokenEncoded);
  yield();
  logMemory();
  yield();
  Serial.print("[TEMPERATURA] Publicando temperatura ");
  Serial.println(String(temp));
  if (standalone)
  {
    HTTPClient http;
    http.begin(espClient, API_HOST + "/api/fridges/push");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + token);

    String body = "";
    StaticJsonDocument<300> jsonDoc;
    jsonDoc["temp"] = temp;
    serializeJson(jsonDoc, body);

    // yield();
    int httpCode = http.POST(body);
    Serial.print("[TEMPERATURA] Estatus code de la respuesta: ");
    Serial.println(String(httpCode));
    String payload = http.getString();
    Serial.println(payload);
    http.end();

    // processResponse(httpCode, http);
  }
  else
  {
    localClient.publish("temp/" + id, String(temp));
  }

  // yield();

  // payload.clear();
}

/// Crear nueva nevera
bool crearNevera(String userId)
{

  Serial.println("[CONFIGURACION] Enviando peticion de crear nevera");
  delay(500);
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
    jsonDoc["type"] = 0;
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
        http.end();

        return true;
      }
      http.end();

      return false;
    }
    else
    {
      Serial.print("Error code: ");
      Serial.println(httpCode);
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      http.end();

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

  // yield();
}

/// Reiniciar valores de fabrica
void factoryRestore()
{

  Serial.println(String(EEPROM.length()));

  light = false;        // Salida luz
  compressor = false;   // Salida compressor
  door = false;         // Sensor puerta abierta/cerrada
  standalone = true;    // Quieres la nevera en modo independiente?
  temperature = 0;      // Sensor temperature
  humidity = 70;        // Sensor humidity
  maxTemperature = 20;  // Parametro temperatura minima permitida.
  minTemperature = -10; // Parametro temperatura maxima permitida.
  temperaturaDeseada = 4;
  configurationMode = true;
  id = "ZONA-REFRI";
  userId = "";
  name = "";
  ssid = id;                        // Nombre del wifi en modo standalone
  password = "12345678";            // Clave del wifi en modo standalone
  ssidCoordinator = "";             // Wifi al que se debe conectar (coordinador)
  passwordCoordinator = "12345678"; // Clave del Wifi del coordinador
  ssidInternet = "Sanchez Fuentes 2";
  passwordInternet = "09305573";

  setMemoryData();

  Serial.println("Controlador reiniciado de fabrica");
  Serial.println("Reiniciando...");
  ESP.restart();
}

// Control del compresor
void controlCompresor()
{
  if (fallaElectrica())
  {
    compressor = false;
    compresorFlag = false;
    tiempoAnterior = millis();
    // Serial.println("[COMPRESOR] APAGANDO EL COMPRESOR");
    digitalWrite(COMPRESOR, HIGH); /// Apago compresor
    return;
  }

  if (temperature > temperaturaDeseada)
  {
    if ((millis() - tiempoAnterior >= 1000 * 60 * compresorMinutesToWait))
    {
      digitalWrite(COMPRESOR, LOW); // Prender compresor
      if (!compressor)
      {
        Serial.println("[COMPRESOR] PRENDIENDO EL COMPRESOR");
        compressor = true;
      }
      compresorFlag = true;
    }
  }

  if (temperature < temperaturaDeseada)
  {
    if (compresorFlag)
    {
      compresorFlag = false;
      tiempoAnterior = millis();
      Serial.println("[COMPRESOR] APAGANDO EL COMPRESOR");
    }

    digitalWrite(COMPRESOR, HIGH); // Apagar compresor
    compressor = false;
  }
}

/// Set temperatura deseada
void setTemperature(int newTemperaturaDeseada)
{

  if (newTemperaturaDeseada <= 30 && newTemperaturaDeseada >= -20)
  {

    temperaturaDeseada = newTemperaturaDeseada;
    Serial.println("Temperatura deseada cambiada");
    setMemoryData();
    // String title = "Mensaje de " + name;
    // String message = "Se ha cambiado la temperatura referencial con éxito";
    // publishMessage(
    //   title,
    //   "Se ha cambiado la temperatura referencial con éxito"
    // );
    Serial.println("Temperatura cambiada con exito");
  }

  // displayTemperature(temperature);
}

void controlBotones()
{
  int temperaturaDeseada2 = temperaturaDeseada;
  // Serial.print("[BOTONES] Control botones time: ");
  // Serial.print(millis());
  // Serial.println();
  if (digitalRead(BAJARTEMP))
  {
    // Serial.print("Boton bajar temp en contacto");

    // temperaturaDeseada2--;
    // downTempFlag = true;
    // setTemperature(temperaturaDeseada2);
    if (!downTempFlag)
    {
      temperaturaDeseada2--;
      downTempFlag = true;
      setTemperature(temperaturaDeseada2);
    }
  }
  else
  {
    downTempFlag = false;
  }
  // Serial.print("Bajar Temp: ");
  // Serial.println(digitalRead(BAJARTEMP));
  // Serial.print("Subir Temp: ");
  // Serial.println(digitalRead(SUBIRTEMP));
  if (digitalRead(SUBIRTEMP))
  {
    // Serial.println("Boton subir temp en contacto");
    // temperaturaDeseada2++;
    // upTempFlag = true;
    // setTemperature(temperaturaDeseada2);
    if (!upTempFlag)
    {
      temperaturaDeseada2++;
      upTempFlag = true;
      setTemperature(temperaturaDeseada2);
    }
  }
  else
  {
    upTempFlag = false;
    // Serial.println("Boton subir temp fuera");
  }
  // float temperatureRead = dht.readTemperature();
  // displayTemperature(temperatureRead);

  // if (digitalRead(FACTORYREST))
  // {
  //   if (!restoreFactoryFlag)
  //   {
  //     tiempoAnteriorRf = millis();
  //     restoreFactoryFlag = true;
  //     Serial.println("....................................5 segundos para reinicir el equipo...............................");
  //   }
  //   /// Si pasan 5 segundos aplica el if
  //   if ((millis() - tiempoAnteriorRf) >= 5000)
  //   {
  //     restoreFactoryFlagFinish = true;
  //     Serial.println("Equipo se reiniciara de fabrica");
  //   }
  // }
  // else
  // {
  //   restoreFactoryFlag = false;
  //   if (restoreFactoryFlagFinish)
  //   {
  //     restoreFactoryFlagFinish = false;
  //     Serial.println("Equipo reiniciado de fabrica");
  //     factoryRestore();
  //   }
  // }
}

void shouldPushTempNotification()
{
  unsigned long currentMillis = millis();
  boolean canSendTemperatureNotification = currentMillis - previousTemperatureNoticationMillis >= interval;
  // Serial.println("[TIEMPO] Current millis: " + String(currentMillis));
  // Serial.println("[TIEMPO] Ultima notificacion de temperatura en millis: " + String(previousTemperatureNoticationMillis));

  if (!(canSendTemperatureNotification || millis() < 15000))
    return;

  if (temperature > maxTemperature)
  {

    if (canSendTemperatureNotification || millis() < 15000)
    {
      if (WiFi.status() != WL_CONNECTED)
        return;
      Serial.println("[NOTIFICACION] Notificando temperatura máxima alcanzada");
      previousTemperatureNoticationMillis = millis();
      sendNotification("Se ha alcanzado la temperatura máxima.");
      String title = "Error ocurrido en " + name;
      publishErrorMessage(title, "Se ha alcanzado la temperatura máxima.");
    }
    else
    {
      // Serial.println("[NOTIFICACION] No se puede notificar al usuario aun");
    }
  }
  if (temperature < minTemperature)
  {

    digitalWrite(COMPRESOR, HIGH); // Apagar compresor
    if (canSendTemperatureNotification)
    {
      if (standalone)
      {
        if (WiFi.status() != WL_CONNECTED)
          return;
        Serial.println("[NOTIFICACION] Notificando temperatura minima alcanzada");
        previousTemperatureNoticationMillis = millis();
        sendNotification("Se ha alcanzado la temperatura mínima.");
        String title = "Error ocurrido en " + name;
        publishErrorMessage(title, "Se ha alcanzado la temperatura mínima.");
      }
    }
  }
  // yield();
}

//=================================== display functions
//===================================

/// Display temperature
void displayTemperature(float temp)
{
  display.clearDisplay();
  if (configurationMode)
  { //[MODO CONFIGURACION]
    display.setTextColor(SSD1306_WHITE);
    display.clearDisplay();
    //  display.setCursor(0,0);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    //  display.println("Start");
    //  display.display();
    //  delay(2000);
    display.setCursor(9, 27);
    display.print("Esperando \n configuracion...");
    display.display();
    return;
  }

  // display.display();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  // display.setCursor(0, 55);
  // display.println("---------------------");
  display.setCursor(28, 27);
  display.setTextSize(3);
  if (std::isnan(temp) || temp == -127.0)
  {
    display.print(temperature, 1);
    // Serial.println("Display nan");
  }
  else
  {
    display.print(temp, 1);
    // Serial.println("Display temp nan");
  }
  display.print((char)247);

  display.setTextSize(1);
  display.setCursor(0, 55);
  String tempDText = "Temp. deseada: " + String(temperaturaDeseada) + " C";
  display.print("T. deseada: ");
  display.print(temperaturaDeseada);
  display.print(" C");
  // if (WiFi.status() == WL_CONNECTED && !fallaElectrica() &&
  //     internetConnectionOk)
  // {

  // }

  display.setCursor(0, 1);

  if (fallaElectrica())
  {
    display.print("Falla electrica");
  }
  else if (!internetConnectionOk)
  {
    display.print("Sin internet");
  }
  else if (WiFi.status() != WL_CONNECTED)
  {
    display.print("No conectado");
  }
  else
  {
    display.print("               ");
  }

  display.display();
}

void displayLoading()
{
  display.clearDisplay();
  if (configurationMode)
  { //[MODO CONFIGURACION]
    display.setTextColor(SSD1306_WHITE);
    display.clearDisplay();
    //  display.setCursor(0,0);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    //  display.println("Start");
    //  display.display();
    //  delay(2000);
    display.setCursor(9, 27);
    display.print("CARGANDO");
    display.display();
    return;
  }
}
void wifiAPLoop()
{
  /// Normal state => Connected to WiFi and No Electrical Issue
  // TODO: Add no internet

  /// Puede haber un UPS, por lo tanto puedo estar conectado a internet pero
  /// tener una falla electrica al mismo tiempo

  /// Puedo estar conectado al WiFi, pero no tener internet
  /// Por lo tanto requiere un wifi local para poder comunicarse
  // Serial.print("[time#0.2.1] time: ");
  // Serial.print(millis());
  // Serial.println();
  /// add internetConnected()
  if (WiFi.status() == WL_CONNECTED && !fallaElectrica() &&
      internetConnectionOk)
  {
    /// Disable soft ap
    if (softApEnabled)
    {
      // WiFi.enableSTA(true);
      // WiFi.enableAP(false);
      startInternetClient();
      Serial.println("[BATTERY] Intentando Apagado WiFi AP. Modo ahorro desactivado.");
      if (WiFi.softAPdisconnect())
      {
        Serial.println("[BATTERY] Desactivado WiFi AP exitosamente.");
        String notificationMessage = "Conexión restablecida en " + String(name) + ", trás ocurrir una falla. ";
        sendNotification(notificationMessage);
        softApEnabled = false;
      }
    }
    // Serial.print("[time#0.2.1.1] time: ");
    // Serial.print(millis());
    // Serial.println();
  }
  else
  {
    /// Enable soft ap
    if (!softApEnabled)
    {
      Serial.println("[BATTERY] Iniciando WiFi AP. Modo ahorro activado.");
      startWiFiAP();
      softApEnabled = true;
      // WiFi.enableSTA(false);
      // WiFi.enableAP(true);
      // Serial.println("[BATTERY] Intentando inicio WiFi AP. Modo ahorro activado.");
      // if (WiFi.disconnect())
      // {

      //   cloudClient.disconnect();

      // }
    }
    // Serial.print("[time#0.2.1.2] time: ");
    // Serial.print(millis());
    // Serial.println();
  }
}

bool fallaElectrica()
{
  bool batteryOn = false;
  if (digitalRead(ELECTRICIDAD))
  {
    /// Normal
    batteryOn = true;
    /// Disable soft ap
    // if (softApEnabled){
    //   Serial.println("[BATTERY] Apagando WiFi AP. Modo ahorro desactivado.");

    //   WiFi.softAPdisconnect(true);
    //   softApEnabled = false;
    // }
  }
  else
  {
    /// Emergency mode
    batteryOn = false;
    // if (!softApEnabled){
    //   Serial.println("[BATTERY] Iniciando WiFi AP. Modo ahorro activado.");
    //   startWiFiAP();
    //   softApEnabled = true;
    // }
  }
  return batteryOn;
}

void levelBattery()
{

  // Variables
  int analogValor = 0;
  float voltaje = 0;
  int porcentaje = 0;

  if (serialCount == 10000)
  {
    analogValor = analogRead(ANALOGPILA);
    serialCount = 0;

    voltaje = 0.00322 * analogValor;
    if (analogValor > 745)
    {
      porcentaje = 0.35842 * (analogValor - 745); // 1024 - 745 ---> el voltaje minimo aceptado es 3,3V va de 4.2V a 3,3V. En la entrada se lee de 3,3V a 2,4V
    }
    else
    {
      porcentaje = 0;
    }

    batteryPorcentage = porcentaje;

    // Serial.print("[BATTERY] Porcentaje: ");
    // Serial.println(porcentaje);
  }
  else
  {
    serialCount++;
  }
  // Leemos valor de la entrada analógica

  // Obtenemos el voltaje

  if (porcentaje <= 20 && fallaElectrica())
  {
    unsigned long currentMillis = millis();
    boolean canSendNotification = currentMillis - previousBatteryNoticationMillis >= intervalBatteryNotification;
    if (canSendNotification)
    {
      if (WiFi.status() != WL_CONNECTED)
        return;

      Serial.println("[NOTIFICACION] Notificando de bateria baja");
      previousBatteryNoticationMillis = millis();
      String notificationMessage = "Bateria baja " + String(porcentaje) + "%. Temperatura de " + String(temperature) + " grados celsius";
      sendNotification(notificationMessage);
      String title = "Bateria baja en " + name;
      String description = "Temperatura intera de " + String(temperature) + " grados celsius";
      publishErrorMessage(title, description);
    }
  }
}
