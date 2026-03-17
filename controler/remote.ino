#include <HardwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

/* ---------------- LCD ---------------- */

LiquidCrystal_I2C lcd(0x27,20,4);

/* ---------------- KEYPAD ---------------- */

const byte ROWS=4;
const byte COLS=4;

char keys[ROWS][COLS]={
{'1','2','3','A'},
{'4','5','6','B'},
{'7','8','9','C'},
{'*','0','#','D'}
};

byte rowPins[ROWS]={32,33,25,26};
byte colPins[COLS]={27,14,12,13};

Keypad keypad=Keypad(makeKeymap(keys),rowPins,colPins,ROWS,COLS);

/* ---------------- VFD ---------------- */

#define RXD2 16
#define TXD2 17
#define RS485_EN 23

HardwareSerial VFD(2);

/* ---------------- ENCODER ---------------- */

#define ENC_RX 19
#define ENC_TX 18
#define ENC_EN 4

#define COUNTS_PER_REV 32768.0

HardwareSerial ENC(1);

uint8_t encSlave=1;

volatile float encoderAngle=0;

/* ---------------- STATE ---------------- */

String entry="";
String currentCommand="";

bool moving=false;
bool jogActive=false;

char direction=0;

float targetAngle=0;
float baseFreq=0;
float lastFreq=0;

/* emergency stop */

bool starHeld=false;
unsigned long starPressTime=0;

/* ---------------- CRC ---------------- */

