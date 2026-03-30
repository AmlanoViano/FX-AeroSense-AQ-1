/**
 * Integrated Air Quality Monitor – Dhaka Pollution Mapping
 * Sensors: SPS30, SHT35, MH-Z19E, MICS-4514
 * Logging: DS3231 RTC, microSD card (FS.h + SD.h)
 * 
 * Wiring:
 *   I2C (SDA/SCL)   -> GPIO21 / GPIO22
 *   SD Card         -> CS=5, MOSI=23, MISO=19, SCK=18 (default SPI)
 *   MH-Z19E (UART2) -> RX=16, TX=17
 *   MICS-4514       -> NOX=34, RED=35 (with voltage dividers)
 *   LED             -> GPIO2
 * 
 * Power: External 5V supply recommended (common ground with ESP32)
 */

#include <Wire.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <RTClib.h>
#include <Adafruit_SHT31.h>
#include <MHZ19.h>
#include <SensirionI2cSps30.h>

// ========== Pin Definitions ==========
#define PIN_LED        2
#define PIN_MICS_NOX   34
#define PIN_MICS_RED   35

// I2C addresses
#define SHT35_ADDR     0x44
#define SPS30_ADDR     SPS30_I2C_ADDR_69

// ========== Measurement Settings ==========
const unsigned long MEASUREMENT_INTERVAL_MS = 5000;  // 5 seconds
const int ADC_SAMPLES = 10;

// ========== Global Objects ==========
RTC_DS3231 rtc;
Adafruit_SHT31 sht31;
MHZ19 mhz19;
HardwareSerial mhZ19Serial(2);
SensirionI2cSps30 sps30;

// ========== State Variables ==========
unsigned long lastMeasurementTime = 0;
bool sdInitialized = false;
bool cleaningInProgress = false;
unsigned long cleaningStartTime = 0;
const unsigned long CLEANING_DURATION = 10000;

// ========== Function Prototypes ==========
void printError(uint16_t error);
String getTimestampString(DateTime now);
String readAllSensors(DateTime now);
void writeToSD(String dataLine);
bool writeToFile(String filename, String dataLine);
void blinkLED(int times, int onMs, int offMs);
void triggerManualCleaning();
void listSDContents();
void scanI2C();

// ========== Setup ==========
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(100);
    delay(1000);
    
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    
    Serial.println("\n=========================================");
    Serial.println("Integrated Air Quality Monitor");
    Serial.println("Dhaka Pollution Mapping Project");
    Serial.println("=========================================\n");
    
    // I2C scanner
    scanI2C();
    Wire.begin();
    
    // SD card initialization (as in working example)
    if (!SD.begin()) {  // uses default SPI pins
        Serial.println("SD card initialization failed!");
        sdInitialized = false;
        blinkLED(3, 200, 200);
    } else {
        sdInitialized = true;
        Serial.println("SD card initialized.");
        blinkLED(1, 500, 0);
        
        // Optional: list root directory
        listSDContents();
        
        // Test write to confirm writability
        File testFile = SD.open("/test.txt", FILE_WRITE);
        if (testFile) {
            testFile.println("SD card write test");
            testFile.close();
            Serial.println("SD write test: SUCCESS");
            SD.remove("/test.txt");
        } else {
            Serial.println("SD write test: FAILED");
            sdInitialized = false;
        }
    }
    
    // RTC
    if (!rtc.begin()) {
        Serial.println("RTC not found! Check wiring.");
        while (1) blinkLED(1, 100, 100);
    } else {
        if (rtc.lostPower()) {
            Serial.println("RTC lost power! Setting to compile time.");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        DateTime now = rtc.now();
        Serial.printf("RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
                      now.year(), now.month(), now.day(),
                      now.hour(), now.minute(), now.second());
    }
    
    // SHT35
    if (!sht31.begin(SHT35_ADDR)) {
        Serial.println("SHT35 not found! Check wiring.");
    } else {
        Serial.println("SHT35 found.");
    }
    
    // MH-Z19E
    mhZ19Serial.begin(9600, SERIAL_8N1, 16, 17);
    mhz19.begin(mhZ19Serial);
    mhz19.autoCalibration(false);
    delay(100);
    Serial.println("MH-Z19E initialized.");
    
    // SPS30
    bool sps30Ok = false;
    for (int retry = 0; retry < 3; retry++) {
        sps30.begin(Wire, SPS30_ADDR);
        sps30.stopMeasurement();
        delay(200);
        
        int8_t serialNumber[32] = {0};
        uint16_t error = sps30.readSerialNumber(serialNumber, 32);
        if (error == 0) {
            Serial.print("SPS30 serial: ");
            Serial.println((const char*)serialNumber);
            sps30Ok = true;
            break;
        } else {
            Serial.print("SPS30 error (attempt ");
            Serial.print(retry + 1);
            Serial.print("): ");
            printError(error);
            delay(1000);
        }
    }
    
    if (!sps30Ok) {
        Serial.println("SPS30 not responding! Check power and wiring.");
    } else {
        uint16_t error = sps30.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_UINT16);
        if (error != 0) {
            Serial.print("SPS30 start measurement error: ");
            printError(error);
        } else {
            Serial.println("SPS30 measurement started.");
        }
        delay(500);
    }
    
    Serial.println("\nSystem ready. Logging every 5 seconds.");
    Serial.println("Commands: 'c' = manual fan cleaning (SPS30)");
    Serial.println("Data format: Timestamp,PM1.0,PM2.5,PM4.0,PM10.0,"
                    "NC0.5,NC1.0,NC2.5,NC4.0,NC10.0,TypicalSize,"
                    "Temperature_C,Humidity_%,CO2_ppm,NOX_ADC,RED_ADC\n");
    
    // Create CSV header if SD card is writable
    if (sdInitialized) {
        DateTime now = rtc.now();
        char filename[20];
        snprintf(filename, sizeof(filename), "/%04d-%02d-%02d.csv",
                 now.year(), now.month(), now.day());
        if (!SD.exists(filename)) {
            File file = SD.open(filename, FILE_WRITE);
            if (file) {
                file.println("Timestamp,PM1.0,PM2.5,PM4.0,PM10.0,"
                             "NC0.5,NC1.0,NC2.5,NC4.0,NC10.0,TypicalSize,"
                             "Temperature_C,Humidity_%,CO2_ppm,NOX_ADC,RED_ADC");
                file.close();
                Serial.println("CSV header written.");
            } else {
                Serial.print("Failed to create header file: ");
                Serial.println(filename);
                sdInitialized = false;
            }
        } else {
            Serial.print("File already exists: ");
            Serial.println(filename);
        }
    }
}

