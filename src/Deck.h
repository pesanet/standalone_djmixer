#ifndef DECK_H
#define DECK_H

#include <Audio.h>
#include <TeensyVariablePlayback.h>
#include "DeckState.h"

class Deck {
public:
    Deck(AudioPlaySdResmp& player);

    void update();

    bool load(const char* filename);
    void play();
    void pause();
    void stop();

    void setTempo(float bpm);
    float getTempo() const;

    float getPositionSec();
    float getLengthSec();

    DeckState getState() const;

private:
    AudioPlaySdResmp& playRaw;

    DeckState state = DeckState::EMPTY;

    float bpm = 0.0f;
    float playbackRate = 1.0f;

    const char* currentFile = nullptr;

    void applyPlaybackRate();
};

#endif