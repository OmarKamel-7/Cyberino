//////////////////////////////////////////////////////////////
// CyberDeck Pro
// nfc.ino - NFC/MFRC522 Reader with Polished UI + Scan History
// Pin: SDA=21, RST=15
//////////////////////////////////////////////////////////////

#include <SPI.h>
#include <MFRC522.h>

//==========================================================
// NFC Pin Definitions
//==========================================================

#define NFC_SDA     21
#define NFC_RST     15

//==========================================================
// NFC Objects
//==========================================================

MFRC522 mfrc522(NFC_SDA, NFC_RST);

//==========================================================
// Theme
//==========================================================
// Centralised so the whole screen can be restyled from one place.

#define THEME_BG          TFT_BLACK
#define THEME_HEADER_BG    TFT_NAVY
#define THEME_HEADER_TXT   TFT_WHITE
#define THEME_ACCENT       TFT_SKYBLUE
#define THEME_LABEL        TFT_CYAN
#define THEME_VALUE        TFT_WHITE
#define THEME_MUTED        TFT_DARKGREY
#define THEME_OK           TFT_GREEN
#define THEME_ERR          TFT_RED
#define THEME_WARN         TFT_YELLOW
#define THEME_SELECT_BG    TFT_BLUE
#define THEME_SELECT_TXT   TFT_WHITE

// Tag-family accent colors (used for icon + badge)
#define COL_CLASSIC   TFT_ORANGE
#define COL_ULTRALT   TFT_GREENYELLOW
#define COL_DESFIRE   TFT_PURPLE
#define COL_UNKNOWN   TFT_MAROON

//==========================================================
// NFC State Machine
//==========================================================

enum NFCClientState {
    NFC_MENU,
    NFC_READ,
    NFC_STATUS,
    NFC_DATA,
    NFC_HISTORY,
    NFC_ERROR
};

NFCClientState nfcState = NFC_MENU;

//==========================================================
// NFC Variables
//==========================================================

bool nfcInitialized = false;
int nfcSelectedItem = 0;
String lastUID = "";
String tagType = "";
String sakInfo = "";
String atqaInfo = "";
int tagSize = 0;
bool scanning = false;
byte lastSAK = 0;

const char *nfcMenu[] = {
    "Read Tag",
    "Scan History",
    "Module Status",
    "Back"
};
const int nfcMenuLength = 4;

//==========================================================
// Scan History
//==========================================================

struct NFCRecord {
    String uid;
    String type;
    byte sak;
    unsigned long timestamp;
};

const int NFC_HISTORY_SIZE = 8;
NFCRecord nfcHistory[NFC_HISTORY_SIZE];
int nfcHistoryCount = 0;   // how many valid entries
int nfcHistoryHead = 0;    // ring buffer write position
int nfcHistoryScroll = 0;  // top-of-list scroll offset for the history screen

void addToHistory(String uid, String type, byte sak) {
    nfcHistory[nfcHistoryHead].uid = uid;
    nfcHistory[nfcHistoryHead].type = type;
    nfcHistory[nfcHistoryHead].sak = sak;
    nfcHistory[nfcHistoryHead].timestamp = millis();
    nfcHistoryHead = (nfcHistoryHead + 1) % NFC_HISTORY_SIZE;
    if (nfcHistoryCount < NFC_HISTORY_SIZE) nfcHistoryCount++;
}

//==========================================================
// Shared Chrome (header / footer)
//==========================================================

void drawNFCHeader(const char *title) {
    tft.fillRect(0, 0, 480, 40, THEME_HEADER_BG);
    tft.drawFastHLine(0, 40, 480, THEME_ACCENT);

    // small back-chevron glyph, purely decorative
    tft.setTextColor(THEME_ACCENT, THEME_HEADER_BG);
    tft.drawString("<", 14, 10, 4);

    tft.setTextColor(THEME_HEADER_TXT, THEME_HEADER_BG);
    tft.drawCentreString(title, 240, 10, 4);
}

void drawNFCFooter(const char *hint) {
    tft.drawFastHLine(0, 292, 480, THEME_MUTED);
    tft.setTextColor(THEME_MUTED, THEME_BG);
    tft.drawCentreString(hint, 240, 300, 2);
}

