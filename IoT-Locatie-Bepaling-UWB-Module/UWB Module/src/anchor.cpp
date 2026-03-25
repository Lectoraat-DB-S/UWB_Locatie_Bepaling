#include "dw3000.h"
#include "SPI.h"
#include "WiFi.h"
#include "config.h"
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "anchor.h"

extern SPISettings _fastSPI;

// DW3000 pin configuration
#define DW3000_PIN_RST 27
#define DW3000_PIN_IRQ 34
#define DW3000_PIN_SS 4

// UWB timing and protocol constants
#define TX_ANT_DLY 16385
#define RX_ANT_DLY 16385
#define ALL_MSG_COMMON_LEN 10
#define ALL_MSG_SN_IDX 2
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define RESP_MSG_TS_LEN 4
#define POLL_RX_TO_RESP_TX_DLY_UUS 750

#define UWB_STARTUP_DELAY_MS 2
#define WIFI_CONNECTION_RETRY_DELAY_MS 500
#define UWB_POLLING_TIMEOUT_MS 500

#define MAC_LENGTH 6
#define BUFFER_SIZE 26
#define CHECKSUM_OFFSET 2

#define MAC_POSITION (BUFFER_SIZE - MAC_LENGTH - CHECKSUM_OFFSET)

// RTOS task priorities and stack sizes
#define UWB_TASK_PRIORITY 2
#define WIFI_TASK_PRIORITY 1
#define TASK_STACK_SIZE 4096

/* ----------------------------------------- */
// Prototypes
void anchor_setup();
void anchor_loop();
void anchor_initUWB();
void anchor_initWiFi();
void anchor_processUwbResponse();
void anchor_sendUwbPollMessage();
void anchor_pollForUwbMessage();
bool isTagDiscovered(uint8_t *tagMac, int length);
void addDiscoveredTag(uint8_t *tagMac, int length);
void removeStaleTags();
void tagCleanupTask(void *pvParameters);

// Default communication configuration. We use default non-STS DW mode.
static dwt_config_t config = {
    5,                /* Channel number. */
    DWT_PLEN_128,     /* Preamble length. Used in TX only. */
    DWT_PAC8,         /* Preamble acquisition chunk size. Used in RX only. */
    9,                /* TX preamble code. Used in TX only. */
    9,                /* RX preamble code. Used in RX only. */
    1,                /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
    DWT_BR_6M8,       /* Data rate. */
    DWT_PHRMODE_STD,  /* PHY header mode. */
    DWT_PHRRATE_STD,  /* PHY header rate. */
    (129 + 8 - 8),    /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
    DWT_STS_MODE_OFF, /* STS disabled */
    DWT_STS_LEN_64,   /* STS length see allowed values in Enum dwt_sts_lengths_e */
    DWT_PDOA_M0       /* PDOA mode off */
};

/* Structure Transmission Array:
0-1, 3-8: Checksum Numbers and Chars to double check transmission
2: Frame Sequence Number
9: Identifier for Responder/Initiator (Anchor/Tag)
10-13: Poll RX Timestamp (4 bytes)
14-17: Response TX Timestamp (4 bytes)
18-23: MAC address of the device (6 bytes)
24-25: Checksum (2 bytes)
*/
static uint8_t rx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0xE0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t tx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0xE1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t frame_seq_nb = 0;
static uint8_t rx_buffer[BUFFER_SIZE];
static uint32_t status_reg = 0;
static uint64_t poll_rx_ts;
static uint64_t resp_tx_ts;

static std::vector<String> discoveredTags;

// Map to store discovered tags and their last interaction timestamp
std::map<String, std::chrono::steady_clock::time_point> tagTimestamps;

static bool isDiscovered = false;
static bool uwbInitialized = false;
static bool wifiInitialized = false;

static uint8_t baseMac[MAC_LENGTH];
static uint8_t mac[MAC_LENGTH];

const uint8_t unsetMac[MAC_LENGTH] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

extern dwt_txconfig_t txconfig_options;

// RTOS handles
TaskHandle_t uwbTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;

// Mutex for protecting shared resource
extern SemaphoreHandle_t uwbSemaphore;
extern SemaphoreHandle_t wifiSemaphore;

// Internal message queue for communication between tasks
QueueHandle_t uwbQueue;

