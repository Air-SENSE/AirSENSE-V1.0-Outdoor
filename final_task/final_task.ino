#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>

#include <ESP8266HTTPClient.h>

String  httpurl;
String  TheHiddenAnswerOfClient;
HTTPClient http;

String SendWithAnswer(String IPcache) {
  httpurl = "";
  httpurl+=IPcache;
  http.begin(httpurl);
  http.GET();
  TheHiddenAnswerOfClient = (http.getString());
  http.end();
  return TheHiddenAnswerOfClient;
}
const char* ssid =     "ActiveLearn";     // Set your router SSID
const char* password = "LabSPARC"; // Set your router password
const String VERSION = "0.0";

String  httpurl;
String  TheHiddenAnswerOfClient;
HTTPClient http;

String SendWithAnswer(String IPcache) {
  httpurl = "";
  httpurl+=IPcache;
  http.begin(httpurl);
  http.GET();
  TheHiddenAnswerOfClient = (http.getString());
  http.end();
  return TheHiddenAnswerOfClient;
}
const char* ssid =     "donotsmile";     // Set your router SSID
const char* password = "05021993"; // Set your router password
const String VERSION = "0.0";
void 
/*
  AIR SENSE code 

  when pressing a pushbutton arduino starts "smart config"
    The circuit:
  - sds011 sleeps from s0 until s30
  - read data at s0
  - sds011 wake up at s30
  - read htu21d data every 30 seconds
  - every three minutes, 3 data from sensor will be sent to mqtt

  - note: successfully
  created 2019
  by Lê Duy Nhật <https://www.facebook.com/conca.bietbay.7>
  modified 16/11/2019
  by hocj2me- Lê Chí Tuyền
  
*/
#include "./esp.h"
void setup() 
{
    timeClient.begin();
    Serial.begin(9600);
    pinMode(PIN_BUTTON, INPUT);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);
    WiFi.mode(WIFI_STA);
//    SD.begin(PIN_CS_SD);
    Wire.begin();
    Clock.begin();
    Clock.armAlarm1(false);
    Clock.armAlarm2(false);
    Clock.clearAlarm1();
    
    Clock.clearAlarm2();
  
    // Set Alarm - Every full minute.
    // DS3231_EVERY_MINUTE is available only on Alarm2.
    // setAlarm2(Date or Day, Hour, Minute, Mode, Armed = true)
    Clock.setAlarm2(0, 0, 0, DS3231_EVERY_MINUTE,true);
  
    // Set Alarm1 - Every 30s in each minute
    // setAlarm1(Date or Day, Hour, Minute, Second, Mode, Armed = true)
    Clock.setAlarm1(0, 0, 0, 30, DS3231_MATCH_S,true);
//    logDataToSD(Temperature, Humidity, PM1, PM2_5, PM10, CO);
    mqtt_begin();
    SPIFFS.begin();
    htu.begin();
    f_init(fileName);
//    Csleep();
    Serial.println(F("setup done"));
	  randomSeed(analogRead(A0));
    ESP.wdtDisable();//watchdog timer

// ========================================================

 WiFi.begin(ssid, password);
  /*connection to WiFi*/
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println(".");
    delay(1000);
  }
  String last = SendWithAnswer("http://airfunction.000webhostapp.com/BIN/version.txt");
  Serial.println(last);
  if (VERSION < last)
  {
    t_httpUpdate_return ret = ESPhttpUpdate.update("http://airfunction.000webhostapp.com/BIN/file.bin"); //Location of your binary file
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;
      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
  }
  else
  {
    Serial.println("This is the last version");
  }
