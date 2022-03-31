import 'dart:convert';
import 'dart:io';
import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'dart:math';

final APP_COMMANDS_TX = [
  "@set_temperature",
  "@set_temperature_for_all",
  "@toggle_light",
  "@set_temperature_max",
  "@set_temperature_min",
  "@set_temperature_max_for_all",
  "@set_temperature_min_for_all",
  "@delete",
  "@delete_all"
];
final APP_COMMAND_RX = ["@send_data", "@error"];

final FRIDGE_COMMANDS_RX = [
  "@set_temperature",
  "@toggle_light",
  "@verify_conection", // Enviado desde el coordinador.
  "@set_temperature_max",
  "@set_temperature_min",
];
final FRIDGE_COMMANDS_TX = ["@send_state"];

late var channel;
String id = "prueba-id";
dynamic fridgeState = {
  'id': id,
  'temperature': -10,
  'light': false,
  'date': DateTime.now().toString(),
  'status':
      'ok', // disconnected | ok | warning_connection | warning_temperature
  'temperature_max': 99,
  'temperature_min': -20,
};
/* NEVERA*/
/* Funciones nevera*/
void connect() async {
  channel = IOWebSocketChannel.connect(Uri.parse('ws://192.168.0.1:81'));
}

int readTemperature() {
  final random = Random();
  int newTemperature = random.nextInt(100);
  fridgeState['temperature'] = newTemperature;
  return newTemperature;
}

void toggleLight() {
  fridgeState['light'] = !fridgeState['light'];
}

void sendState() async {
  final data = {"cmd": "@send_state", "payload": fridgeState};
  final String jsonString = jsonEncode(data);

  await Future.delayed(Duration(seconds: 4));
  sendCmd(jsonString);
}

/* */

/* Aplicacions */
/*Funciones de la App */

void setTemperatureFor(int temperature, String id) async {
  final data = {
    "cmd": "@set_temperature",
    "payload": {"id": id, "temperature": temperature}
  };
  final String jsonString = jsonEncode(data);

  await Future.delayed(Duration(seconds: 4));
  sendCmd(jsonString);
}

void setTemperatureForAll(int temperature) async {
  final data = {
    "cmd": "@set_temperature_for_all",
    "payload": {"temperature": temperature}
  };
  final String jsonString = jsonEncode(data);

  await Future.delayed(Duration(seconds: 4));
  sendCmd(jsonString);
}

void toggleMyLight(String id) async {
  final data = {
    "cmd": "@toggle_light",
    "payload": {"id": id}
  };
  final String jsonString = jsonEncode(data);

  await Future.delayed(Duration(seconds: 4));
  sendCmd(jsonString);
}

/* TODO: Schedule features */
void scheduleActionForSpecificDay(DateTime date, dynamic action) {}

void deleteScheduleAction(String scheduleId) {}

void scheduleActionForEveryDay() {}

void sendCmd(dynamic data) {
  channel.sink.add(data);
}

main() async {
  print('Hello');
  connect();

  channel.stream.listen((message) {
    print(message);
  });

  for (int i = 0; i < 3; i++) {
    sendState();
  }

  setTemperatureFor(20, '1');
}
