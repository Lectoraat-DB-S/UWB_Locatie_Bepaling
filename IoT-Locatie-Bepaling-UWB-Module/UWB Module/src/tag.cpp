#include "dw3000.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include "config.h"
#include <vector>

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

// Configuration constants
#define MIN_ANCHORS_NEEDED 3 // Minimum number of anchors needed for positioning
#define MAX_ANCHORS 10       // Maximum number of anchors to track

/* ----------------------------------------- */

// DW3000 pin configuration
#define DW3000_PIN_RST 27
#define DW3000_PIN_IRQ 34
#define DW3000_PIN_SS 4

// UDP configuration
#define UDP_BUFFER_SIZE 255
#define UDP_PORT 8000
#define UDP_SERVER_DISCOVERY_TIMEOUT_MS 30000
#define UDP_LOOP_DELAY_MS 100

// UWB timing and protocol constants
#define UWB_RANGE_DELAY_MS 18
#define UWB_TX_ANTENNA_DELAY 16385
#define UWB_RX_ANTENNA_DELAY 16385
#define UWB_MSG_COMMON_LENGTH 10
#define UWB_MSG_SEQUENCE_IDX 2
#define UWB_RESP_MSG_POLL_RX_TS_IDX 10
#define UWB_RESP_MSG_RESP_TX_TS_IDX 14
#define UWB_RESP_MSG_TIMESTAMP_LEN 4
#define UWB_POLL_TX_TO_RESP_RX_DELAY_US 540
#define UWB_RESP_RX_TIMEOUT_US 5000
#define UWB_STARTUP_DELAY_MS 2
#define WIFI_CONNECTION_RETRY_DELAY_MS 500
#define UWB_TARGET_MAC_IDX 18 // Position to store target MAC in poll message

// FreeRTOS task configuration
#define UWB_TASK_PRIORITY 5
#define ANCHOR_TASK_PRIORITY 4
#define NETWORK_TASK_PRIORITY 3
#define UWB_TASK_STACK_SIZE 4096
#define ANCHOR_TASK_STACK_SIZE 4096
#define NETWORK_TASK_STACK_SIZE 4096

// Prototypes
void tag_setup();
void tag_loop();
void getServerIPFromBroadcast();
void assertConditionBlocking(bool condition, const char *message);
void tag_initUWB();
bool waitForUwbResponse();
void tag_processUwbResponse();
void computeDistanceFromTimeOfFlight(uint32_t poll_tx_ts, uint32_t resp_rx_ts, uint32_t poll_rx_ts, uint32_t resp_tx_ts);
void updateAnchorData(String mac_adr, int distance);
void logAnchorsData();
void sendDataToServer();
void sendErrorToServer();
void handleUwbError();
int calculateHorizontalDistance(int direct_distance);
void initUDPNormal();
void sendOnConnectMessageToServer();
String getHexStr(uint8_t *data, int length);
void cleanupStaleAnchors();
void processUwbResponseTargeted(String expectedMac);
void sendUwbPollMessageTargeted(String targetMac);
void sendUwbPollMessageBroadcast();
void hexStrToBytes(String hexStr, uint8_t *bytes, int length);
uint8_t charToNibble(char c);
void resetTxFrame();
String getShortMacFromFull(String fullMac);
void shortMacToBytes(String shortMac, uint8_t *bytes);
// Timer periods
const TickType_t DISCOVERY_PERIOD_TICKS = pdMS_TO_TICKS(15000); // 15 seconds
const TickType_t ANCHOR_TIMEOUT_TICKS = pdMS_TO_TICKS(60000);   // 60 seconds
const TickType_t SERVER_REPORT_TICKS = pdMS_TO_TICKS(2000);     // 2 seconds

// Queue sizes and message types
#define DISTANCE_QUEUE_SIZE 10
#define SERVER_QUEUE_SIZE 5
#define MODE_QUEUE_SIZE 3

// Message types
enum ServerMsgType
{
  MSG_DATA,
  MSG_ERROR,
  MSG_CONNECT
};

enum ModeType
{
  MODE_DISCOVERY,
  MODE_TARGETED
};

// Message structures for queues
typedef struct
{
  String macAddress;
  int distance;
  TickType_t timestamp;
} DistanceMeasurement_t;

typedef struct
{
  ServerMsgType type;
  String data;
} ServerMessage_t;

typedef struct
{
  ModeType mode;
  String targetMac;
} ModeMessage_t;

/* ----------------------------------------- */
// Global variables
WiFiUDP udp;
String serverIP = ""; // will be set by the broadcast message

/* Default communication configuration. We use default non-STS DW mode. */
static dwt_config_t config = {
    CHANNEL_NUM,          /* Channel number. */
    DWT_PLEN_128,         /* Preamble length. Used in TX only. */
    DWT_PAC8,             /* Preamble acquisition chunk size. Used in RX only. */
    TX_RX_PREAMBLE,       /* TX preamble code. Used in TX only. */
    TX_RX_PREAMBLE,       /* RX preamble code. Used in RX only. */
    SFD_SYMBOL_NON_STD_8, /* use non-standard 8 symbol */
    DWT_BR_6M8,           /* Data rate. */
    DWT_PHRMODE_STD,      /* PHY header mode. */
    DWT_PHRRATE_STD,      /* PHY header rate. */
    SFD_TIMEOUT,          /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
    DWT_STS_MODE_OFF,     /* STS disabled */
    DWT_STS_LEN_64,       /* STS length see allowed values in Enum dwt_sts_lengths_e */
    DWT_PDOA_M0           /* PDOA mode off */
};