uint16_t crc16(uint8_t *buf,int len)
{
uint16_t crc=0xFFFF;

for(int pos=0;pos<len;pos++)
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

/* ---------------- MODBUS ---------------- */

void sendModbus(uint16_t reg,uint16_t value)
{
uint8_t frame[8];

frame[0]=1;
frame[1]=0x06;
frame[2]=reg>>8;
frame[3]=reg & 0xFF;
frame[4]=value>>8;
frame[5]=value & 0xFF;

uint16_t crc=crc16(frame,6);

frame[6]=crc & 0xFF;
frame[7]=crc >> 8;

digitalWrite(RS485_EN,HIGH);

VFD.write(frame,8);
VFD.flush();

delay(10);

digitalWrite(RS485_EN,LOW);
}

void setFrequency(float hz)
{
uint16_t raw=hz*200;
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

/* ---------------- ENCODER ---------------- */

void sendEncoderRequest()
{
uint8_t pkt[]={encSlave,0x03,0x00,0x00,0x00,0x01};

uint16_t crc=crc16(pkt,6);

digitalWrite(ENC_EN,HIGH);

ENC.write(pkt,6);
ENC.write(crc & 0xFF);
ENC.write(crc>>8);

ENC.flush();

delay(3);

digitalWrite(ENC_EN,LOW);
}

bool readEncoder(uint16_t &pos)
{
uint8_t buffer[16];
int index=0;

unsigned long start=millis();

while(millis()-start<30)
{
while(ENC.available())
{
buffer[index++]=ENC.read();

if(index>=7)
{
if(buffer[0]==encSlave && buffer[1]==0x03 && buffer[2]==0x02)
{
uint16_t crc_rx=buffer[5]|(buffer[6]<<8);
uint16_t crc_calc=crc16(buffer,5);

if(crc_rx==crc_calc)
{
pos=((uint16_t)buffer[3]<<8)|buffer[4];
return true;
}
}
}

if(index>=15) index=0;
}
}

return false;
}

/* ---------------- ENCODER TASK ---------------- */

void encoderTask(void *pv)
{
uint16_t pos;

for(;;)
{
sendEncoderRequest();

if(readEncoder(pos))
encoderAngle=pos*360.0/COUNTS_PER_REV;

vTaskDelay(100/portTICK_PERIOD_MS);
}
}

/* ---------------- DISPLAY ---------------- */

void updateDisplay()
{

lcd.setCursor(0,0);
lcd.print("Position");
lcd.setCursor(12,0);
lcd.print("Status");

lcd.setCursor(0,1);
lcd.print(encoderAngle,2);
lcd.print((char)223);
lcd.print("        ");

lcd.setCursor(12,1);

if(moving)
{
if(direction=='C') lcd.print("RUN CW ");
else lcd.print("RUN ACW");
}
else lcd.print("STOP   ");

lcd.setCursor(0,2);
lcd.print("Current: ");
lcd.print(currentCommand);
lcd.print("       ");

lcd.setCursor(0,3);
lcd.print("Set: ");
lcd.print(entry);
lcd.print("           ");
}

/* ---------------- SETUP ---------------- */

void setup()
{

Serial.begin(9600);

Wire.begin(21,22);

lcd.init();
lcd.backlight();

pinMode(RS485_EN,OUTPUT);
digitalWrite(RS485_EN,LOW);

pinMode(ENC_EN,OUTPUT);
digitalWrite(ENC_EN,LOW);

VFD.begin(9600,SERIAL_8N1,RXD2,TXD2);
ENC.begin(9600,SERIAL_8N1,ENC_RX,ENC_TX);

xTaskCreatePinnedToCore(
encoderTask,
"encoderTask",
4000,
NULL,
1,
NULL,
1);

}

/* ---------------- LOOP ---------------- */

void loop()
{

char key=keypad.getKey();

/* ---------- keypad typing ---------- */

if(key)
{

if(key>='0' && key<='9')
entry+=key;

else if(key=='A' || key=='C')
entry+=key;

/* execute */

else if(key=='#')
{

currentCommand=entry;

int dirIndex=entry.indexOf('A');
if(dirIndex==-1) dirIndex=entry.indexOf('C');

if(dirIndex>0)
{

baseFreq=entry.substring(0,dirIndex).toFloat();
direction=entry.charAt(dirIndex);
targetAngle=entry.substring(dirIndex+1).toFloat();

setFrequency(baseFreq);

delay(200);

if(direction=='C') runForward();
if(direction=='A') runReverse();

moving=true;

entry="";
}
}

/* clear */

else if(key=='*')
{
entry="";
starHeld=true;
starPressTime=millis();
}

/* jog ACW */

else if(key=='B')
{
setFrequency(5);
runReverse();
jogActive=true;
}

/* jog CW */

else if(key=='D')
{
setFrequency(5);
runForward();
jogActive=true;
}

}

/* ---------- jog stop ---------- */

if(jogActive && keypad.getState()==IDLE)
{
stopMotor();
jogActive=false;
}

/* ---------- emergency stop ---------- */

if(starHeld)
{
if(millis()-starPressTime>2000)
{
stopMotor();
moving=false;
currentCommand="";
starHeld=false;
}
}

if(!key) starHeld=false;

/* ---------- motion control ---------- */

if(moving)
{

float error=targetAngle-encoderAngle;

if(error>180) error-=360;
if(error<-180) error+=360;

float absError=abs(error);

/* exponential slowdown */

if(absError<10 && absError>0.3)
{

float newFreq=1+(baseFreq-1)*(absError/10.0)*(absError/10.0);

if(abs(newFreq-lastFreq)>0.2)
{
setFrequency(newFreq);
lastFreq=newFreq;
}

}

/* stop conditions */

if(absError<=0.3 ||
(direction=='C' && error<0) ||
(direction=='A' && error>0))
{

stopMotor();

moving=false;
currentCommand="";
lastFreq=0;

}

}

/* ---------- serial commands ---------- */

if (Serial.available())
{

String cmd=Serial.readStringUntil('\n');
cmd.trim();

if(cmd=="s")
{
stopMotor();
moving=false;
currentCommand="";
return;
}

char dir=cmd.charAt(cmd.length()-1);
float freq=cmd.substring(0,cmd.length()-1).toFloat();

setFrequency(freq);

delay(200);

if(dir=='f') runForward();
else if(dir=='r') runReverse();

}

/* ---------- update LCD ---------- */

updateDisplay();

}
