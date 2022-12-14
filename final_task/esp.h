#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <FS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>    
//#include <PMS.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <DS3231.h>// already included Wire.h
#include <Adafruit_HTU21DF.h>
//#include <random>

#define PIN_BUTTON D0  //for v2.1
#define PIN_LED D4
#define PIN_CS_SD D8

#define queue_size 1000
#define debug false

#define f_front_rear_size 3
#define f_data_size 20
#define f_queue_size 5000    //f_data_size*num_data

const char fileName[11]="/test4.txt";
const long utcOffsetInSeconds = 3600*7; // UTC +7 

const uint32_t timeLimit = 4294937296;  //time that allow sensor send data to server. 
// Because millis() will overflow after 49.7 days 
// If time2SendData gets time on less than 10 seconds after timer overflows, we have to wait 49.7 days more till millis() get over time2SendData in (1) expression 
uint8_t bytes[19] = {
  0xAA, // head
  0xB4, // command id
  0x08, // data byte 1
  0x01, // data byte 2 set
  0x01, // data byte 3 work period 5 minutes: measure for 30 seconds + sleep the rest
  0x00, // data byte 4
  0x00, // data byte 5
  0x00, // data byte 6
  0x00, // data byte 7
  0x00, // data byte 8
  0x00, // data byte 9
  0x00, // data byte 10
  0x00, // data byte 11
  0x00, // data byte 12
  0x00, // data byte 13
  0xFF, // data byte 14 (device id byte 1)
  0xFF, // data byte 15 (device id byte 2)
  0x10, // checksum (low 8 bit of sum of data bytes) CHƯA TÍNH
  0xAB  // tail
};
//uint8_t data_buff[num_byte_data_from_arduino]={0};
uint8_t sds_enable=0;
uint8_t ControlFlag;

uint32_t lastPress = 0;
uint32_t lastGetTime = 0;
uint32_t time2SendMessage = 0;
//uint32_t lastCollectionData=0;
//uint32_t macAddessDecimal;
const char mqtt_server[15] = "23.89.159.119"; 
const uint16_t mqtt_port = 1883;
char topic[25];
char espID[8];
char nameDevice[12];

float Temperature = 0;
float Humidity = 0;
float temperatureSum = 0;
float humiditySum = 0;
//float PM1 = 0;
//float PM1Sum = 0;
float PM10 = 0;
float PM10Sum = 0;
float PM2_5 = 0;
float PM2_5Sum = 0;
//float CO = 0;

uint8_t HTUCount = 0;
/*
  AIR SENSE code 

  when pressing a pushbutton arduino starts "smart config"
    The circuit:
  - sds011 sleeps from s0 until s30
  - read data at s0
  - sds011 wake up at s30


  - note: successfully
  created 2019
  by Lê Duy Nhật <https://www.facebook.com/conca.bietbay.7>
  modified 16/11/2019
  by hocj2me
  
*/
uint8_t SDScount = 0;
uint8_t checkQueue = 1;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
//PMS pms(Serial);
//PMS::DATA pmsData;
DS3231 Clock;
RTCDateTime dt;
Adafruit_HTU21DF htu = Adafruit_HTU21DF();

struct data
{
    uint16_t pm1=0;
    uint8_t dot_pm1=0;
    uint16_t pm25=0;
    uint8_t dot_pm25=0;
    uint16_t pm10=0;
    uint8_t dot_pm10=0;
	  uint16_t co=0;
	  uint8_t dot_co=0;
    uint8_t temp=0;
    uint8_t dot_temp=0;
    uint8_t hum=0;
    uint8_t dot_hum=0;
    uint32_t epoch_time=0;
};
data datas[queue_size];
uint8_t front=0;
uint8_t rear=0;

void mqtt_begin();
bool longPress();
void enQueue(data _a);
data deQueue();
bool isQueueFull();
bool isQueueEmpty();
bool f_deQueue(const char* _fileName);
bool f_checkQueue(uint8_t* _data,const char* _fileName);
bool f_view(uint8_t* _buffer, const char* _fileName);
void f_init(const char* _fileName);
bool f_enQueue(uint8_t* _data,const char* _fileName);

