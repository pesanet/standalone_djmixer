// ===================== FileManager.h =====================
#pragma once
#include <Arduino.h>
#include <SD.h>

#define MAX_TRACKS 128
#define MAX_NAME_LEN 128

class FileManager {
public:
    FileManager(uint8_t csPin);

    // Käivita SD-kaart
    bool begin();

    // Loe kaustast failid ja tagasta track list
    // tagastab leitud failide arvu
    int scanFolder(const char *path, char (*outList)[MAX_NAME_LEN], int maxItems);

private:
    uint8_t sdCS;
};