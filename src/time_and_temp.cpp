#include <Arduino.h>
#include <ezTime.h>
#include <ESP8266WiFi.h>
#include "../../CentralBrain/include/WeatherProtocol.h"
#include "../../CentralBrain/include/IngestProtocol.h"
#include <WiFiUdp.h>

#include "creds.h"

#define USE_CENTRAL_BRAIN

#ifdef USE_CENTRAL_BRAIN
IPAddress remote_IP(192, 168, 1, 222);
const static unsigned short RemotePort = 7788;
#else
WiFiUDP Udp;
IPAddress remote_IP(192, 168, 1, 220);
const static unsigned short RemotePort = 5555;
#endif

//D5/6/7 = 14/12/13
#define SHIFT_CLOCK_PIN D5
#define LATCH_CLOCK_PIN D6
#define SERIAL_DATA_PIN D7
//D1 = 5
#define DECIMAL_POINT_PIN D1

Timezone myTZ;
int lastMinute = 0;


//stuff to handle pin swapping
unsigned char OutputToBCD_14[10] =
{
  4, //0
  5, //1
  1, //2
  0, //3
  9, //4
  8, //5
  2, //6
  3, //7
  7, //8
  6  //9
};
// 0123456789
// 3267019854

//1 -> 1 << 3
//2 -> 1 << 1
//4 -> 1 << 0
//8 -> 1 << 2
unsigned char BCDToSerial1_14[10] =
{
  (0 << 2) | (0 << 0) | (0 << 1) | (0 << 3),
  (0 << 2) | (0 << 0) | (0 << 1) | (1 << 3),
  (0 << 2) | (0 << 0) | (1 << 1) | (0 << 3),
  (0 << 2) | (0 << 0) | (1 << 1) | (1 << 3),
  (0 << 2) | (1 << 0) | (0 << 1) | (0 << 3),
  (0 << 2) | (1 << 0) | (0 << 1) | (1 << 3),
  (0 << 2) | (1 << 0) | (1 << 1) | (0 << 3),
  (0 << 2) | (1 << 0) | (1 << 1) | (1 << 3),
  (1 << 2) | (0 << 0) | (0 << 1) | (0 << 3),
  (1 << 2) | (0 << 0) | (0 << 1) | (1 << 3),
};


//1 -> 1 << 7
//2 -> 1 << 5
//4 -> 1 << 4
//8 -> 1 << 6
unsigned char BCDToSerial2_14[10] =
{
  (0 << 6) | (0 << 4) | (0 << 5) | (0 << 7),
  (0 << 6) | (0 << 4) | (0 << 5) | (1 << 7),
  (0 << 6) | (0 << 4) | (1 << 5) | (0 << 7),
  (0 << 6) | (0 << 4) | (1 << 5) | (1 << 7),
  (0 << 6) | (1 << 4) | (0 << 5) | (0 << 7),
  (0 << 6) | (1 << 4) | (0 << 5) | (1 << 7),
  (0 << 6) | (1 << 4) | (1 << 5) | (0 << 7),
  (0 << 6) | (1 << 4) | (1 << 5) | (1 << 7),
  (1 << 6) | (0 << 4) | (0 << 5) | (0 << 7),
  (1 << 6) | (0 << 4) | (0 << 5) | (1 << 7)
};

unsigned char OutputToBCD_12[10] =
{
  5, //0
  6, //1
  7, //2
  3, //3
  2, //4
  4, //5
  1, //6
  8, //7
  9, //8
  0  //9
};

//1 -> 1 << 4
//2 -> 1 << 6
//4 -> 1 << 7
//8 -> 1 << 5
unsigned char BCDToSerial1_12[10] =
{
  (0 << 5) | (0 << 7) | (0 << 6) | (0 << 4),
  (0 << 5) | (0 << 7) | (0 << 6) | (1 << 4),
  (0 << 5) | (0 << 7) | (1 << 6) | (0 << 4),
  (0 << 5) | (0 << 7) | (1 << 6) | (1 << 4),
  (0 << 5) | (1 << 7) | (0 << 6) | (0 << 4),
  (0 << 5) | (1 << 7) | (0 << 6) | (1 << 4),
  (0 << 5) | (1 << 7) | (1 << 6) | (0 << 4),
  (0 << 5) | (1 << 7) | (1 << 6) | (1 << 4),
  (1 << 5) | (0 << 7) | (0 << 6) | (0 << 4),
  (1 << 5) | (0 << 7) | (0 << 6) | (1 << 4),
};

