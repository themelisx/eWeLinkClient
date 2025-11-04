#ifndef EWELINK_h
#define EWELINK_h

#include <Arduino.h>
#include <HttpClient.h>
#include "defines.h"

#ifdef USE_EWELINK
    #include "user_setup.h"

    // every 24 hours
    #define EWELINK_TOKEN_REFRESH_INTERVAL 24UL * 60UL * 60UL * 1000UL

    class EWeLink {
    private:
        // functions
        void login();
        bool doRefreshToken();        
        bool setSwitch(const String& deviceId, bool switchState, bool retryOn401);

        // vars
        char* eWeLinkApiHost;
        int eWeLinkApiHostPort = 8080;
        unsigned long lastTokenRefresh = 0;
        String accessToken;
        String refreshToken;

        // User setup
        String appId;
        String appSecret = EWELINK_AppSecret;
        String email = EWELINK_Email;
        String ewelinkPass = EWELINK_Password;

        WiFiClientSecure client;  
        HTTPClient http;

        bool dataUpdated;
        bool loggedIn;
        SemaphoreHandle_t semaphoreData;
        char* tmp_buf;

    public:
        EWeLink();
        // functions
        void init();
        void checkToken();
        bool fetchData();
        bool isDataUpdated();
        void setDataUpdated(bool updated);
    };
    #endif

#endif