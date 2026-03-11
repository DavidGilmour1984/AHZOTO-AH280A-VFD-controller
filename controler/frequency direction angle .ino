#include <HardwareSerial.h>

/* ---------------- RS485 PORTS ---------------- */

HardwareSerial VFD(2);
HardwareSerial ENC(1);

/* ---------------- VFD PINS ---------------- */

#define VFD_RX 16
#define VFD_TX 17
#define RS485_EN 23

/* ---------------- ENCODER PINS ---------------- */

#define ENC_RX 18
#define ENC_TX 19
#define ENC_EN 4

#define COUNTS_PER_REV 32768.0

uint8_t slave = 1;
uint8_t encSlave = 1;

/* ---------------- STATE ---------------- */

float commandedHz = 0;
int direction = 0;   // -1 reverse, 0 stop, 1 forward

/* ---------------- CRC ---------------- */

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

/* ---------------- VFD WRITE ---------------- */

void sendModbus(uint16_t reg,uint16_t value)
{
  uint8_t frame[8];

  frame[0]=slave;
  frame[1]=0x06;
  frame[2]=reg>>8;
  frame[3]=reg&0xFF;
  frame[4]=value>>8;
  frame[5]=value&0xFF;

  uint16_t crc=crc16(frame,6);

  frame[6]=crc&0xFF;
  frame[7]=crc>>8;

  digitalWrite(RS485_EN,HIGH);
  VFD.write(frame,8);
  VFD.flush();
  delay(3);
  digitalWrite(RS485_EN,LOW);
}

/* ---------------- VFD READ ---------------- */

bool readRegister(uint16_t reg,uint16_t &value)
{
  uint8_t pkt[]={
    slave,
    0x03,
    (uint8_t)(reg>>8),
    (uint8_t)(reg&0xFF),
    0x00,
    0x01
  };

  uint16_t crc=crc16(pkt,6);

  digitalWrite(RS485_EN,HIGH);
  VFD.write(pkt,6);
  VFD.write(crc&0xFF);
  VFD.write(crc>>8);
  VFD.flush();
  digitalWrite(RS485_EN,LOW);

  uint8_t buffer[16];
  int idx=0;
  unsigned long start=millis();

  while(millis()-start<20)
  {
    while(VFD.available())
    {
      buffer[idx++]=VFD.read();
      if(idx>=7)
      {
        if(buffer[0]==slave && buffer[1]==0x03)
        {
          value=(buffer[3]<<8)|buffer[4];
          return true;
        }
      }
    }
  }

  return false;
}

/* ---------------- CONTROL ---------------- */

void setFrequency(float hz)
{
  commandedHz=hz;
  uint16_t raw = hz * 200;
  sendModbus(0x1000,raw);
}

void runForward()
{
  direction=1;
  sendModbus(0x2000,1);
}

void runReverse()
{
  direction=-1;
  sendModbus(0x2000,2);
}

void stopMotor()
{
  direction=0;
  sendModbus(0x2000,0);
}

/* ---------------- ENCODER ---------------- */

void sendEncoderRequest()
{
  uint8_t pkt[]={
    encSlave,
    0x03,
    0x00,
    0x00,
    0x00,
    0x02
  };

  uint16_t crc=crc16(pkt,6);

  digitalWrite(ENC_EN,HIGH);
  ENC.write(pkt,6);
  ENC.write(crc&0xFF);
  ENC.write(crc>>8);
  ENC.flush();
  digitalWrite(ENC_EN,LOW);
}

bool readEncoder(uint32_t &pos)
{
  uint8_t buffer[32];
  int index=0;
  unsigned long start=millis();

  while(millis()-start<20)
  {
    while(ENC.available())
    {
      buffer[index++]=ENC.read();

      if(index>=9)
      {
        if(buffer[0]==encSlave && buffer[1]==0x03 && buffer[2]==0x04)
        {
          uint16_t crc_rx=buffer[7]|(buffer[8]<<8);
          uint16_t crc_calc=crc16(buffer,7);

          if(crc_rx==crc_calc)
          {
            pos=
            ((uint32_t)buffer[3]<<24)|
            ((uint32_t)buffer[4]<<16)|
            ((uint32_t)buffer[5]<<8)|
            buffer[6];

            return true;
          }
        }
      }

      if(index>=31) index=0;
    }
  }

  return false;
}

/* ---------------- SETUP ---------------- */

void setup()
{
  Serial.begin(9600);

  pinMode(RS485_EN,OUTPUT);
  pinMode(ENC_EN,OUTPUT);

  digitalWrite(RS485_EN,LOW);
  digitalWrite(ENC_EN,LOW);

  VFD.begin(9600,SERIAL_8N1,VFD_RX,VFD_TX);
  ENC.begin(9600,SERIAL_8N1,ENC_RX,ENC_TX);

  Serial.println("time,angle_deg,cmd_hz,dir,vfd_freq,vfd_current,vfd_voltage");
}

/* ---------------- LOOP ---------------- */

void loop()
{
  /* ----- SERIAL COMMANDS ----- */

  if(Serial.available())
  {
    String cmd=Serial.readStringUntil('\n');
    cmd.trim();

    if(cmd=="s") stopMotor();
    else
    {
      char dir=cmd.charAt(cmd.length()-1);
      float freq=cmd.substring(0,cmd.length()-1).toFloat();

      setFrequency(freq);
      delay(200);

      if(dir=='f') runForward();
      if(dir=='r') runReverse();
    }
  }

  /* ----- 20Hz TELEMETRY ----- */

  static unsigned long last=0;

  if(millis()-last>=50)
  {
    last=millis();

    uint32_t pos;
    float angle=0;

    if(readEncoder(pos))
    {
      float singleTurn = pos % (uint32_t)COUNTS_PER_REV;
      angle = singleTurn * 360.0 / COUNTS_PER_REV;
    }

    uint16_t freqRaw=0;
    uint16_t currentRaw=0;
    uint16_t voltageRaw=0;

    readRegister(0x2103,freqRaw);
    readRegister(0x2104,currentRaw);
    readRegister(0x2105,voltageRaw);

    float vfdFreq = freqRaw/100.0;
    float current = currentRaw/10.0;
    float voltage = voltageRaw;

    Serial.print(millis());
    Serial.print(",");
    Serial.print(angle,2);
    Serial.print(",");
    Serial.print(commandedHz,2);
    Serial.print(",");
    Serial.print(direction);
    Serial.print(",");
    Serial.print(vfdFreq,2);
    Serial.print(",");
    Serial.print(current,2);
    Serial.print(",");
    Serial.println(voltage,1);
  }
}
