#define METER_TO_CENTI 100
#define RX_BUFFER_SIZE 26
#define TOF_OFFSET 26
#define MAC_LENGTH 6
#define RX_PLACE 18

// MAC address format constants
#define SHORT_MAC_LENGTH 3    // Using only last 3 bytes of MAC
#define SOURCE_MAC_IDX 18     // Position of source MAC in message
#define TARGET_MAC_IDX 21     // Position of target MAC in message

//defines struct
#define STRUCT_FULL 3
#define STRUCT_EMTPY 0
//defines communication config
#define CHANNEL_NUM 5
#define TX_RX_PREAMBLE 9
#define SFD_SYMBOL_NON_STD_8 1
#define SFD_TIMEOUT (129 + 8 - 8)

// WiFi
#define USE_WINDESHEIM_WIFI false  // true for wifi at windesheim, false for wifi at PERRON038
#define USE_TPLINK false
#define USE_AWL false

#if USE_WINDESHEIM_WIFI
  /* WiFi bij Windesheim */
  #define WIFI_SSID     "iotroam"
  #define WIFI_PASSWORD "b75VgrRXcj"
#elif USE_TPLINK
    /* WiFi bij TPLINK */
  #define WIFI_SSID     "TP-Link_B9BD"
  #define WIFI_PASSWORD "13224394"
#elif USE_AWL
  /* WiFi bij PERRON038 */
  #define WIFI_SSID     "AWL"
  #define WIFI_PASSWORD "4wLPR3shared!@-"
#else
  #define WIFI_SSID     "MikroTik-1B9B14"
  #define WIFI_PASSWORD "BrrBrrPatapim!?" 
#endif

