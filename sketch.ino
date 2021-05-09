/* ABOUT
 *  
 *  This script was written by Ronald van der Meer for use with the 
 *  LilyGO TTGO T-Higrow ESP32 (BME280 edition). Although other versions 
 *  can be found on github I found those to be complex and not easy to 
 *  customize. My aim was to provide a sketch which is simple, easy to understand 
 *  and easily customizable because its plain and simple.
 *  
 *  Currently experimental; centibars conversion. 
 *  
 *  Because its a battery powered device I implemented a sleep mode so this sketch
 *  will let the device connect to wifi, print variables and then go into deep
 *  sleep mode for X amount of time. 
 *  
 *  I personally use HTTPClient.h to submit some data to my home automation system.
 * 
 */

#include <WiFi.h>
#include <Button2.h>            //https://github.com/LennartHennigs/Button2
#include <BH1750.h>             //https://github.com/claws/BH1750 LUX
#include <Adafruit_BME280.h>    //https://github.com/adafruit/Adafruit_BME280_Library BME280

/* WIFI AND mDNS SETTINGS
 *  
 *  Fill in the SSID and the password for your accesspoint. 
 *  
 */
 
const char* ssid      = "<YOUR SSID>";
const char* password  = "<YOUR WIFI AP PASSWORD>";
const int wifitimeout = 30;

/* DEEP SLEEP 
 *  
 *  Most important variable is the second which specifies the time to sleep in 
 *  seconds. Because the moisture of a plant won't change every second specify
 *  something reasonable to make the most of the battery. Every 12 hours sounds
 *  good enough to me personally.
 */
 
const unsigned long long int uS_TO_S_FACTOR  = 1000000;
const int TIME_TO_SLEEP                      = 43200; 

/* MAPPING RAW READINGS FROM SOILSENSOR TO PERCENTAGES
 *  
 *  Use the map function to let it display the correct humidity percentage.
 *  For this we need to know what the airValue is and what the waterValue is.
 *  
 *  Put the capacitive sensor in a glass of water. Leave it there for a few 
 *  seconds and write down the lowest "soilSensorValue" value and specify it as 
 *  waterValue below.
 *  
 *  Remove the capacitive sensor from the glass of water and make it dry.
 *  Leave it be for a few seconds and write down the highest "soilSensorValue" 
 *  number and specify it as airValue. 
 *  
 *  Now you know what raw value equals 100% humidity and what raw value equals
 *  to 0% humidity.The script will use these variables to calculate the percentages.
 */

const int airValue   = 3305;
const int waterValue = 1500;

void setup()
{
  Serial.begin(115200);

  BH1750              lightMeter(0x23); /* 0x23 */
  Adafruit_BME280     bme;              /* 0x77 */
  Button2             button(0);        /* 0 */
  Button2             useButton(35);    /* 35 */

  const int POWER_CTRL = 4;
  pinMode(POWER_CTRL, OUTPUT);
  digitalWrite(POWER_CTRL, 1);
  delay(1000);

  const int I2C_SDA = 25;
  const int I2C_SCL = 26;
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!lightMeter.begin()) {
      Serial.println("Could not find a valid BH1750 sensor, check wiring!");
  }

  if (!bme.begin()) {
      Serial.println("Could not find a valid BMP280 sensor, check wiring!");
  }
    
  initWiFi();

  // LIGHT
  float light = lightMeter.readLightLevel();

  // SOIL Humidity level
  int soilSensorValue = analogRead(32);
  int soilhumperc = map(soilSensorValue, airValue, waterValue, 0, 100);

  // Centibars conversion (a bit experimental based on some realworld tests)
  // 00 - 09 = saturated, 10 - 19 = adequately wet, 20 - 59 = irrigation advice, 60 - 99 = irrigation, 100-200 = Dangerously dry,
  int soilcb = map(soilhumperc, 40, 0, 0, 100);
  if (soilcb < 0 ) { 
     soilcb = 0;
  }

  // SOIL Minerals level
  uint8_t samples = 120;
  uint32_t humi = 0;
  uint16_t array[120];
  for (int i = 0; i < samples; i++) {
      array[i] = analogRead(34);
      delay(2);
  }
  std::sort(array, array + samples);
  for (int i = 1; i < samples - 1; i++) {
      humi += array[i];
  }
  humi /= samples - 2;
  uint8_t saltperc = humi;
  
  // Battery voltage
  int vref = 1100;
  uint16_t volt = analogRead(33);
  float voltage = ((float)volt / 4095.0) * 6.6 * (vref);

  // BME280
  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = (bme.readPressure() / 100.0F);
  float altitude = bme.readAltitude(1013.25);

  // Printing the values
  Serial.print("soilSensorValue = " + String(soilSensorValue) + ", ");
  Serial.print("soilHumidityValue = " + String(soilhumperc) + " %, ");
  Serial.print("soilCBValue = " + String(soilcb) + " cb, ");
  Serial.println("soilSaltValue = " + String(saltperc) + " %");

  Serial.print("bmeTemperature = " + String(temperature) + " *C, ");
  Serial.print("bmeHumidity = " + String(humidity) + " %, ");
  Serial.print("bmeAltitude = " + String(altitude) + " m, ");
  Serial.println("bmePressure = " + String(pressure) + " hPa");

  Serial.println("illumination = " + String(light) + " lx");
  Serial.println("batteryValue = " + String(voltage) + " mV");
  
  WiFi.disconnect();
  delay(100);
  
  Serial.println("Going into sleep mode for " + String(TIME_TO_SLEEP) + " seconds. Night night!");

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
  
  Serial.println("This will never be printed because the device is already sleeping at this point");

}

void loop(){
  /* DEEP SLEEP will only use setup. Loop will be ignored */
}

/* CUSTOM FUNCTIONS
 *  
 *  Functions for this specific sketch.
 *  
 */
 
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi " + String(ssid) + " ..");

  int x = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    if (x >= wifitimeout) {
      Serial.println(" unable to connect, restarting");
      ESP.restart();
    }
    x = x + 1;
    delay(1000);
  }
  Serial.println(" connected!");

  Serial.println("IP: " + String(WiFi.localIP().toString()) + ", Subnet: " + String(WiFi.subnetMask().toString()) + ", Gateway: " + String(WiFi.gatewayIP().toString()));
}
