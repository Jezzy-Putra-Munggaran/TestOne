#define BLYNK_TEMPLATE_ID "TMPL6HxAQ8qWi"
#define BLYNK_TEMPLATE_NAME "MAX30105"
#define BLYNK_AUTH_TOKEN "sRS3CJLZ39k8sisB6pBCi3i3dLUgoYAE"

#define ADC_VREF_mV    3300.0 // in millivolt
#define ADC_RESOLUTION 4096.0
#define PIN_LM35       34// ESP32 pin GPIO36 (ADC0) connected to LM35

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#include <Fuzzy.h>

#include <MAX30105.h>
#include <heartRate.h>
#include <spo2_algorithm.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

using namespace std;

char auth[]="sRS3CJLZ39k8sisB6pBCi3i3dLUgoYAE";
char ssid[]="nona";
char pass[]="12345678";

MAX30105 particleSensor;

const byte RATE_SIZE = 4; // Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE];    // Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; // Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;
float spo2; // Added variable for SpO2

Fuzzy*fuzzy;

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  // Inisialisasi Blynk

  Serial.println("Initializing...");

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  { // Alamat I2C OLED biasanya 0x3C
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.display();
  delay(2000);
  display.clearDisplay();

  Serial.println("Initializing...");

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 was not found. Please check wiring/power. ");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  particleSensor.setup(); // Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); // Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0);  // Turn off Green LED

  
  Blynk.begin(auth, ssid, pass);
  fuzzy = new Fuzzy();
  
 FuzzyInput *detakJantung = new FuzzyInput(1);
  FuzzySet *lemah = new FuzzySet(35, 47, 57, 60);
  FuzzySet *normal = new FuzzySet(57, 60, 80, 83);
  FuzzySet *hyper = new FuzzySet(80, 83, 93, 105);
  detakJantung->addFuzzySet(lemah);
  detakJantung->addFuzzySet(normal);
  detakJantung->addFuzzySet(hyper);
  fuzzy->addFuzzyInput(detakJantung);

  // Fuzzy Input for Ammonia
  FuzzyInput *saturasiOksigen = new FuzzyInput(2);
  FuzzySet *parah = new FuzzySet(74, 79, 85, 86);
  FuzzySet *sedang = new FuzzySet(85, 86, 90, 91);
  FuzzySet *ringan = new FuzzySet(90, 91, 94, 95);
  FuzzySet *normall = new FuzzySet(94, 95, 101, 106);
  saturasiOksigen->addFuzzySet(parah);
  saturasiOksigen->addFuzzySet(sedang);
  saturasiOksigen->addFuzzySet(ringan);
  saturasiOksigen->addFuzzySet(normall);
  fuzzy->addFuzzyInput(saturasiOksigen);


  // Fuzzy Output for PWM Blower
  FuzzyOutput *hipoksia = new FuzzyOutput(1);
  FuzzySet *ringann = new FuzzySet(-0.3, 0, 0.1, 0.3);
  FuzzySet *tidak = new FuzzySet(0.1, 0.3, 0.4, 0.6);
  FuzzySet *sedangg = new FuzzySet(0.4, 0.6, 0.7, 0.9);
  FuzzySet *parahh = new FuzzySet(0.7, 0.9, 1, 1.3);
  hipoksia->addFuzzySet(ringann);
  hipoksia->addFuzzySet(tidak);
  hipoksia->addFuzzySet(sedangg);
  hipoksia->addFuzzySet(parahh);
  fuzzy->addFuzzyOutput(hipoksia);

  // Fuzzy Rules
  FuzzyRuleAntecedent *lemah_ringan = new FuzzyRuleAntecedent();
  lemah_ringan->joinWithAND(lemah, ringan);
  FuzzyRuleConsequent *rule1_output = new FuzzyRuleConsequent();
  rule1_output->addOutput(ringann);
  FuzzyRule *fuzzyRule1 = new FuzzyRule(1, lemah_ringan, rule1_output);
  fuzzy->addFuzzyRule(fuzzyRule1);

  FuzzyRuleAntecedent *lemah_sedang = new FuzzyRuleAntecedent();
  lemah_sedang->joinWithAND(lemah, sedang);
  FuzzyRuleConsequent *rule2_output = new FuzzyRuleConsequent();
  rule2_output->addOutput(sedangg);
  FuzzyRule *fuzzyRule2 = new FuzzyRule(2, lemah_sedang, rule2_output);
  fuzzy->addFuzzyRule(fuzzyRule2);

  FuzzyRuleAntecedent *lemah_normall = new FuzzyRuleAntecedent();
  lemah_normall->joinWithAND(lemah, normall);
  FuzzyRuleConsequent *rule3_output = new FuzzyRuleConsequent();
  rule3_output->addOutput(tidak);
  FuzzyRule *fuzzyRule3 = new FuzzyRule(3, lemah_normall, rule3_output);
  fuzzy->addFuzzyRule(fuzzyRule3);

  FuzzyRuleAntecedent *lemah_parah = new FuzzyRuleAntecedent();
  lemah_parah->joinWithAND(lemah, parah);
  FuzzyRuleConsequent *rule4_output = new FuzzyRuleConsequent();
  rule4_output->addOutput(parahh);
  FuzzyRule *fuzzyRule4 = new FuzzyRule(4, lemah_parah, rule4_output);
  fuzzy->addFuzzyRule(fuzzyRule4);

  FuzzyRuleAntecedent *normal_ringan = new FuzzyRuleAntecedent();
  normal_ringan->joinWithAND(normal, ringan);
  FuzzyRuleConsequent *rule5_output = new FuzzyRuleConsequent();
  rule5_output->addOutput(ringann);
  FuzzyRule *fuzzyRule5 = new FuzzyRule(5, normal_ringan, rule5_output);
  fuzzy->addFuzzyRule(fuzzyRule5);

  FuzzyRuleAntecedent *normal_sedang = new FuzzyRuleAntecedent();
  normal_sedang->joinWithAND(normal, sedang);
  FuzzyRuleConsequent *rule6_output = new FuzzyRuleConsequent();
  rule6_output->addOutput(sedangg);
  FuzzyRule *fuzzyRule6 = new FuzzyRule(6, normal_sedang, rule6_output);
  fuzzy->addFuzzyRule(fuzzyRule6);

  FuzzyRuleAntecedent *normal_normall = new FuzzyRuleAntecedent();
  normal_normall->joinWithAND(normal, normall);
  FuzzyRuleConsequent *rule7_output = new FuzzyRuleConsequent();
  rule7_output->addOutput(tidak);
  FuzzyRule *fuzzyRule7 = new FuzzyRule(7, normal_normall, rule7_output);
  fuzzy->addFuzzyRule(fuzzyRule7);

  FuzzyRuleAntecedent *normal_parah = new FuzzyRuleAntecedent();
  normal_parah->joinWithAND(normal, parah);
  FuzzyRuleConsequent *rule8_output = new FuzzyRuleConsequent();
  rule8_output->addOutput(parahh);
  FuzzyRule *fuzzyRule8 = new FuzzyRule(8, normal_parah, rule8_output);
  fuzzy->addFuzzyRule(fuzzyRule8);

  FuzzyRuleAntecedent *hyper_ringan = new FuzzyRuleAntecedent();
  hyper_ringan->joinWithAND(hyper, ringan);
  FuzzyRuleConsequent *rule9_output = new FuzzyRuleConsequent();
  rule9_output->addOutput(ringann);
  FuzzyRule *fuzzyRule9 = new FuzzyRule(9, hyper_ringan, rule9_output);
  fuzzy->addFuzzyRule(fuzzyRule9);

  FuzzyRuleAntecedent *hyper_sedang = new FuzzyRuleAntecedent();
  hyper_sedang->joinWithAND(hyper, sedang);
  FuzzyRuleConsequent *rule10_output = new FuzzyRuleConsequent();
  rule10_output->addOutput(sedangg);
  FuzzyRule *fuzzyRule10 = new FuzzyRule(10, hyper_sedang, rule10_output);
  fuzzy->addFuzzyRule(fuzzyRule10);

  FuzzyRuleAntecedent *hyper_normall = new FuzzyRuleAntecedent();
  hyper_normall->joinWithAND(hyper, normall);
  FuzzyRuleConsequent *rule11_output = new FuzzyRuleConsequent();
  rule11_output->addOutput(tidak);
  FuzzyRule *fuzzyRule11 = new FuzzyRule(11, hyper_normall, rule11_output);
  fuzzy->addFuzzyRule(fuzzyRule11);

  FuzzyRuleAntecedent *hyper_parah = new FuzzyRuleAntecedent();
  hyper_parah->joinWithAND(hyper, parah);
  FuzzyRuleConsequent *rule12_output = new FuzzyRuleConsequent();
  rule12_output->addOutput(sedangg);
  FuzzyRule *fuzzyRule12 = new FuzzyRule(12, hyper_parah, rule12_output);
  fuzzy->addFuzzyRule(fuzzyRule12);

}

