//version 0.1 01.05.2021
//version 1.0 08.05.2021
//B200mini sdr base
// using F()-Makro https://www.arduino.cc/reference/de/language/variables/utilities/progmem/
// https://www.arduinoforum.de/arduino-Thread-F-Makro-f%C3%BCr-Variablen-ein-kleiner-Tipp
//Funktion
//setup:
// 1. start serial
// 2. init INA219
// 3. init A0 - A3
// 4. init R1-4
// 5. init A0-A3
// 6. init MQTT
// 7. init Loop Timer
//Loop (poll A0 - A3, set TX/RX and Relais, send U/I println and mqtt
// A: main loop
// A.1. read band
// A.2. set band
// A.3. read RX TX
// A.4. set RX TX (9cm, 6cm, and 2m)
// A.5. call Communication Loop
// B: Communication Loop
// 1. measure U
// 2. println
// 3. measure I
// 4. println
// 5. read A0 - A3

// for INA219
#include <Wire.h>
#include <Adafruit_INA219.h>

//for Ethernet Shield
#include <SPI.h>
#include <Ethernet.h>

/* for MQTT */
#include <PubSubClient.h> // MQTT Bibliothek

//Adafruit_INA219 ina219;

Adafruit_INA219 ina219_A;
bool bIna219A = false;

// 2. INA219
float ub = 0;       //ina219_A
char  sub[5];
float ib = 0;       //ina219_A
char  sib[10];
//float uShunt =0;

// 4. Init R1-R4
int iRelPin1 = 2;
int iRelPin2 = 3;
int iRelPin3 = 4;
int iRelPin4 = 5;
int iRelPin5 = 6;
int iTX9cm   = 7;
int iTX6cm   = 8;
int iTX2m    = 9;
bool bRelPin1 = true;

// 5. Init A0-A3
int iInpTXRX = A0;
int iInpB1   = A1;
int iInpB2   = A2;
int iInpB3   = A3;
int iBand    = 0;
bool bTX     = false;


// 6. Init MQTT

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network.
// gateway and subnet are optional:
byte mac[] = {
  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x03
};

EthernetClient ethClient;
PubSubClient client;

//char cEthFail_1[] = "Error: renewed fail";


char cMQTTstat[] = "DD0VS/SDR-base/stat";
char cMQTTtxrx[] = "DD0VS/SDR-base/RXTX";
char cMQTTband[] = "DD0VS/SDR-base/band";
char cMQTTvolt[] = "DD0VS/SDR-base/volt";
char cMQTTamps[] = "DD0VS/SDR-base/amps";
char cMQTTntme[] = "nothing to measure";
//char cMQTTalive[] = "DD0VS/SDR-base/stat";
char cBand[2];
char cRXTX[3];
// 6. Init Loop timer

unsigned long ulCommLoopTimerPrevious = 0;
int           iCommLoopStat = 0;

//Functions

