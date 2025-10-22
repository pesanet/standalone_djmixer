#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>
#include <Wire.h>
#include <ILI9341_t3n.h>
#include <TeensyVariablePlayback.h>
#include <Encoder.h>
// === Audio Setup ===
AudioFilterBiquad biquadA_L; // Deck A Left
AudioFilterBiquad biquadA_R; // Deck A Right
AudioFilterBiquad biquadB_L; // Deck B Left
AudioFilterBiquad biquadB_R; // Deck B Right

AudioPlaySdResmp playRaw1;
AudioPlaySdResmp playRaw2;
AudioOutputI2S audioOutput;
AudioMixer4 mixer1; // Left mixer
AudioMixer4 mixer2; // Right mixer

// --- Deck A ühendused ---
AudioConnection patchCord1(playRaw1, 0, biquadA_L, 0); // A left in
AudioConnection patchCord2(playRaw1, 1, biquadA_R, 0); // A right in
AudioConnection patchCord3(biquadA_L, 0, mixer1, 0);   // A left -> left mixer
AudioConnection patchCord4(biquadA_R, 0, mixer2, 0);   // A right -> right mixer

// --- Deck B ühendused ---
AudioConnection patchCord5(playRaw2, 0, biquadB_L, 0); // B left in
AudioConnection patchCord6(playRaw2, 1, biquadB_R, 0); // B right in
AudioConnection patchCord7(biquadB_L, 0, mixer1, 1);   // B left -> left mixer
AudioConnection patchCord8(biquadB_R, 0, mixer2, 1);   // B right -> right mixer

// --- Output ---
AudioConnection patchCord9(mixer1, 0, audioOutput, 0); // Left out
AudioConnection patchCord10(mixer2, 0, audioOutput, 1); // Right out

int menuCurrentPage = 0;
int menuTotalPages = 0;
#define MENU_TRACKS_PER_PAGE 15
// === Display Setup ===
#define TFT_MISO 12
#define TFT_MOSI 11
#define TFT_CLK 13

#define TFT_CS 0
#define TFT_DC 3
#define TFT_RST 255
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_CLK, TFT_MISO);
#define TFT_DARKGREY 0x7BEF


// === SD / File Setup ===
const int SD_CS = 10;
char trackFiles[10][200];
int totalTracks = 0;
int selectedTrackA = 0;
int selectedTrackB = 1;
bool selectingDeckA = true;
bool inMenu = true;
bool inMenu2 = false;
int targetDeck = 1; // default Deck B
// === Display Config ===
#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240
#define WAVEFORM_Y1    20
#define WAVEFORM_Y2    100
#define WAVEFORM_H     80
#define PLAYHEAD_X     (SCREEN_WIDTH / 2)
const int FIXED_VIEW_WIDTH = 4000;

// === Encoder & Mixer Control ===
Encoder myEnc(28, 29);
#define CLK 29
#define DT  28
#define SW  31
#define OPEN_MENU 8
#define OK_BTN 30
#define START_BTN_A 33
#define START_BTN_B 32
#define STOP_BTN_A 4
#define STOP_BTN_B 5
#define BASS_BTN_A 6
#define BASS_BTN_B 27
unsigned long RAW_DURATION_MS = 0;
unsigned long RAW_DURATION2_MS = 0;

#define BASE_BPM           120
float baseBpmA = 128.0f;
float baseBpmB = 128.0f;

File datFile;
bool loadingWaveform = false;
int pointsLoaded = 0;
int totalPointsTarget = 0;
int16_t *activeWaveform = nullptr;   // <-- muudetud float -> int16_t
char activeFilename[64];
#define LOAD_CHUNK_SIZE 500
#define STREAM_READ_INTERVAL 100 // ms between chunk reads
#define WAVEFORM_SCALE 32767.0f
unsigned long startMillisA;
unsigned long startMillisB;
bool resetScrollA = false;
bool resetScrollB = false;
int scrollOffsetA = 0;
int scrollOffsetB = 0;
elapsedMillis drawTimer;
unsigned long lastUpdateMs = 0;
double virtualTimeMs = 0;
double virtualTimeMs2 = 0;