// Small vector "waves" icon used for the NFC/tag family glyph
void drawTagIcon(int cx, int cy, uint16_t color) {
    tft.drawCircle(cx, cy, 4, color);
    tft.drawCircle(cx, cy, 11, color);
    tft.drawCircle(cx, cy, 18, color);
    tft.fillCircle(cx, cy, 2, color);
}

uint16_t colorForType(const String &type) {
    if (type.indexOf("Classic") >= 0) return COL_CLASSIC;
    if (type.indexOf("Ultralight") >= 0 || type.indexOf("NTAG") >= 0) return COL_ULTRALT;
    if (type.indexOf("DESFire") >= 0) return COL_DESFIRE;
    return COL_UNKNOWN;
}

//==========================================================
// NFC Helper Functions
//==========================================================

String getTagType(byte sak) {
    switch(sak) {
        case 0x08: return "MIFARE Classic 1K";
        case 0x18: return "MIFARE Classic 4K";
        case 0x09: return "MIFARE Mini";
        case 0x20: return "MIFARE Ultralight";
        case 0x00: return "NTAG / Ultralight";
        case 0x28: return "MIFARE DESFire";
        case 0x88: return "Classic 1K (Infineon)";
        case 0x98: return "Classic 4K (Infineon)";
        case 0x59: return "MIFARE Plus S 2K";
        case 0x19: return "MIFARE Plus S 4K";
        default: return "Unknown Tag";
    }
}

String getSAKInfo(byte sak) {
    String info = "0x" + String(sak, HEX);
    if (sak & 0x20) info += " - Proprietary";
    if (sak & 0x40) info += " - No memory";
    if (sak & 0x80) info += " - ISO 14443-4";
    return info;
}

String getATQAInfo(byte atqaH, byte atqaL) {
    String info = "0x" + String(atqaH, HEX) + " 0x" + String(atqaL, HEX);
    return info;
}

int getTagSize(byte sak) {
    switch(sak) {
        case 0x08: case 0x88: return 1024;
        case 0x18: case 0x98: return 4096;
        case 0x09: return 320;
        case 0x20: case 0x00: return 64;
        case 0x28: return 4096;
        default: return 0;
    }
}

//==========================================================
// NFC Initialization
//==========================================================

void initNFCModule() {
    Serial.println("\n--- NFC Module Initialization ---");
    Serial.println("SDA: " + String(NFC_SDA) + ", RST: " + String(NFC_RST));

    SPI.begin();
    mfrc522.PCD_Init();
    delay(50);

    byte version = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);

    if(version == 0x00 || version == 0xFF) {
        nfcInitialized = false;
        Serial.println("FAILED - No module detected");
    } else {
        nfcInitialized = true;
        Serial.println("OK - Module detected");
        Serial.print("Firmware: v");
        Serial.print((version >> 4) & 0x0F);
        Serial.print(".");
        Serial.println(version & 0x0F);
    }

    Serial.println("-----------------------------------\n");
}

//==========================================================
// NFC Status Screen
//==========================================================

void drawStatusRow(int y, const char *label, String value, uint16_t valColor) {
    tft.setTextColor(THEME_LABEL, THEME_BG);
    tft.setTextSize(1);
    tft.drawString(label, 30, y, 2);
    tft.setTextColor(valColor, THEME_BG);
    tft.drawRightString(value, 450, y, 2);
}

