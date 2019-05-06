/*
 * IRremoteESP8266: IRServer - demonstrates sending IR codes controlled from a webserver
 * Version 0.2 June, 2017
 * Copyright 2015 Mark Szabo
 *
 * An IR LED circuit *MUST* be connected to the ESP8266 on a pin
 * as specified by kIrLed below.
 *
 * TL;DR: The IR LED needs to be driven by a transistor for a good result.
 *
 * Suggested circuit:
 *     https://github.com/markszabo/IRremoteESP8266/wiki#ir-sending
 *
 * Common mistakes & tips:
 *   * Don't just connect the IR LED directly to the pin, it won't
 *     have enough current to drive the IR LED effectively.
 *   * Make sure you have the IR LED polarity correct.
 *     See: https://learn.sparkfun.com/tutorials/polarity/diode-and-led-polarity
 *   * Typical digital camera/phones can be used to see if the IR LED is flashed.
 *     Replace the IR LED with a normal LED if you don't have a digital camera
 *     when debugging.
 *   * Avoid using the following pins unless you really know what you are doing:
 *     * Pin 0/D3: Can interfere with the boot/program mode & support circuits.
 *     * Pin 1/TX/TXD0: Any serial transmissions from the ESP8266 will interfere.
 *     * Pin 3/RX/RXD0: Any serial transmissions to the ESP8266 will interfere.
 *   * ESP-01 modules are tricky. We suggest you use a module with more GPIOs
 *     for your first time. e.g. ESP-12 etc.
 */
#ifndef UNIT_TEST
#include <Arduino.h>
#endif
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <WiFiClient.h>

const char* kSsid = "telenet-31238";
const char* kPassword = "dEwCwYCKz9HQ";
const char delim[] = "|";
MDNSResponder mdns;

ESP8266WebServer server(80);

struct dynamic_buffer { //this class leaks memory. If you want to pass it around, use more than one, etc: add a proper destructor
    uint16_t* _data = NULL;
    uint16_t _size = 0;
    uint16_t _capacity = 0;
    
    void init(uint16_t capacity = 100){
        _capacity = capacity;
        _data = (uint16_t*)malloc(_capacity * sizeof(uint16_t));
    }
    void push_back(uint16_t element){
        if(_size < _capacity){
            _data[_size++] = element;
            Serial.println("Sb1");
        } else {
            Serial.println("P1");
            void* mem = calloc(_capacity * 2, sizeof(uint16_t));
            Serial.println("P2");
            memcpy(mem, _data, _size);
            Serial.println("P3");
            free(_data);
            Serial.println("P4");
            _data = static_cast<uint16_t*>(mem);
            Serial.println("P5");
            _data[_size++] = element;
            _capacity = _capacity*2;
            Serial.println("Sb2");
        }
    }
    void clear(){
        free(_data);
        void* mem = calloc(_capacity, sizeof(uint16_t));
        _data = static_cast<uint16_t*>(mem);
        _size = 0;
    }
    dynamic_buffer() = default;
    dynamic_buffer(const dynamic_buffer&) = delete; //not copyable
    dynamic_buffer& operator=(dynamic_buffer const&) = delete;
};

void parse_data(dynamic_buffer& buffer, char* data){
    char *ptr = strtok(data, ",");  
    while (ptr != NULL) {
      uint16_t i = static_cast<uint16_t>(atoi(ptr));
      buffer.push_back(i);
      ptr = strtok(NULL, ",");
    }
}

struct dynamic_buffer coolBuffer;

const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).

IRsend irsend(kIrLed);  // Set the GPIO to be used to sending the message.

void handleRoot() {
  server.send(200, "text/html", "");
}

void handleIr() {
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "codes") {
      char* codes = strdup(server.arg(i).c_str());
      char *code = strtok(codes, delim);
      Serial.println(code);
      while (code != NULL) {
        parse_data(coolBuffer, code);
        Serial.println("Sending..");
        irsend.sendRaw(coolBuffer._data, coolBuffer._size, 38);
        Serial.println("Sent");

        code = strtok(NULL, delim);
      }
    }
  }
  handleRoot();
  coolBuffer.clear();
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  server.send(404, "text/plain", message);
}

void setup(void) {
  
  irsend.begin();

  Serial.begin(115200);
  WiFi.begin(kSsid, kPassword);
  Serial.println("");
  coolBuffer.init();

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(kSsid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP().toString());

  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/ir", handleIr);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
}
