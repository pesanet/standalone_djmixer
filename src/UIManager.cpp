#include "UIManager.h"

UIManager::UIManager(ILI9341_t3n &display) : tft(display) {}

void UIManager::setTracks(char files[][128], int count) {
    trackFiles = files;
    totalTracks = count;

    menuTotalPages = (totalTracks + MENU_TRACKS_PER_PAGE - 1) / MENU_TRACKS_PER_PAGE;
    menuCurrentPage = 0;
}

void UIManager::setDeck(bool isA) {
    selectingA = isA;
}

bool UIManager::isDeckA() const {
    return selectingA;
}

int UIManager::getSelectedA() const {
    return selectedA;
}

int UIManager::getSelectedB() const {
    return selectedB;
}

void UIManager::begin() {
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(0xFD20); // oranž
    tft.setTextSize(1);

    tft.setCursor(10, 10);
    tft.print("Vali lugu: ");
    tft.println(selectingA ? "Deck A" : "Deck B");

    int startIndex = menuCurrentPage * MENU_TRACKS_PER_PAGE;
    int endIndex = min(startIndex + MENU_TRACKS_PER_PAGE, totalTracks);

    for (int i = startIndex; i < endIndex; i++) {

        if ((selectingA && i == selectedA) ||
            (!selectingA && i == selectedB))
            tft.setTextColor(ILI9341_YELLOW);
        else
            tft.setTextColor(ILI9341_WHITE);

        tft.setCursor(10, 25 + (i - startIndex) * 12);
        tft.println(trackFiles[i]);
    }

    tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(10, 25 + (MENU_TRACKS_PER_PAGE + 1) * 12);
    tft.print("Leht ");
    tft.print(menuCurrentPage + 1);
    tft.print("/");
    tft.println(menuTotalPages);
}

void UIManager::nextPage() {
    if (menuCurrentPage < menuTotalPages - 1) {
        menuCurrentPage++;
        begin();
    }
}

void UIManager::prevPage() {
    if (menuCurrentPage > 0) {
        menuCurrentPage--;
        begin();
    }
}

void UIManager::updateSelection(int direction) {
    if (totalTracks == 0) return;

    int &sel = selectingA ? selectedA : selectedB;

    sel += direction;

    if (sel < 0) sel = totalTracks - 1;
    if (sel >= totalTracks) sel = 0;

    int newPage = sel / MENU_TRACKS_PER_PAGE;
    if (newPage != menuCurrentPage) {
        menuCurrentPage = newPage;
        begin();
    } else {
        begin();
    }
}