int zoomLevel = 4;
int lastCLK = HIGH;
#define WAVE_WINDOW_SIZE 4000   // Nähtava lainepildi punktid
#define LOAD_MARGIN       1000  // Ettelaadimise buffer (enne ja pärast)
int currentDatOffset;   // mitu rida failis juba loetud
#define MAX_POINTS 100000
DMAMEM int16_t waveform1[MAX_POINTS];
DMAMEM int16_t waveform2[MAX_POINTS];
int totalPoints1 = 0;
int totalPoints2 = 0;

float currentPlaybackRate = 1.0;
float currentPlaybackRate2 = 1.0;

bool startedA = false;
bool startedB = false;

struct DeckLoadState {
File file; // SD fail, kust loeme .dat
int16_t *buffer; // viide bufferile (waveform1 või waveform2)
int pointsLoaded; // mitu sample'it on hetkel mällu loetud
int *totalPoints; // viide üldisele sample loendile
unsigned long lastReadTime; // Time of last read (for throttling)
char filename[100]; // Local filename buffer for safety

// uued voogedastuse väljad
bool streaming; // kas on aktiivne streamimine
unsigned long nextChunkStartMs; // järgmise laadimise algusaja positsioon
unsigned long chunkPreloadMs; // eellaadimise ajavahemik (ms)
};


DeckLoadState deckA;
DeckLoadState deckB;

// --- Ajad ja juhtimine --- //
unsigned long lastStreamUpdate = 0;
float playheadProgressA = 0.0f;
float playheadProgressB = 0.0f;

float smoothedCrossfade = 0.0f;  // global
float getPlaybackRate(int16_t analog) {
  return analog / 612.0;
  
}


void setup() {
  SPI.usingInterrupt(digitalPinToInterrupt(10)); // Audio shield uses pin 14 for I2S
  Serial.begin(9600);
  unsigned long timeout = millis();
  while (!Serial && millis() - timeout < 3000) {}
  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT_PULLUP);
   pinMode(OK_BTN, INPUT_PULLUP);
  pinMode(OPEN_MENU, INPUT_PULLUP);
  pinMode(START_BTN_A, INPUT_PULLUP);
  pinMode(START_BTN_B, INPUT_PULLUP);
   pinMode(STOP_BTN_A, INPUT_PULLUP);
  pinMode(STOP_BTN_B, INPUT_PULLUP);
  pinMode(BASS_BTN_A, INPUT_PULLUP);
  pinMode(BASS_BTN_B, INPUT_PULLUP);
    tft.begin();
  tft.setRotation(3);


  //tft.useFrameBuffer(true);
  //tft.fillScreen(ILI9341_BLACK);
  //tft.updateScreen();  // Push framebuffer to display
  tft.fillScreen(ILI9341_BLACK); 

    digitalWrite(TFT_CS, HIGH);   // vabasta ekraan
digitalWrite(SD_CS, HIGH);    // vabasta kaardi CS, et ta ei segaks
delayMicroseconds(5);

if (!SD.begin(10)) {
  Serial.println("SD-kaardi initsialiseerimine ebaõnnestus!");
} else {
  //Serial.println("SD-kaart leitud.");
}

digitalWrite(TFT_CS, LOW);    // anna SPI tagasi ekraanile



  AudioMemory(60);
  listTracksFromSD();

  drawMenu();
}

