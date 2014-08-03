/*
 WebsocketClient, a websocket client for Spark Core based on Arduino websocket client
 Copyright 2011 Kevin Rohling
 Copyright 2012 Ian Moore
 Copyright 2014 Ivan Davletshin
 Copyright 2014 Matthew Goodman
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#define RETRY_TIMEOUT 3000

#include <stdlib.h>
#include "spark_wiring_tcpclient.h"

class WebSocketClient {
public:
  typedef void (*OnMessage)(WebSocketClient client, char* message);
  typedef void (*OnOpen)(WebSocketClient client);
  typedef void (*OnClose)(WebSocketClient client, int code, char* message);
  typedef void (*OnError)(WebSocketClient client, char* message);

  void connect(const char hostname[], int port, const char protocol[], const char path[]);
  void connect(IPAddress ipAddy, const char hostname[], int port, const char protocol[], const char path[]);

//  void connect(const char hostname[], int port = 80, const char protocol[] = NULL, const char path[] = "/");
//  void connect(UINT32 ipAddress, const char hostname[], int port = 80, const char protocol[] = NULL, const char path[] = "/");
  bool connected();
  void disconnect();
  void monitor();
  bool send(char* message);

  void setOnOpen(OnOpen function);
  void setOnClose(OnClose function);
  void setOnMessage(OnMessage function);
  void setOnError(OnError function);
private:
  const char* _hostname;
  IPAddress _ipAddress;
  int _port;
  const char* _path;
  const char* _protocol;

  TCPClient _client;

  bool _configured;
  bool _reconnecting;
  unsigned long _retryTimeout;

  void _reconnect();
  void _restartConnection();
  void _sendHandshake(const char* hostname, const char* path);
  bool _readHandshake();

  // Security/handshare validataion
  char _key[45];
  void generateHash(char* buffer, size_t bufferlen);
  size_t base64Encode(byte* src, size_t srclength, char* target, size_t targetsize);
  
  // User provided callback storage
  OnOpen _onOpen;
  OnClose _onClose;
  OnMessage _onMessage;
  OnError _onError;
  
  // Internal handles to callbacks w/ NULL checks
  void _callOnMessage(char*);
  void _callOnOpen();
  void _callOnClose(unsigned int, char*);
  void _callOnError(char*);
  
  // The current transaction data, length and opcode 
  byte _opCode;
  char* _packet;
  unsigned int _packetLength;
  void _freePacket();


  // TCP/io calls
  byte _nextByte();
  void _readNBytes(char*, int);
  void _readNBytes(char*, int, char*, int);
  void _readLine(char* buffer);
  void _appendFrameData(int, char*, bool);

  // Debugging/reporting tools
  void _dbPrint(char*);
  void _dbPrint(String);
  void _errState(char*);
};

const char b64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#endif
