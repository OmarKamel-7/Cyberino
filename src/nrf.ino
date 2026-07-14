//////////////////////////////////////////////////////////////
// CyberDeck Pro
// nrf.ino - Complete NRF24 Suite
// Features: Spectrum Analyzer (2.4GHz Scanner), Packet Counter, WiFi/BLE Jammer
//////////////////////////////////////////////////////////////

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

//==========================================================
// NRF24 Pin Definitions
//==========================================================

#define NRF_CE1    27
#define NRF_CSN1   26
#define NRF_CE2    25
#define NRF_CSN2   33

//==========================================================
// NRF24 Objects
//==========================================================

RF24 radio1(NRF_CE1, NRF_CSN1);
RF24 radio2(NRF_CE2, NRF_CSN2);

//==========================================================
// Constants
//==========================================================

constexpr int PAYLOAD_SIZE = 32;
constexpr int VISIBLE_ITEMS = 4;
constexpr int JAM_DURATION = 50;

// Scanner/Analyzer Constants
constexpr uint8_t SCANNER_WIDTH = 128;
constexpr uint8_t SCANNER_HEIGHT = 64;
constexpr uint8_t GRAPH_HEIGHT = 54;
constexpr uint8_t TOTAL_CHANNELS = 126;
constexpr uint8_t SAMPLES = 4;

//==========================================================
// NRF State Machine
//==========================================================

enum NRFState {
    NRF_MENU,
    NRF_ANALYZER,
    NRF_PACKET_COUNTER,
    NRF_JAMMING,
    NRF_MODULE_STATUS
};

NRFState nrfState = NRF_MENU;

//==========================================================
// NRF Variables
//==========================================================

bool nrf1_ok = false;
bool nrf2_ok = false;
bool stopJamming = false;
bool jammingActive = false;
unsigned long jamStartTime = 0;
int jamChannelNumber = 0;
int currentJamChannel = 0;

// Menu
int menuSelectedIndex = 0;
int menuScrollOffset = 0;

// Scanner/Analyzer Data
uint8_t strength1[TOTAL_CHANNELS] = {0};
uint8_t strength2[TOTAL_CHANNELS] = {0};
uint16_t hits1[TOTAL_CHANNELS] = {0};
uint16_t hits2[TOTAL_CHANNELS] = {0};
uint8_t mostActive1 = 0;
uint8_t mostActive2 = 0;

// Packet Counter Data
unsigned long packetCount1 = 0;
unsigned long packetCount2 = 0;
unsigned long packetRate1 = 0;
unsigned long packetRate2 = 0;
unsigned long lastPacketTime = 0;
bool counterRunning = false;

//==========================================================
// Menu Items
//==========================================================

const char* nrfMainMenu[] = {
    "2.4GHz Analyzer",
    "Packet Counter",
    "WiFi/BLE Jammer",
    "Module Status"
};
const int nrfMainMenuCount = 4;

// Jamming Modes
const char* jamModes[] = {
    "WiFi (14 CH)",
    "BLE (40 CH)",
    "All 2.4GHz",
    "Specific CH"
};
const int jamModesCount = 4;
int jamModeIndex = 0;

//==========================================================
// Drawing Functions - Optimized to prevent flicker
//==========================================================

