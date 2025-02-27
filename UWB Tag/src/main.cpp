#include "dw3000.h"
#include <WiFi.h>
#include <WiFiUdp.h>

//defines pins
#define PIN_RST 27
#define PIN_IRQ 34
#define PIN_SS 4
//defines UWB
#define RNG_DELAY_MS 1000
#define TX_ANT_DLY 16385
#define RX_ANT_DLY 16385
#define ALL_MSG_COMMON_LEN 10
#define ALL_MSG_SN_IDX 2
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define RESP_MSG_TS_LEN 4
#define POLL_TX_TO_RESP_RX_DLY_UUS 240
#define RESP_RX_TIMEOUT_UUS 400
#define START_UP_TIME 2
#define WAITING_TIME 500

//defines struct
#define STRUCT_FULL 3
#define STRUCT_EMTPY 0
//defines communication config
#define CHANNEL_NUM 5
#define TX_RX_PREAMBLE 9
#define SFD_SYMBOL_NON_STD_8 1
#define SFD_TIMEOUT (129 + 8 - 8)

#define METER_TO_CENTI 100
#define RX_BUFFER_SIZE 26
#define TOF_OFFSET 26
#define MAC_LENGTH 4
#define RX_PLACE 20

// Tag height and anchor height in centimeters
#define TAG_HEIGHT 100
#define ANCHOR_HEIGHT 430


/* Default communication configuration. We use default non-STS DW mode. */
static dwt_config_t config = {
    CHANNEL_NUM,              /* Channel number. */
    DWT_PLEN_128,             /* Preamble length. Used in TX only. */
    DWT_PAC8,                 /* Preamble acquisition chunk size. Used in RX only. */
    TX_RX_PREAMBLE,           /* TX preamble code. Used in TX only. */
    TX_RX_PREAMBLE,           /* RX preamble code. Used in RX only. */
    SFD_SYMBOL_NON_STD_8,     /* use non-standard 8 symbol */
    DWT_BR_6M8,               /* Data rate. */
    DWT_PHRMODE_STD,          /* PHY header mode. */
    DWT_PHRRATE_STD,          /* PHY header rate. */
    SFD_TIMEOUT,              /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
    DWT_STS_MODE_OFF,         /* STS disabled */
    DWT_STS_LEN_64,           /* STS length see allowed values in Enum dwt_sts_lengths_e */
    DWT_PDOA_M0               /* PDOA mode off */
};

static uint8_t tx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0xE0, 0, 0};
static uint8_t rx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0xE1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t frame_seq_nb = 0;
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint32_t status_reg = 0;
static double tof;
static int distance;
static String macAddress = WiFi.macAddress();
unsigned long previousMillis = 0;  // Globale variabele om tijdsstempel op te slaan
const long interval = 2000;        // Interval van 2 seconden

struct IDDistance {
    String id = "";
    int distance = NULL;
} Anchor1,Anchor2,Anchor3,Anchor4;

static const struct IDDistance EmptyStruct;


static uint8_t id_count = 0;


/* WiFi network name and password */
// Your wirelless router ssid and password


/**
 * WiFi bij Windesheim
*/


// const char * ssid = "iotroam"; 
// const char * pwd = "b75VgrRXcj";

/**
 * WiFi bij PERRON038 (AWL)
*/
const char * ssid = "";
const char * pwd = "";


// IP address to send UDP data to.
// it can be ip address of the server or 
// a network broadcast address
// here is broadcast address

// const char * udpAddress = ""; // target pc ip (IOT_WOUTER)
const char * udpAddress = ""; // target pc ip (AWL_WOUTER)
// const char * udpAddress = ""; // target pc ip (AWL_YORICK)
const int udpPort = 8080; //port server

//create UDP instance
WiFiUDP udp;  


extern dwt_txconfig_t txconfig_options;

String getHexStr(uint8_t* data, int length) {
  String hexStr = "";
  for (int i = 0; i < length; i++) {
    if (data[i] < 0x10) {
      hexStr += "0"; // Leading zero
    }
    hexStr += String(data[i], HEX);
  }
  return hexStr;
}

void setup()
{
  UART_init();

  spiBegin(PIN_IRQ, PIN_RST);
  spiSelect(PIN_SS);

  delay(START_UP_TIME); // Time needed for DW3000 to start up (transition from INIT_RC to IDLE_RC, or could wait for SPIRDY event)


  while (!dwt_checkidlerc()) // Need to make sure DW IC is in IDLE_RC before proceeding
  {
    UART_puts("IDLE FAILED\r\n");
    while (1)
      ;
  }

  if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
  {
    UART_puts("INIT FAILED\r\n");
    while (1)
      ;
  }

  // Enabling LEDs here for debug so that for each TX the D1 LED will flash on DW3000 red eval-shield boards.
  dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

  /* Configure DW IC. */
  if (dwt_configure(&config)) // if the dwt_configure returns DWT_ERROR either the PLL or RX calibration has failed the host should reset the device
  {
    UART_puts("CONFIG FAILED\r\n");
    while (1)
      ;
  }

  /* Configure the TX spectrum parameters (power, PG delay and PG count) */
  dwt_configuretxrf(&txconfig_options);

  /* Apply default antenna delay value. */
  dwt_setrxantennadelay(RX_ANT_DLY);
  dwt_settxantennadelay(TX_ANT_DLY);

  /* Set expected response's delay and timeout. */
  dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
  dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);

  /* Next can enable TX/RX states output on GPIOs 5 and 6 to help debug, and also TX/RX LEDs */
  dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);

  Serial.println("Range RX");
  Serial.println("Setup over........");


  //Connect to the WiFi network
  WiFi.begin(ssid, pwd);
  

  // Wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(WAITING_TIME);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //This initializes udp and transfer buffer
  udp.begin(udpPort);
  // Enable modem sleep
  WiFi.setSleep(true);
}