static uint8_t tx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0xE0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t rx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0xE1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t frame_seq_nb = 0;
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint32_t status_reg = 0;
static double tof;
static int distance;
static String macAddress = WiFi.macAddress();
unsigned long previousMillis = 0; // Globale variabele om tijdsstempel op te slaan
const long interval = 2000;       // Interval van 2 seconden

extern dwt_txconfig_t txconfig_options;

// Define anchor struct
struct IDDistance
{
  String id = "";
  int distance = -1;

  // Default constructor
  IDDistance() : id(""), distance(-1) {}

  // Constructor with parameters
  IDDistance(String _id, int _dist) : id(_id), distance(_dist) {}

  void reset()
  {
    id = "";
    distance = -1;
  }
};

struct AnchorTracking
{
  String id;
  TickType_t lastSeenTime;

  AnchorTracking(String _id) : id(_id), lastSeenTime(xTaskGetTickCount()) {}
};

// FreeRTOS handles
extern TaskHandle_t uwbTaskHandle;
extern TaskHandle_t anchorTaskHandle;
TaskHandle_t networkTaskHandle = NULL;

// Queues
QueueHandle_t distanceQueue = NULL;
QueueHandle_t serverQueue = NULL;
QueueHandle_t modeQueue = NULL;

// Semaphores
extern SemaphoreHandle_t uwbSemaphore;
extern SemaphoreHandle_t wifiSemaphore;
SemaphoreHandle_t anchorDataSemaphore = NULL;

// Timers
TimerHandle_t discoveryTimer = NULL;
TimerHandle_t serverReportTimer = NULL;

// Add these at the top of main.cpp, before setup()
// External references to resources created in tag.cpp
extern SemaphoreHandle_t anchorDataSemaphore;
extern QueueHandle_t distanceQueue;
extern QueueHandle_t serverQueue;
extern QueueHandle_t modeQueue;
extern TimerHandle_t discoveryTimer;
extern TimerHandle_t serverReportTimer;

// Protected data structures
std::vector<String> knownAnchorAddresses;
std::vector<AnchorTracking> knownAnchors;
std::vector<IDDistance> anchors;

static uint8_t id_count = 0;
size_t currentAnchorIndex = 0;
const int MAX_ANCHOR_RETRIES = 3;
int currentRetryCount = 0;
bool discoveryMode = true;

/* ----------------------------------------- */
// Main setup function
void tag_setup();

// Timer Callbacks
void discoveryTimerCallback(TimerHandle_t xTimer);
void serverReportTimerCallback(TimerHandle_t xTimer);

// Task Functions
void uwbTask(void *pvParameters);
void tagAnchorTask(void *pvParameters);
void networkTask(void *pvParameters);

// UWB Communication Functions
bool waitForUwbResponse();
void processUwbResponseTargeted(String expectedMac);
void sendUwbPollMessageBroadcast();
void sendUwbPollMessageTargeted(String targetMac);
void tag_initUWB();
void handleUwbError();

// Anchor Management Functions
void processDistanceMeasurement(String mac_adr, int distance);
void cleanupStaleAnchors();

// Server Communication Functions
void getServerIPFromBroadcast();
void sendDataToServer();
void sendErrorToServer();

// Utility Functions
void logAnchorsData();
int calculateHorizontalDistance(int direct_distance);
String getHexStr(uint8_t *data, int length);
void resetTxFrame();
void hexStrToBytes(String hexStr, uint8_t *bytes, int length);
uint8_t charToNibble(char c);
void assertConditionBlocking(bool condition, const char *message);

/* ----------------------------------------- */
void tag_setup()
{
  digitalWrite(LED_BUILTIN, HIGH); // Turn on LED for setup

  // Initialize serial
  Serial.begin(115200);
  Serial.println("STARTING TAG WITH FREERTOS...");

  // Create semaphores
  uwbSemaphore = xSemaphoreCreateMutex();
  wifiSemaphore = xSemaphoreCreateMutex();
  anchorDataSemaphore = xSemaphoreCreateMutex();

  // Create queues
  distanceQueue = xQueueCreate(DISTANCE_QUEUE_SIZE, sizeof(DistanceMeasurement_t));
  serverQueue = xQueueCreate(SERVER_QUEUE_SIZE, sizeof(ServerMessage_t));
  modeQueue = xQueueCreate(MODE_QUEUE_SIZE, sizeof(ModeMessage_t));

  // Create timers
  discoveryTimer = xTimerCreate(
      "DiscoveryTimer",
      DISCOVERY_PERIOD_TICKS,
      pdTRUE, // Auto reload
      (void *)0,
      discoveryTimerCallback);

  serverReportTimer = xTimerCreate(
      "ServerReportTimer",
      SERVER_REPORT_TICKS,
      pdTRUE, // Auto reload
      (void *)0,
      serverReportTimerCallback);

  // Reserve space for vectors
  if (xSemaphoreTake(anchorDataSemaphore, portMAX_DELAY))
  {
    anchors.reserve(MAX_ANCHORS);
    xSemaphoreGive(anchorDataSemaphore);
  }

  // Take semaphore first
  if (xSemaphoreTake(wifiSemaphore, portMAX_DELAY))
  {
    Serial.println("Connecting to WiFi...");

    // Initialize the WiFi connection with SSID and password
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
      vTaskDelay(pdMS_TO_TICKS(WIFI_CONNECTION_RETRY_DELAY_MS));
      Serial.print(".");
    }

    Serial.println();
    Serial.print("Connected to WiFi. IP address: ");
    Serial.println(WiFi.localIP());

    Serial.print("MAC address: ");
    macAddress = WiFi.macAddress();
    Serial.println(macAddress);

    xSemaphoreGive(wifiSemaphore);
  }

  // Get server IP address
  getServerIPFromBroadcast();

  // Initialize UWB hardware
  if (serverIP != "")
  {
    tag_initUWB();

    // Create tasks
    xTaskCreate(
        uwbTask,
        "UWBTask",
        UWB_TASK_STACK_SIZE,
        NULL,
        UWB_TASK_PRIORITY,
        &uwbTaskHandle);

    xTaskCreate(
        tagAnchorTask, // Changed from anchorTask
        "AnchorTask",
        ANCHOR_TASK_STACK_SIZE,
        NULL,
        ANCHOR_TASK_PRIORITY,
        &anchorTaskHandle);

    xTaskCreate(
        networkTask,
        "NetworkTask",
        NETWORK_TASK_STACK_SIZE,
        NULL,
        NETWORK_TASK_PRIORITY,
        &networkTaskHandle);

    // Start timers
    xTimerStart(discoveryTimer, 0);
    xTimerStart(serverReportTimer, 0);

    // Send initial connection message
    ServerMessage_t connectMsg;
    connectMsg.type = MSG_CONNECT;
    xQueueSend(serverQueue, &connectMsg, 0);

    digitalWrite(LED_BUILTIN, LOW); // Turn off LED to signal setup completion
  }
  else
  {
    Serial.println("No server found, halting.");
    digitalWrite(LED_BUILTIN, LOW);
    vTaskDelay(portMAX_DELAY); // Effectively halt the system
  }
}

