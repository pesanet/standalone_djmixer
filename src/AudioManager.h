#pragma once

#include <Arduino.h>
#include <Audio.h>
#include <SD.h>
#include <TeensyVariablePlayback.h>

class AudioManager {
public:
    AudioManager();

    bool begin(uint8_t csPin = BUILTIN_SDCARD);

    AudioPlaySdResmp& getPlayerA();
    AudioPlaySdResmp& getPlayerB();

    // ===== Deck A =====
    bool playA(const char* filename);
    void stopA();
    bool isPlayingA() const;
    float positionA() const;
    float lengthA() const;
    void setRateA(float rate);

    // ===== Deck B =====
    bool playB(const char* filename);
    void stopB();
    bool isPlayingB() const;
    float positionB() const;
    float lengthB() const;
    void setRateB(float rate);

private:
    bool sdReady = false;

    // Players
    AudioPlaySdResmp playAraw;
    AudioPlaySdResmp playBraw;
    
    // Mixer
    AudioMixer4 mixer;

    // Output
    AudioOutputI2S i2s;

    // Audio connections
    AudioConnection* patchCords[6];
};