void drawNFCStatus() {
    tft.fillScreen(THEME_BG);
    drawNFCHeader("MODULE STATUS");

    int y = 55;

    // Status pill
    tft.fillRoundRect(30, y, 420, 40, 8, nfcInitialized ? TFT_DARKGREEN : TFT_MAROON);
    tft.setTextColor(THEME_VALUE, nfcInitialized ? TFT_DARKGREEN : TFT_MAROON);
    tft.drawCentreString(nfcInitialized ? "READER ONLINE" : "READER NOT DETECTED", 240, y + 10, 2);
    y += 60;

    drawStatusRow(y, "SDA Pin", String(NFC_SDA), THEME_VALUE); y += 30;
    drawStatusRow(y, "RST Pin", String(NFC_RST), THEME_VALUE); y += 30;

    if(nfcInitialized) {
        byte version = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
        String fw = "v" + String((version >> 4) & 0x0F) + "." + String(version & 0x0F);
        drawStatusRow(y, "Firmware", fw, THEME_VALUE); y += 30;
    }

    drawStatusRow(y, "Scans this session", String(nfcHistoryCount), THEME_ACCENT); y += 30;

    if(lastUID.length() > 0) {
        drawStatusRow(y, "Last UID", lastUID, THEME_OK); y += 30;
    }

    if(!nfcInitialized) {
        tft.setTextColor(THEME_WARN, THEME_BG);
        tft.setTextSize(1);
        tft.drawCentreString("Check wiring: SDA/RST/SPI lines & 3.3V power", 240, y + 15, 1);
    }

    drawNFCFooter("BACK TO RETURN");
}

//==========================================================
// NFC Error Screen (module missing, entered Read Tag)
//==========================================================

void drawNFCError() {
    tft.fillScreen(THEME_BG);
    drawNFCHeader("NFC READER");

    drawTagIcon(240, 120, THEME_ERR);
    tft.fillRoundRect(220, 108, 40, 24, 4, THEME_BG); // punch a gap so the "X" reads clearly
    tft.setTextColor(THEME_ERR, THEME_BG);
    tft.setTextSize(2);
    tft.drawCentreString("X", 240, 108, 4);

    tft.setTextColor(THEME_ERR, THEME_BG);
    tft.setTextSize(1);
    tft.drawCentreString("No reader detected", 240, 165, 2);
    tft.setTextColor(THEME_MUTED, THEME_BG);
    tft.drawCentreString("Check module wiring and power, then retry", 240, 195, 1);

    tft.fillRoundRect(170, 220, 140, 40, 8, THEME_SELECT_BG);
    tft.setTextColor(THEME_SELECT_TXT, THEME_SELECT_BG);
    tft.drawCentreString("RETRY", 240, 232, 2);

    drawNFCFooter("SELECT TO RETRY  |  BACK TO MENU");
}

//==========================================================
// NFC Complete Data Display
//==========================================================

