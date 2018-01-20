#include "HttpStream.h"

HttpStream::HttpStream(WiFiClient conn): TcpStream(conn) {
}

void HttpStream::parseNextChunkLength() {
  String chunkLength = TcpStream::readStringUntil('\r');
  waitAvailable(1);
  TcpStream::read(); //consume '\n'
  remainingChunkBytes = (int) strtol(chunkLength.c_str(), NULL, 16);
  // TODO: check if length is zero (last chunk)
}

byte HttpStream::read() {
  waitAvailable(1);
  byte result = TcpStream::read();
  if (requestChunkedEncoded) {
    remainingChunkBytes--;
    if (remainingChunkBytes == 0) {
      // consume CRLF chunk end delimiter
      waitAvailable(2);
      TcpStream::read();
      TcpStream::read();
      parseNextChunkLength();
    }
  }
  return result;
}

bool HttpStream::hasMoreData() {
  return TcpStream::hasMoreData() && remainingChunkBytes != 0; //TODO: make it work for non-chunked requests as well
}

bool HttpStream::parseRequestHeader() {
  requestMethod = readStringUntil(' ');
  if (requestMethod == "") {
    return false;
  }
  requestPath = readStringUntil(' ');
  if (requestPath == "") {
    return false;
  }

  readStringUntil('\r');
  read(); //consume the '\n'

  bool chunkedEncoded = false;
  String header;
  while ((header = readStringUntil('\r')) != "") {
    header.toLowerCase(); //does not return a new string, modifies the existing one in-place (source: https://github.com/esp8266/Arduino/blob/master/cores/esp8266/WString.cpp#L732)
    if (header.startsWith(CONTENT_LENGTH_HEADER)) {
      requestContentLength = header.substring(STRLEN(CONTENT_LENGTH_HEADER)).toInt();
    } else if (header == CHUNKED_ENCODING_HEADER) {
      chunkedEncoded = true;
      remainingChunkBytes = 0;
    }
    read(); //consume the '\n'
  }
  read(); //consume the '\n'

  requestChunkedEncoded = chunkedEncoded;
  if (chunkedEncoded) {
    parseNextChunkLength();
  }
  return true;
}

std::map<String, String> HttpStream::parseUrlencodedRequestBody() {
  std::map<String, String> result;
  int parsedBytes = 0;
  while (parsedBytes < requestContentLength) {
    String key, value;
    key.reserve(32);
    value.reserve(32);
    char readChar;
    while (parsedBytes < requestContentLength && ((readChar = read()) != '=')){
      key += readChar;
      parsedBytes++;
    }
    parsedBytes++; // '='
    while (parsedBytes < requestContentLength && ((readChar = read()) != '&')){
      value += readChar;
      parsedBytes++;
    }
    parsedBytes++; // '&'
    result[key] = value;
  }
  return result;
}

String HttpStream::getRequestMethod() {
  return requestMethod;
}

String HttpStream::getRequestPath() {
  return requestPath;
}