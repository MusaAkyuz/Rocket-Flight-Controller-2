//Kütüphaneler
#include <SFE_BMP180.h>  //Basınç ve Sıcaklık Sensörü kütüphanesi
#include <MPU6050.h>    //Eğim ve İvme Sensörü kütüphanesi
#include <Servo.h>     //Servo motor kütüphanesi
#include <SD.h>       //Sd Card Modülü kütüphanesi
#include <SPI.h>     //SPI haberleşme protokolü
#include <Wire.h>

//İsimlendirmeler
MPU6050 MPU;  //Proje boyunca MPU olarak devam edeceğiz
SFE_BMP180 BMP;  //Proje boyunca BMP olarak devam edeceğiz
Servo servoX, servoY, parachuteServo;  

//Değişkenkenler
int ring = 0;  //Hesaplama döngüsünü saymak için

//Servo Pinleri
byte servoXPin = 5, servoYPin = 6, parachuteServoPin = 3;

//Kurtarma Sistemi Pini
byte selfSavePin = 7; 

//SD CARD Modülü pini
byte sdCardPin = 10;

//Ledlerin Pinleri
byte bmpReadyPin = A1, mpuReadyPin = A2, sdReadyPin = A3;
byte bmpUnreadyPin = 8, mpuUnreadyPin = 9, sdUnreadyPin = 4;

//MPU Eğim Değerleri
int16_t gx, gy, gz; //Sırasıyla x, y ve z eksenlerindeki eğimler

//BMP Basınç ve Yükseklik Değerleri 
double p, p0;
double alti, firstAlti, lastAlti, maxAlti;

//Alev Sensörü
short flameValue, range;

//Servo Motor Açı Değerleri
short angleX, angleY;

//Sd Card için Dosya oluşturma
File file;

void setup()
{
    Wire.begin();  //12C iletişim protokülünü başlatıyoruz.
    SPI.begin();
    Serial.begin(115200);
    
    pinMode(bmpReadyPin, OUTPUT);
    pinMode(mpuReadyPin, OUTPUT);
    pinMode(sdReadyPin, OUTPUT);
    pinMode(bmpUnreadyPin, OUTPUT);
    pinMode(mpuUnreadyPin, OUTPUT);
    pinMode(sdUnreadyPin, OUTPUT);
    pinMode(selfSavePin, OUTPUT);
    digitalWrite(selfSavePin, HIGH);
    servoX.attach(servoXPin);
    servoX.write(90);
    servoY.attach(servoYPin);
    servoY.write(90);
    parachuteServo.attach(parachuteServoPin);
    parachuteServo.write(0);
    
    BMPTest();
    MPUTest();
    SDCARDTest();
    
}//end of setup function

