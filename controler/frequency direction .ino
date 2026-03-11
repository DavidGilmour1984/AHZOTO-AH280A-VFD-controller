#include <HardwareSerial.h>

#define RXD2 16
#define TXD2 17
#define RS485_EN 23

HardwareSerial VFD(2);

uint16_t crc16(uint8_t *buf, int len)
{
  uint16_t crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++)
  {
    crc ^= buf[pos];

    for (int i = 0; i < 8; i++)
    {
      if (crc & 1)
      {
        crc >>= 1;
        crc ^= 0xA001;
      }
      else crc >>= 1;
    }
  }

  return crc;
}

void sendModbus(uint16_t reg,uint16_t value)
{
  uint8_t frame[8];

  frame[0]=1;
  frame[1]=0x06;
  frame[2]=reg>>8;
  frame[3]=reg&0xFF;
  frame[4]=value>>8;
  frame[5]=value&0xFF;

  uint16_t crc=crc16(frame,6);

  frame[6]=crc&0xFF;
  frame[7]=crc>>8;

  Serial.print("TX: ");
  for(int i=0;i<8;i++)
  {
    Serial.printf("%02X ",frame[i]);
  }
  Serial.println();

  digitalWrite(RS485_EN,HIGH);

  VFD.write(frame,8);
  VFD.flush();

  delay(5);

  digitalWrite(RS485_EN,LOW);
}

/* -------- CONTROL FUNCTIONS -------- */

void setFrequency(float hz)
{
  uint16_t raw = hz * 200;
  sendModbus(0x1000,raw);
}

void runForward()
{
  sendModbus(0x2000,1);
}

void runReverse()
{
  sendModbus(0x2000,2);
}

void stopMotor()
{
  sendModbus(0x2000,0);
}

/* -------- SETUP -------- */

void setup()
{
  Serial.begin(9600);

  pinMode(RS485_EN,OUTPUT);
  digitalWrite(RS485_EN,LOW);

  VFD.begin(9600,SERIAL_8N1,RXD2,TXD2);

  Serial.println("VFD controller ready");
  Serial.println("Commands:");
  Serial.println("20f = 20Hz forward");
  Serial.println("20r = 20Hz reverse");
  Serial.println("s = stop");
}

/* -------- LOOP -------- */

void loop()
{
  if(Serial.available())
  {
    String cmd=Serial.readStringUntil('\n');
    cmd.trim();

    if(cmd=="s")
    {
      Serial.println("STOP");
      stopMotor();
      return;
    }

    char dir = cmd.charAt(cmd.length()-1);
    float freq = cmd.substring(0,cmd.length()-1).toFloat();

    Serial.print("Set frequency ");
    Serial.println(freq);

    setFrequency(freq);

    delay(200);   // IMPORTANT delay between commands

    if(dir=='f')
    {
      Serial.println("Forward");
      runForward();
    }
    else if(dir=='r')
    {
      Serial.println("Reverse");
      runReverse();
    }
  }
}