// ========== Main Loop ==========
void loop() {
    // Handle cleaning state
    if (cleaningInProgress) {
        if (millis() - cleaningStartTime >= CLEANING_DURATION) {
            cleaningInProgress = false;
            digitalWrite(PIN_LED, LOW);
            Serial.println("\n[INFO] Fan cleaning completed.");
            sps30.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_UINT16);
        } else {
            static unsigned long lastBlink = 0;
            if (millis() - lastBlink > 500) {
                digitalWrite(PIN_LED, !digitalRead(PIN_LED));
                lastBlink = millis();
            }
        }
        lastMeasurementTime = millis();
    }
    
    // Take measurement at fixed interval
    if (!cleaningInProgress && (millis() - lastMeasurementTime >= MEASUREMENT_INTERVAL_MS)) {
        DateTime now = rtc.now();
        String dataLine = readAllSensors(now);
        Serial.println(dataLine);
        
        if (sdInitialized) {
            writeToSD(dataLine);
        } else {
            Serial.println("[WARN] SD card not available, data not logged.");
        }
        
        digitalWrite(PIN_LED, HIGH);
        delay(100);
        digitalWrite(PIN_LED, LOW);
        
        lastMeasurementTime = millis();
    }
    
    // Manual cleaning command
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == 'c' || cmd == 'C') {
            triggerManualCleaning();
        }
    }
}

// ========== Sensor Reading ==========
String readAllSensors(DateTime now) {
    String line = getTimestampString(now);
    
    // ----- SPS30 -----
    uint16_t dataReadyFlag = 0;
    uint16_t mc1p0=0, mc2p5=0, mc4p0=0, mc10p0=0;
    uint16_t nc0p5=0, nc1p0=0, nc2p5=0, nc4p0=0, nc10p0=0;
    uint16_t typicalParticleSize = 0;
    
    int16_t error = sps30.readDataReadyFlag(dataReadyFlag);
    if (error == 0 && dataReadyFlag) {
        error = sps30.readMeasurementValuesUint16(
            mc1p0, mc2p5, mc4p0, mc10p0,
            nc0p5, nc1p0, nc2p5, nc4p0,
            nc10p0, typicalParticleSize);
        if (error != 0) {
            Serial.print("SPS30 read error: ");
            printError(error);
        }
    } else if (error != 0) {
        Serial.print("SPS30 dataReady error: ");
        printError(error);
        static unsigned long lastRecovery = 0;
        if (millis() - lastRecovery > 10000) {
            Serial.println("Attempting SPS30 recovery...");
            sps30.stopMeasurement();
            delay(100);
            sps30.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_UINT16);
            lastRecovery = millis();
        }
    }
    
    float pm1_0  = mc1p0 / 10.0;
    float pm2_5  = mc2p5 / 10.0;
    float pm4_0  = mc4p0 / 10.0;
    float pm10_0 = mc10p0 / 10.0;
    float nc0_5  = nc0p5 / 10.0;
    float nc1_0  = nc1p0 / 10.0;
    float nc2_5  = nc2p5 / 10.0;
    float nc4_0  = nc4p0 / 10.0;
    float nc10_0 = nc10p0 / 10.0;
    float typicalSize = typicalParticleSize / 10.0;
    
    line += "," + String(pm1_0, 1);
    line += "," + String(pm2_5, 1);
    line += "," + String(pm4_0, 1);
    line += "," + String(pm10_0, 1);
    line += "," + String(nc0_5, 1);
    line += "," + String(nc1_0, 1);
    line += "," + String(nc2_5, 1);
    line += "," + String(nc4_0, 1);
    line += "," + String(nc10_0, 1);
    line += "," + String(typicalSize, 1);
    
    // ----- SHT35 -----
    float temp = sht31.readTemperature();
    float hum = sht31.readHumidity();
    if (isnan(temp)) temp = -999.9;
    if (isnan(hum))  hum = -999.9;
    line += "," + String(temp, 1);
    line += "," + String(hum, 1);
    
    // ----- MH-Z19E -----
    int co2 = mhz19.getCO2();
    if (co2 <= 0) co2 = -999;
    line += "," + String(co2);
    
    // ----- MICS-4514 -----
    long noxSum = 0, redSum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        noxSum += analogRead(PIN_MICS_NOX);
        redSum += analogRead(PIN_MICS_RED);
        delay(5);
    }
    int noxADC = noxSum / ADC_SAMPLES;
    int redADC = redSum / ADC_SAMPLES;
    float noxVoltage = (noxADC / 4095.0) * 3.3;
    float redVoltage = (redADC / 4095.0) * 3.3;
    line += "," + String(noxADC);
    line += "," + String(redADC);
    line += "," + String(noxVoltage);
    line += "," + String(redVoltage);
    
    return line;
}

