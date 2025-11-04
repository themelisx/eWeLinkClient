#include <Arduino.h>
#include <HttpClient.h>
#include <ArduinoJson.h> 

#include "defines.h"
#include "vars.h"

#include "MyDebug.h"
#include "eWeLink.h"

// e-WeLink
#ifdef USE_EWELINK

EWeLink::EWeLink() {
  myDebug->println(DEBUG_LEVEL_DEBUG, "[EWeLink]");
  dataUpdated = false;
  loggedIn = false;
  eWeLinkApiHost = "https://eu-api.coolkit.cc";   // or us-api.coolkit.cc for USA
  eWeLinkApiHostPort = 443;
  lastTokenRefresh = 0;
  tmp_buf = (char*)malloc(128);
  client.setInsecure();
  semaphoreData = xSemaphoreCreateMutex();
  xSemaphoreGive(semaphoreData);
}

void EWeLink::init() {
  myDebug->println(DEBUG_LEVEL_DEBUG, "Initializing eWeLink");  
  login();
}

bool EWeLink::isDataUpdated() {
  bool ret;
  xSemaphoreTake(semaphoreData, portMAX_DELAY);
  ret = dataUpdated;
  xSemaphoreGive(semaphoreData);
  return ret;
}

void EWeLink::setDataUpdated(bool updated) {
  xSemaphoreTake(semaphoreData, portMAX_DELAY);
  dataUpdated = updated;
  xSemaphoreGive(semaphoreData);
}

void EWeLink::login() {
  
  myDebug->println(DEBUG_LEVEL_INFO, "eWeLink login...");
  loggedIn = false;  
  
  String url = String(eWeLinkApiHost) + "/v2/user/login";

  String body = "{";
  body += "\"appid\":\"" + appId + "\",";
  body += "\"email\":\"" + email + "\",";
  body += "\"password\":\"" + ewelinkPass + "\",";
  body += "\"grantType\":\"password\"";
  body += "}";

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(body);

  if (httpCode <= 0) {
    myDebug->println(DEBUG_LEVEL_ERROR, "HTTP POST failed: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return;
  }

  if (httpCode != 200) {
    myDebug->println(DEBUG_LEVEL_ERROR, "eWeLink login failed (HTTP %d)\n", httpCode);
    myDebug->println(DEBUG_LEVEL_DEBUG, http.getString());
    http.end();
    return;
  }

  String response = http.getString();
  http.end();

  myDebug->println(DEBUG_LEVEL_DEBUG2, "Raw response:");
  myDebug->println(DEBUG_LEVEL_DEBUG2, response);

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    myDebug->println(DEBUG_LEVEL_ERROR, "JSON parse error");
    return;
  }

  if (doc["error"].as<int>() == 0) {
    accessToken = doc["data"]["at"].as<String>();
    refreshToken = doc["data"]["rt"].as<String>();
    myDebug->println(DEBUG_LEVEL_DEBUG, "Access Token: %s", accessToken);
    myDebug->println(DEBUG_LEVEL_DEBUG, "Refresh Token: %s", refreshToken);
    mySettings->writeString(PREF_ACCESS_TOKEN, accessToken);
    mySettings->writeString(PREF_REFRESH_TOKEN, refreshToken);
    loggedIn = true;
  } else {
    myDebug->println(DEBUG_LEVEL_ERROR, "Login failed (%d): %s\n", err, doc["msg"].as<const char*>());
  }
}

void EWeLink::checkToken() {
  if (millis() - lastTokenRefresh > EWELINK_TOKEN_REFRESH_INTERVAL) {
      myDebug->println(DEBUG_LEVEL_DEBUG, "Updating eWeLink token...");
      if (doRefreshToken()) {
          lastTokenRefresh = millis();
      }
  }
}

