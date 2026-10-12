#define USE_PSRAM

#include <Arduino.h>
#include <BluetoothA2DPSource.h>

#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSD.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

#include <SD.h>
#include <SPI.h>
#include <sd_card_setup.h>
#include <pins.h>

struct RemoteDevice
{
    String name;
    esp_bd_addr_t address;
};

RemoteDevice found_devices[15];
int device_count = 0;
bool is_connected = false;

BluetoothA2DPSource a2dp_source;

AudioSourceSD source("/", ".mp3", SD_CS);
MP3DecoderHelix decoder;

SynchronizedNBuffer audio_buffer(512, 80, portMAX_DELAY, 10);
QueueStream<uint8_t> out(audio_buffer);
AudioPlayer player(source, out, decoder);

// ---- PLAYBACK STATE ----
bool is_playing = false;
float current_volume = 0.5;

String mp3_files[50];
int mp3_count = 0;

void scanMP3Files(fs::FS &fs)
{
    mp3_count = 0;
    File root = fs.open("/");
    if (!root)
        return;

    File file = root.openNextFile();
    while (file)
    {
        if (!file.isDirectory())
        {
            String name = file.name();
            if (name.endsWith(".mp3") || name.endsWith(".MP3"))
            {
                mp3_files[mp3_count] = String("/") + name;
                mp3_count++;
                if (mp3_count >= 50)
                    break;
            }
        }
        file = root.openNextFile();
    }
    root.close();
}

int32_t get_sound_data(uint8_t *data, int32_t byteCount)
{
    return audio_buffer.readArray(data, byteCount);
}

bool found_device_callback(const char *ssid, esp_bd_addr_t address, int rssi)
{
    if (ssid == nullptr)
        return true;

    for (int i = 0; i < device_count; i++)
    {
        if (found_devices[i].name == ssid)
            return true;
    }

    if (device_count < 15)
    {
        found_devices[device_count].name = ssid;
        memcpy(found_devices[device_count].address, address, 6);
        device_count++;
    }
    return true;
}

void is_connected_callback(esp_a2d_connection_state_t state, void *obj)
{
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED)
    {
        is_connected = true;
        Serial.println("Connected successfully!");
    }
    else
    {
        is_connected = false;
        is_playing = false;
    }
}

void setup()
{
    Serial.begin(115200);
    // AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Debug);
    if (!psramInit())
    {
        Serial.println("PSRAM Fail! WROVER needs PSRAM for A2DP.");
        while (1)
            ;
    }

    initSDCard();

    out.begin(95);

    player.setVolume(current_volume);
    player.setAutoNext(false);

    a2dp_source.set_data_callback(get_sound_data);
    a2dp_source.set_ssid_callback(found_device_callback);
    a2dp_source.set_on_connection_state_changed(is_connected_callback);

    Serial.println("Starting Discovery...");
    Serial.println("'p' list devices // 'c' connect // 'd' disconnect // 'l' list songs // ' ' play/pause // ',' vol down // '.' vol up");

    a2dp_source.start("");
}

void loop()
{
    if (is_connected && is_playing)
    {
        player.copy();
    }

    if (Serial.available())
    {
        char cmd = Serial.read();

        if (cmd == 'p')
        {
            Serial.println("\nDiscovered Devices:");
            for (int i = 0; i < device_count; i++)
            {
                Serial.printf("[%d] %s\n", i, found_devices[i].name.c_str());
            }
            Serial.println("--------------------------");
        }

        else if (cmd == 'c')
        {
            Serial.println("Enter number of device to connect (0-9): ");
            while (!Serial.available())
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            char key = Serial.read();
            if (key >= '0' && key <= '9')
            {
                int selection = key - '0';
                if (selection < device_count)
                {
                    Serial.printf("Connecting to: %s...\n", found_devices[selection].name.c_str());
                    a2dp_source.start(found_devices[selection].name.c_str());
                }
                else
                {
                    Serial.println("Invalid selection.");
                }
            }
        }

        else if (cmd == 'd')
        {
            if (is_connected)
            {
                a2dp_source.disconnect();
                is_connected = false;
                is_playing = false;
                Serial.println("Disconnected.");
            }
            else
            {
                Serial.println("Not connected.");
            }
        }

        else if (cmd == 'l')
        {
            scanMP3Files(SD);
            if (mp3_count == 0)
            {
                Serial.println("No MP3 files found on SD card!");
            }
            else
            {
                Serial.println("\n--- MP3 Files ---");
                for (int i = 0; i < mp3_count; i++)
                {
                    Serial.printf("[%d] %s\n", i, mp3_files[i].c_str());
                }
                Serial.println("Enter song number to play:");

                while (!Serial.available())
                {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                }
                int selection = Serial.parseInt();

                if (selection >= 0 && selection < mp3_count)
                {
                    if (is_connected)
                    {
                        Serial.printf("Playing: %s\n", mp3_files[selection].c_str());
                        player.setPath(mp3_files[selection].c_str());
                        player.begin(selection); // idk why that works
                        player.setAutoNext(false);
                        is_playing = true;
                    }
                    else
                    {
                        Serial.println("Cannot play: Connect headphones first!");
                    }
                }
                else
                {
                    Serial.println("Invalid song selection.");
                }
            }
        }

        else if (cmd == ' ')
        {
            if (is_connected)
            {
                is_playing = !is_playing;
                if (is_playing)
                {
                    Serial.println("Playback Resumed.");
                }
                else
                {
                    Serial.println("Playback Paused.");
                }
            }
            else
            {
                Serial.println("Connect headphones first!");
            }
        }

        else if (cmd == ',')
        {
            current_volume -= 0.1;
            if (current_volume < 0.0)
                current_volume = 0.0;
            player.setVolume(current_volume);
            Serial.printf("Volume down: %.1f\n", current_volume);
        }

        else if (cmd == '.')
        {
            current_volume += 0.1;
            if (current_volume > 1.0)
                current_volume = 1.0;
            player.setVolume(current_volume);
            Serial.printf("Volume up: %.1f\n", current_volume);
        }
    }
}