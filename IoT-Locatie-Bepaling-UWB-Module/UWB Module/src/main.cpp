#include "tag.h"
#include "anchor.h"
#include "device.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "arduino.h"

#define UWB_TASK_PRIORITY       5
#define UWB_TASK_STACK_SIZE     4096

// Task handles
TaskHandle_t tagTaskHandle = NULL;
TaskHandle_t anchorTaskHandle = NULL;

// Semaphores for UWB and WiFi resources
SemaphoreHandle_t uwbSemaphore = NULL;
SemaphoreHandle_t wifiSemaphore = NULL;

// Task functions for tag and anchor operation
void tagTask(void *pvParameters);
void anchorTask(void *pvParameters);

// Shared flag to indicate device role
static bool is_Tag = false;

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  Serial.println("UWB Module starting with RTOS...");
  
  // Create semaphores for resource protection
  uwbSemaphore = xSemaphoreCreateMutex();
  wifiSemaphore = xSemaphoreCreateMutex();
  
  if (uwbSemaphore == NULL || wifiSemaphore == NULL) {
    Serial.println("Error: Failed to create semaphores");
    while(1); // Stop execution if semaphores can't be created
  }
  
  // Determine if device is tag or anchor
  is_Tag = isTag();
  
  // Create the appropriate task based on device role
  // Update the task creation in setup()
  if (is_Tag) {
    Serial.println("Creating TAG task");
    xTaskCreatePinnedToCore(
      tagTask,          // Task function
      "TagTask",        // Task name
      UWB_TASK_STACK_SIZE, // Use the constant from tag.cpp (4096)
      NULL,             // Parameters 
      UWB_TASK_PRIORITY,// Use higher priority from tag.cpp
      &tagTaskHandle,   // Task handle
      1                 // Core (1 = Arduino loop core)
    );
  } else {
    Serial.println("Creating ANCHOR task");
    xTaskCreatePinnedToCore(
      anchorTask,       // Task function
      "AnchorTask",     // Task name
      8192,             // Stack size (bytes)
      NULL,             // Parameters
      1,                // Priority
      &anchorTaskHandle,// Task handle
      1                 // Core (1 = Arduino loop core)
    );
  }
  
  // Delete the setup and loop task
  vTaskDelete(NULL);
}

void loop() {
  // Empty - not used with FreeRTOS
}

// Tag task implementation
void tagTask(void *pvParameters) {
  // Initialize tag and create all required subtasks
  tag_setup();

  while(1) {
    // Main loop for tag operation
    //tag_loop();
  }
  
  // The tag_setup() function now creates all necessary tasks
  // and the main tag task can terminate
  vTaskDelete(NULL);
}

// Anchor task implementation
void anchorTask(void *pvParameters) {
  // Initialize anchor hardware and connections
  anchor_setup();
  
  // Task loop
  while (1) {
    anchor_loop();
    
    // Allow other tasks to run
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}