//========================================================================    
}
void loop() 
{
    if (longPress())
    {
        digitalWrite(PIN_LED, LOW);
        if(WiFi.beginSmartConfig())
        {
           Serial.println(F("Enter smart config"));
        }
 
    } 
    else if(WiFi.status() == WL_CONNECTED)
    {
      digitalWrite(PIN_LED, HIGH);  
      if((lastGetTime == 0) || ((millis() - lastGetTime) > 163000))
      {
        if(timeClient.update())
        {
          lastGetTime = millis();
          uint32_t internetTime = timeClient.getEpochTime();
          dt = Clock.getDateTime();
          if (abs(internetTime - dt.unixtime) > 3)
          {
            Clock.setDateTime(internetTime);
            Serial.println("done update time"); 
          } 
        }
      }
      if((isQueueEmpty()==false) && (checkQueue == 1) && (millis() < timeLimit))// (1)
      {
//        Serial.println(millis());
        time2SendMessage = millis() + random(50,100)*100;
//        Serial.println(time2SendMessage);
        checkQueue = 0;
      }
//      Serial.println("ok");
//      Serial.println((millis() - time2SendMessage));
//      Serial.println("ok1");
      if((millis() > time2SendMessage)&& (checkQueue == 0))
      {
//        Serial.println(millis());
        //if queue is not empty, publish data to server
//        Serial.println(millis() - time2SendMessage);
        if(mqttClient.connected())
        {
            data send_data = deQueue();
            createAndPrintJSON(send_data, nameDevice);
            checkQueue = 1;
            mqttClient.loop();      
        }
//        else //if(millis()-lastMqttReconnect>5000)
//        {
          //  lastMqttReconnect=millis();
        else if(mqttClient.connect(espID))
          {
            Serial.println(F("reconnected mqtt"));
          }
        }      
      }
       
   checkAlarm();
//   if((ControlFlag & 0b00001000) || (ControlFlag & 0b00000100))
//   {
//      getHTUData(); 
//      getSDSdata();
//      dt = Clock.getDateTime();
//      Serial.println(F("get data"));
//      ControlFlag &= 0b11110011;
//   } 
   if(ControlFlag & 0b00001000)
   {
      getHTUData();
      Serial.println("Get Data");
      readDataSDS(&PM2_5, &PM10);
//      SDScount++;
      Csleep();
      ControlFlag &= 0b11110111;
   }
   else if(ControlFlag & 0b00000100)
   {
      getHTUData();
      Cwakeup();
      delay(100);
      ControlFlag &= 0b11111011;
   }
   if((ControlFlag & 0b00000011) == 3)
   {
      processingData();
      dt = Clock.getDateTime();
      queueData(Temperature, Humidity, 0, PM2_5, PM10, 0, dt.unixtime);
      logDataToSD(Temperature, Humidity, 0, PM2_5, PM10, 0);
      ControlFlag &= 0b11111100;
//      Serial.println(F("queuing completed!"));
   }
   ESP.wdtFeed();  
//   Serial.println(1);
  delay(500);
}
void checkAlarm()
{
  if(Clock.isAlarm1())
  { 
    ControlFlag |= 0b01010100;//set fag sleep PMS,
  }
  if(Clock.isAlarm2())
  {
    ControlFlag |= 0b10101000;//set bit wake PMS, 
    ControlFlag ++;
  }
}
void processingData()
 {
   if(HTUCount != 0)
   {
     Humidity = (float)humiditySum/HTUCount;
     Temperature = (float)temperatureSum/HTUCount;
     HTUCount = 0;
     humiditySum = 0;
     temperatureSum = 0;
   }
   if(SDScount != 0)
   {
     PM2_5 = (float)PM2_5Sum/SDScount;
     PM10 = (float)PM10Sum/SDScount;
     SDScount = 0;
     PM2_5Sum = 0;
     PM10Sum = 0;
   }
 }
