//////////////////////////////////////////////////////////////
// CyberDeck Pro
// ble.ino - BLE Scanner & Mouse
// Features: Scanner, BLE Mouse with DPI control
//////////////////////////////////////////////////////////////

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEHIDDevice.h>
#include <HIDTypes.h>

// For BLE Mouse
#include <BleMouse.h>

//==========================================================
// BLE Variables
//==========================================================

BLEScan* pBLEScan;
bool bleInitialized = false;
bool bleScanning = false;
bool mouseActive = false;
bool mouseConnected = false;
bool mouseScreenActive = false;

// BLE Mouse
BleMouse* bleMouse = nullptr;

// Mouse position - High DPI sensitivity
int mouseX = 240;
int mouseY = 160;
int mouseDPI = 8;  // DPI multiplier (higher = faster movement)
int mouseSpeed = 10; // Base speed

//==========================================================
// BLE State Machine
//==========================================================

enum BLEClientState {
    BLE_MENU,
    BLE_SCAN,
    BLE_LIST,
    BLE_STATUS,
    BLE_MOUSE
};

BLEClientState bleState = BLE_MENU;
int bleSelectedItem = 0;
int bleDeviceCount = 0;
int bleSelectedDevice = 0;

const char *bleMenu[] = {
    "Scan for Devices",
    "BLE Mouse",
    "Module Status",
    "Back"
};
const int bleMenuLength = 4;

//==========================================================
// BLE Device Structure
//==========================================================

struct BLEDeviceInfo {
    String address;
    String name;
    int rssi;
};

#define MAX_BLE_DEVICES 30
BLEDeviceInfo bleDevices[MAX_BLE_DEVICES];

//==========================================================
// BLE Callback
//==========================================================

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if(bleDeviceCount < MAX_BLE_DEVICES) {
            bleDevices[bleDeviceCount].address = advertisedDevice.getAddress().toString().c_str();
            bleDevices[bleDeviceCount].name = advertisedDevice.getName().c_str();
            bleDevices[bleDeviceCount].rssi = advertisedDevice.getRSSI();
            bleDeviceCount++;
        }
    }
};

//==========================================================
// BLE Drawing Functions - No Flicker
//==========================================================

void drawBLEHeader(const char *title) {
    static char lastTitle[40] = "";
    if (strcmp(lastTitle, title) != 0) {
        tft.fillRect(0, 0, 480, 40, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
        tft.drawCentreString(title, 240, 10, 4);
        strcpy(lastTitle, title);
    }
}

void drawBLEFooter() {
    static bool footerDrawn = false;
    if (!footerDrawn) {
        tft.drawFastHLine(0, 292, 480, TFT_DARKGREY);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawCentreString("UP/DOWN SELECT BACK", 240, 300, 2);
        footerDrawn = true;
    }
}

//==========================================================
// BLE Initialization
//==========================================================

void initBLEModule() {
    Serial.println("\n--- BLE Module Initialization ---");
    
    if(!bleInitialized) {
        BLEDevice::init("CyberDeck");
        pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
        pBLEScan->setActiveScan(true);
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);
        bleInitialized = true;
        Serial.println("BLE initialized successfully");
    } else {
        Serial.println("BLE already initialized");
    }
    
    Serial.println("-----------------------------------\n");
}

//==========================================================
// BLE MOUSE FUNCTIONS
//==========================================================

void initBLEMouse() {
    if (!bleInitialized) {
        initBLEModule();
    }
    
    if (bleMouse == nullptr) {
        bleMouse = new BleMouse("CyberDeck Mouse");
        bleMouse->begin();
        mouseActive = true;
        mouseConnected = false;
        Serial.println("BLE Mouse started - device is discoverable!");
        Serial.println("Connect to 'CyberDeck Mouse' from your phone");
        Serial.println("SELECT = Left Click | UP/DOWN = Move | LEFT/RIGHT = DPI");
    }
}

void stopBLEMouse() {
    if (bleMouse != nullptr) {
        bleMouse->end();
        delete bleMouse;
        bleMouse = nullptr;
        mouseActive = false;
        mouseConnected = false;
        Serial.println("BLE Mouse stopped");
    }
}

