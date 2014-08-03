#include "websocketclient.h"

char server[] = "192.168.42.66";

WebSocketClient client;

void ddpStarter(WebSocketClient client){
  char msg[] = "{\"msg\":\"connect\", \"version\": \"pre2\", \"versions\":[\"pre2\"]}";
  Serial.print(msg);
  client.send(msg);
}

void msgHandler(WebSocketClient client, char* message) {
  Serial.println("Received: ");
  Serial.println(message);
}

void errHandler(WebSocketClient client, char* message) {
  Serial.println("Error: ");
  Serial.println(message);
}

void debugHandler(WebSocketClient client, char* message) {
    Serial.println("Debug:");
    Serial.println(message);
}


void setup() {
  Serial.begin(9600);

  client.setOnMessage(msgHandler);
  client.setOnOpen(ddpStarter);
  client.setOnError(errHandler);
  pinMode(D4, OUTPUT);
}



void loop() {
  digitalWrite(D4, LOW);
  delay(1000*60);
  
  digitalWrite(D4, HIGH);
  Serial.println("Hello Computer");

  Serial.println("Starting WS Connection...");
  IPAddress addr = IPAddress(192, 168, 42, 66);
  client.connect(addr, "localhost", 3000, "", "/websocket");
  client.monitor();

  Serial.print("Connection test:");
  Serial.println(client.connected() ? "\tPassed!" : "\tFailed :(");

  for(int i=0; i<50; i++){
    client.monitor();
    delay(10);
  }

  for(int i=0; i<50; i++){
    client.monitor();
    delay(10);
  }

  client.disconnect();
}