//1 -> 1 << 3
//2 -> 1 << 1
//4 -> 1 << 0
//8 -> 1 << 2
unsigned char BCDToSerial2_12[10] =
{
  0,
  (0 << 2) | (0 << 0) | (0 << 1) | (1 << 3),
  (0 << 2) | (0 << 0) | (1 << 1) | (0 << 3),
  (0 << 2) | (0 << 0) | (1 << 1) | (1 << 3),
  (0 << 2) | (1 << 0) | (0 << 1) | (0 << 3),
  (0 << 2) | (1 << 0) | (0 << 1) | (1 << 3),
  (0 << 2) | (1 << 0) | (1 << 1) | (0 << 3),
  (0 << 2) | (1 << 0) | (1 << 1) | (1 << 3),
  (1 << 2) | (0 << 0) | (0 << 1) | (0 << 3),
  (1 << 2) | (0 << 0) | (0 << 1) | (1 << 3)
};


unsigned char NumberToSerial_14(int number)
{
  int n1 = number % 10;
  int n2 = number / 10;
  unsigned char r1 = BCDToSerial1_14[OutputToBCD_14[n1]];
  unsigned char r2 = BCDToSerial2_14[OutputToBCD_14[n2]];
  //Serial.println("BCD1 for " + String(n1) + " = " + String(OutputToBCD[n1]));
  //Serial.println("Serial for " + String(OutputToBCD[n1]) + " = " + String(r1) + " or 0x" + String(r1, HEX) + " or 0b" + String(r1, BIN));
  //Serial.println("Serial for " + String(number) + " is 0b" + String(r1 | r2, BIN));
  return r1 | r2;
}

unsigned char NumberToSerial_12(int number)
{
  int n1 = number % 10;
  int n2 = number / 10;
  unsigned char r1 = BCDToSerial2_12[OutputToBCD_12[n1]];
  unsigned char r2 = BCDToSerial1_12[OutputToBCD_12[n2]];
  //Serial.println("BCD1 for " + String(n1) + " = " + String(OutputToBCD[n1]));
  //Serial.println("Serial for " + String(OutputToBCD[n1]) + " = " + String(r1) + " or 0x" + String(r1, HEX) + " or 0b" + String(r1, BIN));
  //Serial.println("Serial for " + String(number) + " is 0b" + String(r1 | r2, BIN));
  return r1 | r2;
}

void OutputTimeAndTemp(int hours, int minutes, int temp)
{
  if (temp < 0)
    temp = -temp;

  Serial.println("Time is " + String(hours) + ":" + String(minutes) + ", temp = " + String(temp));
  unsigned char bytes[4];
  bytes[0] = NumberToSerial_14(hours);
  bytes[1] = NumberToSerial_14(minutes);
  bool decimalPoint = true;
  if (temp >= 1000)
  {
    temp /= 10;
    decimalPoint = false;
  }
  if (temp >= 1000)
  {
    //invalid temperature - clear the display 
    bytes[2] = 0xFF;
    bytes[3] = 0xFF;
  }
  else
  {
    bytes[2] = NumberToSerial_12(temp % 10);
    bytes[3] = NumberToSerial_12(temp / 10);
  }

  digitalWrite(LATCH_CLOCK_PIN, LOW);
  for (int i = 0; i < 4; i++)
    shiftOut(SERIAL_DATA_PIN, SHIFT_CLOCK_PIN, MSBFIRST, bytes[i]);
  digitalWrite(LATCH_CLOCK_PIN, HIGH);

  digitalWrite(DECIMAL_POINT_PIN, decimalPoint ? HIGH : LOW);

  lastMinute = minutes;
}

void ClearDisplay()
{
  digitalWrite(LATCH_CLOCK_PIN, LOW);
  for (int i = 0; i < 4; i++)
    shiftOut(SERIAL_DATA_PIN, SHIFT_CLOCK_PIN, MSBFIRST, 0xFF);
  digitalWrite(LATCH_CLOCK_PIN, HIGH);
  digitalWrite(DECIMAL_POINT_PIN, LOW);
}

void debugloop(int delayMS)
{
  for (int i = 0; i < 10; i++)
  {
    OutputTimeAndTemp(11 * i, 11 * i, 111 * i);
    digitalWrite(DECIMAL_POINT_PIN, (i % 2) == 0 ? LOW : HIGH);
    delay(delayMS);
  }
}

int CurrentTemperature = 12333;
unsigned int LastTemperatureResponseTime = 0;