void commLoop() {
  if ( (millis() - ulCommLoopTimerPrevious) >= 100) {
    ulCommLoopTimerPrevious = millis();
    //Serial.println("CommLoop");
    switch (iCommLoopStat) {
      case 1: Serial.println(F("case 1")); //publish TX
        iCommLoopStat++;
        //Serial.println("publish start RXTX");
        if (bTX) {
          sprintf(cRXTX, "%s", "TX");
          //client.publish(cMQTTtxrx,"TX");
        } else {
          sprintf(cRXTX, "%s", "RX");
          //client.publish(cMQTTtxrx,"RX");
        }
        client.publish(cMQTTtxrx, cRXTX);
        //Serial.println("publish end");
        break;
      case 2: Serial.println(F("case 2")); //publish Band
        iCommLoopStat++;
        sprintf(cBand, "%d", iBand);
        client.publish(cMQTTband, cBand);
        break;
      case 3: Serial.println(F("case 3")); //measure U
        iCommLoopStat++;
        if (bIna219A) {
          ub = ina219_A.getBusVoltage_V();
          //uShunt = ina219_A.getShuntVoltage_mV();
          // 2. println U
          Serial.print(F("U: "));
          Serial.println(ub);
          //Serial.print(F("Ushunt: "));
          //Serial.println(uShunt);
        }
        else {
          //delay(1000);
          Serial.println(cMQTTntme);
          ub = 12.9;
        }
        break;
      case 4: Serial.println(F("case 4")); //Publish U
        iCommLoopStat++;
        dtostrf(ub, 4, 1, sub);
        client.publish(cMQTTvolt, sub);
        break;
      case 5: Serial.println(F("case 5")); // Measure I
        if (bIna219A) {
          // 3. measure I
          ib = ina219_A.getCurrent_mA();
          Serial.print(F("I: "));
          Serial.println(ib);
        }
        else {
          //delay(1000);
          Serial.println(cMQTTntme);
          ib = 12000.0;
        }
        iCommLoopStat++;
        break;
      case 6: Serial.println(F("case 6")); //publish I
        iCommLoopStat++;
        dtostrf(ib, 7, 1, sib);
        client.publish(cMQTTamps, sib);
        break;
      case 7: Serial.println(F("case 7")); //MQTT loop
        iCommLoopStat++;
        if (!client.connected()) {
          reconnect();
        }
        client.loop();
        break;
      case 8: Serial.println(F("case 8")); //maintain Ehternet
        iCommLoopStat = 1;
        maintainEthernet();
        Serial.println(freeMemory());
        break;
      /*case 6: Serial.println("case 6"); //publish I
              iCommLoopStat++;
              Serial.println("publish start");
              client.publish(cMQTTalive,"alive");
              Serial.println("publish end");
              break; */
      default: Serial.println(F("Default"));
        iCommLoopStat = 1;
        break;
    }
  }
}

//read Band

int readBand() {
  // read LSB last
  int itempBand = 0;
  if (digitalRead(iInpB3)) 
    itempBand++;
  itempBand = itempBand << 1;
  if (digitalRead(iInpB2)) 
    itempBand++;
  itempBand = itempBand << 1;
  if (digitalRead(iInpB1)) 
    itempBand++;
  return itempBand;
}

// set Band
void setBand(int iBand) { 
  switch (iBand) {
    // 8 options possible
    case 0:   //9cm
      digitalWrite(iRelPin1, true); //Rel switches to 1
      digitalWrite(iRelPin2, true); //Rel switches to 1
      digitalWrite(iRelPin3, true); //Rel switches to 1
      digitalWrite(iRelPin4, true); //Rel switches to 1
      digitalWrite(iRelPin5, true); //not used
      break;
    case 1:   //6cm
      digitalWrite(iRelPin1, false); //Rel switches to 2 
      digitalWrite(iRelPin2, false); //Rel switches to 2
      //digitalWrite(iRelPin3, false);
      //digitalWrite(iRelPin4, false);
      //digitalWrite(iRelPin5, false);
      break;
    case 2:   //2m-1
      digitalWrite(iRelPin1, true); //Rel switches to 1 
      digitalWrite(iRelPin2, true); //Rel switches to 1
      digitalWrite(iRelPin3, false); //Rel switches to
      digitalWrite(iRelPin4, false);
      digitalWrite(iRelPin5, true);
      break;
    case 3:   //2m-2
      digitalWrite(iRelPin1, true); //Rel switches to 1 
      digitalWrite(iRelPin2, true); //Rel switches to 1
      digitalWrite(iRelPin3, false); //Rel switches to
      digitalWrite(iRelPin4, false);
      digitalWrite(iRelPin5, false);
      break;
    default:  digitalWrite(iRelPin1, false);
      digitalWrite(iRelPin2, false);
      digitalWrite(iRelPin3, false);
      digitalWrite(iRelPin4, false);
      digitalWrite(iRelPin5, false);
      break;
  }
}

// set TX
void setTX(int iBand, bool _bTX) {
  switch (iBand) {
    case 0: digitalWrite(iTX9cm, _bTX);
      digitalWrite(iTX6cm, false);
      digitalWrite(iTX2m, false);
      break;
deault: digitalWrite(iTX9cm, false);
      digitalWrite(iTX6cm, false);
      digitalWrite(iTX2m, false);
      break;

  }
}

