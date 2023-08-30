#include <thread>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <IRremote.hpp>

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;
#define DHTTYPE DHT11
#define DHTPIN 2
DHT dht(DHTPIN, DHTTYPE);
float Temperature;
float Humidity;
int buttonState = 0;
int mode;
const int BUTTON_PIN = 14;
int lastButtonState = HIGH;   // The previous button state
int currentButtonState = HIGH; // The current button state
unsigned long lastDebounceTime = 0;  // The last time the button state changed
unsigned long debounceDelay = 50;   
int acMode = 1;
bool modeChanged = false;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows); 
const char* mqttServer = "broker.hivemq.com"; 
int port = 1883;




WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

//IR
struct storedIRDataStruct {
    IRData receivedIRData;
    // extensions for sendRaw
    uint8_t rawCode[RAW_BUFFER_LENGTH]; // The durations if raw
    uint8_t rawCodeLength; // The length of the code
} sStoredIRData[3];
void storeCode();
void sendCode(storedIRDataStruct *aIRDataToSend);

void setup(){
  // initialize LCD
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();
  dht.begin();
  wifiConnect();
  pinMode(16,INPUT); //BUTTON ON
  pinMode(15,INPUT); //BUTTON MODE
  pinMode(14,INPUT); //IR IN 
  pinMode(13,INPUT); //button OFF
  mqttClient.setServer(mqttServer, port);
  mqttClient.setCallback(callback);
  mqttClient.setKeepAlive( 90 );
  Serial.begin(9600);
  IrReceiver.begin(14, ENABLE_LED_FEEDBACK); //IR
  IrSender.begin(12);
}

void lcdMode1(float temp, float humid){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mode1");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TEMP: ");
  lcd.print(temp);
  lcd.setCursor(0,1);
  lcd.print("HUMIDITY: ");
  lcd.print(humid);
}

void lcdMode2(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("AUTO AC: ");
  lcd.print(acMode == 0 ? "OFF" : "ON");
  lcd.setCursor(0,1);
  while (true){
    if (digitalRead(12)==HIGH){
      acMode = 1;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("AUTO AC: ");
      lcd.print(acMode == 0 ? "OFF" : "ON");
      return;
    }
    if (digitalRead(13)==HIGH){
      lcd.clear();
      acMode = 0;
      lcd.setCursor(0,0);
      lcd.print("AUTO AC: ");
      lcd.print(acMode == 0 ? "OFF" : "ON");
            return;
    }
  delay(10);
  }
}

void lcdMode3(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("REGISTER IR:");
  lcd.setCursor(0,1);
  int count=0;
  IrReceiver.start();
    while(count<3){
       if (IrReceiver.decode()){
        storeCode(count);
        IrReceiver.resume();
        count++;
       }
    delay(1000);
} 
    lcd.print("REGISTERED");
    IrReceiver.stop();
    for(int i=0; i<3; i++){
    sendCode(&sStoredIRData[i]);
    delay(3000);
    }
}



void mqttConnect() {
  while(!mqttClient.connected()) {
    Serial.println("Attemping MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if(mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
     
    }
    else {
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}

//MQTT Receiver
void callback(char* topic, byte* message, unsigned int length) {
  Serial.println(topic);
  String strMsg;
  for(int i=0; i<length; i++) {
    strMsg += (char)message[i];
  }
  Serial.println(strMsg);

  //***Code here to process the received package***

}
const char* ssid = "Cafe Thao_2.4G";
const char* password = "123456789";
// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
 

void modeSwitch() {
  modeChanged = true;
  if (mode >= 4) {
    mode = 0;
  } else {
    mode++;
  }
}

void sensorRead(){
  Humidity = dht.readHumidity();
  Temperature = dht.readTemperature();
  if(!dht.readHumidity()||!dht.readTemperature()){
    mqttClient.publish("21127331/status","Warning");
  }
  char tempStr[10];
  dtostrf(Temperature, 5, 2, tempStr);  // Convert temperature to a char array
  mqttClient.publish("21127331/temp", tempStr);
}

void listenButton(int pin) {
  int reading = digitalRead(pin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > debounceDelay) {
    if (reading != currentButtonState) {
      currentButtonState = reading;

      if (currentButtonState == LOW) {
        modeSwitch(); // Call your mode switching function
      }
    }
  }

  lastButtonState = reading;
}



void wifiConnect() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.setCursor(0, 0);
    lcd.print(".");
  }
  lcd.setCursor(0,1);
  lcd.print("!!");
  
}



void loop(){
  if(!mqttClient.connected()) {
    mqttConnect();
  }
  mqttClient.loop();
  sensorRead();
  listenButton(15); // Include this to check the button state

  unsigned long currentMillis = millis();
  static unsigned long previousModeCheckMillis = 0;
  if (currentMillis - previousModeCheckMillis >= 100) {
    previousModeCheckMillis = currentMillis;

    if (modeChanged) {
      modeChanged = false; // Reset the flag
      switch (mode) {
        case 1:
          lcdMode1(Temperature, Humidity);
          break;
        case 2:
          lcdMode2();
          break;
        // Handle the mode 3 case
        case 3:
          lcdMode3();
          break;
        case 4:
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("HI");
        // ... (default case)
      }
    }
  }
}







void storeCode(int i) {
    if (IrReceiver.decodedIRData.rawDataPtr->rawlen < 4) {
        Serial.print(F("Ignore data with rawlen="));
        Serial.println(IrReceiver.decodedIRData.rawDataPtr->rawlen);
        return;
    }
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
        Serial.println(F("Ignore repeat"));
        return;
    }
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_AUTO_REPEAT) {
        Serial.println(F("Ignore autorepeat"));
        return;
    }
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_PARITY_FAILED) {
        Serial.println(F("Ignore parity error"));
        return;
    }
    /*
     * Copy decoded data
     */
    sStoredIRData[i].receivedIRData = IrReceiver.decodedIRData;

    if (sStoredIRData[i].receivedIRData.protocol == UNKNOWN) {
        Serial.print(F("Received unknown code and store "));
        Serial.print(IrReceiver.decodedIRData.rawDataPtr->rawlen - 1);
        Serial.println(F(" timing entries as raw "));
        IrReceiver.printIRResultRawFormatted(&Serial, true); // Output the results in RAW format
        sStoredIRData[i].rawCodeLength = IrReceiver.decodedIRData.rawDataPtr->rawlen - 1;
        /*
         * Store the current raw data in a dedicated array for later usage
         */
        IrReceiver.compensateAndStoreIRResultInArray(sStoredIRData[i].rawCode);
    } else {
        IrReceiver.printIRResultShort(&Serial);
        IrReceiver.printIRSendUsage(&Serial);
        sStoredIRData[i].receivedIRData.flags = 0; // clear flags -esp. repeat- for later sending
        Serial.println();
    }
}

void sendCode(storedIRDataStruct *aIRDataToSend) {
    if (aIRDataToSend->receivedIRData.protocol == UNKNOWN /* i.e. raw */) {
        // Assume 38 KHz
        IrSender.sendRaw(aIRDataToSend->rawCode, aIRDataToSend->rawCodeLength, 38);

        Serial.print(F("raw "));
        Serial.print(aIRDataToSend->rawCodeLength);
        Serial.println(F(" marks or spaces"));
    } else {

        /*
         * Use the write function, which does the switch for different protocols
         */
        IrSender.write(&aIRDataToSend->receivedIRData);
        printIRResultShort(&Serial, &aIRDataToSend->receivedIRData, false);
    }
}
