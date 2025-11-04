#include <Arduino.h>

#include "defines.h"

#ifdef USE_EWELINK
#include "MyDebug.h"
#include "vars.h"
#include "eWeLink.h"

extern void ensureWiFiConnected(bool forceCheck);

void eWeLink_task(void *pvParameters) {

  vTaskSuspend(NULL);

  myDebug->print(DEBUG_LEVEL_INFO, "Http manager: Task running on core %d", xPortGetCoreID());

  int errors = 0;

  while (1) {
    myWiFi->ensureConnection();
    if (myWiFi->isConnected()) {    
      // Refresh token every 24 hours
      eWeLink->checkToken();        
      if (eWeLink->fetchData()) {
        myDebug->println(DEBUG_LEVEL_DEBUG, "eWeLink: data updated");
        vTaskDelay(DELAY_EWELINK_TASK / portTICK_PERIOD_MS);
      } else {
        myDebug->println(DEBUG_LEVEL_ERROR, "eWeLink: error updating data");
        vTaskDelay(DELAY_EWELINK_SHORT_TASK / portTICK_PERIOD_MS);
      }
    } else {
      myDebug->println(DEBUG_LEVEL_DEBUG, "No internet connection");
      vTaskDelay(DELAY_WIFI_RECONNECT_TASK / portTICK_PERIOD_MS);
    }
  }
}
#endif