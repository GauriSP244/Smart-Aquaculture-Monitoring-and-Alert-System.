#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// === Pins ===
#define ONE_WIRE_BUS 2
#define PH_SENSOR_PIN A0
#define TDS_SENSOR_PIN A1
#define TURBIDITY_SENSOR_PIN A3

// === LCD & Sensor Setup ===
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2);

bool showIPDone = false;

void setup() {
  Serial.begin(115200);
  sensors.begin();
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Waiting for IP...");
}

void loop() {
  // Handle IP display from Serial input once
  if (Serial.available() && !showIPDone) {
    String ipData = Serial.readStringUntil('\n');
    if (ipData.startsWith("IP:")) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("IP Address:");
      lcd.setCursor(0, 1);
      lcd.print(ipData.substring(3));
      delay(3000);
      showIPDone = true;
    }
  }

  if (showIPDone) {
    // === Read Sensors ===
    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(0);

    int rawPH = analogRead(PH_SENSOR_PIN);
    float voltagePH = rawPH * (5.0 / 1023.0);
    float pHValue = (voltagePH - 0.3) * 14.0 / 3.0;

    int rawTDS = analogRead(TDS_SENSOR_PIN);
    float voltageTDS = rawTDS * (5.0 / 1023.0);
    float tdsValue = voltageTDS * 500;

    // Turbidity sensor reading
    float volt = 0;
    for (int i = 0; i < 800; i++) {
      volt += ((float)analogRead(TURBIDITY_SENSOR_PIN) / 1023.0) * 5.0;
    }
    volt = volt / 800.0;

    float turbidityNTU;
    if (volt >= 4.2) {
  turbidityNTU = 0;  // Clean water
} else if (volt <= 2.5) {
  turbidityNTU = 3000;  // Very dirty water or error
} else {
  turbidityNTU = -1120.4 * pow(volt, 2) + 5742.3 * volt - 4353.8;
}


    // === Display on LCD ===
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature, 1);
    lcd.print("C ");

    lcd.print("pH:");
    lcd.print(pHValue, 1);

    lcd.setCursor(0, 1);
    lcd.print("TDS:");
    lcd.print(tdsValue, 0);
    lcd.print(" NTU:");
    lcd.print((int)turbidityNTU);

    // === Send to Serial/NodeMCU ===
    Serial.print(temperature);
    Serial.print(",");
    Serial.print(pHValue);
    Serial.print(",");
    Serial.print(tdsValue);
    Serial.print(",");
    Serial.println(turbidityNTU);

    delay(1000);  // 1 second delay
  }
}