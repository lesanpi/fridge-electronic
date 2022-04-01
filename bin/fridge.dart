import 'dart:convert';

import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

enum FridgeStatus { disconnected, ok, warningConnection, warningTemperature }
enum FridgeActionTx { sendState, error, confirmConnection }
enum FridgeActionRx {
  connected,
  setTemperature,
  toggleLight,
  verifyConnection,
  setMaxTemperature,
  setMinTemperature,
  none
}

extension ParseToStringFridgeStatus on FridgeStatus {
  String toShortString() {
    return this.toString().split('.').last;
  }
}

extension ParseToStringFridgeActionTx on FridgeActionTx {
  String toShortString() {
    return this.toString().split('.').last;
  }
}

extension ParseToStringFridgeActionRx on FridgeActionRx {
  String toShortString() {
    return this.toString().split('.').last;
  }
}

class Fridge {
  late WebSocketChannel channel;
  late String id;
  late double temperature;
  bool light = false;
  bool compressor = false;
  late DateTime lastDateTime;
  FridgeStatus status = FridgeStatus.disconnected;
  double maxTemperature = 20;
  double minTemperature = -10;

  Fridge(this.id) {
    temperature = 10.0;
    lastDateTime = DateTime.now();
    connect();
  }

  /* Temperature */
  double readTemperature() => temperature;
  void _increaseTemperature() => temperature = temperature + 0.01;
  void _decreaseTemperature() => temperature = temperature - 0.01;

  /* Temperature Umbral */
  void setMaxTemperature(double maxTemperature) =>
      this.maxTemperature = maxTemperature;
  void setMinTemperature(double minTemperature) =>
      this.minTemperature = minTemperature;

  /* Verify temperature */
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
      'id': id,
      'temperature': temperature,
      'light': light,
      'compressor': compressor,
      'date': lastDateTime.toString(),
      'status': status.toShortString(),
      'temperature_max': maxTemperature,
      'temperature_min': minTemperature,
    };
  }

  void sendState() async {
    final data = {
      "action": FridgeActionTx.sendState.toShortString(),
      "payload": getState()
    };
    final String jsonString = jsonEncode(data);

    _sendCmd(jsonString);
  }

  void confirmConnection() async {
    final data = {
      "action": FridgeActionTx.confirmConnection.toShortString(),
    };
    final String jsonString = jsonEncode(data);

    _sendCmd(jsonString);
  }

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
      _toFridgeAction(action);
      return true;
    } catch (e) {
      return false;
    }
  }

  bool _verifyPayload(dynamic payload) {
    return payload != null;
  }

  FridgeActionRx _toFridgeAction(String action) {
    return FridgeActionRx.values
        .firstWhere((element) => element.toShortString() == action);
  }

  void onMessage(message) {
    // print(message);
    try {
      final json = jsonDecode(message);
      final action = json['action'];
      if (_verifyAction(action)) {
        FridgeActionRx actionRx = _toFridgeAction(action);
        onAction(actionRx);
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

  void onAction(FridgeActionRx action, {dynamic payload = ""}) {
    switch (action) {
      case FridgeActionRx.connected:
        setStatus(FridgeStatus.ok);
        break;

      case FridgeActionRx.setMaxTemperature:
        if (payload["temperature"] != null && _verifyPayload(payload)) {
          setMaxTemperature(payload["temperature"]);
        }
        break;

      case FridgeActionRx.setMinTemperature:
        if (payload["temperature"] != null && _verifyPayload(payload)) {
          setMinTemperature(payload["temperature"]);
        }
        break;

      case FridgeActionRx.toggleLight:
        toggleLight();
        break;

      case FridgeActionRx.verifyConnection:
        confirmConnection();
        break;
      default:
        break;
    }
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
    showState();
  }
}
