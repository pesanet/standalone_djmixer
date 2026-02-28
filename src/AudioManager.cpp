#include "AudioManager.h"

AudioManager::AudioManager() {

    // Deck A -> Mixer
    patchCords[0] = new AudioConnection(playAraw, 0, mixer, 0);
    patchCords[1] = new AudioConnection(playAraw, 1, mixer, 1);

    // Deck B -> Mixer
    patchCords[2] = new AudioConnection(playBraw, 0, mixer, 2);
    patchCords[3] = new AudioConnection(playBraw, 1, mixer, 3);

    // Mixer -> I2S
    patchCords[4] = new AudioConnection(mixer, 0, i2s, 0);
    patchCords[5] = new AudioConnection(mixer, 0, i2s, 1);
}

bool AudioManager::begin(uint8_t csPin) {

    AudioMemory(80);

    if (!SD.begin(csPin)) {
        sdReady = false;
        return false;
    }

    sdReady = true;

    mixer.gain(0, 0.8);  // Deck A L
    mixer.gain(1, 0.8);  // Deck A R
    mixer.gain(2, 0.8);  // Deck B L
    mixer.gain(3, 0.8);  // Deck B R

    return true;
}

// ===== Deck A =====

bool AudioManager::playA(const char* filename) {
    if (!sdReady) return false;
    return playAraw.playRaw(filename,2);
}

void AudioManager::stopA() {
    playAraw.stop();
}

bool AudioManager::isPlayingA() const {
    return playAraw.isPlaying();
}

float AudioManager::positionA() const {
    return playAraw.positionMillis() / 1000.0f;
}

float AudioManager::lengthA() const {
    return playAraw.lengthMillis() / 1000.0f;
}

void AudioManager::setRateA(float rate) {
    playAraw.setPlaybackRate(rate);
}


// ===== Deck B =====

bool AudioManager::playB(const char* filename) {
    if (!sdReady) return false;
    return playBraw.playRaw(filename,2);
}

void AudioManager::stopB() {
    playBraw.stop();
}

bool AudioManager::isPlayingB() const {
    return playBraw.isPlaying();
}

float AudioManager::positionB() const {
    return playBraw.positionMillis() / 1000.0f;
}

float AudioManager::lengthB() const {
    return playBraw.lengthMillis() / 1000.0f;
}

void AudioManager::setRateB(float rate) {
    playBraw.setPlaybackRate(rate);
}

AudioPlaySdResmp& AudioManager::getPlayerA() {
    return playAraw;
}

AudioPlaySdResmp& AudioManager::getPlayerB() {
    return playBraw;
}