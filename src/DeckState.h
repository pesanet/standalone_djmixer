#ifndef DECK_STATE_H
#define DECK_STATE_H

enum class DeckState {
    EMPTY,      // pole lugu
    LOADING,    // fail laaditakse
    LOADED,     // valmis mängimiseks
    PLAYING,
    PAUSED,
    CUEING
};

#endif