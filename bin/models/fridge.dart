import '../enum/enums.dart';

class Fridge {
  String id;
  String name;
  String ssid;
  String? lastDateTime;
  int temperature;
  double? maxTemperature;
  double? minTemperature;
  bool standaloneMode = true;
  bool light = false;
  bool compressor = false;
  FridgeStatus status;

  Fridge({
    required this.name,
    required this.id,
    required this.temperature,
    required this.standaloneMode,
    required this.light,
    required this.compressor,
    required this.lastDateTime,
    required this.maxTemperature,
    required this.minTemperature,
    required this.status,
    required this.ssid,
  });

  bool sameFridge(Fridge fridge) => fridge.id == id;

  factory Fridge.fromJson(Map<String, dynamic> json) => Fridge(
        name: json["name"],
        id: json["id"],
        temperature: json["temperature"],
        light: json["light"],
        compressor: json["compressor"],
        lastDateTime: json["lastDateTime"],
        status: json["status"],
        maxTemperature: json["maxTemperature"],
        minTemperature: json["minTemperature"],
        standaloneMode: json["standaloneMode"],
        ssid: json["ssid"],
      );

  static Fridge empty() => Fridge(
        name: "",
        id: "",
        temperature: -127,
        compressor: false,
        light: false,
        standaloneMode: true,
        maxTemperature: null,
        minTemperature: null,
        lastDateTime: null,
        status: FridgeStatus.disconnected,
        ssid: "",
      );
}
