#include <Arduino.h>
#include <SPI.h>
#include <LibAPRS.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
  #include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "BluefruitConfig.h"

#define ADC_REFERENCE REF_3V3
#define OPEN_SQUELCH false

#define FACTORYRESET_ENABLE         1
#define MINIMUM_FIRMWARE_VERSION    "0.6.6"
#define MODE_LED_BEHAVIOUR          "MODE"

#define QUEUESIZE                   16

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

boolean gotPacket = false;
AX25Msg incomingPacket;
uint8_t *packetData;

char outgoingQueue[QUEUESIZE][BUFSIZE];
int currentQueueSize;

void aprs_msg_callback(struct AX25Msg *msg) {
  // If we already have a packet waiting to be
  // processed, we must drop the new one.
  if (!gotPacket) {
    // Set flag to indicate we got a packet
    gotPacket = true;

    // The memory referenced as *msg is volatile
    // and we need to copy all the data to a
    // local variable for later processing.
    memcpy(&incomingPacket, msg, sizeof(AX25Msg));

    // We need to allocate a new buffer for the
    // data payload of the packet. First we check
    // if there is enough free RAM.
    if (freeMemory() > msg->len) {
      packetData = (uint8_t*)malloc(msg->len);
      memcpy(packetData, msg->info, msg->len);
      incomingPacket.info = packetData;
    } else {
      // We did not have enough free RAM to receive
      // this packet, so we drop it.
      gotPacket = false;
    }
  }
}

void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

void messageExample() {
  // We first need to set the message recipient
  APRS_setMessageDestination("AA3BBB", 0);
  
  // And define a string to send
  char *message = "Hi there! This is a message.";
  APRS_sendMsg(message, strlen(message));
}

void processPacket() {
  if (gotPacket) {
    gotPacket = false;
    
    ble.print(F("Received APRS packet. SRC: "));
    ble.print(incomingPacket.src.call);
    ble.print(F("-"));
    ble.print(incomingPacket.src.ssid);
    ble.print(F(". DST: "));
    ble.print(incomingPacket.dst.call);
    ble.print(F("-"));
    ble.print(incomingPacket.dst.ssid);
    ble.print(F(". Data: "));

    for (int i = 0; i < incomingPacket.len; i++) {
      ble.write(incomingPacket.info[i]);
    }
    ble.println("");
    free(packetData);
  }
}

void setup() {
  while (!Serial);  // required for Flora & Micro
  delay(500);

  Serial.begin(115200);
  if (!ble.begin(VERBOSE_MODE)) {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );
  if (FACTORYRESET_ENABLE) {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ){
      error(F("Couldn't factory reset"));
    }
  }
  /* Disable command echo from Bluefruit */
  ble.echo(false);
  ble.info();
  ble.verbose(false);  // debug info is a little annoying after this point!

  /* Wait for connection */
  while (! ble.isConnected()) {
      delay(500);
  }

  Serial.println(F("******************************"));

  // LED Activity command is only supported from 0.6.6
  if (ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION)) {
    // Change Mode LED Activity
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }

  // Set module to DATA mode
  Serial.println( F("Switching to DATA mode!") );
  ble.setMode(BLUEFRUIT_MODE_DATA);
  
  APRS_init(ADC_REFERENCE, OPEN_SQUELCH);
  APRS_setCallsign("NOCALL", 1);
  APRS_printSettings();
  Serial.print(F("Free RAM:     "));
  Serial.println(freeMemory());

  currentQueueSize = 0;
}

void loop() {
  char n, inputs[BUFSIZE+1];

  //delay(500);
  processPacket();

  boolean hasMessage = false;
  for (int i = 0; ble.available() && i < BUFSIZE && currentQueueSize < QUEUESIZE; i++) {
    outgoingQueue[currentQueueSize][i] = (char) ble.read();
    Serial.print(outgoingQueue[currentQueueSize][i]);
    hasMessage = true;
  }
  if (hasMessage)
    currentQueueSize++;
    //APRS_sendMsg(writeBuffer, strlen(writeBuffer));
}
