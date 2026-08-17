#include "sd_card_setup.h"
#include "pins.h"
#include <SPI.h>
#include <SD.h>
#include "BluetoothA2DPSource.h"

void initSDCard() {
    pinMode(SD_MISO, INPUT_PULLUP); // Sometimes MISO needs a pullup
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH); // Ensure CS is high before starting SPI

    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    // Try a lower frequency (1 MHz instead of 4 MHz) for better stability
    if(!SD.begin(SD_CS, SPI)){
        Serial.println("Card Mount Failed");
        return;
    }

    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return;
    }
}

void listSDCardFiles(fs::FS &fs) {
    File root = fs.open("/");

    if (!root) {
        Serial.printf("Failed to open root directory!\n");

    }
    if (!root.isDirectory()) {
        Serial.printf("Root is not a directory!\n");
    }

    Serial.println("List of files:");

    File file = root.openNextFile();
    
    while (file) {
        if (!file.isDirectory()) {
            Serial.printf("FILE: %-20s | SIZE: %d bytes\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
    
    root.close();

}
