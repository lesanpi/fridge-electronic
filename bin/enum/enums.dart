enum FridgeStatus { disconnected, ok, warningConnection, warningTemperature }

enum FridgeAction { sendState, error, confirmConnection }

enum FridgeOrder {
  connected,
  toggleLight,
  verifyConnection,
  setMaxTemperature,
  setMinTemperature,
}

enum AppAction {
  toggleLight,
  setMaxTemperature,
  setMinTemperature,
  setMaxTemperatureForAll,
  setMinTemperatureForAll,
  delete,
  deleteAll,
  setWifiCredentials
}

enum AppOrder { sendData, error, connected }

FridgeOrder stringToFridgeOrder(String action) {
  return FridgeOrder.values
      .firstWhere((element) => element.toShortString() == action);
}

AppOrder stringToAppOrder(String action) {
  return AppOrder.values
      .firstWhere((element) => element.toShortString() == action);
}

extension ParseToStringAppActionsTx on AppAction {
  String toShortString() {
    return this.toString().split('.').last;
  }
}

extension ParseToStringAppActionsRx on AppOrder {
  String toShortString() {
    return this.toString().split('.').last;
  }
}

extension ParseToStringFridgeStatus on FridgeStatus {
  String toShortString() {
    return this.toString().split('.').last;
  }
}

extension ParseToStringFridgeActionTx on FridgeAction {
  String toShortString() {
    return this.toString().split('.').last;
  }
}

extension ParseToStringFridgeActionRx on FridgeOrder {
  String toShortString() {
    return this.toString().split('.').last;
  }
}