bool f_deQueue(const char* _fileName)
{
    File _f = SPIFFS.open(_fileName, "r+");
    if (_f)
    {
        _f.seek(0);
        uint8_t _front[f_front_rear_size];
        _f.read(_front, f_front_rear_size);// front in text file

        _f.seek(f_front_rear_size);
        uint8_t _rear[f_front_rear_size];
        _f.read(_rear, f_front_rear_size);// rear in text file

        uint32_t __front = 0;
        uint32_t __rear = 0;

        for (size_t i = 0; i < f_front_rear_size; i++)
        {
            __front += _front[i] << (8 * (f_front_rear_size - i - 1));
            __rear += _rear[i] << (8 * (f_front_rear_size - i - 1));
        }

        if (__front == __rear)
        {
            return false;// file empty
        }

        if(debug) Serial.print("f_deQueue ");
        if(debug) Serial.print(__front);
        if(debug) Serial.print(" ");
        if(debug) Serial.print(__rear);
        
        __front = (__front + f_data_size) % f_queue_size;

        if(debug) Serial.print(" -> ");
        if(debug) Serial.print(__front);
        if(debug) Serial.print(" ");
        if(debug) Serial.println(__rear);
        
        for (size_t i = 0; i < f_front_rear_size; i++)
        {
            _front[f_front_rear_size - i - 1] = __front % 256;
            __front /= 256;
        }
        _f.seek(0);
        _f.write(_front, f_front_rear_size);// new front in text file

        _f.close();
        return true;// deQueue
    }
    else
    {
        return false;// text file is not exists
    }
}

bool f_checkQueue(uint8_t* _data,const char* _fileName)
{
    File _f = SPIFFS.open(_fileName, "r");
    if (_f)
    {
        _f.seek(0);
        uint8_t _front[f_front_rear_size];
        _f.read(_front, f_front_rear_size);// front in text file

        _f.seek(f_front_rear_size);
        uint8_t _rear[f_front_rear_size];
        _f.read(_rear, f_front_rear_size);// rear in text file

        uint32_t __front = 0;
        uint32_t __rear = 0;

        for (size_t i = 0; i < f_front_rear_size; i++)
        {
            __front += _front[i] << (8 * (f_front_rear_size - i - 1));
            __rear += _rear[i] << (8 * (f_front_rear_size - i - 1));
        }

        if (__front == __rear)
        {
            return false;// file empty
        }

        _f.seek(__front);
        _f.read(_data,f_data_size);// data
        _f.close();
        
        return true;
    }
    else
    {
        return false;// text file is not exists
    }   
}

bool f_view(uint8_t* _buffer, const char* _fileName)
{
    File _f = SPIFFS.open(_fileName, "r");
    if (_f)
    {
        _f.read(_buffer, 2 * f_front_rear_size);
        _f.close();
        return true;
    }
    else
    {
        return false;
    }   
}

void f_init(const char* _fileName)
{
    if (f_queue_size%f_data_size!=0)
    {
        return;//
    }

    File _f;

    if (SPIFFS.exists(_fileName)==false)
    {
        _f = SPIFFS.open(_fileName, "w");
        uint8_t front_rear[f_front_rear_size] = { 0 };
        front_rear[f_front_rear_size - 1] = 2 * f_front_rear_size;

        _f.seek(0);
        _f.write(front_rear, f_front_rear_size);
        _f.seek(f_front_rear_size);
        _f.write(front_rear, f_front_rear_size);

        _f.close();
    }
}

bool f_enQueue(uint8_t* _data,const char* _fileName)
{
    File _f = SPIFFS.open(_fileName, "r+");
    if (_f)
    {
        _f.seek(0);
        uint8_t _front[f_front_rear_size];
        _f.read(_front, f_front_rear_size);

        _f.seek(f_front_rear_size);
        uint8_t _rear[f_front_rear_size];
        _f.read(_rear, f_front_rear_size);

        uint32_t __front = 0;
        uint32_t __rear = 0;

        for (size_t i = 0; i < f_front_rear_size; i++)
        {
            __front += _front[i] << (8 * (f_front_rear_size - i - 1));
            __rear += _rear[i] << (8 * (f_front_rear_size - i - 1));
        }

        _f.seek(__rear);
        _f.write(_data, f_data_size);

        if (__front == (__rear + f_data_size) % f_queue_size)
        {
            __front = (__front + f_data_size) % f_queue_size;
            for (size_t i = 0; i < f_front_rear_size; i++)
            {
                _front[f_front_rear_size - i - 1] = __front % 256;
                __front /= 256;
            }
            _f.seek(0);
            _f.write(_front, f_front_rear_size);
        }

        if(debug) Serial.print("f_deQueue ");
        if(debug) Serial.print(__front);
        if(debug) Serial.print(" ");
        if(debug) Serial.print(__rear);
                
        __rear = (__rear + f_data_size) % f_queue_size;

        if(debug) Serial.print(" -> ");
        if(debug) Serial.print(__front);
        if(debug) Serial.print(" ");
        if(debug) Serial.println(__rear);
        
        for (size_t i = 0; i < f_front_rear_size; i++)
        {
            _rear[f_front_rear_size - i - 1] = __rear % 256;
            __rear /= 256;
        }
        _f.seek(f_front_rear_size);
        _f.write(_rear, f_front_rear_size);
    
        _f.close();
        
        return true;
    }
    else
    {
        return false;
    }
}

bool isQueueEmpty()
{
    if(front==rear)
        return true;
    else
        return false;
}