void displayNFCData(byte sak) {
    tft.fillScreen(THEME_BG);
    drawNFCHeader("TAG DETAILS");

    uint16_t famColor = colorForType(tagType);

    // Icon + tag-family badge
    drawTagIcon(48, 68, famColor);
    tft.fillRoundRect(75, 52, 200, 32, 16, famColor);
    tft.setTextColor(THEME_BG, famColor);
    tft.setTextSize(1);
    tft.drawCentreString(tagType, 175, 62, 2);

    int y = 100;
    int lineH = 26;

    // ===== UID SECTION =====
    tft.drawFastHLine(15, y, 450, THEME_MUTED);
    y += 10;

    tft.setTextColor(THEME_LABEL, THEME_BG);
    tft.setTextSize(1);
    tft.drawString("UID", 15, y, 2);
    tft.setTextColor(THEME_OK, THEME_BG);
    tft.drawRightString(lastUID, 450, y, 2);
    y += lineH;

    tft.setTextColor(THEME_LABEL, THEME_BG);
    tft.drawString("UID Length", 15, y, 2);
    tft.setTextColor(THEME_VALUE, THEME_BG);
    tft.drawRightString(String(mfrc522.uid.size) + " bytes", 450, y, 2);
    y += lineH + 6;

    // ===== TAG INFO SECTION =====
    tft.drawFastHLine(15, y, 450, THEME_MUTED);
    y += 10;

    tft.setTextColor(THEME_LABEL, THEME_BG);
    tft.drawString("SAK", 15, y, 2);
    tft.setTextColor(THEME_VALUE, THEME_BG);
    sakInfo = getSAKInfo(sak);
    tft.drawRightString(sakInfo, 450, y, 2);
    y += lineH;

    tft.setTextColor(THEME_LABEL, THEME_BG);
    tft.drawString("ATQA", 15, y, 2);
    tft.setTextColor(THEME_VALUE, THEME_BG);

    byte atqaH = 0x00, atqaL = 0x00;
    if(sak == 0x08 || sak == 0x88) { atqaH = 0x00; atqaL = 0x04; }
    else if(sak == 0x18 || sak == 0x98) { atqaH = 0x00; atqaL = 0x02; }
    else if(sak == 0x20 || sak == 0x00) { atqaH = 0x00; atqaL = 0x44; }
    else if(sak == 0x28) { atqaH = 0x03; atqaL = 0x44; }

    atqaInfo = getATQAInfo(atqaH, atqaL);
    tft.drawRightString(atqaInfo, 450, y, 2);
    y += lineH;

    tft.setTextColor(THEME_LABEL, THEME_BG);
    tft.drawString("Memory", 15, y, 2);
    tft.setTextColor(THEME_VALUE, THEME_BG);
    tagSize = getTagSize(sak);
    tft.drawRightString(tagSize > 0 ? (String(tagSize) + " bytes (" + String(tagSize/1024) + "K)") : "Unknown",
                         450, y, 2);
    y += lineH + 8;

    // ===== ADDITIONAL INFO =====
    tft.drawFastHLine(15, y, 450, THEME_MUTED);
    y += 10;
    tft.setTextColor(THEME_ACCENT, THEME_BG);
    tft.drawString("Notes", 15, y, 2);
    y += 20;

    tft.setTextColor(THEME_MUTED, THEME_BG);
    tft.setTextSize(1);

    if(tagType.indexOf("Classic") >= 0) {
        tft.drawString("- Sectors: 16 (1K) or 40 (4K), 4 blocks/sector", 25, y, 1); y += 16;
        tft.drawString("- Key A/B: 6 bytes each, Access Bits: 3 bytes", 25, y, 1);
    } else if(tagType.indexOf("Ultralight") >= 0 || tagType.indexOf("NTAG") >= 0) {
        tft.drawString("- Pages: 16 (Ultralight) / 48 (NTAG213)", 25, y, 1); y += 16;
        tft.drawString("- 4 bytes per page, may be password protected", 25, y, 1);
    } else if(tagType.indexOf("DESFire") >= 0) {
        tft.drawString("- File-system structure, AES-128 encryption", 25, y, 1); y += 16;
        tft.drawString("- Multiple applications, ISO 14443-4 compliant", 25, y, 1);
    } else {
        tft.drawString("- ISO/IEC 14443 contactless smart card", 25, y, 1); y += 16;
        tft.drawString("- SAK: 0x" + String(sak, HEX), 25, y, 1);
    }

    drawNFCFooter("SELECT TO SCAN AGAIN  |  BACK TO MENU");
}

//==========================================================
// NFC Read Tag - Animated Waiting + Read
//==========================================================

void drawScanAnimation() {
    static unsigned long lastFrame = 0;
    static int radius = 0;
    const int cx = 240, cy = 160, maxR = 46;

    if(millis() - lastFrame < 40) return;
    lastFrame = millis();

    // erase previous ring
    tft.drawCircle(cx, cy, radius, THEME_BG);
    if (radius > 1) tft.drawCircle(cx, cy, radius - 1, THEME_BG);

    radius += 2;
    if(radius > maxR) radius = 0;

    // redraw the static dot + growing ring
    tft.fillCircle(cx, cy, 5, THEME_ACCENT);
    tft.drawCircle(cx, cy, radius, THEME_ACCENT);
    if (radius > 1) tft.drawCircle(cx, cy, radius - 1, THEME_ACCENT);
}