void loop()
{
  /* Write frame data to DW IC and prepare transmission. */
  tx_poll_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);
  dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0); /* Zero offset in TX buffer. */
  dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1);          /* Zero offset in TX buffer, ranging. */

  /* Start transmission, indicating that a response is expected so that reception is enabled automatically after the frame is sent and the delay
   * set by dwt_setrxaftertxdelay() has elapsed. */
  dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

  /* We assume that the transmission is achieved correctly, poll for reception of a frame or error/timeout. */
  while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR)))
  {
  };

  /* Increment frame sequence number after transmission of the poll message (modulo 256). */
  frame_seq_nb++;

  if (status_reg & SYS_STATUS_RXFCG_BIT_MASK)
  {
    uint32_t frame_len;

    /* Clear good RX frame event in the DW IC status register. */
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);

    /* A frame has been received, read it into the local buffer. */
    frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;
    if (frame_len <= sizeof(rx_buffer))
    {
      dwt_readrxdata(rx_buffer, frame_len, 0);

      /* Check that the frame is the expected response from the companion.
       * As the sequence number field of the frame is not relevant, it is cleared to simplify the validation of the frame. */
      rx_buffer[ALL_MSG_SN_IDX] = 0;
      if (memcmp(rx_buffer, rx_resp_msg, ALL_MSG_COMMON_LEN) == 0)
      {
        uint32_t poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
        int32_t rtd_init, rtd_resp;
        float clockOffsetRatio;

        /* Retrieve poll transmission and response reception timestamps. */
        poll_tx_ts = dwt_readtxtimestamplo32();
        resp_rx_ts = dwt_readrxtimestamplo32();

        /* Read carrier integrator value and calculate clock offset ratio. this is dependent on the antenne calibration */
        clockOffsetRatio = ((float)dwt_readclockoffset()) / (uint32_t)(1 << TOF_OFFSET);

        /* Get timestamps embedded in response message. */
        resp_msg_get_ts(&rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
        resp_msg_get_ts(&rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

        /* Compute time of flight and distance, using clock offset ratio to correct for differing local and remote clock rates */
        rtd_init = resp_rx_ts - poll_tx_ts;
        rtd_resp = resp_tx_ts - poll_rx_ts;

        tof = ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
        distance = tof * SPEED_OF_LIGHT * METER_TO_CENTI;
        if(distance < 330) distance = 330;

        // Pythagorean theorem 
        int formulaPY = sqrt(pow(distance, 2) - pow(ANCHOR_HEIGHT - TAG_HEIGHT, 2 )); 
        
        //Mac address to Hexadecimal
        String mac_adr = getHexStr(&rx_buffer[RX_PLACE], MAC_LENGTH);
        bool id_exists = false;

        // Check if the ID already exists in one of the Anchors
        if (Anchor1.id == mac_adr) {
          Anchor1.distance = formulaPY;
          id_exists = true;
        } else if (Anchor2.id == mac_adr) {
          Anchor2.distance = formulaPY;
          id_exists = true;
        } else if (Anchor3.id == mac_adr) {
          Anchor3.distance = formulaPY;
          id_exists = true;
        }

        // If the ID doesn't exist, add it to an empty anchor slot
        if (!id_exists) {
          if (Anchor1.id == "") {
            Anchor1.id = mac_adr;
            Anchor1.distance = formulaPY;
            id_count++;
          } else if (Anchor2.id == "") {
            Anchor2.id = mac_adr;
            Anchor2.distance = formulaPY;
            id_count++;
          } else if (Anchor3.id == "") {
            Anchor3.id = mac_adr;
            Anchor3.distance = formulaPY;
            id_count++;
          }
        }

        Serial.println(Anchor1.id+";"+Anchor1.distance+";"+
                        Anchor2.id+";"+Anchor2.distance+";"+
                        Anchor3.id+";"+Anchor3.distance);
        
              // send full struct via wifi
        if (id_count == STRUCT_FULL) {
          WiFi.setSleep(false); // enable wifi transfer
          udp.beginPacket(udpAddress, udpPort);
          udp.print(macAddress+";"+
                    Anchor1.id+";"+Anchor1.distance+";"+
                    Anchor2.id+";"+Anchor2.distance+";"+
                    Anchor3.id+";"+Anchor3.distance);
          udp.endPacket();

          //Clear structs for new data from other anchors
          Anchor1 = EmptyStruct;
          Anchor2 = EmptyStruct;
          Anchor3 = EmptyStruct;
          id_count = STRUCT_EMTPY;
          WiFi.setSleep(true); // set wifi in sleep mode

        } else if ((id_count < STRUCT_FULL) && (millis() - previousMillis >= interval)) {
          udp.beginPacket(udpAddress, udpPort);
          udp.print("ERROR: tag " + macAddress + " is not in range of three anchors!");
          udp.endPacket();
          previousMillis = millis();
        }      
      }
    }
  }
  else
  {
    /* Clear RX error/timeout events in the DW IC status register. */
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
  }
}