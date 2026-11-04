/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : usbd_cdc_if.c
 * @version        : v1.0_Cube
 * @brief          : Usb device for Virtual Com Port.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc_if.h"


/* USER CODE BEGIN INCLUDE */
/* Signal-only test; TX-A interlock open and laser disconnected. */
extern volatile uint8_t arm_dryrun_request;
extern volatile uint8_t dummy_test_active;
extern volatile uint8_t pattern_demo_request;
extern volatile uint8_t pattern_demo_pending_len;
extern uint8_t pattern_demo_pending[24U];
extern volatile uint8_t optical_tx_active;

#include <string.h>
#include "main.h"

/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
 * @brief Usb device library.
 * @{
 */

/** @addtogroup USBD_CDC_IF
 * @{
 */

/** @defgroup USBD_CDC_IF_Private_TypesDefinitions USBD_CDC_IF_Private_TypesDefinitions
 * @brief Private types.
 * @{
 */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
 * @}
 */

/** @defgroup USBD_CDC_IF_Private_Defines USBD_CDC_IF_Private_Defines
 * @brief Private defines.
 * @{
 */

/* USER CODE BEGIN PRIVATE_DEFINES */
/* USER CODE END PRIVATE_DEFINES */

/**
 * @}
 */

/** @defgroup USBD_CDC_IF_Private_Macros USBD_CDC_IF_Private_Macros
 * @brief Private macros.
 * @{
 */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
 * @}
 */

/** @defgroup USBD_CDC_IF_Private_Variables USBD_CDC_IF_Private_Variables
 * @brief Private variables.
 * @{
 */

/* Create buffer for reception and transmission           */
/* It's up to user to redefine and/or remove those define */
/** Received data over USB are stored in this buffer      */
uint8_t UserRxBufferHS[APP_RX_DATA_SIZE]
__attribute__((aligned(4)));

/** Data to send over USB CDC are stored in this buffer   */
uint8_t UserTxBufferHS[APP_TX_DATA_SIZE]
__attribute__((aligned(4)));


/* USER CODE BEGIN PRIVATE_VARIABLES */
/* Short binary-framed demo messages */
#define PATTERN_RX_LIMIT 24U
#define OPTICAL_RX_LIMIT 16U
static uint8_t pattern_rx_state = 0U; /* 0=idle, 1=len, 2=payload */
static uint8_t pattern_rx_optical = 0U;
static uint8_t pattern_rx_expected = 0U;
static uint8_t pattern_rx_count = 0U;
static uint8_t pattern_rx_buffer[PATTERN_RX_LIMIT];
static uint32_t pattern_rx_started_ms = 0U;



#define ECHO_DEPTH 8U
#define ECHO_PACKET_SIZE 512U

// Eight buffers, each holding one USB packet
static uint8_t echo_queue[ECHO_DEPTH][ECHO_PACKET_SIZE]
__attribute__((aligned(4)));

// Length of each queued packet
static uint16_t echo_lengths[ECHO_DEPTH];

// Circular queue state
static uint8_t echo_head = 0;
static uint8_t echo_tail = 0;
static uint8_t echo_count = 0;

// USB transfer state
static uint8_t echo_tx_active = 0;
static uint8_t echo_rx_paused = 0;


static volatile uint32_t rx_packet_count = 0;
static volatile uint32_t rx_512_count = 0;
static volatile uint32_t queue_full_count = 0;
static volatile uint32_t tx_complete_count = 0;


static volatile uint32_t rx_total_bytes = 0;
static volatile uint32_t rx_min_length = 0xFFFFFFFFU;
static volatile uint32_t rx_max_length = 0;
static volatile uint8_t queue_max_depth = 0;


/* TX-only benchmark configuration */
#define TX_BENCH_TOTAL (10U * 1024U * 1024U)
#define TX_BENCH_CHUNK 16384U

/* Persistent buffer for asynchronous USB transfers */
static uint8_t tx_bench_buffer[TX_BENCH_CHUNK]
__attribute__((aligned(4)));

/* Benchmark state */
static volatile uint32_t tx_bench_sent = 0;
static uint8_t tx_bench_active = 0;
static uint8_t tx_bench_inflight = 0;

static volatile uint32_t tx_gen_cycles_min = 0xFFFFFFFFU;
static volatile uint32_t tx_gen_cycles_max = 0;
static volatile uint64_t tx_gen_cycles_total = 0;
static volatile uint32_t tx_gen_calls = 0;



