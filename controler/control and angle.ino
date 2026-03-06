#include <HardwareSerial.h>

/* ---------------- RS485 SERIAL PORTS ---------------- */

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

/* ---------------- CONSTANTS ---------------- */

#define COUNTS_PER_REV 32768.0

uint8_t slave = 1;
uint8_t encSlave = 1;

/* ---------------- CRC FUNCTION ---------------- */

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

/* ---------------- VFD FUNCTIONS ---------------- */

void sendPacket(uint8_t *data, int len)
{
  uint16_t crc = crc16(data,len);

  digitalWrite(RS485_EN,HIGH);

  VFD.write(data,len);
  VFD.write(crc & 0xFF);
  VFD.write(crc >> 8);

  VFD.flush();

  digitalWrite(RS485_EN,LOW);
}

void writeRegister(uint16_t reg, uint16_t value)
{
  uint8_t pkt[]={
  slave,
  0x06,
  (uint8_t)(reg>>8),
  (uint8_t)(reg&0xFF),
  (uint8_t)(value>>8),
  (uint8_t)(value&0xFF)
  };

  sendPacket(pkt,6);
}

void runForward(){ writeRegister(0x2000,1); }
void runReverse(){ writeRegister(0x2000,2); }
void stopMotor(){ writeRegister(0x2000,0); }

/* ---------------- ENCODER FUNCTIONS ---------------- */

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

  uint16_t crc = crc16(pkt,6);

  digitalWrite(ENC_EN,HIGH);

  ENC.write(pkt,6);
  ENC.write(crc & 0xFF);
  ENC.write(crc >> 8);

  ENC.flush();

  digitalWrite(ENC_EN,LOW);
}

bool readEncoder(uint32_t &pos)
{
  uint8_t buffer[32];
  int index = 0;
  unsigned long start = millis();

  while(millis() - start < 30)
  {
    while(ENC.available())
    {
      buffer[index++] = ENC.read();

      if(index >= 9)
      {
        for(int i = 0; i <= index - 9; i++)
        {
          if(buffer[i] == encSlave &&
             buffer[i+1] == 0x03 &&
             buffer[i+2] == 0x04)
          {
            uint16_t crc_rx = buffer[i+7] | (buffer[i+8] << 8);
            uint16_t crc_calc = crc16(&buffer[i],7);

            if(crc_rx == crc_calc)
            {
              pos =
              ((uint32_t)buffer[i+3] << 24) |
              ((uint32_t)buffer[i+4] << 16) |
              ((uint32_t)buffer[i+5] << 8)  |
              buffer[i+6];

              return true;
            }
          }
        }
      }

      if(index >= 31) index = 0;
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

  Serial.println("READY");
}

/* ---------------- LOOP ---------------- */

void loop()
{
  /* ----- SERIAL COMMANDS ----- */

  if(Serial.available())
  {
    String cmd=Serial.readStringUntil('\n');
    cmd.trim();

    if(cmd=="RUN") runForward();
    else if(cmd=="REVERSE") runReverse();
    else if(cmd=="STOP") stopMotor();
  }

  /* ----- ENCODER POLLING ----- */

  static unsigned long lastEncoder = 0;

  if(millis() - lastEncoder > 100)
  {
    lastEncoder = millis();

    sendEncoderRequest();

    uint32_t pos;

    if(readEncoder(pos))
    {
      float singleTurn = pos % (uint32_t)COUNTS_PER_REV;
      float angle = singleTurn * 360.0 / COUNTS_PER_REV;

      Serial.print("Angle: ");
      Serial.println(angle,2);
    }
  }
}
