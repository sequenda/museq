#pragma once

#include <Arduino.h>
#include <FS.h>

void initSDCard();
void listSDCardFiles(fs::FS &fs);
// bool playTrack(const char* filename);
// void stopTrack();