void loop() { 



if (inMenu2) {
  tft.setRotation(3);

  if (digitalRead(OPEN_MENU) == LOW) {
    // Menüüst väljumine
    inMenu2 = false;
    resetScrollA = true;
    resetScrollB = true;
    delay(200);

  }
  return;
}
if (inMenu) {
  tft.setRotation(3);
  
  handleMenuEncoder();

  if (digitalRead(OK_BTN) == LOW) {
  // Menüüst väljumine
    inMenu = false;
    Serial.println("sulgesin menu");
    resetScrollA = true;
    resetScrollB = true;
    tft.fillScreen(ILI9341_BLACK);
    delay(100);
    
      lastUpdateMs = millis();
   // Kui Deck A mängib, lae ainult Deck B
    if (playRaw1.isPlaying()) {
      //virtualTimeMs = playRaw1.positionMillis();
      Serial.print("Sync A -> "); Serial.println(virtualTimeMs);
      loadSelectedTrack(false);
      
    }
    // Kui Deck B mängib, lae ainult Deck A
    else if (playRaw2.isPlaying()) {
      loadSelectedTrack(true);
       //virtualTimeMs2 = playRaw2.positionMillis();
      Serial.print("Sync B -> "); Serial.println(virtualTimeMs2);
    }
    // Kui kumbki ei mängi, lae mõlemad
    else {
      loadSelectedTrack(true);
      loadSelectedTrack(false);
    }
  }
  return;
}



    if (!startedA && digitalRead(START_BTN_A) == LOW) {
      baseBpmA = extractBpmFromFilename(trackFiles[selectedTrackA]);
  playRaw1.playRaw(trackFiles[selectedTrackA], 2);
  startMillisA = millis();
  //virtualTimeMs = 0;
  startedA = true;
  delay(10);
}
    if (startedA && digitalRead(STOP_BTN_A) == LOW) {
  playRaw1.stop();
  virtualTimeMs = 0;
  startedA = false;
  delay(10);
}

if (!startedB && digitalRead(START_BTN_B) == LOW) {
  baseBpmB = extractBpmFromFilename(trackFiles[selectedTrackB]);
  playRaw2.playRaw(trackFiles[selectedTrackB], 2);
  startMillisB = millis();
  //virtualTimeMs2 = 0;
  startedB = true;
  delay(10);
}
    if (startedB && digitalRead(STOP_BTN_B) == LOW) {
  playRaw2.stop();
  virtualTimeMs2 = 0;
  startedB = false;
  delay(10);
}

// Check if Deck A finished playing
if (!playRaw1.isPlaying()) {
  startedA = false;
  //virtualTimeMs = 0;
  //inMenu = true;
  //drawMenu();

}

// Check if Deck B finished playing (optional)
if (!playRaw2.isPlaying()) {
  startedB = false;
  //virtualTimeMs2 = 0;
  //inMenu = true;
  //drawMenu();

}
    updateWaveformStream();


  int newsensorValue = map(analogRead(A10), 0, 1023, 560, 660);
  currentPlaybackRate = getPlaybackRate(newsensorValue);
  playRaw1.setPlaybackRate(currentPlaybackRate);

  int newsensorValue2 = map(analogRead(A12), 0, 1023, 560, 660);
  currentPlaybackRate2 = getPlaybackRate(newsensorValue2);
  playRaw2.setPlaybackRate(currentPlaybackRate2);


  unsigned long now2 = millis();
  unsigned long delta = now2 - lastUpdateMs;
  lastUpdateMs = now2;
  float bpm1 = baseBpmA;
  float bpm2 = baseBpmB;
  unsigned long beatIntervalA = 60000.0 / bpm1;
  unsigned long beatIntervalB = 60000.0 / bpm2;
  if (startedA) {
    virtualTimeMs += delta * currentPlaybackRate;
    //Serial.print("delta A -> "); Serial.println(virtualTimeMs);
    if (virtualTimeMs > RAW_DURATION_MS) virtualTimeMs = RAW_DURATION_MS;
    drawScrollingWaveform(waveform1, totalPoints1, virtualTimeMs, beatIntervalA, WAVEFORM_Y1, bpm1, RAW_DURATION_MS);

  }

  if (startedB) {
    virtualTimeMs2 += delta * currentPlaybackRate2;
    if (virtualTimeMs2 > RAW_DURATION2_MS) virtualTimeMs2 = RAW_DURATION2_MS;
    drawScrollingWaveform(waveform2, totalPoints2, virtualTimeMs2, beatIntervalB, WAVEFORM_Y2, bpm2, RAW_DURATION2_MS);

  }

  updateCrossfade();

 int cutoffA = map(analogRead(A0), 0, 1023, 500, 2000);


 int cutoffB = map(analogRead(A13), 0, 1023, 500, 2000);

biquadA_L.setBandpass(0, cutoffA, 0.707); // kanal 0, freq 200Hz, Q=0.707
biquadA_R.setBandpass(0, cutoffA, 0.707); 
biquadB_L.setBandpass(0, cutoffB, 0.707);
biquadB_R.setBandpass(0, cutoffB, 0.707);

    delay(10);


  if (drawTimer > 20) {
    drawTimer = 0;
    handleEncoder();
    //drawDeckLabels(bpm1, bpm2);
    //drawTrackDurations();
    //tft.setTextSize(1);
    //  tft.setCursor(50, 10);
  //tft.print(virtualTimeMs);
  //tft.setCursor(50, 130);
   // tft.print(virtualTimeMs2);
  }
  
  if (digitalRead(OPEN_MENU) == LOW && !inMenu2) {
    delay(50); // debounce
  if (digitalRead(OPEN_MENU) == LOW) {
    inMenu2 = true;
  drawMenu2();
 }
 }

// Open menu and auto-select non-playing deck
if (digitalRead(SW) == LOW && !inMenu) {
  delay(50); // debounce
  if (digitalRead(SW) == LOW) {
    inMenu = true;
    Serial.println("avasin menu ");
    Serial.println(virtualTimeMs);
    selectingDeckA = !startedA; // If Deck A is playing, select Deck B, else Deck A
    drawMenu();
  }
}



}

