import 'dart:convert';
import 'dart:io';
import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'dart:math';

import '../models/fridgeController.dart';

main() async {
  print('Starting test fridge');
  final fridgeClient = FridgeController('fridge-test-1');

  fridgeClient.sendState();
  // connect();

  // channel.stream.listen((message) {
  //   print(message);
  // });

  // for (int i = 0; i < 3; i++) {
  //   sendState();
  // }

  // setTemperatureFor(20, '1');
}
