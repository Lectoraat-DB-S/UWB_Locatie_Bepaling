#ifndef ANCHOR_H
#define ANCHOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Function prototypes - keep the existing interface for compatibility with main.cpp
void anchor_setup();
void anchor_loop();

// RTOS task functions
void anchor_uwb_task(void *pvParameters);
void anchor_wifi_task(void *pvParameters);

// Internal helper functions
void anchor_initUWB();
void anchor_initWiFi();
void anchor_processUwbResponse();
void anchor_sendUwbPollMessage();
void anchor_pollForUwbMessage();

#endif // ANCHOR_H