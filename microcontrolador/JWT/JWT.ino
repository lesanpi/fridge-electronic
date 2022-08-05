#include <ArduinoJWT.h>


void setup() {
    Serial.begin(115200);
}

String key = "secretphrase";
void loop() {
    delay(1000);
    
    String exampleToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.WlE3tdHJ-M9mFxt4DtD1nGAz0SPNX8rTdFbPa-YNdRA";
    String exampleBadToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.05Vxxda7TGkLMCxnBSXB_3dtv7ErLQ7AZpjbJEqozdI";

    ArduinoJWT jwt = ArduinoJWT(key);
    String payload = "";
    jwt.decodeJWT(exampleToken, payload);
    Serial.println(payload);
    ArduinoJWT jwt2 = ArduinoJWT(key);
    String payload2 = "";
    jwt2.decodeJWT(exampleBadToken, payload2);
    Serial.println(payload2);
    Serial.print("Is invalid? ");
    Serial.println(bool(payload2 == ""));
    // Serial.println(bool(payload2.length() == 0) == 1);

}