void anchor_setup()
{
  UART_init();
  Serial.println("STARTING IN ANCHOR MODE WITH RTOS...");

  // Create message queue
  uwbQueue = xQueueCreate(10, sizeof(uint32_t));

  // Create additional RTOS tasks
  xTaskCreatePinnedToCore(
      anchor_wifi_task,
      "WiFi_Task",
      TASK_STACK_SIZE,
      NULL,
      WIFI_TASK_PRIORITY,
      &wifiTaskHandle,
      0 // Run on core 0
  );

  // Create the tag cleanup task
  xTaskCreate(
    tagCleanupTask,
    "TagCleanupTask",
    TASK_STACK_SIZE,
    NULL,
    WIFI_TASK_PRIORITY,
    NULL
  );
}

void anchor_loop()
{
  if (!uwbInitialized && wifiInitialized)
  {
    // Initialize UWB only after WiFi is ready (to get the MAC address)
    if (xSemaphoreTake(uwbSemaphore, portMAX_DELAY) == pdTRUE)
    {
      anchor_initUWB();
      uwbInitialized = true;
      xSemaphoreGive(uwbSemaphore);
    }
    return;
  }

  if (!uwbInitialized || !wifiInitialized)
  {
    // Wait until both systems are initialized
    vTaskDelay(pdMS_TO_TICKS(10));
    return;
  }

  // Main UWB polling logic with timeout
  if (xSemaphoreTake(uwbSemaphore, portMAX_DELAY) == pdTRUE)
  {
    anchor_pollForUwbMessage();

    if (status_reg & SYS_STATUS_RXFCG_BIT_MASK)
    {
      anchor_processUwbResponse();
    }
    else
    {
      // Clear RX error events in the DW IC status register
      dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR);
    }

    xSemaphoreGive(uwbSemaphore);
  }
  // Small yield to scheduler
  vTaskDelay(pdMS_TO_TICKS(1));
}

void anchor_wifi_task(void *pvParameters)
{
  Serial.println("WiFi task starting...");

  // Initialize WiFi
  if (xSemaphoreTake(wifiSemaphore, portMAX_DELAY) == pdTRUE)
  {
    anchor_initWiFi();
    wifiInitialized = true;
    xSemaphoreGive(wifiSemaphore);
  }

  const TickType_t xDelay = pdMS_TO_TICKS(30000); // 30 seconds

  // Periodically check WiFi connection
  while (1)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      if (xSemaphoreTake(wifiSemaphore, portMAX_DELAY) == pdTRUE)
      {
        Serial.println("WiFi disconnected, reconnecting...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        xSemaphoreGive(wifiSemaphore);
      }
    }
    vTaskDelay(xDelay);
  }
}

void anchor_pollForUwbMessage()
{
  // Activate reception immediately
  dwt_rxenable(DWT_START_RX_IMMEDIATE);

  // Poll for reception with timeout
  TickType_t startTime = xTaskGetTickCount();
  TickType_t timeout = pdMS_TO_TICKS(UWB_POLLING_TIMEOUT_MS);

  while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) &
           (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_ERR)))
  {

    // Check for timeout
    if ((xTaskGetTickCount() - startTime) > timeout)
    {
      // Timeout occurred, exit polling loop
      status_reg = 0;
      break;
    }

    // Yield to other tasks
    taskYIELD();
  }
}