bool isQueueFull()
{
    if(front==(rear+1)%queue_size)
        return true;
    else
        return false;
}

void enQueue(data _a)
{
    if(isQueueFull())
    {
        //if queue is full, save data to file
        data save_data = deQueue();
        uint8_t _data[f_data_size]={0,0,0,0,save_data.temp,save_data.dot_temp,save_data.hum,save_data.dot_hum,save_data.pm1/256,save_data.pm1%256,save_data.dot_pm1,save_data.pm25/256,save_data.pm25%256,save_data.dot_pm25,save_data.pm10/256,save_data.pm10%256,save_data.dot_pm10,save_data.co/256,save_data.co%256,save_data.dot_co};
        uint32_t epoch_t = save_data.epoch_time;
        _data[3] = epoch_t & 0xff;
        epoch_t  = epoch_t >> 8;
        _data[2] = epoch_t & 0xff;
        epoch_t  = epoch_t >> 8;
        _data[1] = epoch_t & 0xff;
        epoch_t  = epoch_t >> 8;
        _data[0] = epoch_t & 0xff;
        f_enQueue(_data,fileName);        
    }
    datas[rear]=_a;
    rear=(rear+1)%queue_size;    
}

data deQueue()
{
    // chi goi deQueue khi queue is not empty           
    uint8_t _b=front;
    front=(front+1)%queue_size;
    
    if(isQueueEmpty())
    {
        uint8_t f_data[f_data_size];
        if (f_checkQueue(f_data,fileName))
        {
            if(f_deQueue(fileName))
            {
                data _data;
                _data.epoch_time = (f_data[0]<<24) + (f_data[1]<<16) + (f_data[2]<<8) + f_data[3];
                _data.temp       = f_data[4];
                _data.dot_temp   = f_data[5];
                _data.hum        = f_data[6];
                _data.dot_hum    = f_data[7];
                _data.pm1        = (f_data[8]<<8)+f_data[9];
                _data.dot_pm1    = f_data[10];
                _data.pm25       = (f_data[11]<<8)+f_data[12];
                _data.dot_pm1    = f_data[13];
                _data.pm10       = (f_data[14]<<8)+f_data[15];
                _data.dot_pm1    = f_data[16];
				        _data.co         = (f_data[17]<<8)+f_data[18];
                _data.dot_co     = f_data[19];
                enQueue(_data);
            }
        }
    }
    
    return datas[_b];    
}

bool longPress()
{
    if (millis() - lastPress > 2000 && digitalRead(PIN_BUTTON) == 0) 
    { 
        return true;
    } 
    else if (digitalRead(PIN_BUTTON) == 1) 
    {
        lastPress = millis();
    }
    return false;
}
//char dec2hex(uint8_t conv)
//{
//  switch (conv)
//  {
//    case 0: return '0'; break;
//    case 1: return '1'; break;
//    case 2: return '2'; break;
//    case 3: return '3'; break;
//    case 4: return '4'; break;
//    case 5: return '5'; break;
//    case 6: return '6'; break;
//    case 7: return '7'; break;
//    case 8: return '8'; break;
//    case 9: return '9'; break;
//    case 10: return 'A'; break;
//    case 11: return 'B'; break;
//    case 12: return 'C'; break;
//    case 13: return 'D'; break;
//    case 14: return 'E'; break;
//    case 15: return 'F'; break;
//    default: break;
//  }
//}
void mqtt_begin()
{
    uint8_t MacAddress[6];
    WiFi.macAddress(MacAddress);
//    char MAC[12];
//    for(uint8_t i = 5; i>=0; i--)
//    {
//      uint8_t convert;
//      convert = MacAddress[i] & 0x0F;
//      nameDevice[2*i+1] = dec2hex(convert);
//      convert = MacAddress[i] >>4 ;
//      nameDevice[2*i] = dec2hex(convert);
//    }
    uint32_t macAddressDecimalTail = MacAddress[3]*256*256+MacAddress[4]*256+MacAddress[5];
    uint32_t macAddressDecimalHead = MacAddress[0]*256*256+MacAddress[1]*256+MacAddress[2];
    sprintf(nameDevice,"%06X%06X",macAddressDecimalHead, macAddressDecimalTail);
//    sprintf(nameDevice,"%06X",macAddressDecimalTail);
//    sprintf(nameDevice,MAC);
    sprintf(topic,"/SPARC/LOLIN-%06X%06X/",macAddressDecimalHead, macAddressDecimalTail);
    sprintf(espID,"%06X",macAddressDecimalTail);
    for(uint8_t i = 12; i<18; i++)
    {
      nameDevice[i] = NULL;
    }   
    //sprintf(nameDevice,"%06X",macAddessDecimal);
   // sprintf(nameDevice,"ESP_%06X",macAddessDecimal);
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.connect(espID);
}