// --- Failide lugemine SD-kaardilt ---
void listTracksFromSD() {
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  delayMicroseconds(50);

  File root = SD.open("/");
  totalTracks = 0;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      String name = entry.name();
      if (name.endsWith(".raw")) {
        name.toCharArray(trackFiles[totalTracks], 100);
        totalTracks++;
      }
    }
    entry.close();
  }
  digitalWrite(TFT_CS, LOW);

  // Arvuta mitu lehte vaja
  menuTotalPages = (totalTracks + MENU_TRACKS_PER_PAGE - 1) / MENU_TRACKS_PER_PAGE;
  menuCurrentPage = 0;
}
// --- Menüü joonistamine ---
void drawMenu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);

  tft.setCursor(10, 10);
  tft.print("Vali lugu: ");
  tft.println(selectingDeckA ? "Deck A" : "Deck B");

  int startIndex = menuCurrentPage * MENU_TRACKS_PER_PAGE;
  int endIndex = min(startIndex + MENU_TRACKS_PER_PAGE, totalTracks);

  for (int i = startIndex; i < endIndex; i++) {
    if ((selectingDeckA && i == selectedTrackA) || (!selectingDeckA && i == selectedTrackB))
      tft.setTextColor(ILI9341_YELLOW);
    else
      tft.setTextColor(ILI9341_WHITE);

    tft.setCursor(10, 25 + (i - startIndex) * 12);
    tft.println(trackFiles[i]);
  }

  // --- Kuvame lehekülje info ---
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(10, 25 + (MENU_TRACKS_PER_PAGE + 1) * 12);
  tft.print("Leht ");
  tft.print(menuCurrentPage + 1);
  tft.print("/");
  tft.println(menuTotalPages);

  // --- Kuvame nupukäsud ---
  tft.setTextColor(ILI9341_DARKGREY);
  tft.setCursor(180, 25 + (MENU_TRACKS_PER_PAGE + 1) * 12);
  tft.print("<- / -> lehe vahetus");
}

