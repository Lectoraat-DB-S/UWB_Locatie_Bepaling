#ifndef TAG_RTOS_H
#define TAG_RTOS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

// Main setup function
void tag_setup();
void tag_loop();

// External references for shared resources
extern SemaphoreHandle_t anchorDataSemaphore;
extern QueueHandle_t distanceQueue;
extern QueueHandle_t serverQueue;
extern QueueHandle_t modeQueue;
extern TimerHandle_t discoveryTimer;
extern TimerHandle_t serverReportTimer;

#endif // TAG_RTOS_H