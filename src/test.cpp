#include <Arduino.h>
#include <BluetoothA2DPSource.h>
#include <SD.h>
#include <SPI.h>
#include <sd_card_setup.h>
#include <pins.h>

BluetoothA2DPSource a2dp_source;
File currentAudioFile;

struct RemoteDevice {
    String name;
    esp_bd_addr_t address;
};

RemoteDevice found_devices[15];
int device_count = 0;
bool should_print_list = false;
bool is_connected = false;

int32_t get_sound_data(uint8_t *data, int32_t byteCount) {
    return byteCount;
}

bool found_device_callback(const char* ssid, esp_bd_addr_t address, int rssi) {
    if (ssid == nullptr) return true;

    for (int i = 0; i < device_count; i++) {
        if (found_devices[i].name == ssid) return true;
    }

    if (device_count < 15) {
        found_devices[device_count].name = ssid;
        memcpy(found_devices[device_count].address, address, 6);
        device_count++;
        
        should_print_list = true; 
    }
    return true;


}

void is_connected_callback(esp_a2d_connection_state_t state, void* obj) {
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        is_connected = true;
        Serial.println("Connected successfully!");
    }


    else {
        is_connected = false;
    }
}

void setup() {
    Serial.begin(115200);
    if (!psramInit()) {
        Serial.println("PSRAM Fail! WROVER needs PSRAM for A2DP.");
        while(1);
    }

    initSDCard();

    a2dp_source.set_data_callback(get_sound_data);
    a2dp_source.set_ssid_callback(found_device_callback); 
    a2dp_source.set_on_connection_state_changed(is_connected_callback);


    Serial.println("Starting Discovery...");
    Serial.println("'p' see current list // 'c' connect // 'd' disconnect (only when connected) . . .");
    
    a2dp_source.start("");
}

void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();

        if (cmd == 'p') {
            Serial.println("\nDiscovered Devices:");
            for (int i = 0; i < device_count; i++) {
                Serial.printf("[%d] %s\n", i, found_devices[i].name.c_str());
            }
            Serial.println("--------------------------");
        } 

        if (cmd == 'c') {
            Serial.println("Enter number of device to connect (0-9): ");
            while (!Serial.available()) {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }

            char key = Serial.read();

            if (key >= '0' && key <= '9') {
                int selection = key - '0'; // ascii to int
                
                if (selection < device_count) {
                    Serial.printf("Connecting to: %s...\n", found_devices[selection].name.c_str());
                    a2dp_source.start(found_devices[selection].name.c_str());
                } else {
                    Serial.println("Invalid selection.");
                }
            }
        }

        if (cmd == 'd') {
            if (is_connected) {
                a2dp_source.disconnect();
                is_connected = false;

                Serial.println("Disconnected.");
            }
            else {
                Serial.println("Not connected.");
            }
        }

        if (cmd == 'l') {
            if (SD.totalBytes() <= 0) {
                Serial.println("SD Card not mounted yet, maybe a bug or just wait lmao...");
            }
            else {
                listSDCardFiles(SD);
            }
        }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);


}