#ifdef USE_CENTRAL_BRAIN
void FetchTemperature()
{
  WiFiClient client;
  client.setTimeout(1000); //be pretty quick
  if (!client.connect(remote_IP, RemotePort))
  {
    Serial.println("Failed to connect to centeral brain");
    return;
  }

  unsigned char pkt[1 + sizeof(WeatherHeader)];
  pkt[0] = DATA_TYPE_WEATHER;
  WeatherHeader* outHeader = (WeatherHeader*)(pkt + 1);
  outHeader->m_DataIncluded = WEATHER_TEMP_BIT;
  int outBytes = client.write(pkt, sizeof(pkt));
  Serial.printf("Out bytes = %d of %d\n", outBytes, sizeof(pkt));
  if (outBytes != sizeof(pkt))
  {
    Serial.println("Failed to write request");
    client.stop();
    return;
  }

  unsigned char inPkt[sizeof(WeatherHeader) + sizeof(TemperatureData)];
  int numBytes = client.readBytes(inPkt, sizeof(inPkt));
  if (numBytes < sizeof(inPkt))
  {
    Serial.printf("Only got %d of %d bytes in response\n", numBytes, sizeof(inPkt));
    client.stop();
    return;
  }

  WeatherHeader* weather = (WeatherHeader*)inPkt;
  TemperatureData* tempData = (TemperatureData*)(weather + 1);
  Serial.printf("new temp is %u or %.1f F\n", tempData->m_Temperature, tempData->m_Temperature / 100.0 * 9 / 5 + 32);

  CurrentTemperature = tempData->m_Temperature;
  LastTemperatureResponseTime = millis();
}

#else

void RequestTemperature()
{
  Serial.println("Requesting temperature...");
  Udp.beginPacket(remote_IP, RemotePort);
  unsigned char pkt[1 + sizeof(struct WeatherHeader)];
  WeatherHeader* weather = (WeatherHeader*)(pkt + 1);
  weather->m_DataIncluded = WEATHER_TEMP_BIT;
  Udp.write(pkt, sizeof(pkt));
  Udp.endPacket();  
}

void CheckForTemperature()
{
  if (!Udp.parsePacket())
  {
    //if we haven't gotten a response in a while, stop displaying the temperature
    if (millis() - LastTemperatureResponseTime > 300 * 1000)
      CurrentTemperature = 55555;  //invalidate it!
    return;
  }

  Serial.println("++++++++++++++++++++++++++++++++++++++++++++++++ got a packet!");
  char buffer[32];
  int bytes = Udp.read(buffer, sizeof(buffer));
  if (bytes >= sizeof(WeatherHeader) + sizeof(TemperatureData))
  {
    WeatherHeader* weather = (WeatherHeader*)buffer;
    TemperatureData* tempData = (TemperatureData*)(weather + 1);
    Serial.println("new temp is " + String(tempData->m_Temperature));
    CurrentTemperature = tempData->m_Temperature;
    LastTemperatureResponseTime = millis();
  }
}
#endif

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Started!");
  pinMode(SHIFT_CLOCK_PIN, OUTPUT);
  pinMode(LATCH_CLOCK_PIN, OUTPUT);
  pinMode(SERIAL_DATA_PIN, OUTPUT);
  //pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DECIMAL_POINT_PIN, OUTPUT);
  digitalWrite(DECIMAL_POINT_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.begin(MY_SSID, MY_WIFI_PASSWORD);
  debugloop(1000);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.println("Waiting for wifi to connect...");
  }
  
  Serial.println("Connected to WiFi, syncing time...");
  waitForSync();
  myTZ.setLocation("America/New_York");
  Serial.println("Ready!");
}

//bool on = true;
unsigned int LastRequestTemperatureTime = 0;

void loop()
{
  events(); //ezTime events()

  unsigned int now = millis();
  if (now - LastRequestTemperatureTime > 10000)
  {
#ifdef USE_CENTRAL_BRAIN
    FetchTemperature();
#else
    RequestTemperature();
#endif
    LastRequestTemperatureTime = now;
  }
#ifndef USE_CENTRAL_BRAIN
  CheckForTemperature();
#endif

  int hour = myTZ.hour();
  int minute = myTZ.minute();

  if (hour == 4 && minute == 10)
  {
    //roll through all of the digits so all of them get a chance to turn on
    //once in a while (preserves life of the tubes)
    debugloop(1000 * 10);
  }

  //between these hours, turn completely off since no one is likely around to read the clock!
  if (hour > 1 && hour < 6)
  {
    ClearDisplay();
  }
  else
  {
    //otherwise, show the time!
    OutputTimeAndTemp(hour, minute, CurrentTemperature);
  }
  delay(100);
}