/* USER CODE END PRIVATE_VARIABLES */

/**
 * @}
 */

/** @defgroup USBD_CDC_IF_Exported_Variables USBD_CDC_IF_Exported_Variables
 * @brief Public variables.
 * @{
 */

extern USBD_HandleTypeDef hUsbDeviceHS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
 * @}
 */

/** @defgroup USBD_CDC_IF_Private_FunctionPrototypes USBD_CDC_IF_Private_FunctionPrototypes
 * @brief Private functions declaration.
 * @{
 */

static int8_t CDC_Init_HS(void);
static int8_t CDC_DeInit_HS(void);
static int8_t CDC_Control_HS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_HS(uint8_t* pbuf, uint32_t *Len);
static int8_t CDC_TransmitCplt_HS(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */

static void Echo_KickTx(void);
static void TxBench_Kick(void);

/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
 * @}
 */

USBD_CDC_ItfTypeDef USBD_Interface_fops_HS =
{
  CDC_Init_HS,
  CDC_DeInit_HS,
  CDC_Control_HS,
  CDC_Receive_HS,
  CDC_TransmitCplt_HS
};

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Initializes the CDC media low layer over the USB HS IP
 * @retval USBD_OK if all operations are OK else USBD_FAIL
 */
static int8_t CDC_Init_HS(void)
{
  /* USER CODE BEGIN 8 */

  /* Reset echo queue state */
  echo_head = 0;
  echo_tail = 0;
  echo_count = 0;

  echo_tx_active = 0;
  echo_rx_paused = 0;

  rx_packet_count = 0;
  rx_512_count = 0;
  queue_full_count = 0;
  tx_complete_count = 0;


  /* Reset receive-length diagnostics */
  rx_total_bytes = 0;
  rx_min_length = 0xFFFFFFFFU;
  rx_max_length = 0;
  queue_max_depth = 0;

  /* Reset TX benchmark state */
  tx_bench_sent = 0;
  tx_bench_active = 0;
  tx_bench_inflight = 0;


  /* Reset CPU cycle profiling counters */
  tx_gen_cycles_min = 0xFFFFFFFFU;
  tx_gen_cycles_max = 0;
  tx_gen_cycles_total = 0;
  tx_gen_calls = 0;



  /* Set Application Buffers */
  USBD_CDC_SetTxBuffer(&hUsbDeviceHS, UserTxBufferHS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceHS, UserRxBufferHS);

  /* Enable Cortex-M7 cycle counter */



  return (USBD_OK);

  /* USER CODE END 8 */
}

/**
 * @brief  DeInitializes the CDC media low layer
 * @param  None
 * @retval USBD_OK if all operations are OK else USBD_FAIL
 */
static int8_t CDC_DeInit_HS(void)
{
  /* USER CODE BEGIN 9 */
  return (USBD_OK);
  /* USER CODE END 9 */
}

/**
 * @brief  Manage the CDC class requests
 * @param  cmd: Command code
 * @param  pbuf: Buffer containing command data (request parameters)
 * @param  length: Number of data to be sent (in bytes)
 * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
 */
static int8_t CDC_Control_HS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  /* USER CODE BEGIN 10 */
  switch(cmd)
  {
    case CDC_SEND_ENCAPSULATED_COMMAND:

      break;

    case CDC_GET_ENCAPSULATED_RESPONSE:

      break;

    case CDC_SET_COMM_FEATURE:

      break;

    case CDC_GET_COMM_FEATURE:

      break;

    case CDC_CLEAR_COMM_FEATURE:

      break;

      /*******************************************************************************/
      /* Line Coding Structure                                                       */
      /*-----------------------------------------------------------------------------*/
      /* Offset | Field       | Size | Value  | Description                          */
      /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
      /* 4      | bCharFormat |   1  | Number | Stop bits                            */
      /*                                        0 - 1 Stop bit                       */
      /*                                        1 - 1.5 Stop bits                    */
      /*                                        2 - 2 Stop bits                      */
      /* 5      | bParityType |  1   | Number | Parity                               */
      /*                                        0 - None                             */
      /*                                        1 - Odd                              */
      /*                                        2 - Even                             */
      /*                                        3 - Mark                             */
      /*                                        4 - Space                            */
      /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
      /*******************************************************************************/
    case CDC_SET_LINE_CODING:

      break;

    case CDC_GET_LINE_CODING:

      break;

    case CDC_SET_CONTROL_LINE_STATE:

      break;

    case CDC_SEND_BREAK:

      break;

    default:
      break;
  }

  return (USBD_OK);
  /* USER CODE END 10 */
}