void loop()
{
     file = SD.open("test.txt", FILE_WRITE);

     if(file)
     {
         file.print("------------ "); file.print(ring); file.println(". hesaplama döngüsündeyiz ------------");
         
         if(ring == 0)
         {
              //BMP başlangıç basıncı ve yüksekliğini öğrenme
              p0 = getPressure();
              firstAlti = BMP.altitude(p0, p0);
              file.print(" İlk yükseklik : "); file.println(firstAlti);
         }
        
         //Birinci senaryo
         //Roket yönlendirmesi
         //Servo ve eğim sensörünün senkranizasyonu
         
         MPU.getAcceleration(&gx, &gy, &gz);
         angleX = map(gx, -17000, 17000, 0, 180);
         angleY = map(gy, -17000, 17000, 0, 180);
         servoX.write(angleX);
         servoY.write(angleY);
         file.print(" X eksen açısı : "); file.print(angleX);
         file.print(" --- Y eksen açısı : "); file.println(angleY);
    
         //İkinci senaryo
         //Düşüş sırasında paraşütü açma ve roketin ulaştığı irtifayı ölçme
         //Servo ve Basınç sensörünün senkranizasyonu
    
         p = getPressure();
         alti = BMP.altitude(p, p0);
         file.print(" Anlık Yükseklik : "); file.println(alti);
          
         if(alti - lastAlti <= -4)
         { 
            p = getPressure();
            alti = BMP.altitude(p, p0);
            file.print(" Anlık Yükseklik : "); file.println(alti);
            
            if(alti - lastAlti <= -8)
            {
                p = getPressure();
                alti = BMP.altitude(p, p0);
                file.print(" Anlık Yükseklik : "); file.println(alti);
                
                if(alti - lastAlti <= -12)
                {
                    parachuteServo.write(180);
                    file.println(" Paraşüt açıldı");
                    file.print(" Yükseklik : "); file.println(alti);
                }
                else lastAlti = alti;
            }
            else lastAlti = alti;
         }
         else lastAlti = alti;
    
         if(alti > maxAlti)
         {
             maxAlti = alti;
             file.print(" Ulaşılan yükseklik : "); file.println(maxAlti);
         }
         
         //Üçüncü senaryo
         //Yolcu ve ekipmanları kurtarma
         //Alev sensörü ve Kurtarma sistemi senkranizasyonu
    
         flameValue = analogRead(A0);  //0-1024 arasında bir değer
         range = map(flameValue, 0, 1024, 0, 5);
         
         if(range == 4)
         {
             digitalWrite(selfSavePin, HIGH);
             file.println(" Alev uzaklığı: çok uzak ");
         }
         else if(range == 3)
         {
             file.println(" Alev uzaklığı: çok ");
             digitalWrite(selfSavePin, HIGH);
         }      
         else if(range == 2)
         {
             file.println(" Alev uzaklığı: orta ");
             digitalWrite(selfSavePin, HIGH);
         }
         else
         {
             file.println(" Alev uzaklığı: yakın --- KURTARMA İŞLEMİ BAŞLADI ");
             digitalWrite(selfSavePin, LOW); //Röle aktif oldu
             delay(4000);                    //Barutu patlatana kadar bulaşık teli ısınacak
             digitalWrite(selfSavePin, HIGH);
             file.print(" Yükseklik : "); file.println(alti);
         }
         file.close();  
     }
     else
     {
         digitalWrite(sdReadyPin, LOW);
         delay(10);
         digitalWrite(sdUnreadyPin, HIGH);
         delay(500);
         digitalWrite(sdUnreadyPin, LOW);
         delay(500);
         file.close();        
     }
     ring++;
     delay(100);
}

double getPressure()
{
     char status;
     double T,P;

     status = BMP.startTemperature();
     if(status != 0)
     {
         delay(status);
         status = BMP.getTemperature(T);
         if(status != 0)
         {
             delay(status);
             status = BMP.startPressure(3);
             if(status != 0)
             {
                 delay(status);
                 status = BMP.getPressure(P,T);
                 if(status != 0)
                 {
                     return(P);
                 }
             }
         }
     }
     else
     {
         file = SD.open("test.txt", FILE_WRITE);
         file.println("Basınç ölçümünde hata");
         file.close();
     }
}//end of getPressure function

bool BMPTest()
{
     //BMP Bağlantı Kontrolü
     if(!BMP.begin())
     {
         digitalWrite(bmpUnreadyPin, HIGH);
         return(0);
     }
     Serial.println("BMP Bağlandı");

     digitalWrite(bmpReadyPin, HIGH);
     return(1);
}

bool SDCARDTest()
{
     //SD CARD Modülü Bağlantı Kontrolü
     if(!SD.begin(sdCardPin))
     {
         digitalWrite(sdUnreadyPin, HIGH);
         return(0);
     }
     Serial.println("SD Bağlandı");
     digitalWrite(sdReadyPin, HIGH);
     return(1);
}

bool MPUTest()
{
     //MPU Bağlantı Kontrolü
     MPU.initialize();
     if(!MPU.testConnection())
     {
         digitalWrite(mpuUnreadyPin, HIGH);
         return(0);
     }
     Serial.println("MPU Bağlandı");
     digitalWrite(mpuReadyPin, HIGH);
     return(1);
}
