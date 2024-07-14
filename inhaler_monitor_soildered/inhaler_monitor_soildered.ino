#include <LiquidCrystal_I2C.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include "MAX30105.h"
#include <TinyGPS.h>
#include <HardwareSerial.h>
#include "spo2_algorithm.h"


//--------------------------------------------------------------------------------------------------------------------------------SPO2 VARIABLE DECLARE
#define MAX_BRIGHTNESS 255
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
uint16_t irBuffer[100]; //infrared LED sensor data
uint16_t redBuffer[100];  //red LED sensor data
#else
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data
#endif

int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t spo2Valid; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid
int8_t fingerDetected; //indicator to show if a finger is detected

byte pulseLED = 25; //Must be on PWM pin (example: GPIO25 on ESP32)
byte readLED = 13; //Blinks with each data read (example: GPIO13 on ESP32)

void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid);



LiquidCrystal_I2C lcd(0x27,16,2);  

MAX30105 particleSensor;

int oxygenSaturation = 0;

TinyGPS gps;

WiFiClient espclient;
PubSubClient client(espclient); 

HardwareSerial gpsSerial(2);
// SoftwareSerial gpsSerial(4, 0); // RX, TX


int total_volume = 250;

int refill_btn = 15;
int dose_btn = 4;
int buzzer_pin = 27;

String payload = "";

String gps_string="0.00,0.00";

float lat_g = 0.00;
float lon_g = 0.00;

int32_t irValue;

// Wifi credentials
// const char* SSID = "TORUS";
// const char* PASS = "webdark$";
const char* SSID = "internet";
const char* PASS = "password";

// MQTT credentials
const char* mqtt_broker = "broker.hivemq.com";
// const char* mqtt_broker ="broker.emqx.io";
const char* pub_topic = "inhaler_monitor";
const char* sub_topic = "inhaler_monitor_callback";
const char* mqtt_username = "emqx13";
const char* mqtt_password = "public13";
const int mqtt_port = 1883;

String client_id = "inhaler-monitor-esp-222222";


int pub_spo2 = 0;



//------------------------------------------------------------------------------------------------------------------------------------Setup
void setup() {
  Serial.begin(9600);
    
  Wire.begin(18, 19);   


  // gpsSerial.begin(9600); // GPS module baud rate
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  lcd.init();                      
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Inhaler Monitor");


  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println(F("MAX30102 was not found. Please check wiring/power."));
    // while (1);
  }

  Serial.println("Attach sensor to finger with rubber band. Press any key to start conversion");
  // while (Serial.available() == 0) ; //wait until user presses a key
  // Serial.read();


  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

  //pin 23 to GND for buzzer 
  pinMode(23,OUTPUT);
  digitalWrite(23,LOW);  

  pinMode(refill_btn,INPUT_PULLUP);
  pinMode(dose_btn,INPUT_PULLUP);
  pinMode(buzzer_pin,OUTPUT);
  



  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the Wi-Fi network");

  // Connect to MQTT
  client.setServer(mqtt_broker, mqtt_port);
  if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
    Serial.println("Public MQTT broker connected");
    
    client.subscribe(sub_topic);
    client.setCallback(callback); 
  } else {
    Serial.print("Failed to connect to MQTT broker, rc=");
    Serial.print(client.state());
    Serial.println(" Try again...");
    delay(5000);
    ESP.restart();
  }

}