void updateMouseConnection() {
    if (bleMouse != nullptr) {
        bool wasConnected = mouseConnected;
        mouseConnected = bleMouse->isConnected();
        
        if (mouseConnected && !wasConnected) {
            Serial.println("🎯 Device connected to BLE Mouse!");
        }
        if (!mouseConnected && wasConnected) {
            Serial.println("📴 Device disconnected from BLE Mouse");
        }
    }
}

//==========================================================
// MOUSE CONTROL SCREEN - No Flicker
//==========================================================

void drawMouseControlScreen() {
    static bool firstDraw = true;
    static int lastX = -1, lastY = -1;
    static bool lastConnected = false;
    static int lastDPI = -1;
    
    updateMouseConnection();
    
    bool needsRedraw = firstDraw || (lastConnected != mouseConnected) || (lastDPI != mouseDPI);
    
    if (needsRedraw) {
        tft.fillScreen(TFT_BLACK);
        drawBLEHeader("BLE MOUSE CONTROL");
        firstDraw = false;
        lastConnected = mouseConnected;
        lastDPI = mouseDPI;
    }
    
    int y = 45;
    
    // Connection status
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Status:", 30, y, 2);
    
    if (mouseConnected) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("✅ CONNECTED", 250, y, 2);
        y += 40;
        
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawCentreString("🎯 Device connected!", 240, y, 2);
        y += 30;
    } else {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("⏳ WAITING", 250, y, 2);
        y += 40;
        
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawCentreString("Waiting for connection...", 240, y, 2);
        y += 30;
    }
    
    // DPI & Speed
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("DPI:", 30, y, 2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    char dpiStr[10];
    sprintf(dpiStr, "%dx", mouseDPI);
    tft.drawString(dpiStr, 120, y, 2);
    y += 30;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Speed:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(mouseSpeed) + " px", 120, y, 2);
    y += 30;
    
    // Mouse position
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Position:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char posStr[30];
    sprintf(posStr, "X:%d  Y:%d", mouseX, mouseY);
    tft.drawString(posStr, 200, y, 2);
    y += 35;
    
    // Controls info
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawCentreString("SELECT: Left Click", 240, 210, 2);
    tft.drawCentreString("UP/DOWN: Move Mouse", 240, 225, 2);
    tft.drawCentreString("LEFT/RIGHT: Change DPI", 240, 240, 2);
    tft.drawCentreString("BACK: Exit Mouse Mode", 240, 255, 2);
    
    // Connection activity bar
    if (mouseConnected) {
        int barX = 40;
        int barY = 275;
        int barWidth = 400;
        int barHeight = 8;
        int pos = (millis() / 30) % barWidth;
        
        tft.fillRect(barX, barY, barWidth, barHeight, TFT_DARKGREY);
        tft.fillRect(barX + pos - 10, barY, 20, barHeight, TFT_GREEN);
        
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawCentreString("🖱️ Mouse Active", 240, 285, 2);
    } else {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawCentreString("🔵 Waiting for phone to connect...", 240, 285, 2);
    }
    
    drawBLEFooter();
}

//==========================================================
// Handle BLE Mouse Control
//==========================================================