void drawNFCReader() {
    if (!scanning) {
        tft.fillScreen(THEME_BG);
        drawNFCHeader("SCANNING");
        tft.setTextColor(THEME_VALUE, THEME_BG);
        tft.setTextSize(1);
        tft.drawCentreString("Hold a tag near the reader", 240, 80, 2);
        tft.setTextColor(THEME_MUTED, THEME_BG);
        tft.drawCentreString("Keep it steady for a moment", 240, 220, 2);
        drawNFCFooter("BACK TO CANCEL");
        scanning = true;
    }

    drawScanAnimation();

    // Try to read a tag
    if(nfcInitialized) {
        if(!mfrc522.PICC_IsNewCardPresent()) return;
        if(!mfrc522.PICC_ReadCardSerial()) return;

        // === READ ALL DATA ===
        lastUID = "";
        for(byte i = 0; i < mfrc522.uid.size; i++) {
            if(i > 0) lastUID += ":";
            if(mfrc522.uid.uidByte[i] < 0x10) lastUID += "0";
            lastUID += String(mfrc522.uid.uidByte[i], HEX);
        }
        lastUID.toUpperCase();

        lastSAK = mfrc522.uid.sak;
        tagType = getTagType(lastSAK);

        addToHistory(lastUID, tagType, lastSAK);
        displayNFCData(lastSAK);

        // success toast
        tft.fillRoundRect(90, 258, 300, 24, 6, TFT_DARKGREEN);
        tft.setTextColor(THEME_VALUE, TFT_DARKGREEN);
        tft.setTextSize(1);
        tft.drawCentreString("Tag read successfully", 240, 265, 2);

        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();

        delay(100);
        scanning = false;
        nfcState = NFC_DATA;
        return;
    }
}

//==========================================================
// NFC Scan History
//==========================================================

void drawNFCHistory() {
    tft.fillScreen(THEME_BG);
    drawNFCHeader("SCAN HISTORY");

    if (nfcHistoryCount == 0) {
        tft.setTextColor(THEME_MUTED, THEME_BG);
        tft.setTextSize(1);
        tft.drawCentreString("No tags scanned yet this session", 240, 140, 2);
        drawNFCFooter("BACK TO MENU");
        return;
    }

    int y = 50;
    int shown = 0;
    const int maxRows = 6;

    // most recent first
    for(int i = 0; i < nfcHistoryCount && shown < maxRows; i++) {
        int idx = (nfcHistoryHead - 1 - i + NFC_HISTORY_SIZE) % NFC_HISTORY_SIZE;
        NFCRecord &rec = nfcHistory[idx];
        uint16_t famColor = colorForType(rec.type);

        tft.fillRoundRect(15, y, 450, 34, 6, TFT_BLACK);
        tft.drawRoundRect(15, y, 450, 34, 6, THEME_MUTED);

        tft.fillCircle(32, y + 17, 6, famColor);

        tft.setTextColor(THEME_VALUE, THEME_BG);
        tft.setTextSize(1);
        tft.drawString(rec.uid, 50, y + 5, 2);

        tft.setTextColor(famColor, THEME_BG);
        tft.drawRightString(rec.type, 455, y + 5, 1);

        y += 40;
        shown++;
    }

    if (nfcHistoryCount > maxRows) {
        tft.setTextColor(THEME_MUTED, THEME_BG);
        tft.drawCentreString("+" + String(nfcHistoryCount - maxRows) + " more not shown", 240, y + 6, 1);
    }

    drawNFCFooter("BACK TO MENU");
}

//==========================================================
// NFC Menu
//==========================================================

void drawMenuIcon(int cx, int cy, int index, uint16_t color) {
    switch(index) {
        case 0: // Read Tag -> waves
            drawTagIcon(cx, cy, color);
            break;
        case 1: // History -> clock
            tft.drawCircle(cx, cy, 12, color);
            tft.drawLine(cx, cy, cx, cy - 8, color);
            tft.drawLine(cx, cy, cx + 6, cy + 2, color);
            break;
        case 2: // Status -> gear-ish (simple circle + tick marks)
            tft.drawCircle(cx, cy, 10, color);
            tft.fillCircle(cx, cy, 3, color);
            break;
        default: // Back -> arrow
            tft.drawLine(cx - 10, cy, cx + 10, cy, color);
            tft.drawLine(cx - 10, cy, cx - 3, cy - 7, color);
            tft.drawLine(cx - 10, cy, cx - 3, cy + 7, color);
            break;
    }
}