void drawNRFHeader(const char *title) {
    static char lastTitle[40] = "";
    if (strcmp(lastTitle, title) != 0) {
        tft.fillRect(0, 0, 480, 40, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
        tft.drawCentreString(title, 240, 10, 4);
        strcpy(lastTitle, title);
    }
}

void drawNRFFooter() {
    static bool footerDrawn = false;
    if (!footerDrawn) {
        tft.drawFastHLine(0, 292, 480, TFT_DARKGREY);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawCentreString("UP/DOWN SELECT BACK", 240, 300, 2);
        footerDrawn = true;
    }
}

void drawNRFMenu(const char* title, const char* items[], int itemCount, int selected, int offset) {
    static char lastTitle[40] = "";
    static int lastSelected = -1;
    static int lastOffset = -1;
    
    bool needsRedraw = (strcmp(lastTitle, title) != 0) || (lastSelected != selected) || (lastOffset != offset);
    
    if (needsRedraw) {
        tft.fillScreen(TFT_BLACK);
        drawNRFHeader(title);
        strcpy(lastTitle, title);
        lastSelected = selected;
        lastOffset = offset;
    }
    
    int y = 60;
    for(int i = 0; i < VISIBLE_ITEMS; i++) {
        int idx = i + offset;
        if(idx >= itemCount) break;
        
        if(idx == selected) {
            tft.fillRoundRect(40, y - 4, 400, 38, 8, TFT_BLUE);
            tft.setTextColor(TFT_WHITE, TFT_BLUE);
            tft.drawString(">", 55, y, 4);
            tft.drawString(items[idx], 80, y, 4);
        } else {
            tft.fillRoundRect(40, y - 4, 400, 38, 8, TFT_BLACK);
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawString(items[idx], 80, y, 4);
        }
        y += 48;
    }
    
    // Page dots
    if(itemCount > VISIBLE_ITEMS) {
        int totalPages = (itemCount + VISIBLE_ITEMS - 1) / VISIBLE_ITEMS;
        int currentPage = selected / VISIBLE_ITEMS;
        int spacing = 30;
        int startX = 240 - (totalPages * spacing) / 2;
        for(int i = 0; i < totalPages; i++) {
            int dotY = 270;
            if(i == currentPage) {
                tft.fillCircle(startX + i * spacing, dotY, 6, TFT_CYAN);
            } else {
                tft.drawCircle(startX + i * spacing, dotY, 6, TFT_DARKGREY);
            }
        }
    }
    
    drawNRFFooter();
}

//==========================================================
// 2.4GHz ANALYZER / SCANNER
//==========================================================

void scanAllChannels() {
    for (uint8_t ch = 0; ch < TOTAL_CHANNELS; ch++) {
        uint8_t r1 = 0, r2 = 0;

        for (uint8_t i = 0; i < SAMPLES; i++) {
            radio1.setChannel(ch);
            delayMicroseconds(130);
            if (radio1.testRPD()) {
                r1++;
                hits1[ch]++;
            }

            radio2.setChannel(ch);
            delayMicroseconds(130);
            if (radio2.testRPD()) {
                r2++;
                hits2[ch]++;
            }
        }

        strength1[ch] = map(r1, 0, SAMPLES, 0, GRAPH_HEIGHT);
        strength2[ch] = map(r2, 0, SAMPLES, 0, GRAPH_HEIGHT);
    }

    uint16_t max1 = 0, max2 = 0;
    for (uint8_t ch = 0; ch < TOTAL_CHANNELS; ch++) {
        if (hits1[ch] > max1) {
            max1 = hits1[ch];
            mostActive1 = ch;
        }
        if (hits2[ch] > max2) {
            max2 = hits2[ch];
            mostActive2 = ch;
        }
    }
}

void drawAnalyzerScreen() {
    static bool firstDraw = true;
    if (firstDraw) {
        tft.fillScreen(TFT_BLACK);
        firstDraw = false;
    }
    
    drawNRFHeader("2.4GHz ANALYZER");
    
    int graphY = 50;
    int graphHeight = 180;
    int graphWidth = 440;
    int startX = 20;
    
    // Clear graph area
    tft.fillRect(startX, graphY, graphWidth, graphHeight + 20, TFT_BLACK);
    
    // Draw bars for both modules
    float channelStep = (float)TOTAL_CHANNELS / graphWidth;
    
    for (int x = 0; x < graphWidth; x++) {
        uint8_t ch = round(x * channelStep);
        if (ch >= TOTAL_CHANNELS) continue;
        
        uint8_t h1 = strength1[ch];
        uint8_t h2 = strength2[ch];
        
        // Smooth
        if (ch > 0) {
            h1 = (h1 + strength1[ch - 1]) / 2;
            h2 = (h2 + strength2[ch - 1]) / 2;
        }
        if (ch < TOTAL_CHANNELS - 1) {
            h1 = (h1 + strength1[ch + 1]) / 2;
            h2 = (h2 + strength2[ch + 1]) / 2;
        }
        
        // Scale to graph height
        int barHeight1 = map(h1, 0, GRAPH_HEIGHT, 0, graphHeight);
        int barHeight2 = map(h2, 0, GRAPH_HEIGHT, 0, graphHeight);
        
        int xPos = startX + x;
        
        // Radio 1 - Blue bars
        int y1 = graphY + graphHeight - barHeight1;
        tft.fillRect(xPos, y1, 2, barHeight1, TFT_BLUE);
        
        // Radio 2 - Green bars (offset)
        int y2 = graphY + graphHeight - barHeight2;
        tft.fillRect(xPos + 3, y2, 2, barHeight2, TFT_GREEN);
    }
    
    // Draw baseline
    tft.drawLine(startX, graphY + graphHeight, startX + graphWidth, graphY + graphHeight, TFT_DARKGREY);
    
    // Channel labels
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    for (uint8_t ch = 0; ch <= 125; ch += 25) {
        uint8_t x = startX + round((float)ch / 125 * graphWidth);
        tft.drawString(String(ch), x, graphY + graphHeight + 5, 1);
    }
    
    // Info labels
    char label[40];
    sprintf(label, "CH1:%d(%d) CH2:%d(%d)", mostActive1, hits1[mostActive1], mostActive2, hits2[mostActive2]);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(label, startX, graphY - 5, 1);
    
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.drawString("M1", startX, graphY + graphHeight + 18, 1);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("M2", startX + 40, graphY + graphHeight + 18, 1);
    
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("2400 MHz", startX, graphY + graphHeight + 28, 1);
    tft.drawRightString("2480 MHz", startX + graphWidth, graphY + graphHeight + 28, 1);
    
    drawNRFFooter();
    
    // Run scan periodically
    static unsigned long lastScan = 0;
    if (millis() - lastScan > 150) {
        lastScan = millis();
        scanAllChannels();
    }
    
    if (isPressed(btnBack)) {
        nrfState = NRF_MENU;
        drawNRFMenu("NRF24 SUITE", nrfMainMenu, nrfMainMenuCount, menuSelectedIndex, menuScrollOffset);
    }
}

//==========================================================
// PACKET COUNTER - Real packet counting
//==========================================================

void startPacketCounter() {
    packetCount1 = 0;
    packetCount2 = 0;
    packetRate1 = 0;
    packetRate2 = 0;
    lastPacketTime = millis();
    counterRunning = true;
    
    // Setup radios for listening
    radio1.setChannel(40);
    radio2.setChannel(40);
    radio1.openReadingPipe(0, 0xF0F0F0F0E1LL);
    radio2.openReadingPipe(0, 0xF0F0F0F0E2LL);
    radio1.startListening();
    radio2.startListening();
}

void stopPacketCounter() {
    counterRunning = false;
}

void updatePacketCounter() {
    if (!counterRunning) return;
    
    uint8_t buf[32];
    
    // Read from module 1
    while (radio1.available()) {
        radio1.read(&buf, sizeof(buf));
        packetCount1++;
    }
    
    // Read from module 2
    while (radio2.available()) {
        radio2.read(&buf, sizeof(buf));
        packetCount2++;
    }
    
    // Calculate rates
    unsigned long now = millis();
    if (now - lastPacketTime >= 1000) {
        packetRate1 = packetCount1 / ((now - lastPacketTime) / 1000);
        packetRate2 = packetCount2 / ((now - lastPacketTime) / 1000);
        lastPacketTime = now;
    }
}

void drawPacketCounterScreen() {
    static bool firstDraw = true;
    if (firstDraw) {
        tft.fillScreen(TFT_BLACK);
        drawNRFHeader("PACKET COUNTER");
        firstDraw = false;
    }
    
    // Update packet counts
    updatePacketCounter();
    
    int y = 60;
    int lineH = 40;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Module 1:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(packetCount1), 250, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Rate:", 30, y, 2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(String(packetRate1) + " pkts/s", 250, y, 2);
    y += lineH + 10;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Module 2:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(packetCount2), 250, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Rate:", 30, y, 2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(String(packetRate2) + " pkts/s", 250, y, 2);
    y += lineH + 10;
    
    tft.drawFastHLine(0, y, 480, TFT_DARKGREY);
    y += 15;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Total:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(packetCount1 + packetCount2), 250, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Total Rate:", 30, y, 2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(String(packetRate1 + packetRate2) + " pkts/s", 250, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Status:", 30, y, 2);
    tft.setTextColor(counterRunning ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawString(counterRunning ? "RUNNING" : "STOPPED", 250, y, 2);
    
    // Instructions
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawCentreString("SELECT: Start/Stop  BACK: Menu", 240, 280, 2);
    
    drawNRFFooter();
    
    if (isPressed(btnSelect)) {
        if (counterRunning) {
            stopPacketCounter();
        } else {
            startPacketCounter();
        }
        drawPacketCounterScreen();
        delay(200);
    }
    
    if (isPressed(btnBack)) {
        if (counterRunning) stopPacketCounter();
        nrfState = NRF_MENU;
        drawNRFMenu("NRF24 SUITE", nrfMainMenu, nrfMainMenuCount, menuSelectedIndex, menuScrollOffset);
    }
}

//==========================================================
// JAMMING ENGINE
//==========================================================

void generateNoise(byte* data, int size) {
    for (int i = 0; i < size; i++) {
        data[i] = random(0, 256);
    }
}

void jamChannels(const char* label, int startCh, int endCh, int channelCount) {
    if(!nrf1_ok && !nrf2_ok) return;
    
    stopJamming = false;
    jammingActive = true;
    jamStartTime = millis();
    jamChannelNumber = channelCount;
    
    byte data1[PAYLOAD_SIZE];
    byte data2[PAYLOAD_SIZE];
    generateNoise(data1, PAYLOAD_SIZE);
    generateNoise(data2, PAYLOAD_SIZE);
    
    radio1.stopListening();
    radio2.stopListening();
    radio1.setPALevel(RF24_PA_MAX);
    radio2.setPALevel(RF24_PA_MAX);
    radio1.setDataRate(RF24_2MBPS);
    radio2.setDataRate(RF24_2MBPS);
    radio1.setPayloadSize(PAYLOAD_SIZE);
    radio2.setPayloadSize(PAYLOAD_SIZE);
    radio1.setAutoAck(false);
    radio2.setAutoAck(false);
    radio1.setRetries(0, 0);
    radio2.setRetries(0, 0);
    
    int channelIndex = 0;
    unsigned long lastUIUpdate = 0;
    
    while(!stopJamming) {
        int ch = startCh + (channelIndex % (endCh - startCh + 1));
        currentJamChannel = ch;
        
        radio1.setChannel(ch);
        radio1.write(data1, sizeof(data1));
        delayMicroseconds(80);
        
        radio2.setChannel(ch);
        radio2.write(data2, sizeof(data2));
        delayMicroseconds(80);
        
        channelIndex++;
        
        if(millis() - lastUIUpdate > 100) {
            lastUIUpdate = millis();
            drawJammingScreen(ch, channelCount, label);
        }
        
        if(digitalRead(BTN_BACK) == LOW) {
            stopJamming = true;
            break;
        }
        
        yield();
    }
    
    radio1.setAutoAck(true);
    radio2.setAutoAck(true);
    radio1.startListening();
    radio2.startListening();
    jammingActive = false;
}

void jamWiFi() {
    jamChannels("WiFi", 1, 14, 14);
}

void jamBLE() {
    jamChannels("BLE", 0, 39, 40);
}

void jamAll() {
    jamChannels("ALL", 1, 125, 125);
}

void jamSpecific(int channel) {
    char label[20];
    sprintf(label, "CH %d", channel);
    jamChannels(label, channel, channel, 1);
}

void drawJammingScreen(int currentChannel, int totalChannels, const char* modeName) {
    static char lastMode[20] = "";
    static int lastChannel = -1;
    static int lastTotal = -1;
    static unsigned long lastDuration = 0;
    
    bool needsRedraw = (strcmp(lastMode, modeName) != 0) || 
                       (lastChannel != currentChannel) || 
                       (lastTotal != totalChannels);
    
    if (needsRedraw) {
        tft.fillScreen(TFT_BLACK);
        drawNRFHeader("⚡ JAMMING ACTIVE ⚡");
        strcpy(lastMode, modeName);
        lastChannel = currentChannel;
        lastTotal = totalChannels;
    }
    
    int y = 55;
    
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawCentreString(">> JAMMING <<", 240, y, 2);
    y += 45;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("Mode:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(modeName, 180, y, 2);
    y += 32;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Channels:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char chanStr[20];
    sprintf(chanStr, "%d total", totalChannels);
    tft.drawString(chanStr, 180, y, 2);
    y += 32;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Current CH:", 30, y, 2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    char curStr[10];
    sprintf(curStr, "%d", currentChannel);
    tft.drawString(curStr, 180, y, 2);
    y += 32;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Duration:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    unsigned long elapsed = (millis() - jamStartTime) / 1000;
    if (elapsed != lastDuration || needsRedraw) {
        char durStr[30];
        sprintf(durStr, "%lu seconds", elapsed);
        tft.fillRect(180, y, 100, 20, TFT_BLACK);
        tft.drawString(durStr, 180, y, 2);
        lastDuration = elapsed;
    }
    y += 35;
    
    // Activity bar
    int barX = 30;
    int barY = 240;
    int barWidth = 420;
    int barHeight = 22;
    
    static int lastPos1 = -1;
    int pos1 = (millis() / 20) % barWidth;
    int pos2 = (millis() / 30 + 100) % barWidth;
    int pos3 = (millis() / 40 + 200) % barWidth;
    
    if (pos1 != lastPos1 || needsRedraw) {
        tft.fillRect(barX, barY, barWidth, barHeight, TFT_DARKGREY);
        tft.fillRect(barX + pos1 - 10, barY, 20, barHeight, TFT_RED);
        tft.fillRect(barX + pos2 - 10, barY, 20, barHeight, TFT_ORANGE);
        tft.fillRect(barX + pos3 - 10, barY, 20, barHeight, TFT_YELLOW);
        lastPos1 = pos1;
    }
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawCentreString("🔊 FAST HOPPING - PRESS BACK TO STOP 🔊", 240, 272, 2);
    
    drawNRFFooter();
}

void drawJammerMenu() {
    static bool firstDraw = true;
    if (firstDraw) {
        tft.fillScreen(TFT_BLACK);
        drawNRFHeader("WiFi/BLE JAMMER");
        firstDraw = false;
    }
    
    int y = 60;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Mode:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(jamModes[jamModeIndex], 200, y, 2);
    y += 45;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Status:", 30, y, 2);
    tft.setTextColor(jammingActive ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    tft.drawString(jammingActive ? "JAMMING" : "READY", 200, y, 2);
    y += 45;
    
    if (jammingActive) {
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("Channel:", 30, y, 2);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString(String(currentJamChannel), 200, y, 2);
        y += 45;
        
        unsigned long elapsed = (millis() - jamStartTime) / 1000;
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("Duration:", 30, y, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(String(elapsed) + "s", 200, y, 2);
    }
    
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawCentreString("UP/DOWN: Change Mode  SELECT: Start/Stop", 240, 270, 2);
    
    drawNRFFooter();
    
    // Handle jammer menu navigation
    if (isPressed(btnBack)) {
        if (jammingActive) {
            stopJamming = true;
            jammingActive = false;
        }
        nrfState = NRF_MENU;
        drawNRFMenu("NRF24 SUITE", nrfMainMenu, nrfMainMenuCount, menuSelectedIndex, menuScrollOffset);
        return;
    }
    
    if (isPressed(btnUp)) {
        jamModeIndex = (jamModeIndex - 1 + jamModesCount) % jamModesCount;
        if (jammingActive) {
            stopJamming = true;
            jammingActive = false;
            delay(50);
        }
        drawJammerMenu();
        delay(200);
    } else if (isPressed(btnDown)) {
        jamModeIndex = (jamModeIndex + 1) % jamModesCount;
        if (jammingActive) {
            stopJamming = true;
            jammingActive = false;
            delay(50);
        }
        drawJammerMenu();
        delay(200);
    } else if (isPressed(btnSelect)) {
        if (jammingActive) {
            stopJamming = true;
            jammingActive = false;
            drawJammerMenu();
        } else {
            switch(jamModeIndex) {
                case 0: jamWiFi(); break;
                case 1: jamBLE(); break;
                case 2: jamAll(); break;
                case 3: jamSpecific(6); break;
            }
            drawJammerMenu();
        }
        delay(200);
    }
}

//==========================================================
// Module Status Screen
//==========================================================

void drawModuleStatus() {
    static bool firstDraw = true;
    if (firstDraw) {
        tft.fillScreen(TFT_BLACK);
        drawNRFHeader("MODULE STATUS");
        firstDraw = false;
    }
    
    int y = 60;
    int lineH = 40;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Module 1:", 30, y, 2);
    tft.setTextColor(nrf1_ok ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawString(nrf1_ok ? "OK" : "FAIL", 400, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Module 2:", 30, y, 2);
    tft.setTextColor(nrf2_ok ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawString(nrf2_ok ? "OK" : "FAIL", 400, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("CE1:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(NRF_CE1), 400, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("CSN1:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(NRF_CSN1), 400, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("CE2:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(NRF_CE2), 400, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("CSN2:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(NRF_CSN2), 400, y, 2);
    y += lineH;
    
    drawNRFFooter();
    
    if (isPressed(btnBack)) {
        nrfState = NRF_MENU;
        drawNRFMenu("NRF24 SUITE", nrfMainMenu, nrfMainMenuCount, menuSelectedIndex, menuScrollOffset);
    }
}

//==========================================================
// NRF Initialization
//==========================================================

void initNRFModules() {
    Serial.println("\n--- NRF24 Module Initialization ---");
    Serial.println("CE1: " + String(NRF_CE1) + ", CSN1: " + String(NRF_CSN1));
    Serial.println("CE2: " + String(NRF_CE2) + ", CSN2: " + String(NRF_CSN2));

    Serial.print("Initializing Module 1... ");
    if (radio1.begin()) {
        radio1.setChannel(40);
        radio1.setPALevel(RF24_PA_MAX);
        radio1.setDataRate(RF24_250KBPS);
        radio1.setPayloadSize(32);
        radio1.openReadingPipe(0, 0xF0F0F0F0E1LL);
        radio1.startListening();
        nrf1_ok = true;
        Serial.println("OK");
    } else {
        nrf1_ok = false;
        Serial.println("FAILED");
    }

    Serial.print("Initializing Module 2... ");
    if (radio2.begin()) {
        radio2.setChannel(40);
        radio2.setPALevel(RF24_PA_MAX);
        radio2.setDataRate(RF24_250KBPS);
        radio2.setPayloadSize(32);
        radio2.openReadingPipe(0, 0xF0F0F0F0E2LL);
        radio2.startListening();
        nrf2_ok = true;
        Serial.println("OK");
    } else {
        nrf2_ok = false;
        Serial.println("FAILED");
    }

    Serial.println("Status: M1=" + String(nrf1_ok ? "OK" : "FAIL") + 
                   ", M2=" + String(nrf2_ok ? "OK" : "FAIL"));
    Serial.println("-----------------------------------\n");
}

//==========================================================
// Init NRF Menu
//==========================================================

void initNRFMenu() {
    nrfState = NRF_MENU;
    menuSelectedIndex = 0;
    menuScrollOffset = 0;
    jammingActive = false;
    stopJamming = false;
    jamModeIndex = 0;
    counterRunning = false;
    
    // Clear scanner data
    memset(strength1, 0, sizeof(strength1));
    memset(strength2, 0, sizeof(strength2));
    memset(hits1, 0, sizeof(hits1));
    memset(hits2, 0, sizeof(hits2));
    mostActive1 = 0;
    mostActive2 = 0;
    
    initNRFModules();
    
    drawNRFMenu("NRF24 SUITE", nrfMainMenu, nrfMainMenuCount, menuSelectedIndex, menuScrollOffset);
}

//==========================================================
// Handle NRF Menu
//==========================================================

void handleNRFMenu() {
    updateButton(btnUp);
    updateButton(btnDown);
    updateButton(btnSelect);
    updateButton(btnBack);
    
    // Handle sub-states
    switch(nrfState) {
        case NRF_ANALYZER:
            drawAnalyzerScreen();
            return;
        case NRF_PACKET_COUNTER:
            drawPacketCounterScreen();
            return;
        case NRF_JAMMING:
            drawJammerMenu();
            return;
        case NRF_MODULE_STATUS:
            drawModuleStatus();
            return;
        default:
            break;
    }
    
    // Main menu
    if(isPressed(btnBack)) {
        currentScreen = SCREEN_MAIN;
        drawMainMenu();
        return;
    }
    
    if(isPressed(btnUp)) {
        menuSelectedIndex = (menuSelectedIndex - 1 + nrfMainMenuCount) % nrfMainMenuCount;
        menuScrollOffset = (menuSelectedIndex / VISIBLE_ITEMS) * VISIBLE_ITEMS;
        drawNRFMenu("NRF24 SUITE", nrfMainMenu, nrfMainMenuCount, menuSelectedIndex, menuScrollOffset);
    } else if(isPressed(btnDown)) {
        menuSelectedIndex = (menuSelectedIndex + 1) % nrfMainMenuCount;
        menuScrollOffset = (menuSelectedIndex / VISIBLE_ITEMS) * VISIBLE_ITEMS;
        drawNRFMenu("NRF24 SUITE", nrfMainMenu, nrfMainMenuCount, menuSelectedIndex, menuScrollOffset);
    } else if(isPressed(btnSelect)) {
        switch(menuSelectedIndex) {
            case 0:
                nrfState = NRF_ANALYZER;
                drawAnalyzerScreen();
                break;
            case 1:
                nrfState = NRF_PACKET_COUNTER;
                counterRunning = false;
                drawPacketCounterScreen();
                break;
            case 2:
                nrfState = NRF_JAMMING;
                drawJammerMenu();
                break;
            case 3:
                nrfState = NRF_MODULE_STATUS;
                drawModuleStatus();
                break;
        }
    }
}