// ========== Helper: Format timestamp ==========
String getTimestampString(DateTime now) {
    char buf[25];
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
            now.year(), now.month(), now.day(),
            now.hour(), now.minute(), now.second());
    return String(buf);
}

// ========== SD Writing with Retry ==========
void writeToSD(String dataLine) {
    DateTime now = rtc.now();
    if (now.year() < 2025) {
        Serial.println("[WARN] RTC date invalid, using default filename");
        if (writeToFile("/data.csv", dataLine)) return;
        else {
            Serial.println("SD write failed permanently. Data not logged.");
            blinkLED(3, 200, 200);
        }
        return;
    }
    
    char filename[20];
    snprintf(filename, sizeof(filename), "/%04d-%02d-%02d.csv",
             now.year(), now.month(), now.day());
    
    if (!writeToFile(String(filename), dataLine)) {
        Serial.print("SD write failed after retries for ");
        Serial.println(filename);
        blinkLED(2, 200, 200);
    }
}

bool writeToFile(String filename, String dataLine) {
    // Wait a moment for power to stabilize
    delay(10);
    
    // Retry opening the file up to 3 times
    for (int attempt = 0; attempt < 3; attempt++) {
        File file = SD.open(filename, FILE_APPEND);
        if (file) {
            file.println(dataLine);
            file.close();
            return true;
        }
        delay(50);
    }
    
    // If still failing, try to reinitialize the SD card
    Serial.println("SD reinitialize attempt...");
    if (SD.begin()) {  // re‑initialize
        delay(100);
        File file = SD.open(filename, FILE_APPEND);
        if (file) {
            file.println(dataLine);
            file.close();
            return true;
        }
    }
    
    return false;
}

// ========== Manual Cleaning ==========
void triggerManualCleaning() {
    Serial.println("\n--- Manual Cleaning Triggered ---");
    sps30.stopMeasurement();
    delay(100);
    uint16_t error = sps30.startFanCleaning();
    if (error != 0) {
        Serial.print("Error starting fan cleaning: ");
        printError(error);
        sps30.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_UINT16);
    } else {
        cleaningInProgress = true;
        cleaningStartTime = millis();
        Serial.println("Fan cleaning started (10 seconds)");
        digitalWrite(PIN_LED, HIGH);
    }
}

// ========== Error Printing ==========
void printError(uint16_t error) {
    char errorMessage[64];
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
}

// ========== Blink LED ==========
void blinkLED(int times, int onMs, int offMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(PIN_LED, HIGH);
        delay(onMs);
        digitalWrite(PIN_LED, LOW);
        if (offMs > 0) delay(offMs);
    }
}

// ========== List SD Contents (from example) ==========
void listSDContents() {
    Serial.println("Root directory:");
    File root = SD.open("/");
    while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        Serial.print("  ");
        Serial.print(entry.name());
        if (entry.isDirectory()) Serial.println("/");
        else {
            Serial.print(" (");
            Serial.print(entry.size());
            Serial.println(" bytes)");
        }
        entry.close();
    }
    root.close();
}

// ========== I2C Scanner ==========
void scanI2C() {
    Serial.println("Scanning I2C bus...");
    byte error, address;
    int nDevices = 0;
    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("  Device found at 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            nDevices++;
        }
    }
    if (nDevices == 0) Serial.println("  No I2C devices found.");
    else Serial.println("I2C scan complete.");
}
