#include <EmonLib.h>          // Include Emon Library for current sensor
#include <HardwareSerial.h>   // Hardware Serial for SIM800
#include <ZMPT101B.h>         // Library for ZMPT101B voltage sensor

// Current Sensor Setup
EnergyMonitor emon1;
const int currentSensorPin = 34; // GPIO for SCT-013 current sensor

// Voltage Sensor Setup
#define SENSITIVITY 500.0f
#define VOLTAGE_SENSOR_PIN 14
ZMPT101B voltageSensor(VOLTAGE_SENSOR_PIN, 60.0);

// SIM800 Module Setup
#define RX_PIN 17
#define TX_PIN 16
HardwareSerial SIM800(2);
int initLed = 12;


// GPRS and Firebase settings
const String APN = "internet.globe.com.ph";
const String USER = "";
const String PASS = "";
const String FIREBASE_HOST = "https://eyyyyyy-b4a2f-default-rtdb.asia-southeast1.firebasedatabase.app/11311111";
const String FIREBASE_SECRET = "PXTMwuIRlr4LYiThjW6jQqsts5aayZRa4uUfCWhV";
//your-firebase-database-key
#define USE_SSL true
const int DELAY_MS = 250;

// Function Declarations
void initGSM();
void gprsConnect();
boolean gprsDisconnect();
boolean isGPRSConnected();
void postToFirebase(String data);
boolean waitResponse(String expectedAnswer = "OK", unsigned int timeout = 2000);

void setup() {
  pinMode(initLed, OUTPUT);
  for(int x = 0; x < 3; x++){
    digitalWrite(initLed, HIGH);
    delay(50);
    digitalWrite(initLed, LOW);
    delay(50);
  }
  digitalWrite(initLed, HIGH);
    delay(1000);
    digitalWrite(initLed, LOW);
    delay(50);

  Serial.begin(115200);
  SIM800.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  
  // Initialize Emon library for current sensor
  emon1.current(currentSensorPin, 0.16);

  // Initialize voltage sensor
  voltageSensor.setSensitivity(SENSITIVITY);

  Serial.println("Initializing SIM800...");
  initGSM();
}

void loop() {
  digitalWrite(initLed, HIGH);
  delay(50);
  digitalWrite(initLed, LOW);
  delay(50);
  digitalWrite(initLed, HIGH);
  delay(500);
  digitalWrite(initLed, LOW);
  // Read current (A) and voltage (V)
  double Irms = emon1.calcIrms(1480); // Calculate RMS current
  float voltage = voltageSensor.getRmsVoltage(); // Calculate RMS voltage

  // Calculate Power (P = V * I)
  double power = voltage * Irms; // Power in watts

  // Energy consumed in this loop iteration (in kWh)
  // Power (W) * time (hr) = Energy (Wh)

  // Prepare data to send
  String unixTimestamp = getUnixTimestamp(); // Fetch the current Unix timestamp

  // Prepare data to send, now including the timestamp
  String dataToSend = "{\"current\":" + String(Irms) + ", \"voltage\":" + String(voltage) + ", \"power\":" + String(power) + ", \"timestamp\":" + unixTimestamp + "}";
  Serial.println(dataToSend);

  // Ensure GPRS connection and post data
  if (!isGPRSConnected()) {
    gprsConnect();
  }
  postToFirebase(dataToSend);

  delay(600); // Delay for stability and data posting interval
}

String getUnixTimestamp() {
  SIM800.println("AT+HTTPINIT");
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+HTTPPARA=\"CID\",1");
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+HTTPPARA=\"URL\",\"http://worldtimeapi.org/api/timezone/Asia/Hong_Kong\"");
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+HTTPACTION=0"); // 0 for GET
  waitResponse("OK", 10000); // Wait for a longer time for the response
  delay(DELAY_MS);

  SIM800.println("AT+HTTPREAD");
  delay(DELAY_MS);

  String response = "";
  while (SIM800.available()) {
    char c = SIM800.read();
    response += c;
    delay(10); // Short delay to allow buffer to fill
  }

  SIM800.println("AT+HTTPTERM");
  waitResponse("OK", 1000);
  delay(DELAY_MS);

  // Extract Unix time from the response
  int unixTimeIndex = response.indexOf("\"unixtime\":");
  if (unixTimeIndex > 0) {
    int startIndex = unixTimeIndex + 11; // Length of "\"unixtime\":"
    int endIndex = response.indexOf(',', startIndex);
    String unixTimeString = response.substring(startIndex, endIndex);
    return unixTimeString;
  }

  return ""; // Return empty string if Unix time not found
}