// --- Lehe muutmine (näiteks encoderi või nuppudega) ---
void nextPage() {
  if (menuCurrentPage < menuTotalPages - 1) {
    menuCurrentPage++;
    drawMenu();
  }
}

void prevPage() {
  if (menuCurrentPage > 0) {
    menuCurrentPage--;
    drawMenu();
  }
}
void drawMenu2() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);

  tft.setCursor(10, 10);
  tft.print("TEST ");
}
void handleMenuEncoder() {
  int clkState = digitalRead(CLK);
  if (clkState != lastCLK) {
    if (digitalRead(DT) != clkState) {
      if (selectingDeckA && !startedA) {
        selectedTrackA++;
        if (selectedTrackA >= totalTracks) selectedTrackA = 0;
      } else if (!selectingDeckA && !startedB) {
        selectedTrackB++;
        if (selectedTrackB >= totalTracks) selectedTrackB = 0;
      }
    } else {
      if (selectingDeckA && !startedA) {
        selectedTrackA--;
        if (selectedTrackA < 0) selectedTrackA = totalTracks - 1;
      } else if (!selectingDeckA && !startedB) {
        selectedTrackB--;
        if (selectedTrackB < 0) selectedTrackB = totalTracks - 1;
      }
    }
    drawMenu();
    //delay(80);
  }

  if (digitalRead(OK_BTN) == LOW) {
    if ((selectingDeckA && !startedB) || (!selectingDeckA && !startedA)) {
      selectingDeckA = !selectingDeckA;
      drawMenu();
    }
    delay(150);
  }

  lastCLK = clkState;
}

// Helper function to format time as MM:SS
String formatDuration(unsigned long ms) {
  int totalSeconds = ms / 1000;
  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, seconds);
  return String(buffer);
}

// Dynamically show elapsed + duration for each deck
void drawTrackDurations() {
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK); // foreground and background
  tft.setTextSize(1);

  // Deck A elapsed + duration
  unsigned long elapsedA = millis() - startMillisA;
  if (!startedA) elapsedA = 0;
  String timeA = formatDuration(elapsedA) + "/" + formatDuration(RAW_DURATION_MS);
  tft.setCursor(240 - timeA.length() * 6 - 5, 5);
  tft.print(timeA);

  // Deck B elapsed + duration
  unsigned long elapsedB = millis() - startMillisB;
  if (!startedB) elapsedB = 0;
  String timeB = formatDuration(elapsedB) + "/" + formatDuration(RAW_DURATION2_MS);
  tft.setCursor(240 - timeB.length() * 6 - 5, 120);
  tft.print(timeB);
}


// --- Voogedastuslaadimise versioon ---

// Updated loadSelectedTrack() for streaming waveform load

// Updated loadSelectedTrack() function
void loadSelectedTrack(bool loadingDeckA) {
if (loadingDeckA) {
char datNameA[100];
strcpy(datNameA, trackFiles[selectedTrackA]);
char *dotA = strchr(datNameA, '.');
if (dotA) strcpy(dotA, ".dat");
Serial.println(datNameA);


if (!startWaveformStream('A', datNameA, waveform1, totalPoints1)) {
Serial.println("Deck A load fail!");
} else {
Serial.println("Deck A streaming started...");
}


delay(50);
RAW_DURATION_MS = calculateRawDurationMs(trackFiles[selectedTrackA]);


} else {
char datNameB[100];
strcpy(datNameB, trackFiles[selectedTrackB]);
char *dotB = strchr(datNameB, '.');
if (dotB) strcpy(dotB, ".dat");
Serial.println(datNameB);


if (!startWaveformStream('B', datNameB, waveform2, totalPoints2)) {
Serial.println("Deck B load fail!");
} else {
Serial.println("Deck B streaming started...");
}


delay(50);
RAW_DURATION2_MS = calculateRawDurationMs(trackFiles[selectedTrackB]);
}


playRaw1.enableInterpolation(true);
playRaw2.enableInterpolation(true);
}


