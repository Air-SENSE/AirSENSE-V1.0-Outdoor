
boolean readDataSDS(float *p25,float *p10)
{
  int i=0;
  int out25, out10;
  int checksum = 0;
  boolean oke = false;
  while((Serial.available()>0)){
    byte k = Serial.read();
    switch(i){
      
        case (0): if (k != 170) { i = -1; }; break;
        case (1): if (k != 192) { i = -1; }; break;
        case (2): out25 = k;        checksum = k; break;
        case (3): out25 += (k<<8);  checksum += k; break;
        case (4): out10 = k;        checksum += k; break;
        case (5): out10 += (k<<8);  checksum += k; break;
        case (6):                   checksum += k; break;
        case (7):                   checksum += k; break;
        case (8): if (k == (checksum % 256)) { 
                    oke = true; 
                  } else {
                    i = -1; 
                  }; break;
        case (9): if (k != 171) i=-1;break;
      }
      i++;
    if ((oke==true)&&(i==10)){
      *p25 = out25/10;
      *p10 = out10/10;
      oke = false;
      i=0;
      return true;
    };
  }
}

int calculateChecksum() 
{
    int sum = 0;
    for (int i = 2; i < 14; ++i) {
      sum += bytes[i];
    }
    sum += bytes[15] + bytes[16];
    return (sum % 256);
}
void Work_per_minute(int times)
{
  bytes[4] = times;
  bytes[17] = calculateChecksum();
  for(uint8_t i = 0;i<19;i++) 
  {
    
    Serial.write(bytes[i]);
    delayMicroseconds(1);
    
  }
  Serial.flush();//delay cho gui xong
}
