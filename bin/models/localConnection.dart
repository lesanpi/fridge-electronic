import 'dart:convert';
import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

import '../enum/enums.dart';
import '../fridge/main.dart';

class LocalConnectionApi {
  late WebSocketChannel channel;
  List<dynamic> fridges = [];
  bool connected = false;
  LocalConnectionApi() {
    connect();
  }

  void connect() {
    print("Connecting to the server...");
    try {
      channel = IOWebSocketChannel.connect(Uri.parse('ws://192.168.0.1:81'));
      channel.stream.listen(onMessage);
    } catch (e) {
      print('Error trying to connect');
    }
  }

  void setConnection(bool connected) => this.connected = connected;

  void _sendCmd(dynamic data) {
    channel.sink.add(data);
  }

  bool _verifyAction(dynamic action) {
    if (action == null) {
      return false;
    }

    try {
      stringToAppOrder(action);
      return true;
    } catch (e) {
      return false;
    }
  }

  /// Orders
  ///
  ///

  /// Actions
  /// 1. turn on/off light
  /// 2. set max temperature
  /// 3. set min temperature
  /// 4. set max temperature for all fridges
  /// 5. set min temperature for all fridges
  /// 6. delete a fridge (if the coordinator exists)
  /// 7. delete all fridges (if the coordinator exists)

  // Turn on/off light
  void toggleLight({required String id}) {
    final data = {
      "action": AppAction.toggleLight.toShortString(),
      "payload": {"id": id}
    };
    final String jsonString = jsonEncode(data);

    _sendCmd(jsonString);
  }

  // Set max temperature parameter
  void setMaxTemperature({required double temperature, required String id}) {
    final data = {
      "action": AppAction.setMaxTemperature.toShortString(),
      "payload": {'id': id, 'temperature': temperature}
    };
    final String jsonString = jsonEncode(data);

    _sendCmd(jsonString);
  }

  // Set min temperature parameter
  void setMinTemperature({required double temperature, required String id}) {
    final data = {
      "action": AppAction.setMinTemperature.toShortString(),
      "payload": {'id': id, 'temperature': temperature}
    };
    final String jsonString = jsonEncode(data);

    _sendCmd(jsonString);
  }

  // Set max temperature for all
  void setMaxTemperatureForAll({required double temperature}) {
    for (var fridge in fridges) {
      setMaxTemperature(temperature: temperature, id: fridge.id);
    }
  }

  // Set min temperature for all
  void setMinTemperatureForAll({required double temperature}) {
    for (var fridge in fridges) {
      setMinTemperature(temperature: temperature, id: fridge.id);
    }
  }

  // Delete a fridge
  void delete({required String id}) {
    final data = {
      "action": AppAction.delete.toShortString(),
      "payload": {'id': id}
    };
    final String jsonString = jsonEncode(data);

    _sendCmd(jsonString);
  }

  // Delete all fridges
  void deleteAll() {
    for (var fridge in fridges) {
      delete(id: fridge.id);
    }
  }

  void setWifiCredentials(String ssid, String password) {
    final data = {
      "action": AppAction.setWifiCredentials.toShortString(),
      "payload": {'ssid': ssid, 'password': password}
    };
    final String jsonString = jsonEncode(data);

    _sendCmd(jsonString);
  }

  /// On message received
  /// 1. Decode json
  /// 2. Get action, verify not null and exists
  /// 3. Execute action

  // Decode message
  void onMessage(message) {
    try {
      final json = jsonDecode(message);
      final action = json['action'];

      if (_verifyAction(action)) {
        print('Action received: $action');
        AppOrder order = stringToAppOrder(action);
        onAction(order);
      }
    } catch (e) {
      if (e is FormatException) {
        print("Error: Message is not a json");
      } else {
        print('Error on message received $e');
      }
    }

    print({"connected": connected, "fridges": fridges});
  }

  // Execute action
  void onAction(AppOrder action) {
    switch (action) {
      case AppOrder.connected:
        setConnection(true);
        break;
      case AppOrder.sendData:
        print('Recibir datos');
        break;
      default:
        break;
    }
  }
}