void anchor_processUwbResponse()
{
  uint32_t frame_len;

  // Clear good RX frame event in the DW IC status register
  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);

  // A frame has been received, read it into the local buffer
  frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;
  if (frame_len <= sizeof(rx_buffer))
  {
    dwt_readrxdata(rx_buffer, frame_len, 0);

    uint8_t sourceMac[SHORT_MAC_LENGTH];

    // Extract source MAC from message (the tag that sent the request)
    memcpy(sourceMac, &rx_buffer[SOURCE_MAC_IDX], SHORT_MAC_LENGTH);

    // Convert source MAC to string
    String tagMacStr = "";
    for (int i = 0; i < SHORT_MAC_LENGTH; i++)
    {
      if (sourceMac[i] < 0x10)
      {
        tagMacStr += "0"; // Add leading zero for single-digit hex values
      }
      tagMacStr += String(sourceMac[i], HEX);
    }

    // Refresh the timestamp for this tag
    tagTimestamps[tagMacStr] = std::chrono::steady_clock::now();

    // Extract the last 3 bytes of our MAC address
    uint8_t ourShortMac[SHORT_MAC_LENGTH];
    for (int i = 0; i < SHORT_MAC_LENGTH; i++)
    {
      ourShortMac[i] = mac[MAC_LENGTH - SHORT_MAC_LENGTH + i];
    }

    // Extract target MAC from message (who the message is intended for)
    uint8_t targetMac[SHORT_MAC_LENGTH];
    memcpy(targetMac, &rx_buffer[TARGET_MAC_IDX], SHORT_MAC_LENGTH);

    // Check if this is a broadcast message (all zeros) or specifically for us
    bool isBroadcast = (targetMac[0] == 0 && targetMac[1] == 0 && targetMac[2] == 0);
    bool isForUs = (memcmp(targetMac, ourShortMac, SHORT_MAC_LENGTH) == 0);

    // Modified condition: respond if it's targeted at us or if it's a broadcast
    // from a tag that hasn't discovered us yet
    if (isForUs || (isBroadcast && !isTagDiscovered(sourceMac, SHORT_MAC_LENGTH)))
    {
      /* Check that the frame is a poll sent by "SS TWR initiator" example.
       * As the sequence number field of the frame is not relevant, it is cleared to simplify the validation of the frame. */
      rx_buffer[ALL_MSG_SN_IDX] = 0;
      if (memcmp(rx_buffer, rx_poll_msg, ALL_MSG_COMMON_LEN) == 0)
      {
        uint32_t resp_tx_time;

        // For targeted messages, mark this tag as having discovered us
        if (isForUs)
        {
          addDiscoveredTag(sourceMac, SHORT_MAC_LENGTH);
        }

        /* Retrieve poll reception timestamp. */
        poll_rx_ts = get_rx_timestamp_u64();

        /* Compute response message transmission time. */
        resp_tx_time = (poll_rx_ts + (POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
        dwt_setdelayedtrxtime(resp_tx_time);

        /* Response TX timestamp is the transmission time we programmed plus the antenna delay. */
        resp_tx_ts = (((uint64_t)(resp_tx_time & 0xFFFFFFFEUL)) << 8) + TX_ANT_DLY;
        /* Write all timestamps in the final message. */
        resp_msg_set_ts(&tx_resp_msg[RESP_MSG_POLL_RX_TS_IDX], poll_rx_ts);
        resp_msg_set_ts(&tx_resp_msg[RESP_MSG_RESP_TX_TS_IDX], resp_tx_ts);

        // When responding, our short MAC becomes the source and the requester's MAC becomes the target
        memcpy(&tx_resp_msg[SOURCE_MAC_IDX], ourShortMac, SHORT_MAC_LENGTH);
        memcpy(&tx_resp_msg[TARGET_MAC_IDX], sourceMac, SHORT_MAC_LENGTH);

        anchor_sendUwbPollMessage();
      }
    }
    else
    {
      // Message not for us, ignore it
      Serial.println("Message not addressed to this anchor");
    }
  }
}

void anchor_sendUwbPollMessage()
{
  int ret;

  // Write and send the response message
  tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
  dwt_writetxdata(sizeof(tx_resp_msg), tx_resp_msg, 0); // Zero offset in TX buffer
  dwt_writetxfctrl(sizeof(tx_resp_msg), 0, 1);          // Zero offset in TX buffer, ranging
  ret = dwt_starttx(DWT_START_TX_DELAYED);

  // If dwt_starttx() returns an error, abandon this ranging exchange and proceed to the next one
  if (ret == DWT_SUCCESS)
  {
    isDiscovered = true;

    // Poll DW IC until TX frame sent event set with timeout
    TickType_t startTime = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(100); // 100ms timeout

    while (!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS_BIT_MASK))
    {
      // Check for timeout
      if ((xTaskGetTickCount() - startTime) > timeout)
      {
        break;
      }

      // Yield to other tasks
      taskYIELD();
    }

    // Clear TXFRS event
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);

    // Increment frame sequence number after transmission of the poll message (modulo 256)
    frame_seq_nb++;
  }
}

void anchor_initWiFi()
{
  // Start WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Wait for WiFi to connect with RTOS-friendly delay
  int attempts = 0;
  const int maxAttempts = 20;

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts)
  {
    vTaskDelay(pdMS_TO_TICKS(WIFI_CONNECTION_RETRY_DELAY_MS));
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected!");

    // Convert MAC address to bytestring array
    String macStr = WiFi.macAddress();

    for (int i = 0; i < MAC_LENGTH; i++)
    {
      String byteStr = macStr.substring(i * 3, i * 3 + 2);
      mac[i] = (uint8_t)strtol(byteStr.c_str(), NULL, HEX);
    }

    // Insert MAC address into tx_resp_msg
    for (int i = 0; i < MAC_LENGTH; i++)
    {
      tx_resp_msg[MAC_POSITION + i] = mac[i];
    }
  }
  else
  {
    Serial.println("\nWiFi connection failed! Will retry...");
  }
}

