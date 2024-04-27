#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

#define FW_VERSION 0x0003        // version number
#define FW_VERSION_SUFFIX "-BETA" // suffix, like -RC1 or whatever.
// #define BAUD_115200              // Defaults to 1 megabaud this is not defined
// #define DEBUGBLOCKS              // uncomment to enable extra debug information on the block transfers
// #define FORCE_IMG_DL             // force tag to re-download every upload
// #define DEBUGPROTO               // debug protocol
// #define DEBUGOTA                 // debug OTA FW updates
#define DEBUGDRAWING             // debug the drawing part
// #define DEBUGEPD                 // debug the EPD driver
#define DEBUGMAIN                // parts in the main loop
// #define DEBUGNFC                 // debug NFC functions
#define DEBUGGUI                 // debug GUI drawing (enabled)
// #define DEBUGSETTINGS            // debug settings module (preferences/eeprom)
// #define DEBUGEEPROM              // eeprom-related debug messages
// #define DEBUG_NV_DATA            // debug eeprom-related data accesses
// #define DEBUG_SLEEP              // debug sleeping
//#define DEBUG_RX_DATA            // display subgig rx packets
//#define DEBUG_TX_DATA            // display subgig tx packets
#define DEBUG_MAX_SLEEP 5000UL   // forced maximum sleep time for debugging 
// #define DEBUG_AP_SEARCH          // log ap search details
// #define DEBUG_FORCE_OVERLAY      // force low bat and no AP icons to display
// #define DEBUG_CHIP_CFG   // log chip configuration
// #define DISABLE_BARCODES   // barcodes are optional
#define ISDEBUGBUILD          // disable clearing and resaving of settings on every reset

#define SFDP_DISABLED         // Disable SFDP to save 1538 bytes.
// #define DISABLE_UI         // when you need to debug and are out of flash
// #define DISABLE_DISPLAY    // don't actually update the display

#if defined(DEBUG_RX_DATA) || defined(DEBUG_TX_DATA)
#define DEBUG_COMMS
#endif

#if defined(DISABLE_UI) && !defined(LEAN_VERSION)
#define LEAN_VERSION
#define DISABLE_BARCODES
#endif

#define SETTINGS_STRUCT_VERSION 0x01

#define DEFAULT_SETTING_FASTBOOT 0
#define DEFAULT_SETTING_RFWAKE 0
#define DEFAULT_SETTING_TAGROAMING 0
#define DEFAULT_SETTING_SCANFORAP 0
#define DEFAULT_SETTING_LOWBATSYMBOL 1
#define DEFAULT_SETTING_NORFSYMBOL 1

#define BAND_UNKNOWN    0
#define BAND_868        1
#define BAND_915        2


/*
struct tagsettings {
    uint8_t settingsVer;                  // the version of the struct as written to the infopage
    uint8_t enableFastBoot;               // default 0; if set, it will skip splashscreen
    uint8_t enableRFWake;                 // default 0; if set, it will enable RF wake. This will add about ~0.9µA idle power consumption
    uint8_t enableTagRoaming;             // default 0; if set, the tag will scan for an accesspoint every few check-ins. This will increase power consumption quite a bit
    uint8_t enableScanForAPAfterTimeout;  // default 1; if a the tag failed to check in, after a few attempts it will try to find a an AP on other channels
    uint8_t enableLowBatSymbol;           // default 1; tag will show 'low battery' icon on screen if the battery is depleted
    uint8_t enableNoRFSymbol;             // default 1; tag will show 'no signal' icon on screen if it failed to check in for a longer period of time
    uint8_t fastBootCapabilities;         // holds the byte with 'capabilities' as detected during a normal tag boot; allows the tag to skip detecting buttons and NFC chip
    uint8_t customMode;                   // default 0; if anything else, tag will bootup in a different 'mode'
    uint16_t batLowVoltage;               // Low battery threshold voltage (2450 for 2.45v). defaults to BATTERY_VOLTAGE_MINIMUM from powermgt.h
    uint16_t minimumCheckInTime;          // defaults to BASE_INTERVAL from powermgt.h
    uint8_t fixedChannel;                 // default 0; if set to a valid channel number, the tag will stick to that channel
} __packed;
*/

extern struct tagsettings __xdata tagSettings;

void loadDefaultSettings();
void writeSettings();
void loadSettings();
void loadSettingsFromBuffer(uint8_t* p);
#endif