/**
 * @brief  Data received over USB OUT endpoint are sent over CDC interface
 *         through this function.
 *
 *         @note
 *         This function will issue a NAK packet on any OUT packet received on
 *         USB endpoint until exiting this function. If you exit this function
 *         before transfer is complete on CDC interface (ie. using DMA controller)
 *         it will result in receiving more data while previous ones are still
 *         not sent.
 *
 * @param  Buf: Buffer of data to be received
 * @param  Len: Number of data received (in bytes)
 * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
 */
static int8_t CDC_Receive_HS(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 11 */

  USBD_CDC_SetRxBuffer(&hUsbDeviceHS, Buf);

/* While transmitting optically, accept STOP only. */
if (optical_tx_active) {
    if (*Len == 1U &&
        (Buf[0] == 0xB3U || Buf[0] == 0xB8U))
        pattern_demo_request = 2U;

    USBD_CDC_ReceivePacket(&hUsbDeviceHS);
    return USBD_OK;
}

/* STOP also cancels any incomplete incoming message. */
if (*Len == 1U &&
    (Buf[0] == 0xB3U || Buf[0] == 0xB8U)) {
    pattern_rx_state = 0U;
    pattern_demo_request = 2U;
    arm_dryrun_request = 4U;
    USBD_CDC_ReceivePacket(&hUsbDeviceHS);
    return USBD_OK;
}


/* During the dummy test, ignore everything except OFF. */
if (dummy_test_active) {
    if (*Len == 1U &&
        (Buf[0] == 0xB8U || Buf[0] == 0xB6U)) {
        arm_dryrun_request = 4U;
    }

    USBD_CDC_ReceivePacket(&hUsbDeviceHS);
    return USBD_OK;
}
  /* USB-controlled LED demonstration */
  if (*Len == 1U &&
      (Buf[0] == 0xB0U || Buf[0] == 0xB1U) &&
      !tx_bench_active &&
      !echo_tx_active &&
      echo_count == 0U)
  {
    HAL_GPIO_WritePin(
        LED2_GPIO_Port,
        LED2_Pin,
        Buf[0] == 0xB1U ? GPIO_PIN_SET : GPIO_PIN_RESET
        );


    USBD_CDC_ReceivePacket(&hUsbDeviceHS);
    return USBD_OK;
  }




/* B4 <length> <ASCII bytes>: GPIO-only transmission
 * BA <length> <ASCII bytes>: optical transmission
 * Supports USB packet fragmentation.
 */
if (*Len > 0U &&
    (pattern_rx_state != 0U ||
     Buf[0] == 0xB4U || Buf[0] == 0xBAU) && // NEW
    !tx_bench_active && !echo_tx_active && echo_count == 0U)
{
    if (pattern_rx_state != 0U &&
        (uint32_t)(HAL_GetTick() - pattern_rx_started_ms) > 1000U) {
        pattern_rx_state = 0U;
    }

    if (pattern_rx_state != 0U &&
        *Len == 1U && Buf[0] == 0xB3U) {
        pattern_rx_state = 0U;
        pattern_demo_request = 2U;
    } else {
        for (uint32_t i = 0U; i < *Len; ++i) {
            uint8_t b = Buf[i];

            if (pattern_rx_state == 0U) {
                if (b == 0xB4U || b == 0xBAU) { // NEW
                    pattern_rx_optical = (b == 0xBAU);
                    pattern_rx_started_ms = HAL_GetTick();
                    pattern_rx_state = 1U;
                }
            }
            else if (pattern_rx_state == 1U) {
                if (b >= 1U &&
                    b <= (pattern_rx_optical ?
                          OPTICAL_RX_LIMIT : PATTERN_RX_LIMIT)) { // NEW
                    pattern_rx_expected = b;
                    pattern_rx_count = 0U;
                    pattern_rx_state = 2U;
                } else {
                    pattern_rx_state = 0U;
                }
            }
            else {
                if (b < 0x20U || b > 0x7EU) {
                    pattern_rx_state = 0U;
                } else {
                    pattern_rx_buffer[pattern_rx_count++] = b;

                    if (pattern_rx_count == pattern_rx_expected) {
                        for (uint8_t j = 0U;
                             j < pattern_rx_count; ++j) {
                            pattern_demo_pending[j] =
                                pattern_rx_buffer[j];
                        }

                        pattern_demo_pending_len = pattern_rx_count;

                        // NEW: select optical or GPIO-only
                        pattern_demo_request =
                            pattern_rx_optical ? 4U : 3U;

                        pattern_rx_state = 0U;
                    }
                }
            }
        }
    }

    USBD_CDC_ReceivePacket(&hUsbDeviceHS);
    return USBD_OK;
}
 /* PC5 digital-arm LINE TEST ONLY; do not connect a laser or fit JP1.
   * B5 = 500 ms pulse on PC5; B6 = force LOW. PE2 stays LOW. */


  /* Isolated PHY-A dry-run commands.
   * B5 = PC5 pulse
   * B6 = PC5 / PE2 off
   * B7 = PE2 pulse
   * B8 = PC5 / PE2 off
   */
if (*Len == 1U &&
    (Buf[0] == 0xB5U || Buf[0] == 0xB6U ||
     Buf[0] == 0xB7U || Buf[0] == 0xB8U ||
     Buf[0] == 0xB9U) &&
    pattern_rx_state == 0U &&
    !tx_bench_active &&
    !echo_tx_active &&
    echo_count == 0U)
{
    arm_dryrun_request = (uint8_t)(Buf[0] - 0xB4U);

    USBD_CDC_ReceivePacket(&hUsbDeviceHS);
    return USBD_OK;
}
  /* Single-byte, self-terminating GPIO demo commands, idle USB only. */
  if (*Len == 1U && (Buf[0] == 0xB2U || Buf[0] == 0xB3U) &&
      !tx_bench_active && !echo_tx_active && echo_count == 0U)
  {
    pattern_demo_request = (Buf[0] == 0xB2U) ? 1U : 2U;
    USBD_CDC_ReceivePacket(&hUsbDeviceHS);
    return USBD_OK;
  }

  /* Runtime trace-state experiment */
  if (*Len == 1U &&
      (Buf[0] == 0xA3U || Buf[0] == 0xA4U) &&
      !tx_bench_active &&
      !echo_tx_active &&
      echo_count == 0U)
  {
    if (Buf[0] == 0xA4U)
    {
      CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    }
    else
    {
      CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
    }

    USBD_CDC_ReceivePacket(&hUsbDeviceHS);
    return USBD_OK;
  }



  // Single-byte command: start TX-only benchmark.
  if (*Len == 1U &&
      Buf[0] == 0xA5U &&
      !tx_bench_active &&
      !echo_tx_active &&
      echo_count == 0U)
  {
    // Reset benchmark state.
    tx_bench_sent = 0;
    tx_bench_inflight = 0;
    tx_bench_active = 1;

    // Begin transmitting generated data.
    TxBench_Kick();

    // Do not rearm USB reception yet.
    // We'll resume receiving after the benchmark finishes.
    return USBD_OK;
  }


  // Reject unexpected packet sizes or a full queue.
  if (*Len > ECHO_PACKET_SIZE || echo_count >= ECHO_DEPTH)
  {
    return USBD_FAIL;
  }


  if (*Len > 0)
  {
    // Count received packets.
    rx_packet_count++;

    rx_total_bytes += *Len;

    if (*Len < rx_min_length)
    {
      rx_min_length = *Len;
    }

    if (*Len > rx_max_length)
    {
      rx_max_length = *Len;
    }


    // Count full-size HS USB packets.
    if (*Len == 512U)
    {
      rx_512_count++;
    }

    // Copy received data into the queue.
    memcpy(echo_queue[echo_tail], Buf, *Len);

    echo_lengths[echo_tail] = (uint16_t)*Len;
    echo_tail = (echo_tail + 1U) % ECHO_DEPTH;
    echo_count++;


    if (echo_count > queue_max_depth)
    {
      queue_max_depth = echo_count;
    }


    // Count occasions when the queue reaches capacity.
    if (echo_count == ECHO_DEPTH)
    {
      queue_full_count++;
    }
  }


  // Start transmitting if USB TX is idle.
  Echo_KickTx();

  // Rearm reception if there's room in the queue.
  if (echo_count < ECHO_DEPTH)
  {
    USBD_CDC_ReceivePacket(&hUsbDeviceHS);
  }
  else
  {
    // Stop accepting packets until a slot is freed.
    echo_rx_paused = 1;
  }

  return USBD_OK;

  /* USER CODE END 11 */
}