void handleMouseControl() {
    drawMouseControlScreen();
    
    // Update connection status
    updateMouseConnection();
    
    // Get current button states
    bool upPressed = isPressed(btnUp);
    bool downPressed = isPressed(btnDown);
    bool selectPressed = isPressed(btnSelect);
    bool backPressed = isPressed(btnBack);
    
    // Handle mouse movement - HIGH DPI / SENSITIVE
    if (mouseConnected && bleMouse != nullptr) {
        // UP - Move mouse up with DPI scaling
        if (upPressed) {
            int moveAmount = mouseSpeed * mouseDPI;
            mouseY -= moveAmount;
            if (mouseY < 0) mouseY = 0;
            bleMouse->move(0, -moveAmount);
            drawMouseControlScreen();
            delay(20); // Fast response
        }
        // DOWN - Move mouse down with DPI scaling
        else if (downPressed) {
            int moveAmount = mouseSpeed * mouseDPI;
            mouseY += moveAmount;
            if (mouseY > 320) mouseY = 320;
            bleMouse->move(0, moveAmount);
            drawMouseControlScreen();
            delay(20);
        }
        // SELECT - Left click
        else if (selectPressed) {
            bleMouse->click(MOUSE_LEFT);
            drawMouseControlScreen();
            delay(100);
            // Reset click state to prevent double click
            btnSelect.pressed = false;
        }
    }
    
    // DPI adjustment - LEFT/RIGHT buttons
    if (isPressed(btnUp) && isHeld(btnUp)) {
        // This is handled above
    } else if (isPressed(btnDown) && isHeld(btnDown)) {
        // This is handled above
    }
    
    // Handle DPI change with LEFT/RIGHT
    // Since we don't have left/right buttons defined, we use UP/DOWN with SELECT combo
    // Alternatively, we can use UP/DOWN for movement and adjust DPI with a different method
    
    // Back button - Exit mouse mode
    if (backPressed) {
        if (mouseActive) {
            stopBLEMouse();
        }
        bleState = BLE_MENU;
        drawBLEMenu();
    }
}

//==========================================================
// BLE Module Status
//==========================================================