void anchor_initUWB()
{
  _fastSPI = SPISettings(16000000L, MSBFIRST, SPI_MODE0);

  spiBegin(DW3000_PIN_IRQ, DW3000_PIN_RST);
  spiSelect(DW3000_PIN_SS);

  vTaskDelay(pdMS_TO_TICKS(UWB_STARTUP_DELAY_MS)); // Time needed for DW3000 to start up

  // Check if DW IC is in IDLE_RC before proceeding
  int idleRetries = 0;
  const int maxIdleRetries = 10;

  while (!dwt_checkidlerc() && idleRetries < maxIdleRetries)
  {
    UART_puts((char*)"IDLE FAILED after max retries"); // Explicit cast to char*
    vTaskDelay(pdMS_TO_TICKS(10));
    idleRetries++;
  }

  if (idleRetries >= maxIdleRetries)
  {
    UART_puts((char*)"IDLE FAILED after max retries"); // Explicit cast to char*
    return; // Don't halt completely, allow retry later
  }

  if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
  {
    UART_puts((char*)"INIT FAILED"); // Explicit cast to char*
    return; // Don't halt completely, allow retry later
  }

  // Enabling LEDs for debug
  dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

  // Configure DW IC
  if (dwt_configure(&config))
  {
    UART_puts((char*)"CONFIG FAILED"); // Explicit cast to char*
    return; // Don't halt completely, allow retry later
  }

  // Configure the TX spectrum parameters
  dwt_configuretxrf(&txconfig_options);

  // Apply default antenna delay value
  dwt_setrxantennadelay(RX_ANT_DLY);
  dwt_settxantennadelay(TX_ANT_DLY);

  // Enable TX/RX states output
  dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);
}

// Add a helper function to check if a tag has discovered this anchor
bool isTagDiscovered(uint8_t *tagMac, int length)
{
  // Convert tag MAC to string for easier comparison
  String tagMacStr = "";
  for (int i = 0; i < length; i++)
  {
    if (tagMac[i] < 0x10)
    {
      tagMacStr += "0"; // Add leading zero for single-digit hex values
    }
    tagMacStr += String(tagMac[i], HEX);
  }

  // Check if this tag is in our discovered list
  for (size_t i = 0; i < discoveredTags.size(); i++)
  {
    if (discoveredTags[i] == tagMacStr)
    {
      return true;
    }
  }
  return false;
}

// Add a helper function to add a tag to the discovered list
void addDiscoveredTag(uint8_t *tagMac, int length)
{
  // Convert tag MAC to string
  String tagMacStr = "";
  for (int i = 0; i < length; i++)
  {
    if (tagMac[i] < 0x10)
    {
      tagMacStr += "0"; // Add leading zero for single-digit hex values
    }
    tagMacStr += String(tagMac[i], HEX);
  }

  // Check if this tag is already in our discovered list
  for (size_t i = 0; i < discoveredTags.size(); i++)
  {
    if (discoveredTags[i] == tagMacStr)
    {
      return; // Tag already in the list
    }
  }

  // Add tag to discovered list
  discoveredTags.push_back(tagMacStr);
  Serial.println("New tag discovered: " + tagMacStr + ", Total tags: " + String(discoveredTags.size()));
}

// Function to remove stale tags
void removeStaleTags()
{
  auto now = std::chrono::steady_clock::now();
  for (auto it = discoveredTags.begin(); it != discoveredTags.end();)
  {
    auto tag = *it;
    if (tagTimestamps.find(tag) != tagTimestamps.end())
    {
      auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - tagTimestamps[tag]).count();
      if (duration > 15)
      {
        it = discoveredTags.erase(it); // Remove stale tag
        tagTimestamps.erase(tag); // Remove from timestamp map
        Serial.println("Removed stale tag: " + tag);
        continue;
      }
    }
    ++it;
  }
}

// Task to periodically clean up stale tags
void tagCleanupTask(void *pvParameters)
{
  while (true)
  {
    removeStaleTags();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Run every second
  }
}
