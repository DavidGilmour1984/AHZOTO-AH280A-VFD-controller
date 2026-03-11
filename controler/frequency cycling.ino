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
      else
      {
        crc >>= 1;
      }
    }
  }

  return crc;
}

void sendModbus(uint8_t addr, uint8_t func, uint16_t reg, uint16_t value)
{
  uint8_t frame[8];

  frame[0] = addr;
  frame[1] = func;
  frame[2] = reg >> 8;
  frame[3] = reg & 0xFF;
  frame[4] = value >> 8;
  frame[5] = value & 0xFF;

  uint16_t crc = crc16(frame, 6);

  frame[6] = crc & 0xFF;
  frame[7] = crc >> 8;

  Serial.print("TX: ");
  for(int i=0;i<8;i++)
  {
    Serial.printf("%02X ",frame[i]);
  }
  Serial.println();

  digitalWrite(RS485_EN,HIGH);
  VFD.write(frame,8);
  VFD.flush();
  delay(2);
  digitalWrite(RS485_EN,LOW);
}

void setup()
{
  Serial.begin(9600);

  pinMode(RS485_EN,OUTPUT);
  digitalWrite(RS485_EN,LOW);

  VFD.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println("Cycling VFD frequency in 5Hz steps");
}

void loop()
{
  for(int hz = 5; hz <= 50; hz += 5)
  {
    uint16_t raw = hz * 200;   // corrected scaling

    Serial.print("Setting ");
    Serial.print(hz);
    Serial.println(" Hz");

    sendModbus(1, 0x06, 0x1000, raw);

    delay(5000);
  }

  Serial.println("Cycle finished, restarting...");
  delay(5000);
}