// MQTT - Ethernet
void reconnect() {
  // Solange wiederholen bis Verbindung wiederhergestellt ist
  while (!client.connected()) {
    Serial.print(F("try MQTT con..."));
    //Versuch die Verbindung aufzunehmen
    if (client.connect("SDR-Base-Clnt")) {
      Serial.println("connected!");
      // Nun versendet der Arduino eine Nachricht in outTopic ...
      client.publish("DD0VS/SDR-base", "connected");
      // und meldet sich für bei inTopic für eingehende Nachrichten an
      //client.subscribe("inTopic");
    } else {
      Serial.print(F("Fehler, rc="));
      Serial.print(client.state());
      Serial.println(F(" next trail in 5 sec"));
      // 5 Sekunden Pause vor dem nächsten Versuch
      //delay(5000);
    }
  }
}

void maintainEthernet() {
  switch (Ethernet.maintain()) {
    case 1:
      //renewed fail
      Serial.println(F("Err: fail"));
      break;

    case 2:
      //renewed success
      Serial.println(F("Renew succ."));
      //print your local IP address:
      Serial.print(F("IP addr: "));
      Serial.println(Ethernet.localIP());
      break;

    case 3:
      //rebind fail
      Serial.println(F("Err: rebind fail"));
      break;

    case 4:
      //rebind success
      Serial.println(F("Rebind success"));
      //print your local IP address:
      Serial.print(F("My IP address: "));
      Serial.println(Ethernet.localIP());
      break;

    default:
      //nothing happened
      break;
  }
  //Serial.println("ethernet maintain end");
}

// myDelay
void myDelay(int iMyDel) {
  unsigned int uiMyDelMillis;

  uiMyDelMillis = millis();
  while ( (millis() - uiMyDelMillis) < iMyDel)
  {}

}

void setup() {
  client.setKeepAlive(1000);

  // put your setup code here, to run once:
  //1. start serial
  Serial.begin(9600);
  Serial.println(F("System Startup - SDR - Base"));
  //2. init INA219
  bIna219A = ina219_A.begin();
  if (!bIna219A)
    Serial.println(F("INA219_A not exists"));

  // 4. init R1-R4
  pinMode(iRelPin1, OUTPUT);
  pinMode(iRelPin2, OUTPUT);
  pinMode(iRelPin3, OUTPUT);
  pinMode(iRelPin4, OUTPUT);

  Serial.println(F("Test Relais 1"));
  digitalWrite(iRelPin1, bRelPin1);
  bRelPin1 = !bRelPin1;
  myDelay(500);
  Serial.println(F("Test Relais !1"));
  digitalWrite(iRelPin1, bRelPin1);
  bRelPin1 = !bRelPin1;
  myDelay(500);
  Serial.println(F("Test Relais 1"));
  digitalWrite(iRelPin1, bRelPin1);
  Serial.println(F("Test Relais end"));

  // 5. Init A0-A3
  pinMode(iInpTXRX, INPUT);
  pinMode(iInpB1, INPUT);
  pinMode(iInpB2, INPUT);
  pinMode(iInpB3, INPUT);


  // start the Ethernet connection:
  Serial.println(F("Initialize Ethernet with DHCP:"));
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Failed to configure Ethernet using DHCP"));
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println(F("Ethernet shield was not found.  Sorry, can't run without hardware. :("));
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println(F("Ethernet cable is not connected."));
    }
    // no point in carrying on, so do nothing forevermore:
    while (true) {
      //delay(1);
    }
  }
  // print your local IP address:
  Serial.print(F("My IP address: "));
  Serial.println(Ethernet.localIP());

  client.setClient(ethClient);
  client.setServer("shf-sdr", 1883);

  // End
  Serial.println(F("loop end"));
}

void loop() {
  // put your main code here, to run repeatedly:

  //read Band
  iBand = readBand();
  //set Band
  if (!bTX) { // never change band during TX
    setBand(iBand);
  }
  //read TX
  bTX = digitalRead(iInpTXRX);

  //set TX
  setTX(iBand, bTX);

  //communication Loop
  commLoop();
  
}

// https://learn.adafruit.com/memories-of-an-arduino/measuring-free-memory
// for debug reasons

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}