// Called periodically from loop() to continue loading waveform data
void updateWaveformStream() {
unsigned long now = millis();


DeckLoadState *decks[2] = { &deckA, &deckB };
char deckNames[2] = { 'A', 'B' };


for (int i = 0; i < 2; i++) {
DeckLoadState *d = decks[i];
if (!d->streaming) continue;
if (now - d->lastReadTime < STREAM_READ_INTERVAL) continue;


d->lastReadTime = now;


char line[32];
int linesRead = 0;


while (linesRead < LOAD_CHUNK_SIZE && d->file.available()) {
int len = d->file.readBytesUntil('\n', line, sizeof(line) - 1);
if (len <= 0) break;
line[len] = '\0';
d->buffer[d->pointsLoaded++] = (int16_t)(atof(line) * 32767.0f);
if (++linesRead >= 100) {
    yield(); // lase audio katkestustel töötada
    linesRead = 0;
}
}


*d->totalPoints = d->pointsLoaded;


Serial.printf("Deck %c progress: %d points\n", deckNames[i], d->pointsLoaded);


if (!d->file.available()) {
Serial.printf("✅ Deck %c finished streaming %d points from %s\n", deckNames[i], d->pointsLoaded, d->filename);
d->file.close();
d->streaming = false;
}
}
}


// --- Peafunktsioon streami uuendamiseks loop() sees ---

void continueWaveformStream(char deck, unsigned long currentPlayheadMs) {
  DeckLoadState *deckState = (deck == 'A') ? &deckA : &deckB;
  if (!deckState->streaming) return;

  // Kontrolli, kas jõudsime bufferi lõppu
  if (currentPlayheadMs > deckState->nextChunkStartMs - deckState->chunkPreloadMs) {
    if (readNextWaveformChunk(deck)) {
      Serial.printf("Deck %c streamed new chunk (%lu ms)\n", deck, currentPlayheadMs);
    }
  }
}

// --- Streami käivitamine ---

// Function to start streaming load for Deck A or Deck B
bool startWaveformStream(char deck, const char *filename, int16_t *buffer, int &totalPoints) {
DeckLoadState *deckState = (deck == 'A') ? &deckA : &deckB;


// Close any previous file
if (deckState->file) deckState->file.close();


strncpy(deckState->filename, filename, sizeof(deckState->filename) - 1);
deckState->filename[sizeof(deckState->filename) - 1] = '\0';


deckState->file = SD.open(deckState->filename, FILE_READ);
if (!deckState->file) {
Serial.printf("❌ Failed to open %s for Deck %c\n", deckState->filename, deck);
return false;
}


deckState->buffer = buffer;
deckState->totalPoints = &totalPoints;
deckState->pointsLoaded = 0;
deckState->streaming = true;
deckState->lastReadTime = millis();


Serial.printf("📀 Started streaming load: %s (Deck %c)\n", deckState->filename, deck);
return true;
}

// --- Chunkide lugemine ---

bool readNextWaveformChunk(char deck) {
  DeckLoadState *deckState = (deck == 'A') ? &deckA : &deckB;
  if (!deckState->file) return false;

  char line[32];
  int readCount = 0;
  while (deckState->file.available() && readCount < LOAD_CHUNK_SIZE) {
    int len = deckState->file.readBytesUntil('\n', line, sizeof(line) - 1);
    if (len > 0) {
      line[len] = '\0';
      int16_t sample = (int16_t)(atof(line) * 32767.0f);
      deckState->buffer[deckState->pointsLoaded++] = sample;
      readCount++;
    }
  }

  if (readCount > 0)
    Serial.printf("Deck %c streamed %d samples, total %d\n", deck, readCount, deckState->pointsLoaded);

  return readCount > 0;
}