/**
 * @brief  Data to send over USB IN endpoint are sent over CDC interface
 *         through this function.
 * @param  Buf: Buffer of data to be sent
 * @param  Len: Number of data to be sent (in bytes)
 * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL or USBD_BUSY
 */
uint8_t CDC_Transmit_HS(uint8_t* Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 12 */
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceHS.pClassData;
  if (hcdc->TxState != 0){
    return USBD_BUSY;
  }
  USBD_CDC_SetTxBuffer(&hUsbDeviceHS, Buf, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceHS);
  /* USER CODE END 12 */
  return result;
}

/**
 * @brief  CDC_TransmitCplt_HS
 *         Data transmitted callback
 *
 *         @note
 *         This function is IN transfer complete callback used to inform user that
 *         the submitted Data is successfully sent over USB.
 *
 * @param  Buf: Buffer of data to be received
 * @param  Len: Number of data received (in bytes)
 * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
 */
static int8_t CDC_TransmitCplt_HS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 14 */

  UNUSED(Buf);
  UNUSED(Len);
  UNUSED(epnum);
  tx_complete_count++;


  /* Handle TX-only benchmark transfers */
  if (tx_bench_active)
  {
    // The previous USB transmission has completed.
    tx_bench_inflight = 0;

    if (tx_bench_sent < TX_BENCH_TOTAL)
    {
      // Generate and transmit the next chunk.
      TxBench_Kick();
    }
    else
    {
      // All 10 MiB have finished transmitting.
      tx_bench_active = 0;

      // Restore USB reception for normal echo mode.
      USBD_CDC_ReceivePacket(&hUsbDeviceHS);
    }

    return result;
  }


  // The previous packet has finished transmitting.
  if (echo_tx_active && echo_count > 0)
  {
    echo_tx_active = 0;

    // Release the oldest packet.
    echo_head = (echo_head + 1U) % ECHO_DEPTH;
    echo_count--;
  }

  // If the queue was full, we now have room.
  // Allow USB to receive another packet.
  if (echo_rx_paused && echo_count < ECHO_DEPTH)
  {
    echo_rx_paused = 0;
    USBD_CDC_ReceivePacket(&hUsbDeviceHS);
  }

  // Begin transmitting the next queued packet.
  Echo_KickTx();

  /* USER CODE END 14 */
  return result;
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */


