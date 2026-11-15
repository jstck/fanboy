#pragma once
// Copy this file to secrets.h (gitignored) and fill in.

// Any number of networks; the strongest one in range is used, and it switches
// automatically if the current one disappears.
#define WIFI_NETWORKS                         \
    {                                         \
        {"home-ssid", "home-password"},       \
        {"bedroom-ssid", "bedroom-password"}, \
    }

// Password for over-the-air updates (env c3-ota reads it from FANBOY_OTA_PASSWORD).
#define OTA_PASSWORD "change-me"