void handleEncoder() {
  int clkState = digitalRead(CLK);
  if (clkState != lastCLK) {
    if (digitalRead(DT) != clkState) {
      zoomLevel++;
      if (zoomLevel > 6) zoomLevel = 6;
    } else {
      zoomLevel--;
      if (zoomLevel < 1) zoomLevel = 1;
    }
  }
  lastCLK = clkState;
}




// Kui waveform on int16_t (sisalduvad -32767..+32767), teisendame enne joonistamist float-ks
void drawScrollingWaveform(const int16_t *waveform, int totalPoints, double now, unsigned long beatInterval, int yOffset, float bpm, unsigned long duration) {
  // Sama turvakontroll nagu float-versioonis
  if (duration == 0 || totalPoints <= 0) {
    tft.fillRect(0, yOffset, SCREEN_WIDTH, WAVEFORM_H, ILI9341_BLACK);
    return;
  }

  int viewWidth = FIXED_VIEW_WIDTH / zoomLevel;
  if (viewWidth < 1) viewWidth = 1;

  float posFrac = (float)now / (float)duration;
  if (posFrac < 0) posFrac = 0;
  if (posFrac > 1) posFrac = 1;
  int idealPlayheadIdx = (int)(posFrac * (float)totalPoints);

  int startIdx = idealPlayheadIdx - viewWidth / 2;
  if (startIdx < 0) startIdx = 0;
  if (startIdx + viewWidth > totalPoints) {
    viewWidth = totalPoints - startIdx;
    if (viewWidth < 1) viewWidth = 1;
  }

  int playheadIdx = startIdx + viewWidth / 2;
  if (playheadIdx < 0) playheadIdx = 0;
  if (playheadIdx >= totalPoints) playheadIdx = totalPoints - 1;

  unsigned long playheadMs = (unsigned long)((float)playheadIdx / (float)totalPoints * (float)duration);
  unsigned long firstBeatMs = 0;
  if (beatInterval > 0) firstBeatMs = playheadMs - (playheadMs % beatInterval);

  // Puhasta ala
  tft.fillRect(0, yOffset, SCREEN_WIDTH, WAVEFORM_H, ILI9341_BLACK);

  // Joonista beat-jooned
  if (beatInterval > 0) {
    for (int i = -100; i <= 100; i++) {
      long long beatTime = (long long)firstBeatMs + (long long)i * (long long)beatInterval;
      if (beatTime < 0 || beatTime > (long long)duration) continue;
      float beatIdxF = ((float)beatTime * (float)totalPoints) / (float)duration;
      int x = (int)((beatIdxF - startIdx) * (long)SCREEN_WIDTH / (long)viewWidth);
      if (x >= 0 && x < SCREEN_WIDTH) {
        tft.drawFastVLine(x, yOffset, WAVEFORM_H, ILI9341_GREEN);
      }
    }
  }

  // Joonista waveform (konverteerime int16 -> float)
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    int idx = startIdx + (x * viewWidth / SCREEN_WIDTH);
    if (idx >= 0 && idx < totalPoints) {
      // konvert: -32767..32767 -> -1.0..+1.0
      float peak = (float)waveform[idx] / 32767.0f;
      int yCenter = yOffset + WAVEFORM_H / 2;
      int height = (int)(peak * (WAVEFORM_H / 2));
      if (height < 0) height = 0;
      if (height > WAVEFORM_H/2) height = WAVEFORM_H/2;
      tft.drawFastVLine(x, yCenter - height, height * 2, TFT_DARKGREY);
    }
  }

  // Playhead viimasena
  tft.drawFastVLine(PLAYHEAD_X, yOffset, WAVEFORM_H, ILI9341_RED);
}