static void Echo_KickTx(void)
{
  // Don't start another transfer if USB is busy
  // or the queue is empty.
  if (echo_tx_active || echo_count == 0)
    return;

  // Get the oldest packet in the queue.
  uint8_t *data = echo_queue[echo_head];
  uint16_t length = echo_lengths[echo_head];

  // Start asynchronous USB transmission.
  if (CDC_Transmit_HS(data, length) == USBD_OK)
  {
    echo_tx_active = 1;
  }
}


static void TxBench_Kick(void)
{
  // Only submit data when the previous transfer
  // has finished and benchmarking is active.
  if (!tx_bench_active || tx_bench_inflight)
    return;

  if (tx_bench_sent >= TX_BENCH_TOTAL)
    return;

  uint32_t remaining = TX_BENCH_TOTAL - tx_bench_sent;

  uint16_t length = (remaining > TX_BENCH_CHUNK)
    ? TX_BENCH_CHUNK
    : (uint16_t)remaining;

  uint32_t offset = tx_bench_sent;

  // Generate deterministic benchmark data.
  for (uint16_t i = 0; i < length; i++)
  {
    uint32_t pos = offset + i;

    tx_bench_buffer[i] = (uint8_t)(
        pos ^ (pos >> 8) ^
        (pos >> 16) ^ (pos >> 24)
        );
  }


  // Submit the asynchronous USB transfer.
  if (CDC_Transmit_HS(tx_bench_buffer, length) == USBD_OK)
  {
    tx_bench_inflight = 1;
    tx_bench_sent += length;
  }
}



/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
 * @}
 */

/**
 * @}
 */