//----------------------------------------------------------------------------------------------------------------------------LOOP
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  lcd.setCursor(0,1);
  lcd.print("                ");

  //--------------------------------------------------------------------------------------------------------------------------check REFILL button
  int refillval = digitalRead(refill_btn); 
  Serial.println("\nRefill btn state : "+String(refillval));
  if( refillval == LOW){
    if(total_volume<200){
        
        total_volume = 250;
        Serial.println("Cylinder refilled");
        lcd.setCursor(0,1);
        lcd.print("Inhaler Refilled");
        delay(1000);
    }
  }


  //--------------------------------------------------------------------------------------------------------------------------check DOSE button
  int doseval = digitalRead(dose_btn) ; 
  Serial.println("\nDose btn state : "+String(doseval));
  if( doseval == LOW){
    if(total_volume>=70){
        total_volume -= 35;
        
        
        Serial.println("Total volume : "+String(total_volume));

        Serial.println("Dosage Used (-5ml)");
        lcd.setCursor(0,1);
        lcd.print("Dosage Used -5ml");

        delay(1000);

    }else{
      Serial.println("One Dose left");
      lcd.print("1 dose left");

      lcd.setCursor(0,1);
      lcd.print("Cylinder Empty");
      digitalWrite(buzzer_pin,HIGH); 
      delay(1000);
      lcd.print("Cylinder Refilled");
      delay(1000); 
      digitalWrite(buzzer_pin,LOW); 
 
    }      
  }
  


  //------------------------------------------------------------------------------------------------------------------------SPO2_START

  if(particleSensor.getIR()>100000){
    Serial.println("====> Before check : finger detected ");
 


    while (particleSensor.available() == false) {
      Serial.println("Waiting for sensor data...");
      particleSensor.check(); // Check the sensor for new data
    }



    // Collect samples
    for (byte i = 0; i < 10  ; i++) {
      // Serial.println("inital sample collection started");
      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      Serial.println("irBuffer[i] = "+String(irBuffer[i]));  

        if(irBuffer[i]>100000){
          Serial.println("===============");
          Serial.println(String(irBuffer[i])+"inital sample finger detected");
          delay(150);
          Serial.println("===============");       
        }

      particleSensor.nextSample(); // Move to the next sample
      // Serial.println("inital sample collection ended");
    }

    int n = 35;
    
    //check if finger is detectd
    if (irBuffer[0]>100000) {
      //check if finger is detectd (if finger is detected then IR values wil be grater than 10000)

      Serial.println("Finger detected! Taking samples for SpO2 calculation...");

      // Take n samples
      for (int i = 0; i < n; i++) {
        while (particleSensor.available() == false) {
            particleSensor.check(); // Check for new data
        }

        // Read red and IR values
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i] = particleSensor.getIR();
        particleSensor.nextSample(); // Move to next sample

        // Print or process the sampled data
        Serial.print("Sample ");
        Serial.print(i);
        Serial.print(": red=");
        Serial.print(redBuffer[i]);
        Serial.print(", ir=");
        Serial.println(irBuffer[i]);
    }

      maxim_heart_rate_and_oxygen_saturation(irBuffer, n, redBuffer, &spo2, &spo2Valid, &heartRate, &validHeartRate);      
      
      Serial.printf("----------------------------HR=%d, HRvalid=%d,SPO2Valid=%d-------------------------\n", heartRate, validHeartRate,spo2Valid);

        if(irBuffer[0]>100000/*if finger is detected*/ && spo2Valid && (spo2>0) ){
            Serial.printf("Filtered Data HR=%d, HRvalid=%d \n", heartRate, validHeartRate);
            Serial.println("==============");
            // pub_spo2 = spo2;
            if(spo2<9){
            oxygenSaturation = spo2+90;
            }
            if(spo2<19){
            oxygenSaturation = spo2+80;
            }
            if(spo2<29){
            oxygenSaturation = spo2+70;
            }
            if(spo2<39){
            oxygenSaturation = spo2+60;
            }
            if(spo2<49){
            oxygenSaturation = spo2+50;
            }
            if(spo2<59){
            oxygenSaturation = spo2+40;
            }
            if(spo2<69){
            oxygenSaturation = spo2+30;
            }
            if(spo2<79){
            oxygenSaturation = spo2+20;
            }
             
            else{
          
               oxygenSaturation=spo2;
               }
            Serial.println("SPO2 : " + String(oxygenSaturation));
            

            //spo2 threshold check for beep sound

            if(spo2<94){
              digitalWrite(buzzer_pin,HIGH);
              delay(1000);
              digitalWrite(buzzer_pin,LOW);
              //Serial.println("spo2 below 94-beeping");
              }
            else{
              digitalWrite(buzzer_pin,LOW);
              }
              Serial.println("==============");
            
            
                      
        }
    }

      else {
      Serial.println("Finger not detected, skipping SpO2 calculation.");
    }
  }


    
  //--------------spo2-end





  gps_string = readGPS();
  Serial.println("GPS string : "+gps_string);

  // Prepare payload
  payload = "[" + String(oxygenSaturation) + "," + String(total_volume) +  "," +String(gps_string)+"]";

  lcd.setCursor(0,1);
  lcd.print("                ");

  Serial.println("payload : "+payload);

  lcd.setCursor(0,1);
  lcd.print(payload);
  delay(100);


  // Publish payload to MQTT
  if (client.publish(pub_topic, payload.c_str())) {
    Serial.println("Data published successfully");
  } else {
    Serial.println("Data publish failed");
  }


  delay(300);
}








//--------------------------------------------------------------------------------------------------------------------------Reconnect
void reconnect() {
  Serial.print("Attempting MQTT connection...");
  if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
    Serial.println("Connected to MQTT broker");
  } else {
    Serial.print("Failed to connect to MQTT broker, rc=");
    Serial.print(client.state());
    Serial.println(" Trying again in 5 seconds");
    delay(5000);
  }
}




//--------------------------------------------------------------------------------------------------------------------------Callback
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("-Message from topic: ");
  Serial.println(topic);

  String data = "";
  for (int i = 0; i < length; i++) {
    data += (char)payload[i];
  }

  Serial.println("Subscribed Message: " + data);

  if(data == "on"){
    Serial.println("Buzzer On");
    digitalWrite(buzzer_pin,HIGH);
    // delay(1000);
  }else{
    Serial.println("Buzzer Off");
    digitalWrite(buzzer_pin,LOW);
  }
  // Serial.println(5);
}


//-------------------------------------------------------------------------------------------------------------------------Read-GPS
String readGPS() {
  String gpsData = "";
  String gpsCoordinates = "";


  bool newData = false;
  unsigned long chars;
  unsigned short sentences, failed;

  //----------------------------------------------------------
   for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (gpsSerial.available())
    {
      char c = gpsSerial.read();
      Serial.print(c); // uncomment this line if you want to see the GPS data flowing
      if (gps.encode(c)) // Did a new valid sentence come in?
        newData = true;
    }
  }

  if (newData)
  {
    float flat, flon;
    unsigned long age;
    gps.f_get_position(&flat, &flon, &age);
    Serial.print("LAT=");
    Serial.print(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flat, 6);
    Serial.print(" LON=");
    Serial.print(flon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flon, 6);
    // Serial.print(" SAT=");
    // Serial.print(gps.satellites() == TinyGPS::GPS_INVALID_SATELLITES ? 0 : gps.satellites());
    // Serial.print(" PREC=");
    // Serial.print(gps.hdop() == TinyGPS::GPS_INVALID_HDOP ? 0 : gps.hdop());

    gpsCoordinates = String(flat,4)+","+String(flon,4);
    Serial.print("GPS UPDATED : "+gpsCoordinates);
  }else{
    gpsCoordinates = "0.00,0.00";
  }
  //----------------------------------------------------------------

  return gpsCoordinates;
}