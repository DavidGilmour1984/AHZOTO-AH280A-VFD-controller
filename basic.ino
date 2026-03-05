/*
=========================================================
ESP32 RS485 VFD CONTROLLER
=========================================================

This firmware allows an ESP32 to control a VFD over RS485
using Modbus RTU. The ESP32 hosts a WiFi access point with
a simple webpage containing three buttons:

Forward
Reverse
Stop

Motor speed is set using the VFD keypad potentiometer.

---------------------------------------------------------
SUPPORTED VFD
---------------------------------------------------------

This code was written for VFDs using the common Chinese
Modbus register layout where:

Run / Stop / Direction register : 0x2000

Value meanings:

0 = Stop
1 = Run Forward
2 = Run Reverse

Frequency is NOT set by this program and must be controlled
by the keypad potentiometer.

---------------------------------------------------------
IMPORTANT VFD SETTINGS
---------------------------------------------------------

These parameters must be configured on the VFD.

Run command source

F0.02 = 2
Meaning:
2 = Run command from RS485 communication

Frequency command source

F0.03 = 4
Meaning:
4 = Keyboard potentiometer (speed set on keypad)

Communication parameters

FC.00 = 3
Meaning:
9600 baud

FC.01 = 0
Meaning:
8 data bits
No parity
1 stop bit (8N1)

FC.02 = 1
Meaning:
Modbus slave address = 1

If the address is changed, update this line in the code:

uint8_t addr = 1;

---------------------------------------------------------
RS485 WIRING
---------------------------------------------------------

ESP32 UART2 is used.

ESP32 pin connections:

GPIO17  → RS485 DI (driver input)
GPIO16  → RS485 RO (receiver output)
GPIO23  → RS485 DE + RE (direction control)
GND     → RS485 GND

RS485 module to VFD:

RS485 A → VFD A
RS485 B → VFD B
GND     → VFD GND

Note:
Some VFD manufacturers label A/B opposite.
If communication fails, swap A and B.

---------------------------------------------------------
WIFI ACCESS POINT
---------------------------------------------------------

SSID: Revolve Control
Password: (none)

Connect to the ESP32 WiFi network and open:

http://192.168.4.1

---------------------------------------------------------
WEB INTERFACE FUNCTIONS
---------------------------------------------------------

Forward button
Writes Modbus register:

0x2000 = 1

Reverse button

0x2000 = 2

Stop button

0x2000 = 0

---------------------------------------------------------
IMPORTANT SAFETY NOTES
---------------------------------------------------------

1. Ensure motor is properly wired before testing.

2. Always start with a low frequency setting on the VFD
   keypad when first testing communication.

3. The VFD may require a reset after changing control
   source parameters (F0 group).

4. Some VFDs will not run if external terminals are
   configured as the primary control source.

---------------------------------------------------------
*/

#include <WiFi.h>
#include <WebServer.h>

#define RX_PIN 16
#define TX_PIN 17
#define DE_PIN 23

HardwareSerial VFD(2);
WebServer server(80);

uint8_t addr = 1;

const char* ssid = "Revolve Control";
const char* password = "";

uint16_t crc16(uint8_t *buf, int len)
{
  uint16_t crc = 0xFFFF;

  for(int pos=0; pos<len; pos++)
  {
    crc ^= buf[pos];

    for(int i=0;i<8;i++)
    {
      if(crc & 1)
      {
        crc >>=1;
        crc ^=0xA001;
      }
      else crc >>=1;
    }
  }
  return crc;
}

void sendPacket(uint8_t *packet,int len)
{
  uint16_t crc = crc16(packet,len);

  digitalWrite(DE_PIN,HIGH);

  VFD.write(packet,len);
  VFD.write(crc & 0xFF);
  VFD.write(crc >> 8);

  VFD.flush();

  digitalWrite(DE_PIN,LOW);
}

void writeRegister(uint16_t reg,uint16_t value)
{
  uint8_t packet[6];

  packet[0]=addr;
  packet[1]=0x06;
  packet[2]=reg>>8;
  packet[3]=reg&0xFF;
  packet[4]=value>>8;
  packet[5]=value&0xFF;

  sendPacket(packet,6);
}

void runForward()
{
  writeRegister(0x2000,1);
}

void runReverse()
{
  writeRegister(0x2000,2);
}

void stopMotor()
{
  writeRegister(0x2000,0);
}

String page =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"
"<style>"
"body{font-family:Helvetica;text-align:center;background:#f5f7fb}"
"button{font-size:30px;padding:20px;margin:20px;width:250px;border-radius:12px}"
".f{background:#16a34a;color:white}"
".r{background:#f59e0b;color:white}"
".s{background:#dc2626;color:white}"
"</style>"
"</head>"
"<body>"
"<h1>VFD Control</h1>"
"<button class='f' onclick=\"fetch('/f')\">Forward</button><br>"
"<button class='r' onclick=\"fetch('/r')\">Reverse</button><br>"
"<button class='s' onclick=\"fetch('/s')\">Stop</button>"
"</body>"
"</html>";

void handleRoot()
{
  server.send(200,"text/html",page);
}

void handleForward()
{
  runForward();
  server.send(200,"text/plain","Forward");
}

void handleReverse()
{
  runReverse();
  server.send(200,"text/plain","Reverse");
}

void handleStop()
{
  stopMotor();
  server.send(200,"text/plain","Stop");
}

void setup()
{
  Serial.begin(9600);

  pinMode(DE_PIN,OUTPUT);
  digitalWrite(DE_PIN,LOW);

  VFD.begin(9600,SERIAL_8N1,RX_PIN,TX_PIN);

  WiFi.softAP(ssid,password);

  server.on("/",handleRoot);
  server.on("/f",handleForward);
  server.on("/r",handleReverse);
  server.on("/s",handleStop);

  server.begin();

  Serial.println("VFD Controller Ready");
}

void loop()
{
  server.handleClient();
}
