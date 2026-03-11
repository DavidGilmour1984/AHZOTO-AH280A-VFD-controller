#include <HardwareSerial.h>

/* ---------------- VFD PINS ---------------- */

#define RXD2 16
#define TXD2 17
#define RS485_EN 23

/* ---------------- ENCODER PINS ---------------- */

#define ENC_RX 18
#define ENC_TX 19
#define ENC_EN 4

#define COUNTS_PER_REV 32768.0

HardwareSerial VFD(2);
HardwareSerial ENC(1);

uint8_t encSlave = 1;

volatile float encoderAngle = 0;

/* ---------------- CRC16 ---------------- */

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

/* =====================================================
   VFD MODBUS SEND (UNCHANGED)
   ===================================================== */

void sendModbus(uint16_t reg, uint16_t value)
{
  uint8_t frame[8];

  frame[0] = 1;
  frame[1] = 0x06;
  frame[2] = reg >> 8;
  frame[3] = reg & 0xFF;
  frame[4] = value >> 8;
  frame[5] = value & 0xFF;

  uint16_t crc = crc16(frame, 6);

  frame[6] = crc & 0xFF;
  frame[7] = crc >> 8;

  Serial.print("TX: ");
  for (int i = 0; i < 8; i++)
  {
    Serial.printf("%02X ", frame[i]);
  }
  Serial.println();

  delay(5);

  digitalWrite(RS485_EN, HIGH);

  VFD.write(frame, 8);
  VFD.flush();

  delay(12);

  digitalWrite(RS485_EN, LOW);

  delay(5);
}

/* ---------------- CONTROL FUNCTIONS ---------------- */

void setFrequency(float hz)
{
  uint16_t raw = hz * 200;
  sendModbus(0x1000, raw);
}

void runForward()
{
  sendModbus(0x2000, 1);
}

void runReverse()
{
  sendModbus(0x2000, 2);
}

void stopMotor()
{
  sendModbus(0x2000, 0);
}

/* =====================================================
   ENCODER
   ===================================================== */

void sendEncoderRequest()
{
  uint8_t pkt[]={
    encSlave,
    0x03,
    0x00,
    0x00,
    0x00,
    0x01
  };

  uint16_t crc = crc16(pkt,6);

  digitalWrite(ENC_EN,HIGH);

  ENC.write(pkt,6);
  ENC.write(crc & 0xFF);
  ENC.write(crc >> 8);

  ENC.flush();

  delay(3);

  digitalWrite(ENC_EN,LOW);
}

bool readEncoder(uint16_t &pos)
{
  uint8_t buffer[16];
  int index = 0;

  unsigned long start = millis();

  while(millis() - start < 30)
  {
    while(ENC.available())
    {
      buffer[index++] = ENC.read();

      if(index >= 7)
      {
        if(buffer[0] == encSlave &&
           buffer[1] == 0x03 &&
           buffer[2] == 0x02)
        {
          uint16_t crc_rx =
          buffer[5] |
          (buffer[6] << 8);

          uint16_t crc_calc =
          crc16(buffer,5);

          if(crc_rx == crc_calc)
          {
            pos =
            ((uint16_t)buffer[3] << 8) |
             buffer[4];

            return true;
          }
        }
      }

      if(index >= 15) index = 0;
    }
  }

  return false;
}

/* =====================================================
   ENCODER TASK (CORE 1)
   ===================================================== */

void encoderTask(void *pv)
{
  uint16_t pos;

  for(;;)
  {
    sendEncoderRequest();

    if(readEncoder(pos))
    {
      encoderAngle =
      pos * 360.0 / COUNTS_PER_REV;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

/* =====================================================
   SETUP
   ===================================================== */

void setup()
{
  Serial.begin(9600);

  delay(500);

  pinMode(RS485_EN, OUTPUT);
  digitalWrite(RS485_EN, LOW);

  pinMode(ENC_EN, OUTPUT);
  digitalWrite(ENC_EN, LOW);

  VFD.begin(9600, SERIAL_8N1, RXD2, TXD2);
  ENC.begin(9600, SERIAL_8N1, ENC_RX, ENC_TX);

  xTaskCreatePinnedToCore(
    encoderTask,
    "encoderTask",
    4000,
    NULL,
    1,
    NULL,
    1);

  Serial.println("System ready");
}

/* =====================================================
   LOOP (CORE 0)
   ===================================================== */

void loop()
{
  static unsigned long lastPrint = 0;

  /* print encoder angle */

  if(millis() - lastPrint > 200)
  {
    lastPrint = millis();

    Serial.println(encoderAngle,2);
  }

  /* handle commands */

  if (Serial.available())
  {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "s")
    {
      Serial.println("STOP");
      stopMotor();
      return;
    }

    char dir = cmd.charAt(cmd.length() - 1);
    float freq = cmd.substring(0, cmd.length() - 1).toFloat();

    Serial.print("Set frequency ");
    Serial.println(freq);

    setFrequency(freq);

    delay(200);

    if (dir == 'f')
    {
      Serial.println("Forward");
      runForward();
    }
    else if (dir == 'r')
    {
      Serial.println("Reverse");
      runReverse();
    }
  }
}
