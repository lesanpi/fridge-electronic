import "dart:convert";
import 'dart:io';

import '../models/app.dart';

main() async {
  print('Starting test local app connection');
  final localAppClient = LocalConnectionApi();

  localAppClient.setMaxTemperature(temperature: 30.0, id: 'fridge-test-1');
}
