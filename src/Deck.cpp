#include "Deck.h"

Deck::Deck(AudioPlaySdResmp& player)
    : playRaw(player)
{
}

bool Deck::load(const char* filename) {

    if (state == DeckState::PLAYING)
        stop();

    state = DeckState::LOADING;

    if (!playRaw.playRaw(filename,2)) {
        state = DeckState::EMPTY;
        return false;
    }

    currentFile = filename;
    state = DeckState::LOADED;

    return true;
}

void Deck::play() {

    if (state == DeckState::LOADED ||
        state == DeckState::PAUSED) {

        playRaw.playRaw(currentFile,2);
        applyPlaybackRate();
        state = DeckState::PLAYING;
    }
}

void Deck::pause() {

    if (state == DeckState::PLAYING) {
        playRaw.stop();
        state = DeckState::PAUSED;
    }
}

void Deck::stop() {

    playRaw.stop();
    state = DeckState::LOADED;
}

void Deck::update() {

    if (state == DeckState::PLAYING) {
        if (!playRaw.isPlaying()) {
            state = DeckState::LOADED;
        }
    }
}

void Deck::setTempo(float newBpm) {

    bpm = newBpm;

    // playbackRate arvutatakse hiljem sync engine'is
}

float Deck::getTempo() const {
    return bpm;
}

float Deck::getPositionSec() {
    return playRaw.positionMillis() / 1000.0f;
}

float Deck::getLengthSec() {
    return playRaw.lengthMillis() / 1000.0f;
}

DeckState Deck::getState() const {
    return state;
}

void Deck::applyPlaybackRate() {
    playRaw.setPlaybackRate(playbackRate);
}