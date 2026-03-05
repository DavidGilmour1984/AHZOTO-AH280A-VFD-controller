#include <HardwareSerial.h>

HardwareSerial VFD(2);

#define RX_PIN 16
#define TX_PIN 17
#define RS485_EN 23

uint8_t slave = 1;

uint16_t crc16(uint8_t *buf, int len)
{
  uint16_t crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++)
  {
    crc ^= (uint16_t)buf[pos];

    for (int i = 8; i != 0; i--)
    {
      if ((crc & 0x0001) != 0)
      {
        crc >>= 1;
        crc ^= 0xA001;
      }
      else crc >>= 1;
    }
  }

  return crc;
}

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

uint16_t readRegister(uint16_t reg)
{
  uint8_t pkt[]={
  slave,
  0x03,
  (uint8_t)(reg>>8),
  (uint8_t)(reg&0xFF),
  0x00,
  0x01
  };

  sendPacket(pkt,6);

  delay(30);

  if(VFD.available()>=7)
  {
    uint8_t buf[7];
    VFD.readBytes(buf,7);

    uint16_t val=(buf[3]<<8)|buf[4];
    return val;
  }

  return 0;
}

void runForward(){ writeRegister(0x2000,1); }
void runReverse(){ writeRegister(0x2000,2); }
void stopMotor(){ writeRegister(0x2000,0); }

void setFreq(float hz)
{
  uint16_t value = hz*100;
  writeRegister(0x2001,value);
}

void sendStatus()
{
  uint16_t freq = readRegister(0x7000);
  uint16_t state = readRegister(0x3000);

  Serial.print("{\"type\":\"status\",\"freq\":");
  Serial.print(freq/100.0);

  Serial.print(",\"state\":");
  Serial.print(state);

  Serial.println("}");
}

void setup()
{
  Serial.begin(9600);

  pinMode(RS485_EN,OUTPUT);

  VFD.begin(9600,SERIAL_8N1,RX_PIN,TX_PIN);

  Serial.println("READY");
}

void loop()
{
  if(Serial.available())
  {
    String cmd=Serial.readStringUntil('\n');
    cmd.trim();

    if(cmd=="RUN") runForward();
    else if(cmd=="REVERSE") runReverse();
    else if(cmd=="STOP") stopMotor();

    else if(cmd.startsWith("FREQ"))
    {
      float hz=cmd.substring(5).toFloat();
      setFreq(hz);
    }
  }

  static unsigned long last=0;

  if(millis()-last>500)
  {
    sendStatus();
    last=millis();
  }
}