void postToFirebase(String data) {
  SIM800.println("AT+HTTPINIT");
  waitResponse();
  delay(DELAY_MS);

  if (USE_SSL) {
    SIM800.println("AT+HTTPSSL=1");
    waitResponse();
    delay(DELAY_MS);
  }

  SIM800.println("AT+HTTPPARA=\"CID\",1");
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+HTTPPARA=\"URL\"," + FIREBASE_HOST + ".json?auth=" + FIREBASE_SECRET);
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+HTTPPARA=\"REDIR\",1");
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+HTTPDATA=" + String(data.length()) + ",10000");
  waitResponse("DOWNLOAD");

  SIM800.println(data);
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+HTTPACTION=1");

  for (uint32_t start = millis(); millis() - start < 20000;) {
    while (!SIM800.available());
    String response = SIM800.readString();
    if (response.indexOf("+HTTPACTION:") > 0) {
      Serial.println(response);
      break;
    }
  }

  delay(DELAY_MS);

  SIM800.println("AT+HTTPREAD");
  waitResponse("OK");
  delay(DELAY_MS);

  SIM800.println("AT+HTTPTERM");
  waitResponse("OK", 1000);
  delay(DELAY_MS);
}

void initGSM() {
  SIM800.println("AT");
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+CPIN?");
  waitResponse("+CPIN: READY");
  delay(DELAY_MS);

  SIM800.println("AT+CFUN=1");
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+CMEE=2");
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+CBATCHK=1");
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+CREG?");
  waitResponse("+CREG: 0,");
  delay(DELAY_MS);

  SIM800.print("AT+CMGF=1\r");
  waitResponse("OK");
  delay(DELAY_MS);
}

void gprsConnect() {
  SIM800.println("AT+SAPBR=0,1");
  waitResponse("OK", 60000);
  delay(DELAY_MS);

  SIM800.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  waitResponse();
  delay(DELAY_MS);

  SIM800.println("AT+SAPBR=3,1,\"APN\"," + APN);
  waitResponse();
  delay(DELAY_MS);

  if (USER != "") {
    SIM800.println("AT+SAPBR=3,1,\"USER\"," + USER);
    waitResponse();
    delay(DELAY_MS);
  }

  if (PASS != "") {
    SIM800.println("AT+SAPBR=3,1,\"PASS\"," + PASS);
    waitResponse();
    delay(DELAY_MS);
  }

  SIM800.println("AT+SAPBR=1,1");
  waitResponse("OK", 30000);
  delay(DELAY_MS);

  SIM800.println("AT+SAPBR=2,1");
  waitResponse("OK");
  delay(DELAY_MS);
}

boolean gprsDisconnect() {
  SIM800.println("AT+CGATT=0");
  waitResponse("OK", 60000);
  return true;
}

boolean isGPRSConnected() {
  SIM800.println("AT+CGATT?");
  return waitResponse("+CGATT: 1", 6000) != 1;
}

boolean waitResponse(String expectedAnswer, unsigned int timeout) {
  uint8_t x = 0, answer = 0;
  String response;
  unsigned long previous;

  while (SIM800.available() > 0) SIM800.read();

  previous = millis();
  do {
    if (SIM800.available() != 0) {
      char c = SIM800.read();
      response.concat(c);
      x++;
      if (response.indexOf(expectedAnswer) > 0) {
        answer = 1;
      }
    }
  } while ((answer == 0) && ((millis() - previous) < timeout));

  Serial.println(response);
  return answer;
}
