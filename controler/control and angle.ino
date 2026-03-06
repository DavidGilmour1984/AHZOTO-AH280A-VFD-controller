#include <WiFi.h>
#include <WebServer.h>

#define VFD_RX 16
#define VFD_TX 17
#define VFD_DE 23

#define ENC_RX 18
#define ENC_TX 19
#define ENC_DE 22

HardwareSerial VFD(2);
HardwareSerial ENC(1);

WebServer server(80);

uint8_t addr = 1;

const char* ssid = "Revolve Control";
const char* password = "";

uint16_t encoderPosition = 0;
float encoderAngle = 0.0;

float setFrequency = 0;
float vfdFrequency = 0;

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

  digitalWrite(VFD_DE,HIGH);

  VFD.write(packet,len);
  VFD.write(crc & 0xFF);
  VFD.write(crc >> 8);

  VFD.flush();

  digitalWrite(VFD_DE,LOW);
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

void setVFDfreq(float hz)
{
  uint16_t value = hz * 100;
  writeRegister(0x2001,value);
}

void readVFDFrequency()
{
  uint8_t frame[8];

  frame[0]=addr;
  frame[1]=0x03;
  frame[2]=0x21;
  frame[3]=0x03;
  frame[4]=0x00;
  frame[5]=0x01;

  uint16_t crc = crc16(frame,6);

  frame[6]=crc & 0xFF;
  frame[7]=crc >> 8;

  digitalWrite(VFD_DE,HIGH);
  VFD.write(frame,8);
  VFD.flush();
  digitalWrite(VFD_DE,LOW);

  delay(5);

  if(VFD.available() >= 7)
  {
    uint8_t resp[7];

    for(int i=0;i<7;i++)
      resp[i]=VFD.read();

    uint16_t raw = resp[3]<<8 | resp[4];

    vfdFrequency = raw / 100.0;
  }
}

void readEncoder()
{
  uint8_t frame[8];

  frame[0]=1;
  frame[1]=0x03;
  frame[2]=0x00;
  frame[3]=0x00;
  frame[4]=0x00;
  frame[5]=0x01;

  uint16_t crc = crc16(frame,6);

  frame[6]=crc & 0xFF;
  frame[7]=crc >> 8;

  digitalWrite(ENC_DE,HIGH);
  ENC.write(frame,8);
  ENC.flush();
  digitalWrite(ENC_DE,LOW);

  delay(5);

  if(ENC.available() >= 7)
  {
    uint8_t resp[7];

    for(int i=0;i<7;i++)
      resp[i]=ENC.read();

    encoderPosition = resp[3]<<8 | resp[4];

    encoderAngle = encoderPosition * 360.0 / 32768.0;
  }
}

String page =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"
"<style>"
"body{font-family:Helvetica;text-align:center;background:#f5f7fb}"
"h1{font-size:40px}"
".angle{font-size:70px;margin:30px}"
"button{font-size:30px;padding:20px;margin:20px;width:250px;border-radius:12px}"
".f{background:#16a34a;color:white}"
".r{background:#f59e0b;color:white}"
".s{background:#dc2626;color:white}"
".set{background:#2563eb;color:white}"
"input{font-size:28px;padding:10px;width:200px;text-align:center}"
"</style>"
"<script>"
"function update(){"

"fetch('/angle').then(r=>r.text()).then(t=>{"
"document.getElementById('angle').innerHTML=t+' deg';"
"});"

"fetch('/freq').then(r=>r.text()).then(t=>{"
"document.getElementById('freqread').innerHTML=t+' Hz';"
"});"

"}"

"setInterval(update,300);"

"function setFreq(){"
"let f=document.getElementById('freq').value;"
"fetch('/setfreq?f='+f);"
"}"

"</script>"
"</head>"
"<body onload='update()'>"

"<h1>Encoder Angle</h1>"
"<div id='angle' class='angle'>0</div>"

"<h1>VFD Frequency</h1>"
"<div id='freqread' class='angle'>0</div>"

"<h1>Set Frequency</h1>"
"<input id='freq' type='number' step='0.1'>"
"<br>"
"<button class='set' onclick='setFreq()'>Set Frequency</button>"

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

void handleAngle()
{
  server.send(200,"text/plain",String(encoderAngle,2));
}

void handleFreq()
{
  server.send(200,"text/plain",String(vfdFrequency,2));
}

void handleSetFreq()
{
  if(server.hasArg("f"))
  {
    float f = server.arg("f").toFloat();
    setFrequency = f;
    setVFDfreq(f);
  }

  server.send(200,"text/plain","OK");
}

void setup()
{
  Serial.begin(9600);

  pinMode(VFD_DE,OUTPUT);
  digitalWrite(VFD_DE,LOW);

  pinMode(ENC_DE,OUTPUT);
  digitalWrite(ENC_DE,LOW);

  VFD.begin(9600,SERIAL_8N1,VFD_RX,VFD_TX);
  ENC.begin(9600,SERIAL_8N1,ENC_RX,ENC_TX);

  WiFi.softAP(ssid,password);

  server.on("/",handleRoot);
  server.on("/f",handleForward);
  server.on("/r",handleReverse);
  server.on("/s",handleStop);
  server.on("/angle",handleAngle);
  server.on("/freq",handleFreq);
  server.on("/setfreq",handleSetFreq);

  server.begin();

  Serial.println("VFD Controller Ready");
}

void loop()
{
  readEncoder();
  readVFDFrequency();
  server.handleClient();
}