void drawDeckLabels(float bpm1, float bpm2) {
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print("Deck A - BPM: "); tft.print(bpm1, 1);
  tft.setCursor(10, 130);
  tft.print("Deck B - BPM: "); tft.print(bpm2, 1);
}

unsigned long calculateRawDurationMs(const char* filename) {
  digitalWrite(TFT_CS, HIGH);  // vabasta ekraan
  digitalWrite(SD_CS, LOW);    // aktiveeri SD
  delayMicroseconds(5);
  File f = SD.open(filename);
  if (!f) {
    Serial.print("Failed to open file for duration: ");
    Serial.println(filename);
    digitalWrite(SD_CS, HIGH);  // vabasta SD
    return 0;
  }
  unsigned long fileSize = f.size();
  f.close();
  digitalWrite(SD_CS, HIGH);    // vabasta SD
  digitalWrite(TFT_CS, LOW);    // aktiveeri ekraan

  unsigned long numSamples = fileSize / 4;
  float durationSec = (float)numSamples / 44100.0;
  unsigned long durationMs = durationSec * 1000;

  return durationMs;
}


// Function to prompt user to choose Deck A or Deck B
int deckPrompt(bool deckAPlaying) {
  int currentDeck = deckAPlaying ? 1 : 0;
  long lastEnc = myEnc.read();

  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(10, 100);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  if (deckAPlaying) {
    tft.print("Deck A is playing");
    tft.setCursor(10, 120);
    tft.print("Only Deck B allowed");
  } else {
    tft.print("Select Deck (Encoder):");
  }

  while (true) {
    long encVal = myEnc.read() / 4;
    if (encVal != lastEnc) {
      lastEnc = encVal;
      if (!deckAPlaying) {
        currentDeck = (encVal % 2 + 2) % 2;
      } else {
        currentDeck = 1;
      }

      tft.fillRect(10, 140, 220, 30, ILI9341_BLACK);
      tft.setCursor(10, 140);
      tft.setTextColor((currentDeck == 0 && !deckAPlaying) ? ILI9341_GREEN : ILI9341_WHITE);
      tft.print("Deck A");

      tft.setCursor(100, 140);
      tft.setTextColor(currentDeck == 1 ? ILI9341_GREEN : ILI9341_WHITE);
      tft.print("Deck B");
    }

    if (digitalRead(SW) == LOW) {
      while (digitalRead(SW) == LOW);  // wait for release
      delay(100); // debounce
      return currentDeck;
    }
  }
}
void updateCrossfade() {
  int raw = analogRead(A11);
  raw = constrain(raw, 0, 1023);
  float x = (float)raw / 1023.0f;

  // 🔧 Madalpääsfilter
  smoothedCrossfade = smoothedCrossfade * 0.9f + x * 0.1f;

  float gainA = sqrtf(1.0f - smoothedCrossfade);
  float gainB = sqrtf(smoothedCrossfade);

  // Väike lävi täielikuks vaigistuseks
  if (gainA < 0.12f) gainA = 0.0f;
  if (gainB < 0.12f) gainB = 0.0f;

  mixer1.gain(0, gainA);
  mixer1.gain(1, gainB);
  mixer2.gain(0, gainA);
  mixer2.gain(1, gainB);

  //Serial.print("Crossfade raw: ");
  //Serial.print(raw);
  //Serial.print("  smooth: ");
  //Serial.print(smoothedCrossfade, 2);
  //Serial.print("  gainA: ");
  //Serial.println(gainA, 2);
}
float extractBpmFromFilename(const char *filename) {
  int bpm = 128; // vaikeväärtus
  for (int i = 0; filename[i] != '\0'; i++) {
    if (isdigit(filename[i]) && isdigit(filename[i+1]) && isdigit(filename[i+2]) &&
        filename[i+3] == '_') {
      char num[4] = { filename[i], filename[i+1], filename[i+2], '\0' };
      bpm = atoi(num);
      break;
    }
  }
  return bpm;
}
