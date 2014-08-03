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

#include "spark_wiring_usbserial.h"
#include "spark_wiring_string.h"

#include "websocketclient.h"
#include <stdlib.h>
#include <string.h>

//#define WSDEBUG

const char *WebSocketClientStringTable = {
			"GET {0} HTTP/1.1\x0d\x0a"
			"Upgrade: websocket\x0d\x0a"
			"Connection: Upgrade\x0d\x0a"
			"Host: {1}:{2}\x0d\x0a"
			"Origin: SparkWebSocketClient\x0d\x0a"
			"Sec-WebSocket-Key: 1VTFj/CydlBCZDucDqw8eA==\x0d\x0a"
			"Sec-WebSocket-Version: 13\x0d\x0a"
			"\x0d\x0a"};


void WebSocketClient::_sendHandshake(const char* hostname, const char* path) {
	String handshake = "";

	handshake.concat(WebSocketClientStringTable);
	handshake.replace("{0}", path);
	handshake.replace("{1}", hostname);
	handshake.replace("{2}", String(_port));

	_client.print(handshake);
	_dbPrint(handshake);
}

bool WebSocketClient::_readHandshake() {
  // XXX Security warning!
  // Handshare return hash is _not_ checked.
  // Also this function will hang if there is no final cr/lf
  int maxAttempts = 300, attempts = 0;

  while(_client.available() == 0)
  {
    // Make sure we havent tried too many times
    if(attempts >= maxAttempts){
       _errState("Handshake Timeout");
       return false;
    }
    // Try again in a bit
    attempts++;
    delay(50);
  }
  
  char headerLine[128];
  
  // Retrieve the http response headers
  while(true) {
    // Clear out the previous line
    memset(headerLine, 0, 128);
    
    // Read the new line in
    _readLine(headerLine);
    
    // An empty line indicates we have the last header
    if(strcmp(headerLine, "") == 0){
      _dbPrint("[blank line]");
      break;
    } else {
      _dbPrint(headerLine);
    }
    // MRG TODO: scrape out and fact check the "Sec-WebSocket-Accept: *****" line hereish
  }
  return true;
}

void WebSocketClient::connect(const char hostname[], int port, const char protocol[], const char path[]) {
  UINT32 ipAddressBytes = 0;
  if(gethostbyname((char*)hostname, strlen(hostname), &ipAddressBytes) <= 0)
    return;

  connect(ipAddressBytes, hostname, port, protocol, path);      
}

void WebSocketClient::connect(IPAddress ipAddy, const char hostname[], int port, const char protocol[], const char path[]) {
  // Store the arguments internally (used in the handshake)
  _hostname = hostname;
  _ipAddress = ipAddy;
  _port = port;
  _protocol = protocol;
  _path = path;

  // Zero out the connection buffers
  _packet = NULL;
  _packetLength = 0;
    
  // These flag the loop to connect on the next pass
  _retryTimeout = millis();
  _configured = true;
}

void WebSocketClient::_errState(char* errMsg) {
    _dbPrint(errMsg);
    _callOnError(errMsg);
}

void WebSocketClient::_dbPrint(char* errMsg) {
    #ifdef WSDEBUG
    Serial.println(errMsg);
    #endif
}

void WebSocketClient::_dbPrint(String errMsg) {
    #ifdef WSDEBUG
    Serial.println(errMsg);
    #endif
}

void WebSocketClient::_reconnect() {
  bool result = false;
  if (_client.connect(_ipAddress, _port)) {
    _sendHandshake(_hostname, _path);
    result = _readHandshake();
  } else {
    _errState("TCP Connection Failure");
  }
  
  if(!result) {
    _errState("WS Connection Failure.");
    _client.stop();
  } else {
    _callOnOpen();
  }
}

bool WebSocketClient::connected() {
  return _client.connected();
}

void WebSocketClient::disconnect() {
  _client.stop();
  _freePacket();
}

byte WebSocketClient::_nextByte() {
  while(_client.available() == 0)
    delay(1);

  byte b = _client.read();
  
  if(b < 0)
    _errState("Internal Error in Ethernet Client Library (-1 returned where >= 0 expected)");

  return b;
}

void WebSocketClient::_appendFrameData(int length, char* maskBuf, bool nullTerm) {
  int addedLength = length + (nullTerm ? 0 : 1);
  
  // Realloc works like malloc if the pointer is NULL
  _packet = (char*) realloc(_packet, _packetLength + addedLength);
  _readNBytes(_packet + _packetLength, length, maskBuf, 4);
  _packetLength += addedLength;
  
  if(nullTerm)
    _packet[_packetLength] = 0x0;
}

void WebSocketClient::_readNBytes(char* storeDest, int n) {
    for(int i=0; i<n; i++)
      storeDest[i] = _nextByte();
}

void WebSocketClient::_readNBytes(char* storeDest, int n, char* mask, int maskLen) {
    for(int i=0; i<n; i++)
      storeDest[i] = _nextByte() ^ mask[i%maskLen];
}