void tag_loop()
{
}
/* ----------------------------------------- */
// Timer Callbacks
void discoveryTimerCallback(TimerHandle_t xTimer)
{
  ModeMessage_t modeMsg;
  modeMsg.mode = MODE_DISCOVERY;
  modeMsg.targetMac = "";

  // Send message to switch to discovery mode
  xQueueSend(modeQueue, &modeMsg, 0);
  Serial.println("Timer: Switching to discovery mode");
}

void serverReportTimerCallback(TimerHandle_t xTimer)
{
  // If we don't have enough data, send error report
  if (xSemaphoreTake(anchorDataSemaphore, pdMS_TO_TICKS(10)))
  {
    if (anchors.size() < MIN_ANCHORS_NEEDED)
    {
      ServerMessage_t errorMsg;
      errorMsg.type = MSG_ERROR;
      xQueueSend(serverQueue, &errorMsg, 0);
    }
    xSemaphoreGive(anchorDataSemaphore);
  }
}

/* ----------------------------------------- */
// UWB Communication Task
void uwbTask(void *pvParameters)
{
  // Task should run forever in a loop
  for (;;)
  {
    unsigned long currentTime = millis();

    // Clean up any stale anchors
    cleanupStaleAnchors();

    // New approach: If no anchors found yet, broadcast until we find one
    if (knownAnchors.size() == 0)
    {
      Serial.println("Searching for first anchor via broadcast");
      sendUwbPollMessageBroadcast();

      if (waitForUwbResponse())
      {
        tag_processUwbResponse();
        // If we found our first anchor, start alternating pattern
        if (knownAnchors.size() > 0)
        {
          discoveryMode = true; // Start with broadcast in alternating pattern
          currentAnchorIndex = 0;
          Serial.println("First anchor found! Switching to alternating pattern");
        }
      }
      else
      {
        handleUwbError();
      }
    }
    // Alternating pattern once we have at least one anchor
    else
    {
      if (discoveryMode)
      {
        // Broadcast mode to find new anchors
        sendUwbPollMessageBroadcast();

        if (waitForUwbResponse())
        {
          tag_processUwbResponse();
        }
        else
        {
          handleUwbError();
        }

        // After broadcasting, switch to targeted mode
          currentAnchorIndex = 0;
          discoveryMode = false;
          Serial.println("Switching to targeted mode");
      }
      else
      {
        // Targeted mode - check for mode message from queue
        ModeMessage_t modeMsg;

        // Check if there's a new mode/target message (non-blocking)
        if (xQueueReceive(modeQueue, &modeMsg, 0) == pdTRUE)
        {
          // Update mode based on message
          if (modeMsg.mode == MODE_DISCOVERY)
          {
            discoveryMode = true;
            Serial.println("Switching to discovery mode based on queue message");
          }
          else
          {
            // We have a targeted mode message - target the specified anchor
            discoveryMode = false;
            String targetAnchor = modeMsg.targetMac;
            Serial.println("Targeting anchor from queue: " + targetAnchor);

            // Send targeted poll message
            sendUwbPollMessageTargeted(targetAnchor);

            if (waitForUwbResponse())
            {
              processUwbResponseTargeted(targetAnchor);
            }
            else
            {
              handleUwbError();
            }
          }
        }
        // If no new message, continue with current target
        else if (currentAnchorIndex < knownAnchors.size())
        {
          String targetAnchor = knownAnchors[currentAnchorIndex].id;
          Serial.println("Targeting known anchor: " + targetAnchor);

          sendUwbPollMessageTargeted(targetAnchor);

          if (waitForUwbResponse())
          {
            processUwbResponseTargeted(targetAnchor);
          }
          else
          {
            handleUwbError();
          }
        }
        else
        {
          // Revert back to broadcast
          discoveryMode = true;
          Serial.println("No anchors to target, switching to discovery mode");
        }

        // Move to next anchor or back to broadcast
        currentAnchorIndex++;
        if (currentAnchorIndex >= knownAnchors.size())
        {
          // Completed all known anchors, switch back to broadcast for discovery
          discoveryMode = true;
          Serial.println("Completed targeting all known anchors, switching back to discovery");
        }
      }

      // If we have enough data, send it periodically
      if (currentTime - previousMillis >= interval)
      {
        previousMillis = currentTime;

        if (anchors.size() >= MIN_ANCHORS_NEEDED)
        {
          sendDataToServer();
        }
      }
    }

    // Critical: Allow other tasks to run by adding a delay
    vTaskDelay(pdMS_TO_TICKS(UWB_RANGE_DELAY_MS));
  }
}