void loop() {

  long irValue = particleSensor.getIR();

  if (checkForBeat(irValue) == true) {
    // We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute; // Store this reading in the array
      rateSpot %= RATE_SIZE; // Wrap variable

      // Take average of readings
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;

      // Calculate SpO2 using the provided formula
      spo2 = calculateSpO2(beatsPerMinute, irValue);
    }
  }

  // read the ADC value from the temperature sensor
  int adcVal = analogRead(PIN_LM35);
  // convert the ADC value to voltage in millivolt
  float milliVolt = adcVal * (ADC_VREF_mV / ADC_RESOLUTION);
  // convert the voltage to the temperature in °C
  float tempC = milliVolt / 10;
  // convert the °C to °F
  float tempF = tempC * 9 / 5 + 32;

  Serial.print("\n\nWELL B");
  
  Serial.print("\nDetakJantung: ");
  Serial.print(beatAvg);
  Serial.print("\nSaturasiOksigen: ");
  Serial.print(spo2);
  Serial.print("\nTemperature: ");
  Serial.print(tempC);   // print the temperature in °C
  Serial.print(" °C");

  fuzzy->setInput(1, beatAvg);
  fuzzy->setInput(2, spo2);
  fuzzy->fuzzify();
  float outputHipoksia = fuzzy->defuzzify(1);

  Serial.print("\nHipoksia: ");
  Serial.println(outputHipoksia);

  if (irValue < 50000)
    Serial.print("No finger?");

  Serial.println();

  // Membuat tampilan di layar OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("WELL B");

  if (irValue < 50000)
  {
    display.setCursor(65, 0);
    display.print("No finger?");
  }
  Serial.println();

  display.setCursor(0, 20);
  display.print("DetakJantung: ");
  display.print(beatAvg);
  display.println(" bpm");

  display.setCursor(0, 30);
  display.print("SaturasiOksi: ");
  display.print(spo2);
  display.println(" %");

  display.setCursor(0, 40);
  display.print("Temperatur: ");
  display.print(tempC);
  display.print(" C");

  display.setCursor(0, 50);
  display.print("Hipoksia: ");
  display.print(outputHipoksia);

  display.display();

  int a = random(0, 100);
  float b = random(0, 100);
  float c = random(0, 100);
  printBylnk(a, b, c);
  // Blynk.run();
  // Blynk.virtualWrite(V0, beatAvg);
  // Blynk.virtualWrite(V1, spo2);
  // Blynk.virtualWrite(V2, tempC);
}


// Function to calculate SpO2
float calculateSpO2(float beatsPerMinute, long irValue) {

  float ratio = beatsPerMinute / irValue;
  float spo2 = -25.5 * ratio + 110.9;

  // Ensure SpO2 value is within the valid range
  float mapspo2 = map(beatsPerMinute, 0, 140, 74, 100);

  return mapspo2;
}

void printBylnk(int beatAvg, float spo2, float tempC){
  Blynk.virtualWrite(V0, beatAvg);
  Blynk.virtualWrite(V1, spo2);
  Blynk.virtualWrite(V2, tempC);
}