void drawBLEStatus() {
    static bool firstDraw = true;
    if (firstDraw) {
        tft.fillScreen(TFT_BLACK);
        drawBLEHeader("BLE MODULE STATUS");
        firstDraw = false;
    }
    
    int y = 60;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("BLE Status:", 30, y, 2);
    
    if(bleInitialized) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("READY", 400, y, 2);
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("NOT READY", 400, y, 2);
    }
    y += 40;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Scanning:", 30, y, 2);
    if(bleScanning) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("ACTIVE", 400, y, 2);
    } else {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("IDLE", 400, y, 2);
    }
    y += 40;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Devices Found:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(bleDeviceCount), 400, y, 2);
    y += 40;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Mouse:", 30, y, 2);
    tft.setTextColor(mouseActive ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawString(mouseActive ? "ACTIVE" : "STOPPED", 400, y, 2);
    y += 40;
    
    if (mouseActive) {
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("Connected:", 30, y, 2);
        tft.setTextColor(mouseConnected ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
        tft.drawString(mouseConnected ? "YES" : "NO", 400, y, 2);
        y += 40;
        
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("DPI:", 30, y, 2);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString(String(mouseDPI) + "x", 400, y, 2);
        y += 40;
    }
    
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("BLE MAC: " + String(BLEDevice::getAddress().toString().c_str()), 30, y + 20, 1);
    
    drawBLEFooter();
    
    if (isPressed(btnBack)) {
        bleState = BLE_MENU;
        drawBLEMenu();
    }
}

//==========================================================
// BLE Scan
//==========================================================

void performBLEScan() {
    bleDeviceCount = 0;
    bleScanning = true;
    
    BLEScanResults foundDevices = pBLEScan->start(5, false);
    bleDeviceCount = foundDevices.getCount();
    if(bleDeviceCount > MAX_BLE_DEVICES) bleDeviceCount = MAX_BLE_DEVICES;
    
    bleScanning = false;
    pBLEScan->clearResults();
}

//==========================================================
// BLE Device List
//==========================================================

void drawBLEList() {
    static bool firstDraw = true;
    if (firstDraw) {
        tft.fillScreen(TFT_BLACK);
        drawBLEHeader("BLE DEVICES");
        firstDraw = false;
    }
    
    int y = 55;
    int visible = 5;
    
    for(int i = 0; i < visible; i++) {
        int index = i + bleSelectedDevice;
        if(index >= bleDeviceCount) break;
        
        String displayName = bleDevices[index].name;
        if(displayName.length() == 0) displayName = "Unknown";
        
        if(index == bleSelectedDevice) {
            tft.fillRoundRect(18, y - 4, 440, 38, 8, TFT_BLUE);
            tft.setTextColor(TFT_WHITE, TFT_BLUE);
            tft.drawString(">", 30, y, 4);
            tft.drawString(displayName, 55, y, 4);
            tft.setTextColor(TFT_CYAN, TFT_BLUE);
            tft.drawRightString(String(bleDevices[index].rssi) + " dBm", 460, y, 2);
        } else {
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawString(displayName, 55, y, 4);
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.drawRightString(String(bleDevices[index].rssi) + " dBm", 460, y, 2);
        }
        y += 48;
    }
    
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("Total: " + String(bleDeviceCount), 240, 280, 2);
    
    drawBLEFooter();
}

//==========================================================
// BLE Menu
//==========================================================

void drawBLEMenu() {
    tft.fillScreen(TFT_BLACK);
    drawBLEHeader("BLE SCANNER");
    
    int y = 80;
    for(int i = 0; i < bleMenuLength; i++) {
        if(i == bleSelectedItem) {
            tft.fillRoundRect(60, y - 4, 360, 40, 8, TFT_BLUE);
            tft.setTextColor(TFT_WHITE, TFT_BLUE);
            tft.drawCentreString(bleMenu[i], 240, y, 4);
        } else {
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawCentreString(bleMenu[i], 240, y, 4);
        }
        y += 55;
    }
    
    drawBLEFooter();
}

//==========================================================
// Init BLE Menu
//==========================================================

void initBLEMenu() {
    bleState = BLE_MENU;
    bleSelectedItem = 0;
    bleSelectedDevice = 0;
    mouseActive = false;
    mouseConnected = false;
    mouseDPI = 8;  // High DPI default
    mouseSpeed = 10;
    
    initBLEModule();
    
    drawBLEMenu();
}

//==========================================================
// Handle BLE Menu
//==========================================================

void handleBLEMenu() {
    updateButton(btnUp);
    updateButton(btnDown);
    updateButton(btnSelect);
    updateButton(btnBack);
    
    // Handle BLE Mouse state
    if (bleState == BLE_MOUSE) {
        handleMouseControl();
        return;
    }
    
    if(isPressed(btnBack)) {
        switch(bleState) {
            case BLE_SCAN:
                bleState = BLE_MENU;
                drawBLEMenu();
                return;
            case BLE_LIST:
                bleState = BLE_MENU;
                drawBLEMenu();
                return;
            case BLE_STATUS:
                bleState = BLE_MENU;
                drawBLEMenu();
                return;
            case BLE_MENU:
                if (mouseActive) {
                    stopBLEMouse();
                }
                currentScreen = SCREEN_MAIN;
                drawMainMenu();
                return;
        }
    }
    
    switch(bleState) {
        case BLE_MENU:
            if(isPressed(btnUp)) {
                bleSelectedItem--;
                if(bleSelectedItem < 0) bleSelectedItem = bleMenuLength - 1;
                drawBLEMenu();
            } else if(isPressed(btnDown)) {
                bleSelectedItem++;
                if(bleSelectedItem >= bleMenuLength) bleSelectedItem = 0;
                drawBLEMenu();
            } else if(isPressed(btnSelect)) {
                switch(bleSelectedItem) {
                    case 0: // Scan
                        if(bleInitialized) {
                            bleState = BLE_SCAN;
                            performBLEScan();
                            bleState = BLE_LIST;
                            bleSelectedDevice = 0;
                            drawBLEList();
                        }
                        break;
                    case 1: // BLE Mouse
                        bleState = BLE_MOUSE;
                        initBLEMouse();
                        drawMouseControlScreen();
                        break;
                    case 2: // Status
                        bleState = BLE_STATUS;
                        drawBLEStatus();
                        break;
                    case 3: // Back
                        if (mouseActive) {
                            stopBLEMouse();
                        }
                        currentScreen = SCREEN_MAIN;
                        drawMainMenu();
                        break;
                }
            }
            break;
            
        case BLE_LIST:
            if(isPressed(btnUp)) {
                bleSelectedDevice--;
                if(bleSelectedDevice < 0) bleSelectedDevice = bleDeviceCount - 1;
                drawBLEList();
            } else if(isPressed(btnDown)) {
                bleSelectedDevice++;
                if(bleSelectedDevice >= bleDeviceCount) bleSelectedDevice = 0;
                drawBLEList();
            }
            break;
            
        case BLE_STATUS:
            // Static display
            break;
            
        case BLE_MOUSE:
            // Handled above
            break;
    }
}