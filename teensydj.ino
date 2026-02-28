// ===================== main.ino =====================
#include <Arduino.h>
#include "src/FileManager.h"
#include "src/UIManager.h"
#include "src/AppState.h"
#include "src/AudioManager.h"
#include "src/Deck.h"
#include <ILI9341_t3n.h>
// === Display Setup ===
#define TFT_MISO 12
#define TFT_MOSI 11
#define TFT_CLK 13

#define TFT_CS 0
#define TFT_DC 3
#define TFT_RST 255
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_CLK, TFT_MISO);
// === Encoder Pins ===

#define CLK 29
#define DT  28
#define BUTTON  31
#define OK_BTN 30
#define OPEN_MENU 8
char trackFiles[MAX_TRACKS][MAX_NAME_LEN];
int totalTracks = 0;


FileManager fileMgr(10); // SD card CS pin
UIManager ui(tft);
AudioManager audio;

Deck* deckA;
Deck* deckB;


AppState appState = AppState::BOOT;
// Encoder state
int lastCLK = LOW;
unsigned long lastEncoderTime = 0;
const unsigned long encoderDebounce = 2; // ms

void setup() {
    Serial.begin(115200);
    pinMode(OPEN_MENU, INPUT_PULLUP);
    pinMode(CLK, INPUT);
    pinMode(DT, INPUT);
    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(OK_BTN, INPUT_PULLUP);
    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(ILI9341_WHITE);
    digitalWrite(TFT_CS, HIGH);   // vabasta ekraan
    delayMicroseconds(5);
    digitalWrite(10, HIGH);    // vabasta kaardi CS, et ta ei segaks
    delayMicroseconds(5);
    
    if (!fileMgr.begin()) {
        Serial.println("SD init failed");
        return;
    }
    digitalWrite(TFT_CS, LOW);    // anna SPI tagasi ekraanile
    totalTracks = fileMgr.scanFolder("/", trackFiles, MAX_TRACKS);
    Serial.print("Found "); Serial.println(totalTracks);

    ui.setTracks(trackFiles, totalTracks);
    ui.setDeck(true); // Deck A
    audio.begin();
    deckA = new Deck(audio.getPlayerA());
    deckB = new Deck(audio.getPlayerB());
     ui.begin();

    appState = AppState::MENU;
}

void loop() {

  
  

    switch (appState) {

        case AppState::BOOT:
            break;

        case AppState::MENU:
            handleMenuState();
            break;

        case AppState::PLAY:
            handlePlayState();
            break;

        case AppState::LOADING:
            handleLoadingState();
            break;
    }

    
}

void handleMenuState() {

    handleMenuEncoder();

    if (digitalRead(OK_BTN) == LOW) {

        appState = AppState::LOADING;
    }
}

void handleLoadingState() {

    tft.fillScreen(ILI9341_BLACK);

    bool deckAPlaying = (deckA->getState() == DeckState::PLAYING);
    bool deckBPlaying = (deckB->getState() == DeckState::PLAYING);



    const char* fileA = trackFiles[ui.getSelectedA()];
    const char* fileB = trackFiles[ui.getSelectedB()];

    bool loadAok = false;
    bool loadBok = false;

    Serial.println("=== LOADING STATE ===");

    if (deckAPlaying && !deckBPlaying) {

        Serial.print("Loading to Deck B: ");
        Serial.println(fileB ? fileB : "NULL");

        if (fileB != nullptr)
            loadBok = deckB->load(fileB);

    } 
    else if (deckBPlaying && !deckAPlaying) {

        Serial.print("Loading to Deck A: ");
        Serial.println(fileA ? fileA : "NULL");

        if (fileA != nullptr)
            loadAok = deckA->load(fileA);

    } 
    else {

        Serial.print("Loading to Deck A: ");
        Serial.println(fileA ? fileA : "NULL");

        Serial.print("Loading to Deck B: ");
        Serial.println(fileB ? fileB : "NULL");

        if (fileA != nullptr)
            loadAok = deckA->load(fileA);

        if (fileB != nullptr)
            loadBok = deckB->load(fileB);
    }

    Serial.print("Load A result: ");
    Serial.println(loadAok ? "OK" : "FAIL");

    Serial.print("Load B result: ");
    Serial.println(loadBok ? "OK" : "FAIL");

    // Mine PLAY state'i ainult kui vähemalt üks load õnnestus
    if (loadAok || loadBok) {

        Serial.println("-> Switching to PLAY state");
        appState = AppState::PLAY;

    } else {

        Serial.println("!!! Load failed, staying in MENU");
    }
}
void handlePlayState() {

    //audio.update();          // transport
    //waveform.update();       // scroll
    //uiRenderer.render();     // ekraan

    if (digitalRead(OPEN_MENU) == LOW) {
        appState = AppState::MENU;
        ui.begin();
    }
}
void handleMenuEncoder() {
    unsigned long now = millis();
    if (now - lastEncoderTime < encoderDebounce) return;

    int clkState = digitalRead(CLK);
    if (clkState != lastCLK) {
        lastCLK = clkState;
        lastEncoderTime = now;

        int direction = (digitalRead(DT) != clkState) ? 1 : -1;
        ui.updateSelection(direction);
    }
}