void createAndPrintJSON(data _send_data,char* names)
 {
    float co    = _send_data.co + _send_data.dot_co*0.01;
    float hum   = _send_data.hum + _send_data.dot_hum*0.01;
    float tem   = _send_data.temp + _send_data.dot_temp*0.01;
    float pm1   = _send_data.pm1 + _send_data.dot_pm1*0.01;
    float pm2p5 = _send_data.pm25 + _send_data.dot_pm25*0.01;
    float pm10  = _send_data.pm10 + _send_data.dot_pm10*0.01;
    uint32_t realtimes = _send_data.epoch_time;
     DynamicJsonDocument doc(256);
    char mes[256] = {0};
    //JsonObject obj = doc.to<JsonObject>();
    JsonObject DATA = doc.createNestedObject("DATA"); 
    DATA["CO"]        = co; 
    DATA["Hum"]       = hum;
    DATA["Pm1"]       = pm1;
    DATA["Pm10"]      = pm10;
    DATA["Pm2p5"]     = pm2p5;
    DATA["Time"]      = realtimes - 7*3600;
    DATA["Tem"]       = tem;
    //JsonObject NAME = doc.createNestedObject("NAME"); 
    doc["NodeId"]     = names;
    //Serial.println();
    serializeJson(doc, mes);
    if(mqttClient.publish(topic, mes, true))
    {
      Serial.println(F("***Published successfully!***"));
    }
 }
 void queueData(float temp, float humi, float pm1, float pm25, float pm10, float co, uint32_t unix)
{
    data queuing;
    
    queuing.temp = temp;
    queuing.dot_temp = (temp - queuing.temp) * 100 ;
    queuing.hum = humi;
    queuing.dot_hum = (humi - queuing.hum) * 100;
  
    queuing.pm1 = pm1;
    queuing.dot_pm1 = (pm1 - queuing.pm1) * 100;
    queuing.pm25 = pm25;
    queuing.dot_pm25 = (pm25 - queuing.pm25) * 100 ;
    queuing.pm10 = pm10;
    queuing.dot_pm10 = (pm10 - queuing.pm10) * 100;
  
    queuing.co = co;
    queuing.dot_co = (co - queuing.co) * 100;
  
    queuing.epoch_time = unix;
    enQueue(queuing);
    Serial.println(F("queuing completed"));
}
void logDataToSD(float _temperature, float _humidity, uint8_t _pm1, float _pm25, float _pm10, uint8_t _COppm)
{
  SD.begin(PIN_CS_SD);
  char fileName[13];
//  String FileName;

  sprintf(fileName, "%02d-%02d-%02d.txt", dt.day, dt.month, dt.year - 2000);
//  fileName[8] = ' ';
//  for(uint8_t i = 0; i< 12; i++)
//  {
//    fileName[i+9] = nameDevice[i];
//  }
//  fileName[22] = '.';
//  fileName[23] = 't';
//  fileName[24] = 'x';
//  fileName[25] = 't';
//  fileName[26] = NULL;
//  FileName = fileName + nameDevice;
  File f = SD.open(fileName, FILE_WRITE);

  if (f)
  { 
  	f.print(nameDevice);
  	f.print(",");
  	
    f.print(dt.year);
    f.print("-");
    f.print(dt.month);
    f.print("-");
    f.print(dt.day);
    f.print(" ");

    f.print(dt.hour);
    f.print(":");
    f.print(dt.minute);
    f.print(":");
    f.print(dt.second);
    f.print(",");
    f.print(dt.unixtime-7*3600);
    f.print(",");
    
    f.print(_pm25);
    f.print(",");
    f.print(_pm10); 
    f.print(",");

    f.print(_temperature);
    f.print(",");
    f.print(_humidity);

//    f.print(_pm1);
//    f.print(",");

    
//    f.println("%06X", macAddessDecimal);
    
    f.println();
//    f.println(_COppm);

    f.close();
  }
  SD.end();
//  Serial.println(fileName);
}
void getHTUData()
{
  Temperature = htu.readTemperature();
  Humidity = htu.readHumidity();
  temperatureSum += Temperature;
  humiditySum += Humidity;
  HTUCount++;
}
//void getSDSdata()
//{
//  uint8_t sdsData = 0;
//  uint8_t i = 0;
//  uint8_t sdsDust[10] = {0};
//  uint8_t sdsChecksum = 0;
//  uint32_t timeLimit = millis();
//  while (Serial.available() > 0) 
//  {  
//    sdsData = Serial.read();     
//    delay(2);// dợi lấy dữ liệu
//    if(sdsData == 0xAA) //head1 ok
//     {
//        sdsDust[0] =  sdsData;
//        sdsData = Serial.read();
//        if(sdsData == 0xc0)//head2 ok
//        {
//          sdsDust[1] =  sdsData;
//          sdsChecksum = 0;
//          for(i=0;i < 6;i++)//lay data    
//            {      
//             sdsDust[i+2] = Serial.read();
//             delay(2);
//             sdsChecksum += sdsDust[i+2];
//            }
//          sdsDust[8] = Serial.read();
//          delay(1);
//          sdsDust[9] = Serial.read();
//          if(sdsChecksum == sdsDust[8])//crc ok
//          {
//            Serial.flush();
//            PM2_5 = (sdsDust[3]*256 + sdsDust[2])/10;
//            PM10 = (sdsDust[5]*256 + sdsDust[4])/10; 
//            
//            PM2_5Sum += PM2_5; 
//            PM10Sum += PM10; 
//            SDScount ++;     
//           // char sds25Char[3];
//           // char sds10Char[3];
//           // sprintf(sds25Char, "%3d", sdsPm25);
//           // sprintf(sds10Char, "%3d", sdsPm10);
//          }
//        }      
//     }   
//     if(millis()-timeLimit > 1000)
//     {
//      break;
//     } 
//   }
//
//}


int readDataSDS(float *p25,float *p10)
{
  int i=0;
  int out25, out10;
  int checksum = 0;
  while((Serial.available()>0))
  {
    byte k = Serial.read();
    switch(i)
    {  
      case (0): if (k != 170) { i = -1; }; delayMicroseconds(1); break;
      case (1): if (k != 192) { i = -1; };  break;
      case (2): out25 = k;        checksum = k; break;
      case (3): out25 += (k<<8);  checksum += k; break;
      case (4): out10 = k;        checksum += k; break;
      case (5): out10 += (k<<8);  checksum += k; break;
      case (6):                   checksum += k; break;
      case (7):                   checksum += k; break;
      case (8): if (k == (checksum % 256)) 
                  { 
                    *p25 = (float)out25/10;
                    *p10 = (float)out10/10;
                    Serial.println(out25);
                    break;
                  } 
                else 
                  {
                    readDataSDS(&*p25,&*p10);
                    i = -1; 
                  }
                break;
      case (9): if (k != 171) i=-1;break;
    }
    i++;
    delayMicroseconds(1);
  }
  PM2_5Sum += *p25 ; 
  PM10Sum += *p10; 
  SDScount ++;   
}
void Csleep()
{
  for(uint8_t i = 0;i<19;i++) 
  {
    Serial.write(SLEEPCMD[i]);
    delayMicroseconds(1);
  }
  Serial.flush();//delay cho gui xong
}
void Cwakeup()
{
  for(uint8_t i = 0;i<19;i++) 
  {
    Serial.write(WAKEUPCMD[i]);
    delayMicroseconds(1);
  }
  Serial.flush();//delay cho gui xong
}