bool EWeLink::doRefreshToken() {
  if (refreshToken.isEmpty()) {
    myDebug->println(DEBUG_LEVEL_ERROR, "No refresh token available!");
    return false;
  }

  String url = String("https://") + eWeLinkApiHost + "/v2/user/refresh";

  String body = "{";
  body += "\"appid\":\"" + appId + "\",";
  body += "\"refreshToken\":\"" + refreshToken + "\"";
  body += "}";

  myDebug->println(DEBUG_LEVEL_INFO, "Refreshing eWeLink token...");
  myDebug->println(DEBUG_LEVEL_DEBUG2, "URL: %s", url);
  myDebug->println(DEBUG_LEVEL_DEBUG2, "BODY: %s", body);

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(body);

  if (httpCode <= 0) {
    myDebug->println(DEBUG_LEVEL_ERROR, "HTTP POST failed: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  String response = http.getString();
  http.end();

  myDebug->println(DEBUG_LEVEL_DEBUG2, "Response:");
  myDebug->println(DEBUG_LEVEL_DEBUG2, response);

  if (httpCode != 200) {
    myDebug->println(DEBUG_LEVEL_ERROR, "HTTP error %d\n", httpCode);
    return false;
  }

  // JSON parsing
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    myDebug->println(DEBUG_LEVEL_ERROR, "JSON parse error: %s\n", err.c_str());
    return false;
  }

  int apiError = doc["error"];
  if (apiError == 0) {
    accessToken  = doc["data"]["at"].as<String>();
    refreshToken = doc["data"]["rt"].as<String>();
    myDebug->println(DEBUG_LEVEL_INFO, "Token refreshed successfully!");
    myDebug->println(DEBUG_LEVEL_DEBUG, "Access Token: %s", accessToken);
    myDebug->println(DEBUG_LEVEL_DEBUG, "Refresh Token: %s", refreshToken);
    return true;
  } else {
    myDebug->println(DEBUG_LEVEL_ERROR, "Token refresh failed (%d): %s\n", apiError, doc["msg"].as<const char*>());
    return false;
  }
}

bool EWeLink::setSwitch(const String& deviceId, bool switchState, bool retryOn401) {

  if (!loggedIn) {
      myDebug->println(DEBUG_LEVEL_ERROR, "Not loggedIn");
      login();
      return false;
  }

  String url = String("https://") + eWeLinkApiHost + "/v2/device/thing/status";
  String state = switchState ? "on" : "off";

  String body = "{";
  body += "\"deviceid\":\"" + deviceId + "\",";
  body += "\"params\":{\"switch\":\"" + state + "\"}";
  body += "}";

  myDebug->println(DEBUG_LEVEL_INFO, "Setting switch state...");
  myDebug->println(DEBUG_LEVEL_DEBUG2, "URL: %s", url);
  myDebug->println(DEBUG_LEVEL_DEBUG2, "BODY: %s", body);

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + accessToken);

  int httpCode = http.POST(body);

  if (httpCode <= 0) {
    myDebug->println(DEBUG_LEVEL_ERROR, "HTTP POST failed: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  String response = http.getString();
  http.end();

  myDebug->println(DEBUG_LEVEL_DEBUG2, "Response:");
  myDebug->println(DEBUG_LEVEL_DEBUG2, response);

  // -expired token (401 Unauthorized) ---
  if (httpCode == 401 && retryOn401) {
    myDebug->println(DEBUG_LEVEL_INFO, "Unauthorized (401), refreshing token...");
    if (doRefreshToken()) {
      delay(500);
      return setSwitch(deviceId, switchState, false);  // retry once
    } else {
      myDebug->println(DEBUG_LEVEL_ERROR, "Token refresh failed, cannot retry.");
      return false;
    }
  }

  if (httpCode != 200) {
    myDebug->println(DEBUG_LEVEL_ERROR, "HTTP error %d\n", httpCode);
    return false;
  }

  // --- JSON parsing ---
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    myDebug->println(DEBUG_LEVEL_ERROR, "JSON parse error: %s\n", err.c_str());
    return false;
  }

  int apiError = doc["error"];
  if (apiError == 0) {
    myDebug->println(DEBUG_LEVEL_INFO, "Switch %s → %s\n", deviceId.c_str(), state.c_str());
    return true;
  } else {
    myDebug->println(DEBUG_LEVEL_ERROR, "Failed to set switch (%d): %s\n", apiError, doc["msg"].as<const char*>());
    return false;
  }
}

bool EWeLink::fetchData() {
  myDebug->println(DEBUG_LEVEL_DEBUG, "Updating eWeLink data...");
  dataUpdated = false;

  if (!loggedIn) {
      myDebug->println(DEBUG_LEVEL_ERROR, "Not loggedIn");
      login();
      return false;
  }

  String url = String("https://") + eWeLinkApiHost + "/v2/device/thing";

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer: %s", accessToken);

  int httpCode = http.GET();

  if (httpCode <= 0) {
    myDebug->println(DEBUG_LEVEL_ERROR, "HTTP GET failed: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  String response = http.getString();
  http.end();

  myDebug->println(DEBUG_LEVEL_DEBUG2, "Response:");
  myDebug->println(DEBUG_LEVEL_DEBUG2, response);

  // token expire
  if (httpCode == 401) {
    myDebug->println(DEBUG_LEVEL_INFO, "Unauthorized (401) — refreshing token...");
    doRefreshToken();
    return false;
  }

  if (httpCode != 200) {
    myDebug->println(DEBUG_LEVEL_ERROR, "HTTP error %d\n", httpCode);
    return false;
  }

  // --- JSON parsing ---
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    myDebug->println(DEBUG_LEVEL_ERROR, "JSON parse error!");
    return false;
  }

  int err = doc["error"];
  if (err != 0) {
    myDebug->println(DEBUG_LEVEL_ERROR, "Device list error: %d\n", err);
    return false;
  }

  JsonArray data = doc["data"].as<JsonArray>();
  myDebug->println(DEBUG_LEVEL_DEBUG, "Found %d devices:\n", data.size());

  for (JsonObject item : data) {
    JsonObject dev = item["itemData"];
    String id   = dev["deviceid"] | "";
    String name = dev["name"] | "";

    myDebug->println(DEBUG_LEVEL_DEBUG, "  • %s (%s)\n", name.c_str(), id.c_str());

    if (dev["params"].containsKey("temperature")) {
      float t = dev["params"]["temperature"] | NAN;
      float h = dev["params"]["humidity"] | NAN;
      myDebug->println(DEBUG_LEVEL_DEBUG, "     Temp: %.1f°C  Humidity: %.0f%%\n", t, h);
    } else if (dev["params"].containsKey("switch")) {
      const char* state = dev["params"]["switch"];
      myDebug->println(DEBUG_LEVEL_DEBUG, "     * Switch: %s\n", state);
    }    
  }

  dataUpdated = true;
  myDebug->println(DEBUG_LEVEL_INFO, "eWeLink data updated successfully!");
  return true;
}

#endif