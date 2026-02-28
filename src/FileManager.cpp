// ===================== FileManager.cpp =====================
#include "FileManager.h"

FileManager::FileManager(uint8_t csPin) {
    sdCS = csPin;
}

bool FileManager::begin() {
    return SD.begin(sdCS);
}

int FileManager::scanFolder(const char *path, char (*outList)[MAX_NAME_LEN], int maxItems) {
    File dir = SD.open(path); 
    if (!dir) return 0;

    int count = 0;

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;

        if (!entry.isDirectory()) {
            const char *name = entry.name();
            if (strstr(name, ".raw") != nullptr) {
                strncpy(outList[count], name, MAX_NAME_LEN - 1);
                outList[count][MAX_NAME_LEN - 1] = '\0';
                count++;
                if (count >= maxItems) break;
            }
        }
        entry.close();
    }

    return count;
}