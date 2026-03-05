//set F0.02 to 2 (page 11 in manual) to 2 so it can communicate with the vfd

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
      else
        crc >>= 1;
    }
  }

  return crc;
}

void sendPacket(uint8_t *data, int len)
{
  uint16_t crc = crc16(data, len);

  digitalWrite(RS485_EN, HIGH);

  VFD.write(data, len);
  VFD.write(crc & 0xFF);
  VFD.write(crc >> 8);

  VFD.flush();

  digitalWrite(RS485_EN, LOW);

  Serial.print("TX: ");

  for (int i = 0; i < len; i++)
  {
    if (data[i] < 16) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }

  Serial.print((crc & 0xFF), HEX);
  Serial.print(" ");
  Serial.println((crc >> 8), HEX);
}

void readResponse()
{
  delay(50);

  Serial.print("RX: ");

  while (VFD.available())
  {
    uint8_t b = VFD.read();

    if (b < 16) Serial.print("0");
    Serial.print(b, HEX);
    Serial.print(" ");
  }

  Serial.println();
}

void runCommand()
{
  uint8_t pkt[] = {slave, 0x06, 0x20, 0x00, 0x00, 0x01};
  sendPacket(pkt, 6);
  readResponse();
}

void stopCommand()
{
  uint8_t pkt[] = {slave, 0x06, 0x20, 0x00, 0x00, 0x00};
  sendPacket(pkt, 6);
  readResponse();
}

void setFreq(float hz)
{
  uint16_t value = hz * 100;

  uint8_t pkt[] =
  {
    slave,
    0x06,
    0x20,
    0x01,
    (uint8_t)(value >> 8),
    (uint8_t)(value & 0xFF)
  };

  sendPacket(pkt, 6);
  readResponse();
}

void readRegister(uint16_t reg)
{
  uint8_t pkt[] =
  {
    slave,
    0x03,
    (uint8_t)(reg >> 8),
    (uint8_t)(reg & 0xFF),
    0x00,
    0x01
  };

  sendPacket(pkt, 6);
  readResponse();
}

void setup()
{
  Serial.begin(9600);

  VFD.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  pinMode(RS485_EN, OUTPUT);
  digitalWrite(RS485_EN, LOW);

  Serial.println();
  Serial.println("ESP32 VFD Bridge Ready");
  Serial.println("Commands:");
  Serial.println("RUN");
  Serial.println("STOP");
  Serial.println("FREQ 50");
  Serial.println("READ 7000");
  Serial.println("STATE");
}

void loop()
{
  if (Serial.available())
  {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "RUN")
      runCommand();

    else if (cmd == "STOP")
      stopCommand();

    else if (cmd.startsWith("FREQ"))
    {
      float hz = cmd.substring(5).toFloat();
      setFreq(hz);
    }

    else if (cmd.startsWith("READ"))
    {
      uint16_t reg = strtol(cmd.substring(5).c_str(), NULL, 16);
      readRegister(reg);
    }

    else if (cmd == "STATE")
    {
      readRegister(0x3000);
    }
  }
}
