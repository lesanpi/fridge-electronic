import "dart:convert";
import 'dart:io';

void main() async {
  var server = await HttpServer.bind('127.0.0.1', 8000);
  print("Turn on Server");
  server.listen((HttpRequest req) async {
    print("Requests ${req.method} ${req.requestedUri}");
    if (req.uri.path == '/ws') {
      var socket = await WebSocketTransformer.upgrade(req);
      socket.listen((data) {
        print("Data: ");
        print(data);
      });
    }
  });
}
