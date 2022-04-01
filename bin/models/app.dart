import 'dart:convert';
import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

enum AppActionTx {
  setTemperature,
  setTemperatureForAll,
  toggleLight,
  setMaxTemperature,
  setMinTemperature,
  setMaxTemperatureForAll,
  setMinTemperatureForAll,
  delete,
  deleteAll
}

enum AppActionRx { sendData, error, connected }

extension ParseToStringAppActionsTx on AppActionTx {
  String toShortString() {
    return this.toString().split('.').last;
  }
}

extension ParseToStringAppActionsRx on AppActionRx {
  String toShortString() {
    return this.toString().split('.').last;
  }
}

class LocalConnectionApi {
  bool connected = false;
  List<dynamic> fridges = [];
  late WebSocketChannel channel;

  LocalConnectionApi() {
    connect();
  }

  void setConnection(bool connected) => this.connected = connected;
  void setMaxTemperature(
      {required double temperature, required String id}) async {
    final data = {
      "action": AppActionTx.setMaxTemperature.toShortString(),
      "payload": {'id': id, 'temperature': temperature}
    };
    final String jsonString = jsonEncode(data);

    _sendCmd(jsonString);
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

  void _sendCmd(dynamic data) {
    channel.sink.add(data);
  }

  bool _verifyAction(dynamic action) {
    if (action == null) {
      return false;
    }

    try {
      stringToAction(action);
      return true;
    } catch (e) {
      return false;
    }
  }

  AppActionRx stringToAction(String action) {
    return AppActionRx.values
        .firstWhere((element) => element.toShortString() == action);
  }

  void onMessage(message) {
    try {
      final json = jsonDecode(message);
      final action = json['action'];
      if (_verifyAction(action)) {
        print('Action received: $action');
        AppActionRx actionRx = stringToAction(action);
        onAction(actionRx);
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

  void onAction(AppActionRx action) {
    switch (action) {
      case AppActionRx.connected:
        setConnection(true);
        break;
      case AppActionRx.sendData:
        print('Recibir datos');
        break;
      default:
        break;
    }
  }
}
