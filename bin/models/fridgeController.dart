import 'dart:convert';

import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

import '../enum/enums.dart';

class FridgeController {
  // Canal de comunicacion
  late WebSocketChannel channel;

  late String id;
  late double temperature;
  bool standaloneMode = false;
  bool light = false;
  bool compressor = false;
  // Updates every communication
  late DateTime lastDateTime;
  FridgeStatus status = FridgeStatus.disconnected;
  // Default temperature pararmters
  double maxTemperature = 20;
  double minTemperature = -10;

  FridgeController(this.id) {
    temperature = 10.0;
    lastDateTime = DateTime.now();
    connect();
  }

  /* Temperature 
    1. Leer temperatura.
    2. Aumenta/disminuye temperatura de acuerdo el ambiente real.
    3. Verificar si pasa los umbrales de temperaturas, prender/apagar el compressor
    dependiendo de cada caso.
  */

  // Leer el sensor
  double readTemperature() => temperature;

  // Simulacion
  void _increaseTemperature() => temperature = temperature + 0.1;
  void _decreaseTemperature() => temperature = temperature - 0.1;

  // Temperature Umbral
  void setMaxTemperature(double maxTemperature) =>
      this.maxTemperature = maxTemperature;
  void setMinTemperature(double minTemperature) =>
      this.minTemperature = minTemperature;

  // Verify temperature
  void verifyTemperature() {
    if (temperature < minTemperature) {
      setStatus(FridgeStatus.warningTemperature);
      if (compressor) {
        toggleCompressor();
      }
      return;
    }

    if (temperature > maxTemperature) {
      setStatus(FridgeStatus.warningTemperature);
      if (!compressor) {
        toggleCompressor();
      }
      return;
    }

    setStatus(FridgeStatus.ok);
  }

  void setStatus(FridgeStatus status) => this.status = status;
  FridgeStatus getStatus() => status;

  void updateLastDate() => lastDateTime = DateTime.now();

  void toggleCompressor() => compressor = !compressor;
  void toggleLight() => light = !light;

  dynamic getState() {
    updateLastDate();
    return {
      "id": id,
      "temperature": temperature.round(),
      "light": light,
      "compressor": compressor,
      "date": lastDateTime.toString(),
      "status": status.toShortString(),
      "temperature_max": maxTemperature.round(),
      "temperature_min": minTemperature.round(),
    };
  }

  void sendState() async {
    final data = {
      "action": FridgeAction.sendState.toShortString(),
      "payload": getState()
    };
    final String jsonString = jsonEncode(data);

    _sendCmd(jsonString);
  }

  void confirmConnection() async {
    final data = {
      "action": FridgeAction.confirmConnection.toShortString(),
    };
    final String jsonString = jsonEncode(data);

    _sendCmd(jsonString);
  }

  // Para ver por consola
  void showState() {
    print(getState().toString());
  }

  void connect() {
    print("Connecting to the server...");
    try {
      channel = IOWebSocketChannel.connect(Uri.parse('ws://192.168.0.1:81'));
      channel.stream.listen(onMessage);
    } catch (e) {
      setStatus(FridgeStatus.disconnected);
    }

    showState();
  }

  void _sendCmd(dynamic data) {
    channel.sink.add(data);
  }

  bool _verifyAction(dynamic action) {
    if (action == null) {
      return false;
    }

    try {
      stringToFridgeOrder(action);
      return true;
    } catch (e) {
      return false;
    }
  }

  bool _verifyPayload(dynamic payload) {
    return payload != null;
  }

  void onMessage(message) {
    // print(message);
    try {
      final json = jsonDecode(message);
      final action = json['action'];
      if (_verifyAction(action)) {
        FridgeOrder actionRx = stringToFridgeOrder(action);
        onAction(actionRx, json: json);
        showState();
      }
    } catch (e) {
      if (e is FormatException) {
        print("Error: Message is not a json");
      } else {
        print('Error on message received $e');
      }
    }

    simulate();
  }

  void onAction(FridgeOrder action, {dynamic json = ""}) {
    print("Action received: ${action.toShortString()}");

    switch (action) {
      case FridgeOrder.connected:
        setStatus(FridgeStatus.ok);
        break;

      case FridgeOrder.setMaxTemperature:
        if (json["payload"]["temperature"] != null &&
            _verifyPayload(json["payload"])) {
          setMaxTemperature(json["payload"]["temperature"].roundToDouble());
        }

        break;

      case FridgeOrder.setMinTemperature:
        if (json["payload"]["temperature"] != null &&
            _verifyPayload(json["payload"])) {
          setMinTemperature(json["payload"]["temperature"]);
        }
        break;

      case FridgeOrder.toggleLight:
        toggleLight();
        break;

      case FridgeOrder.verifyConnection:
        confirmConnection();
        break;
      default:
        break;
    }

    showState();
  }

  void simulate() async {
    verifyTemperature();
    if (compressor) {
      _decreaseTemperature();
    } else {
      _increaseTemperature();
    }
    await Future.delayed(Duration(seconds: 5));
    sendState();
  }
}
