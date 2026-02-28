#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include <ILI9341_t3n.h>


class UIManager {
public:
    UIManager(ILI9341_t3n &display);

    void setTracks(char files[][128], int count);
    void begin();

    void nextPage();
    void prevPage();

    void updateSelection(int direction);

    void setDeck(bool isA);
    bool isDeckA() const;

    int getSelectedA() const;
    int getSelectedB() const;

private:
    ILI9341_t3n &tft;

    char (*trackFiles)[128];
    int totalTracks = 0;

    int menuCurrentPage = 0;
    int menuTotalPages = 1;
    const int MENU_TRACKS_PER_PAGE = 8;

    int selectedA = 0;
    int selectedB = 0;

    bool selectingA = true;
};

#endif