void drawNFCMenu() {
    tft.fillScreen(THEME_BG);
    drawNFCHeader("NFC TOOLS");

    int y = 60;
    for(int i = 0; i < nfcMenuLength; i++) {
        bool sel = (i == nfcSelectedItem);

        if(sel) {
            tft.fillRoundRect(50, y - 4, 380, 46, 8, THEME_SELECT_BG);
            tft.drawRoundRect(50, y - 4, 380, 46, 8, THEME_ACCENT);
        }

        uint16_t iconColor = sel ? THEME_SELECT_TXT : THEME_MUTED;
        drawMenuIcon(85, y + 18, i, iconColor);

        tft.setTextColor(sel ? TFT_WHITE : TFT_LIGHTGREY, sel ? TFT_BLUE : TFT_BLACK);
        tft.drawString(nfcMenu[i], 120, y + 6, 4);

        // small badge showing history count next to "Scan History"
        if(i == 1 && nfcHistoryCount > 0) {
            tft.fillRoundRect(370, y + 4, 40, 24, 12, sel ? THEME_ACCENT : THEME_MUTED);
            tft.setTextColor(THEME_BG, sel ? THEME_ACCENT : THEME_MUTED);
            tft.drawCentreString(String(nfcHistoryCount), 390, y + 9, 2);
        }

        y += 55;
    }

    drawNFCFooter("UP/DOWN SELECT  |  BACK TO EXIT");
}

//==========================================================
// Init NFC Menu
//==========================================================

void initNFCMenu() {
    nfcState = NFC_MENU;
    nfcSelectedItem = 0;
    scanning = false;
    lastUID = "";
    lastSAK = 0;

    initNFCModule();

    drawNFCMenu();
}

//==========================================================
// Handle NFC Menu
//==========================================================

void handleNFCMenu() {
    updateButton(btnUp);
    updateButton(btnDown);
    updateButton(btnSelect);
    updateButton(btnBack);

    if(isPressed(btnBack)) {
        switch(nfcState) {
            case NFC_READ:
            case NFC_DATA:
            case NFC_STATUS:
            case NFC_HISTORY:
            case NFC_ERROR:
                nfcState = NFC_MENU;
                scanning = false;
                drawNFCMenu();
                return;
            case NFC_MENU:
                currentScreen = SCREEN_MAIN;
                drawMainMenu();
                return;
        }
    }

    switch(nfcState) {
        case NFC_MENU:
            if(isPressed(btnUp)) {
                nfcSelectedItem--;
                if(nfcSelectedItem < 0) nfcSelectedItem = nfcMenuLength - 1;
                drawNFCMenu();
            } else if(isPressed(btnDown)) {
                nfcSelectedItem++;
                if(nfcSelectedItem >= nfcMenuLength) nfcSelectedItem = 0;
                drawNFCMenu();
            } else if(isPressed(btnSelect)) {
                switch(nfcSelectedItem) {
                    case 0:
                        scanning = false;
                        if(nfcInitialized) {
                            nfcState = NFC_READ;
                            drawNFCReader();
                        } else {
                            nfcState = NFC_ERROR;
                            drawNFCError();
                        }
                        break;
                    case 1:
                        nfcState = NFC_HISTORY;
                        drawNFCHistory();
                        break;
                    case 2:
                        nfcState = NFC_STATUS;
                        drawNFCStatus();
                        break;
                    case 3:
                        currentScreen = SCREEN_MAIN;
                        drawMainMenu();
                        break;
                }
            }
            break;

        case NFC_READ:
            drawNFCReader();
            break;

        case NFC_DATA:
            // Select scans again from the results screen
            if(isPressed(btnSelect)) {
                scanning = false;
                nfcState = NFC_READ;
                drawNFCReader();
            }
            break;

        case NFC_STATUS:
            // Static display
            break;

        case NFC_HISTORY:
            // Static display (could be extended with scrolling)
            break;

        case NFC_ERROR:
            if(isPressed(btnSelect)) {
                initNFCModule();
                if(nfcInitialized) {
                    nfcState = NFC_READ;
                    scanning = false;
                    drawNFCReader();
                } else {
                    drawNFCError();
                }
            }
            break;
    }
}
