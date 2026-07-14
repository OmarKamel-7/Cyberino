//////////////////////////////////////////////////////////////
// CyberDeck Pro
// main.ino - Main Menu and Core Functions
//////////////////////////////////////////////////////////////

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

//==========================================================
// Button Definitions
//==========================================================

#define BTN_UP      12
#define BTN_DOWN    22
#define BTN_SELECT  13
#define BTN_BACK    14

//==========================================================
// Button Debouncing Structure
//==========================================================

struct Button {
    uint8_t pin;
    bool state;
    bool lastState;
    unsigned long lastDebounceTime;
    bool pressed;
    bool held;
    unsigned long pressTime;
};

Button btnUp = {BTN_UP, HIGH, HIGH, 0, false, false, 0};
Button btnDown = {BTN_DOWN, HIGH, HIGH, 0, false, false, 0};
Button btnSelect = {BTN_SELECT, HIGH, HIGH, 0, false, false, 0};
Button btnBack = {BTN_BACK, HIGH, HIGH, 0, false, false, 0};

#define DEBOUNCE_DELAY 30
#define HOLD_DELAY 500

//==========================================================
// Button Functions
//==========================================================

void updateButton(Button &btn) {
    bool reading = digitalRead(btn.pin);
    
    if (reading != btn.lastState) {
        btn.lastDebounceTime = millis();
    }
    
    if ((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != btn.state) {
            btn.state = reading;
            
            if (btn.state == LOW) {
                btn.pressed = true;
                btn.pressTime = millis();
                btn.held = false;
            } else {
                btn.pressed = false;
                btn.held = false;
            }
        }
    }
    
    if (btn.state == LOW && !btn.held && (millis() - btn.pressTime) > HOLD_DELAY) {
        btn.held = true;
    }
    
    btn.lastState = reading;
}

bool isPressed(Button &btn) {
    if (btn.pressed) {
        btn.pressed = false;
        return true;
    }
    return false;
}

bool isHeld(Button &btn) {
    return btn.held;
}

//==========================================================
// Screen States
//==========================================================

enum ScreenState {
    SCREEN_MAIN,
    SCREEN_WIFI,
    SCREEN_NRF,
    SCREEN_BLE,
    SCREEN_NFC
};

ScreenState currentScreen = SCREEN_MAIN;

//==========================================================
// Main Menu
//==========================================================

const char *mainMenu[] = {
    "2.4 GHz Analyzer",
    "BLE ",
    "WiFi attacks",
    "NFC Tools",
    "About"
};

const int mainMenuLength = 5;
const int visibleItems = 5;

int selectedItem = 0;

//==========================================================
// Drawing Functions
//==========================================================

void drawHeader(const char *title) {
    tft.fillRect(0, 0, 480, 40, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.drawCentreString(title, 240, 10, 4);
}

void drawFooter() {
    tft.drawFastHLine(0, 292, 480, TFT_DARKGREY);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("UP/DOWN SELECT BACK", 240, 300, 2);
}

void drawMainMenu() {
    tft.fillScreen(TFT_BLACK);
    drawHeader("Cyberino");
    
    int y = 60;
    for(int i = 0; i < mainMenuLength; i++) {
        if(i == selectedItem) {
            tft.fillRoundRect(40, y - 4, 400, 38, 8, TFT_BLUE);
            tft.setTextColor(TFT_WHITE, TFT_BLUE);
            tft.drawString(">", 55, y, 4);
            tft.drawString(mainMenu[i], 80, y, 4);
        } else {
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawString(mainMenu[i], 80, y, 4);
        }
        y += 48;
    }
    
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("Heap: " + String(ESP.getFreeHeap()/1024) + "KB", 10, 282, 1);
    tft.drawRightString("v2.0", 470, 282, 1);
    
    drawFooter();
}

//==========================================================
// About Screen
//==========================================================

void showAbout() {
    tft.fillScreen(TFT_BLACK);
    drawHeader("About");
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawCentreString("CyberDeck Pro", 240, 80, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("Multi-Tool Platform", 240, 130, 2);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("ESP32 + TFT + Multiple Modules", 240, 170, 2);
    tft.drawCentreString("Version 2.0", 240, 200, 2);
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString("Modules: NRF24 x2, BLE, WiFi, NFC", 240, 240, 2);
    
    drawFooter();
    
    while(true) {
        updateButton(btnBack);
        if(isPressed(btnBack)) {
            drawMainMenu();
            return;
        }
        delay(10);
    }
}

//==========================================================
// Forward Declarations for Module Functions
//==========================================================

void initWiFiMenu();
void handleWiFiMenu();
void initNRFMenu();
void handleNRFMenu();
void initBLEMenu();
void handleBLEMenu();
void initNFCMenu();
void handleNFCMenu();

//==========================================================
// Setup
//==========================================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n╔══════════════════════════════════╗");
    Serial.println("║     CYBERDECK PRO BOOTING      ║");
    Serial.println("╚══════════════════════════════════╝");
    
    // Initialize buttons
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
    pinMode(BTN_BACK, INPUT_PULLUP);
    
    // Initialize SPI
    SPI.begin();
    
    // Initialize display
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    // Boot splash
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawCentreString("CYBERDECK", 240, 80, 6);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("Initializing System...", 240, 150, 2);
    
    delay(500);
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString("Ready!", 240, 200, 2);
    delay(500);
    
    drawMainMenu();
    
    Serial.println("✓ System initialized successfully!");
    Serial.println("✓ Display ready");
    Serial.println("✓ Buttons configured");
    Serial.println("✓ Main menu loaded");
    Serial.println("═══════════════════════════════════");
}

//==========================================================
// Main Loop
//==========================================================

void loop() {
    updateButton(btnUp);
    updateButton(btnDown);
    updateButton(btnSelect);
    updateButton(btnBack);
    
    switch(currentScreen) {
        case SCREEN_MAIN:
            handleMainMenu();
            break;
            
        case SCREEN_WIFI:
            handleWiFiMenu();
            break;
            
        case SCREEN_NRF:
            handleNRFMenu();
            break;
            
        case SCREEN_BLE:
            handleBLEMenu();
            break;
            
        case SCREEN_NFC:
            handleNFCMenu();
            break;
    }
    delay(10);
}

//==========================================================
// Main Menu Handler
//==========================================================

void handleMainMenu() {
    if(isPressed(btnUp)) {
        selectedItem--;
        if(selectedItem < 0) selectedItem = mainMenuLength - 1;
        drawMainMenu();
    }
    else if(isPressed(btnDown)) {
        selectedItem++;
        if(selectedItem >= mainMenuLength) selectedItem = 0;
        drawMainMenu();
    }
    else if(isPressed(btnSelect)) {
        switch(selectedItem) {
            case 0:
                currentScreen = SCREEN_NRF;
                initNRFMenu();
                break;
            case 1:
                currentScreen = SCREEN_BLE;
                initBLEMenu();
                break;
            case 2:
                currentScreen = SCREEN_WIFI;
                initWiFiMenu();
                break;
            case 3:
                currentScreen = SCREEN_NFC;
                initNFCMenu();
                break;
            case 4:
                showAbout();
                break;
        }
    }
}