void WebSocketClient::_restartConnection()
{
    if(millis() < _retryTimeout)
        return;
    _retryTimeout = millis() + RETRY_TIMEOUT;
    _reconnecting = true;
    _reconnect();
    _reconnecting = false;
}
void WebSocketClient::monitor() {
  // Do nothing if monitor() is called before connect()
  if(!_configured)
    return;
  
  // Do nothing if the connection is not yet established
  if(_reconnecting)
    return;
  
  // Attempt a connection if the timing is right.
  if(!connected()) {
    _restartConnection();
    return;
  }
  
  if (_client.available() > 2) {
    // Mask out the completion bit and opcode from the first byte
    byte hdr = _nextByte();
    bool finalFrame = hdr & 0x80;
    int opCode = hdr & 0x0F;

    // If we receive a continuation opCode, but don't have any previous data fail out 
    if((opCode == 0) && (_packet=NULL)){
      _errState("Received a continuation code at the wrong time.");        
      _freePacket();
      _client.stop();
      return;
    }
      
    // Store the opCode to be acted on later 
    if(opCode != 0)
      _opCode = opCode;
    
    hdr = _nextByte();
    bool mask = hdr & 0x80;
    
    // Decode the length of the rest of the message
    // Using 7 bits, 7 + (16 bits), or 7 + (64 bits) 
    int len = hdr & 0x7F;
    
    int payloadLength = 0;
    if(len < 126){
      payloadLength = len;
    } else if(len == 126) {
      len = _nextByte();
      len <<= 8;
      len += _nextByte();
    } else if (len == 127) {
      len = _nextByte();
      for(int i = 0; i < 7; i++) {
        len <<= 8;
        len += _nextByte();
    }
  }

  char maskBuffer[4];
  if(mask) {
    _readNBytes(maskBuffer, 4);
  } else {
    memset(maskBuffer, 0x0, 4);
  }
  
  _dbPrint("Recv Mask Buffer");
  for(int i=0; i<4; i++)
    _dbPrint(String((int)maskBuffer[i]));
    
  _appendFrameData(payloadLength, maskBuffer, finalFrame);

  // Packets can be broken up into multiple parts
  //_ If this is not the final part
  if(!finalFrame)
    return;

  // Take action based on a complete frame and opCode
  switch(_opCode) {
    case 0x00:
      _dbPrint("Unexpected Continuation OpCode");  
      break;
      
    case 0x01:
      _callOnMessage(_packet);
      break;
        
    case 0x02:
      _errState("Binary messages not yet supported (RFC 6455 section 5.6)");
      break;
        
    case 0x09:
      _dbPrint("onPing");
	  _client.write(0x8A);
      _client.write(byte(0x00));
      break;
        
    case 0x0A:
      _dbPrint("onPong");
      break;
        
    case 0x08:
      // First two bytes are the code, the rest are a text message
      unsigned int code = ((byte)_packet[0] << 8) + (byte)_packet[1];
      _callOnClose(code, _packet+2);
      _client.stop();
      break;
    }
    
    // We have acted on the packet, so deallocate it
    _freePacket();
  }
}

// User facing setters of callbacks
void WebSocketClient::setOnMessage(OnMessage fn) {
  _onMessage = fn;
}
void WebSocketClient::setOnOpen(OnOpen fn) {
  _onOpen = fn;
}
void WebSocketClient::setOnClose(OnClose fn) {
  _onClose = fn;
}
void WebSocketClient::setOnError(OnError fn) {
  _onError = fn;
}

void WebSocketClient::_freePacket() {
    free(_packet);
    _packet = NULL;
    _packetLength = 0;
}

// Callers of the above
void WebSocketClient::_callOnMessage(char* msg) {
  if(_onMessage != NULL)
    _onMessage(*this, msg);
}
void WebSocketClient::_callOnOpen() {
  if(_onOpen != NULL)
    _onOpen(*this);
}
void WebSocketClient::_callOnClose(unsigned int code, char* msg) {
  if(_onClose != NULL)
    _onClose(*this, code, msg);
}
void WebSocketClient::_callOnError(char * msg) {
  if(_onError != NULL)
    _onError(*this, msg);
}

void WebSocketClient::_readLine(char* buffer) {
  char character;
  
  int i = 0;
  while(true) {
    character = _nextByte();
    
    // When we reach a newline char we are done
    if(character == '\n')
      break;
      
    // Skip CR characters
    if (character == '\r')
      continue;
      
    // If we made it this far the character is a keeper!
    buffer[i] = character;
    i++;
  }
  buffer[i] = 0x0;
}

bool WebSocketClient::send(char* message) {
  // If not connected, or trying to connect fail to send
  if(!_configured || _reconnecting) {
    return false;
  }
  
  // Send the header bytes (single text message, no continuation, mask bit)
  _client.write(0x81);

  // Compute and encode the message length
  int len = strlen(message);
  if(len > 125) {
    _client.write(0xFE);
    _client.write(byte(len >> 8));
    _client.write(byte(len & 0xFF));
  } else {
    _client.write(0x80 | byte(len));
  }
  
  int i;
  // Make up some random bytes
  char maskBuffer[4];

  // Create the mask while serializing to the destination
  _dbPrint("Send Mask Buffer");
  for(i = 0; i < 4; i++) {
    maskBuffer[i] = rand() % 255;
    _client.write((byte) maskBuffer[i]);
    _dbPrint(String((int) maskBuffer[i]));
  }
  
  // Send the actual message
  for(i=0; i<len; i++)
    _client.write((byte) (message[i] ^ maskBuffer[i % 4]));

  return true;
}