/* ----------------------------------------- */
// Anchor Management Task
void tagAnchorTask(void *pvParameters)
{
  DistanceMeasurement_t distanceMeasurement;
  bool cycleCompleted = false;

  for (;;)
  {
    // Process any new distance measurements from the queue
    if (xQueueReceive(distanceQueue, &distanceMeasurement, pdMS_TO_TICKS(10)) == pdTRUE)
    {
      // Process new measurement with thread safety
      if (xSemaphoreTake(anchorDataSemaphore, pdMS_TO_TICKS(50)) == pdTRUE)
      {
        processDistanceMeasurement(distanceMeasurement.macAddress, distanceMeasurement.distance);
        xSemaphoreGive(anchorDataSemaphore);
      }
    }

    // Handle target selection for UWB task in targeted mode
    if (xSemaphoreTake(anchorDataSemaphore, pdMS_TO_TICKS(50)) == pdTRUE)
    {
      // Clean up stale anchors periodically
      cleanupStaleAnchors();

      // If we have known anchors, select the next one to target
      if (knownAnchors.size() > 0)
      {

        // Check if we completed a cycle
        if (currentAnchorIndex >= knownAnchors.size())
        {
          currentAnchorIndex = 0;
          cycleCompleted = true;

          // After completing a full cycle, check if we have enough data to report
          if (anchors.size() >= MIN_ANCHORS_NEEDED)
          {
            ServerMessage_t dataMsg;
            dataMsg.type = MSG_DATA;
            xQueueSend(serverQueue, &dataMsg, 0);
            Serial.println("Sending data to server");
          }
        }

        // Send message to UWB task with next target
        ModeMessage_t targetMsg;
        targetMsg.mode = MODE_TARGETED;
        targetMsg.targetMac = knownAnchors[currentAnchorIndex].id;

        xQueueSend(modeQueue, &targetMsg, 0);

        // Increment for next time
        currentAnchorIndex = (currentAnchorIndex + 1) % knownAnchors.size();

        // After completing a full cycle, check if we have enough data to report
        if (cycleCompleted && anchors.size() >= MIN_ANCHORS_NEEDED)
        {
          ServerMessage_t dataMsg;
          dataMsg.type = MSG_DATA;
          xQueueSend(serverQueue, &dataMsg, 0);
        }
      }
      else if (knownAnchors.size() == 0)
      {
        // No known anchors, request discovery mode
        ModeMessage_t discoveryMsg;
        discoveryMsg.mode = MODE_DISCOVERY;
        xQueueSend(modeQueue, &discoveryMsg, 0);
      }

      xSemaphoreGive(anchorDataSemaphore);
    }

    // Allow other tasks to run
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/* ----------------------------------------- */
// Network Communication Task
void networkTask(void *pvParameters)
{
  ServerMessage_t serverMsg;

  // Initialize UDP
  if (xSemaphoreTake(wifiSemaphore, portMAX_DELAY))
  {
    udp.begin(UDP_PORT);
    xSemaphoreGive(wifiSemaphore);
  }

  for (;;)
  {
    // Check for messages to send to server
    if (xQueueReceive(serverQueue, &serverMsg, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      if (xSemaphoreTake(wifiSemaphore, pdMS_TO_TICKS(100)) == pdTRUE)
      {
        // Wake up WiFi for transmission
        WiFi.setSleep(false);

        switch (serverMsg.type)
        {
        case MSG_DATA:
          sendDataToServer();
          break;
        case MSG_ERROR:
          sendErrorToServer();
          break;
        case MSG_CONNECT:
          // Send a message to the server when connected
          udp.beginPacket(serverIP.c_str(), UDP_PORT);
          udp.print("Connected to server: " + serverIP);
          udp.endPacket();
          break;
        }

        // Put WiFi back to sleep to save power
        WiFi.setSleep(true);
        xSemaphoreGive(wifiSemaphore);
      }
    }

    // Allow other tasks to run
    vTaskDelay(pdMS_TO_TICKS(UDP_LOOP_DELAY_MS));
  }
}

// Original broadcast poll message
void sendUwbPollMessageBroadcast()
{
  // Update sequence number in poll message
  tx_poll_msg[UWB_MSG_SEQUENCE_IDX] = frame_seq_nb;

  // Clear transmission status
  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);

  // Extract last 3 bytes of our MAC address
  uint8_t shortMac[SHORT_MAC_LENGTH];
  String mac = macAddress;
  for (int i = 0; i < SHORT_MAC_LENGTH; i++)
  {
    int bytePos = SHORT_MAC_LENGTH + i;
    String byteStr = mac.substring(bytePos * 3, bytePos * 3 + 2);
    shortMac[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
  }

  // Add source MAC (last 3 bytes) to poll message
  memcpy(&tx_poll_msg[SOURCE_MAC_IDX], shortMac, SHORT_MAC_LENGTH);

  // Add target MAC of all zeros (broadcast)
  uint8_t zeroMac[SHORT_MAC_LENGTH] = {0, 0, 0};
  memcpy(&tx_poll_msg[TARGET_MAC_IDX], zeroMac, SHORT_MAC_LENGTH);

  // Write frame data to DW IC and prepare transmission
  dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0);
  dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1);

  Serial.println("Sending broadcast poll message: ");
  for (size_t i = 0; i < sizeof(tx_poll_msg); i++)
  {
    Serial.print(tx_poll_msg[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Start transmission with response expected
  dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

  // Increment frame sequence number for next transmission
  frame_seq_nb++;

  resetTxFrame(); // Reset the tx_poll_msg for next transmission
}

/* ----------------------------------------- */
// UWB and distance calculation functions
bool waitForUwbResponse()
{
  unsigned long startTime = micros();
  bool receivedAny = false;

  // Loop until timeout to collect multiple responses
  while (micros() - startTime < UWB_RESP_RX_TIMEOUT_US)
  {
    status_reg = dwt_read32bitreg(SYS_STATUS_ID);

    if (status_reg & SYS_STATUS_RXFCG_BIT_MASK)
    {
      // Process response
      uint32_t frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;

      if (frame_len <= sizeof(rx_buffer))
      {
        dwt_readrxdata(rx_buffer, frame_len, 0);

        // Check that the frame is the expected response
        rx_buffer[UWB_MSG_SEQUENCE_IDX] = 0; // Clear sequence number for comparison
        if (memcmp(rx_buffer, rx_resp_msg, UWB_MSG_COMMON_LENGTH) == 0)
        {
          uint32_t poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;

          // Retrieve timestamps
          poll_tx_ts = dwt_readtxtimestamplo32();
          resp_rx_ts = dwt_readrxtimestamplo32();

          // Get timestamps embedded in response message
          resp_msg_get_ts(&rx_buffer[UWB_RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
          resp_msg_get_ts(&rx_buffer[UWB_RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

          // Compute time of flight and distance
          float clockOffsetRatio = ((float)dwt_readclockoffset()) / (uint32_t)(1 << TOF_OFFSET);
          int32_t rtd_init = resp_rx_ts - poll_tx_ts;
          int32_t rtd_resp = resp_tx_ts - poll_rx_ts;

          tof = ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
          distance = tof * SPEED_OF_LIGHT * METER_TO_CENTI;

          // Calculate horizontal distance
          distance = calculateHorizontalDistance(distance);

          // Get anchor MAC address
          String mac_adr = getHexStr(&rx_buffer[RX_PLACE], SHORT_MAC_LENGTH);

          // Create distance measurement and send to anchor task
          DistanceMeasurement_t measurement;
          measurement.macAddress = mac_adr;
          measurement.distance = distance;
          measurement.timestamp = xTaskGetTickCount();

          xQueueSend(distanceQueue, &measurement, 0);

          receivedAny = true;
        }
      }

      // Clear reception status and continue waiting
      dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);
      dwt_rxenable(DWT_START_RX_IMMEDIATE);
    }
  }

  return receivedAny;
}

void processUwbResponseTargeted(String expectedMac)
{
  uint32_t frame_len;

  // Clear good RX frame event in the DW IC status register
  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);

  // Read received frame into local buffer
  frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;
  if (frame_len <= sizeof(rx_buffer))
  {
    dwt_readrxdata(rx_buffer, frame_len, 0);

    // Check that the frame is the expected response
    rx_buffer[UWB_MSG_SEQUENCE_IDX] = 0; // Clear sequence number for comparison
    if (memcmp(rx_buffer, rx_resp_msg, UWB_MSG_COMMON_LENGTH) == 0)
    {
      // Get anchor MAC address - use SHORT_MAC_LENGTH instead of MAC_LENGTH for comparison
      String mac_adr = getHexStr(&rx_buffer[RX_PLACE], SHORT_MAC_LENGTH);

      // Verify this response is from the anchor we targeted
      if (mac_adr == expectedMac)
      {
        // Take the semaphore before updating shared data
        if (xSemaphoreTake(anchorDataSemaphore, pdMS_TO_TICKS(50)) == pdTRUE)
        {
          // Update the lastSeenTime for this anchor
          for (size_t j = 0; j < knownAnchors.size(); j++)
          {
            if (knownAnchors[j].id == mac_adr)
            {
              knownAnchors[j].lastSeenTime = xTaskGetTickCount();
              break;
            }
          }
          // Now it's safe to give the semaphore
          xSemaphoreGive(anchorDataSemaphore);
        }

        uint32_t poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;

        // Retrieve timestamps
        poll_tx_ts = dwt_readtxtimestamplo32();
        resp_rx_ts = dwt_readrxtimestamplo32();

        // Get timestamps embedded in response message
        resp_msg_get_ts(&rx_buffer[UWB_RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
        resp_msg_get_ts(&rx_buffer[UWB_RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

        // Compute distance from time of flight
        float clockOffsetRatio = ((float)dwt_readclockoffset()) / (uint32_t)(1 << TOF_OFFSET);
        int32_t rtd_init = resp_rx_ts - poll_tx_ts;
        int32_t rtd_resp = resp_tx_ts - poll_rx_ts;

        tof = ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
        distance = tof * SPEED_OF_LIGHT * METER_TO_CENTI;

        // Calculate horizontal distance
        distance = calculateHorizontalDistance(distance);

        // Create distance measurement and send to anchor task
        DistanceMeasurement_t measurement;
        measurement.macAddress = mac_adr;
        measurement.distance = distance;
        measurement.timestamp = xTaskGetTickCount();

        xQueueSend(distanceQueue, &measurement, 0);
      }
      else
      {
        Serial.println("Received response from unexpected anchor: ");
      }
    }
  }
}

void processDistanceMeasurement(String mac_adr, int distance)
{
  // First check if this anchor already exists in known anchors
  bool anchorKnown = false;

  for (size_t j = 0; j < knownAnchors.size(); j++)
  {
    if (knownAnchors[j].id == mac_adr)
    {
      // Update last seen time
      knownAnchors[j].lastSeenTime = xTaskGetTickCount();
      anchorKnown = true;
      break;
    }
  }

  // If not known, add to known anchors
  if (!anchorKnown)
  {
    knownAnchors.push_back(AnchorTracking(mac_adr));
    Serial.println("New anchor discovered: " + mac_adr);
  }

  // Then update or add to the anchors list
  bool anchorExists = false;
  for (size_t i = 0; i < anchors.size(); i++)
  {
    if (anchors[i].id == mac_adr)
    {
      // Update existing anchor
      anchors[i].distance = distance;
      anchorExists = true;
      break;
    }
  }

  // If anchor doesn't exist and we haven't reached MAX_ANCHORS, add it
  if (!anchorExists && anchors.size() < MAX_ANCHORS)
  {
    anchors.push_back(IDDistance(mac_adr, distance));
  }

  // Log current anchors data
  logAnchorsData();
}

void cleanupStaleAnchors()
{
  TickType_t currentTime = xTaskGetTickCount();

  // Check each anchor to see if it's stale
  for (int i = knownAnchors.size() - 1; i >= 0; i--)
  {
    // Calculate elapsed time
    TickType_t elapsedTime = currentTime - knownAnchors[i].lastSeenTime;

    // Remove stale anchors
    if (elapsedTime > pdMS_TO_TICKS(ANCHOR_TIMEOUT_TICKS))
    {
      Serial.println("Removing stale anchor: " + knownAnchors[i].id +
                     " (not seen for " + (elapsedTime / pdMS_TO_TICKS(1000)) + " seconds)");

      // Remove from known anchors list
      knownAnchors.erase(knownAnchors.begin() + i);

      // Adjust current index if needed
      if (currentAnchorIndex >= knownAnchors.size())
      {
        currentAnchorIndex = 0;
      }
    }
  }
}

/* ----------------------------------------- */
// Server communication functions
void sendDataToServer()
{
  if (xSemaphoreTake(anchorDataSemaphore, pdMS_TO_TICKS(100)))
  {
    // Format the data string
    String dataString = macAddress;

    for (size_t i = 0; i < anchors.size(); i++)
    {
      dataString += ";" + anchors[i].id + ";" + anchors[i].distance;
    }

    // Send data to server
    udp.beginPacket(serverIP.c_str(), UDP_PORT);
    udp.print(dataString);
    udp.endPacket();

    // Clear anchor data after sending
    // anchors.clear();

    xSemaphoreGive(anchorDataSemaphore);
  }
}

void sendErrorToServer()
{
  if (xSemaphoreTake(anchorDataSemaphore, pdMS_TO_TICKS(100)))
  {
    // Send error message when not enough anchors are in range
    String errorMsg = "ERROR: tag " + macAddress +
                      " is not in range of " + MIN_ANCHORS_NEEDED +
                      " anchors! (Only found " + anchors.size() + ")";

    udp.beginPacket(serverIP.c_str(), UDP_PORT);
    udp.print(errorMsg);
    udp.endPacket();

    xSemaphoreGive(anchorDataSemaphore);
  }
}

/* ----------------------------------------- */
// Server discovery
void getServerIPFromBroadcast()
{
  uint8_t buffer[UDP_BUFFER_SIZE];

  if (xSemaphoreTake(wifiSemaphore, portMAX_DELAY))
  {
    // Initialize UDP
    udp.begin(UDP_PORT);

    IPAddress broadcastIP(255, 255, 255, 255);
    Serial.println("Sending discovery broadcast packet");

    // Send a discovery request
    udp.beginPacket(broadcastIP, UDP_PORT);
    udp.print("DISCOVERY_REQUEST");
    udp.endPacket();

    TickType_t startTime = xTaskGetTickCount();
    while (serverIP == "" &&
           (xTaskGetTickCount() - startTime < pdMS_TO_TICKS(UDP_SERVER_DISCOVERY_TIMEOUT_MS)))
    {
      Serial.print(".");
      int packetSize = udp.parsePacket();

      if (packetSize)
      {
        int len = udp.read(buffer, UDP_BUFFER_SIZE);
        if (len > 0)
        {
          buffer[len] = '\0';
          String message = String((char *)buffer);

          if (message.startsWith("Server IP: "))
          {
            serverIP = message.substring(11);
            Serial.println("\nServer IP received: " + serverIP);
            break;
          }
        }
      }
      vTaskDelay(pdMS_TO_TICKS(UDP_LOOP_DELAY_MS));
    }

    if (serverIP == "")
    {
      Serial.println("\nNo server found after timeout.");
    }

    xSemaphoreGive(wifiSemaphore);
  }
}

/* ----------------------------------------- */
// UWB initialization and error handling
void tag_initUWB()
{
  if (xSemaphoreTake(uwbSemaphore, portMAX_DELAY))
  {
    spiBegin(DW3000_PIN_IRQ, DW3000_PIN_RST);
    spiSelect(DW3000_PIN_SS);

    // Time needed for DW3000 to start up
    vTaskDelay(pdMS_TO_TICKS(UWB_STARTUP_DELAY_MS));

    // Need to make sure DW IC is in IDLE_RC before proceeding
    assertConditionBlocking(dwt_checkidlerc(), "IDLE FAILED\r\n");

    // Initialise DW IC
    assertConditionBlocking(dwt_initialise(DWT_DW_INIT) != DWT_ERROR, "INIT FAILED\r\n");

    // Enable LEDs for debug
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    // Configure DW IC
    assertConditionBlocking(dwt_configure(&config) != DWT_ERROR, "CONFIG FAILED\r\n");

    /* Configure the TX spectrum parameters */
    dwt_configuretxrf(&txconfig_options);

    /* Apply default antenna delay values */
    dwt_setrxantennadelay(UWB_RX_ANTENNA_DELAY);
    dwt_settxantennadelay(UWB_TX_ANTENNA_DELAY);

    /* Set expected response's delay and timeout */
    dwt_setrxaftertxdelay(UWB_POLL_TX_TO_RESP_RX_DELAY_US);
    dwt_setrxtimeout(UWB_RESP_RX_TIMEOUT_US);

    /* Enable TX/RX states output on GPIOs */
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);

    Serial.println("UWB initialization complete");

    xSemaphoreGive(uwbSemaphore);
  }
}

void handleUwbError()
{
  // Clear RX error/timeout events in the DW IC status register
  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
}

/* ----------------------------------------- */
// Utility functions
void logAnchorsData()
{
  Serial.println("Number of distinct anchors detected: " + String(anchors.size()));

  String logMessage = "Anchors: ";
  for (size_t i = 0; i < anchors.size(); i++)
  {
    if (i > 0)
      logMessage += ", ";
    logMessage += anchors[i].id + ":" + anchors[i].distance;
  }

  Serial.println(logMessage);
}

int calculateHorizontalDistance(int direct_distance)
{
  // Apply Pythagorean theorem to get horizontal distance
  return direct_distance;
}

String getHexStr(uint8_t *data, int length)
{
  String hexStr = "";
  for (int i = 0; i < length; i++)
  {
    if (data[i] < 0x10)
    {
      hexStr += "0"; // Leading zero
    }
    hexStr += String(data[i], HEX);
  }
  return hexStr;
}

void resetTxFrame()
{
  // Reset tx_poll_msg: index 2 to 0, all 0 from index 10 onward
  for (int i = 2; i < sizeof(tx_poll_msg); i++)
  {
    if (i >= 10)
    {
      tx_poll_msg[i] = 0;
    }
  }
}

// Targeted poll message
void sendUwbPollMessageTargeted(String targetMac)
{
  // Update sequence number in poll message
  tx_poll_msg[UWB_MSG_SEQUENCE_IDX] = frame_seq_nb;

  // Extract last 3 bytes of our MAC address
  uint8_t sourceShortMac[SHORT_MAC_LENGTH];
  String mac = macAddress;
  for (int i = 0; i < SHORT_MAC_LENGTH; i++)
  {
    int bytePos = SHORT_MAC_LENGTH + i;
    String byteStr = mac.substring(bytePos * 3, bytePos * 3 + 2);
    sourceShortMac[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
  }

  // Convert target MAC directly from hex string format to bytes
  uint8_t targetShortMac[SHORT_MAC_LENGTH];
  for (int i = 0; i < SHORT_MAC_LENGTH; i++)
  {
    // Extract each byte (2 hex chars) directly
    String byteStr = targetMac.substring(i * 2, i * 2 + 2);
    targetShortMac[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
  }

  // Add source MAC (last 3 bytes) to poll message
  memcpy(&tx_poll_msg[SOURCE_MAC_IDX], sourceShortMac, SHORT_MAC_LENGTH);

  // Add target MAC (last 3 bytes) to poll message
  memcpy(&tx_poll_msg[TARGET_MAC_IDX], targetShortMac, SHORT_MAC_LENGTH);

  // Clear transmission status
  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);

  // Write frame data to DW IC and prepare transmission
  dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0);
  dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1);

  Serial.println("Sending targeted poll message: ");
  for (size_t i = 0; i < sizeof(tx_poll_msg); i++)
  {
    Serial.print(tx_poll_msg[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Start transmission with response expected
  dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

  // Increment frame sequence number
  frame_seq_nb++;

  resetTxFrame(); // Reset the tx_poll_msg for next transmission
}

// Utility function to convert hex string to bytes
void hexStrToBytes(String hexStr, uint8_t *bytes, int length)
{
  for (int i = 0; i < length && i * 2 + 1 < hexStr.length(); i++)
  {
    bytes[i] = (charToNibble(hexStr[i * 2]) << 4) | charToNibble(hexStr[i * 2 + 1]);
  }
}

uint8_t charToNibble(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return 0;
}

void assertConditionBlocking(bool condition, const char *message)
{
  if (!condition)
  {
    Serial.println(message);
    vTaskDelay(portMAX_DELAY); // Effectively halt the system
  }
}

void tag_processUwbResponse()
{
  uint32_t frame_len;

  // Clear good RX frame event in the DW IC status register
  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);

  // Read received frame into local buffer
  frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;
  if (frame_len <= sizeof(rx_buffer))
  {
    dwt_readrxdata(rx_buffer, frame_len, 0);

    // Check that the frame is the expected response
    rx_buffer[UWB_MSG_SEQUENCE_IDX] = 0; // Clear sequence number for comparison
    if (memcmp(rx_buffer, rx_resp_msg, UWB_MSG_COMMON_LENGTH) == 0)
    {
      uint32_t poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;

      // Extract our short MAC
      uint8_t ourShortMac[SHORT_MAC_LENGTH];
      String mac = macAddress;
      for (int i = 0; i < SHORT_MAC_LENGTH; i++)
      {
        int bytePos = SHORT_MAC_LENGTH + i;
        String byteStr = mac.substring(bytePos * 3, bytePos * 3 + 2);
        ourShortMac[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
      }

      // Extract target MAC from message (should be our short MAC)
      uint8_t targetMac[SHORT_MAC_LENGTH];
      memcpy(targetMac, &rx_buffer[TARGET_MAC_IDX], SHORT_MAC_LENGTH);

      // Check if response is addressed to us
      if (memcmp(targetMac, ourShortMac, SHORT_MAC_LENGTH) == 0)
      {
        // Retrieve timestamps
        poll_tx_ts = dwt_readtxtimestamplo32();
        resp_rx_ts = dwt_readrxtimestamplo32();

        // Get timestamps embedded in response message
        resp_msg_get_ts(&rx_buffer[UWB_RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
        resp_msg_get_ts(&rx_buffer[UWB_RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

        // Compute distance from time of flight
        computeDistanceFromTimeOfFlight(poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts);

        // Get anchor MAC address (source of response) - only the 3 bytes
        uint8_t sourceMac[SHORT_MAC_LENGTH];
        memcpy(sourceMac, &rx_buffer[SOURCE_MAC_IDX], SHORT_MAC_LENGTH);

        // Convert short MAC to hex string format (proper format for storage)
        String mac_adr = "";
        for (int i = 0; i < SHORT_MAC_LENGTH; i++)
        {
          if (sourceMac[i] < 0x10)
          {
            mac_adr += "0";
          }
          mac_adr += String(sourceMac[i], HEX);
        }

        // Verify that the source MAC matches what we expected
        String expectedMac = "";
        if (!discoveryMode && currentAnchorIndex < knownAnchorAddresses.size())
        {
          expectedMac = getShortMacFromFull(knownAnchorAddresses[currentAnchorIndex]);

          if (mac_adr.equalsIgnoreCase(expectedMac))
          {
            // Update anchor data with the shortened MAC
            updateAnchorData(mac_adr, distance);
          }
          else
          {
            Serial.print("Received response from unexpected anchor: ");
            Serial.print(mac_adr);
            Serial.print(" (expected: ");
            Serial.print(expectedMac);
            Serial.println(")");
          }
        }
        else
        {
          // In discovery mode, accept all responses
          updateAnchorData(mac_adr, distance);

          // Add to knownAnchors if not already there
          bool anchorIsKnown = false;
          for (size_t i = 0; i < knownAnchors.size(); i++)
          {
            if (knownAnchors[i].id == mac_adr)
            {
              // Update last seen time if anchor is already known
              knownAnchors[i].lastSeenTime = millis();
              anchorIsKnown = true;
              break;
            }
          }

          // If this is a new anchor, add it to tracking list
          if (!anchorIsKnown)
          {
            Serial.println("New anchor discovered: " + mac_adr);
            knownAnchors.push_back(AnchorTracking(mac_adr));
          }
        }
      }
      else
      {
        Serial.println("Received response not addressed to this tag");
      }
    }
  }
}

void computeDistanceFromTimeOfFlight(uint32_t poll_tx_ts, uint32_t resp_rx_ts, uint32_t poll_rx_ts, uint32_t resp_tx_ts)
{
  float clockOffsetRatio;
  int32_t rtd_init, rtd_resp;

  // Calculate clock offset ratio to correct for differing local and remote clock rates
  clockOffsetRatio = ((float)dwt_readclockoffset()) / (uint32_t)(1 << TOF_OFFSET);

  // Compute round-trip delays
  rtd_init = resp_rx_ts - poll_tx_ts;
  rtd_resp = resp_tx_ts - poll_rx_ts;

  // Compute time of flight and distance
  tof = ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
  distance = tof * SPEED_OF_LIGHT * METER_TO_CENTI;

  // Calculate horizontal distance using Pythagorean theorem
  distance = calculateHorizontalDistance(distance);
}

void updateAnchorData(String mac_adr, int distance)
{
  // First check if this anchor already exists
  bool anchor_exists = false;

  for (size_t i = 0; i < anchors.size(); i++)
  {
    if (anchors[i].id == mac_adr)
    {
      // Update existing anchor
      anchors[i].distance = distance;
      anchor_exists = true;
      break;
    }
  }

  // If anchor doesn't exist and we haven't reached MAX_ANCHORS, add it
  if (!anchor_exists && anchors.size() < MAX_ANCHORS)
  {
    anchors.push_back(IDDistance(mac_adr, distance));
  }

  // Log current anchors data
  logAnchorsData();

  // Check if we have enough anchors for positioning
  if (anchors.size() >= MIN_ANCHORS_NEEDED)
  {
    sendDataToServer();
  }
  else if (millis() - previousMillis >= interval)
  {
    sendErrorToServer();
  }
}

void initUDPNormal()
{
  // Initialize UDP communication
  udp.begin(UDP_PORT);

  // Send a message to the server when connected
  sendOnConnectMessageToServer();
}

void sendOnConnectMessageToServer()
{
  // Send a message to the server when connected
  String message = "Connected to server: " + serverIP;

  udp.beginPacket(serverIP.c_str(), UDP_PORT);
  udp.print(message);
  udp.endPacket();
}

// Helper function to get short MAC from full MAC
String getShortMacFromFull(String fullMac)
{
  // The full MAC format is like "AB:CD:EF:12:34:56"
  // We want to extract "123456" (last 3 bytes without colons)
  String result = "";
  int parts = 0;
  int startPos = 0;

  // Count the number of parts (separated by colons)
  for (int i = 0; i < fullMac.length(); i++)
  {
    if (fullMac.charAt(i) == ':')
    {
      parts++;
    }
  }

  // Extract last 3 parts
  for (int i = 0; i < parts + 1; i++)
  {
    int colonPos = fullMac.indexOf(':', startPos);
    if (colonPos == -1)
      colonPos = fullMac.length();

    if (i >= parts - 2)
    { // Last 3 parts
      String part = fullMac.substring(startPos, colonPos);
      result += part;
    }

    startPos = colonPos + 1;
  }

  return result;
}

// Helper function to convert short MAC to byte array
void shortMacToBytes(String shortMac, uint8_t *bytes)
{
  for (int i = 0; i < SHORT_MAC_LENGTH; i++)
  {
    String byteStr = shortMac.substring(i * 2, i * 2 + 2);
    bytes[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
  }
}