/* pn532.c - PN532 HSU implementation moved out of main.c */
#include "pn532.h"
#include <string.h>

/* extern UART handles from main.c */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* Debug state vars */
volatile uint8_t  pn532_debug_state = 0;
volatile uint8_t  pn532_debug_ack[6] = {0};
volatile uint8_t  pn532_debug_rx[PN532_MAX_FRAME] = {0};
volatile uint8_t  pn532_debug_rx_len = 0;
volatile uint8_t  pn532_debug_uid[10] = {0};
volatile uint8_t  pn532_debug_uid_len = 0;
volatile uint32_t pn532_debug_errors = 0;
volatile uint8_t  last_valid_frame[PN532_MAX_FRAME] = {0};
volatile uint8_t  last_valid_frame_len = 0;
volatile HAL_StatusTypeDef debug_uart_status = HAL_OK;
volatile uint32_t           debug_uart_error_code = 0;
volatile uint8_t            debug_ack_buffer[6] = {0};

static const uint8_t PN532_ACK[] = {0x00,0x00,0xFF,0x00,0xFF,0x00};

/* local debug helpers */
static void Debug_PrintHex(const char *prefix, uint8_t *data, uint8_t len)
{
    char buf[128];
    int idx = 0;

    if (prefix)
    {
        while (*prefix && idx < (int)sizeof(buf) - 8)
        {
            buf[idx++] = *prefix++;
        }
        if (idx < (int)sizeof(buf) - 8)
            buf[idx++] = ' ';
    }

    for (int i = 0; i < len && idx < (int)sizeof(buf) - 8; i++)
    {
        uint8_t hi = (data[i] >> 4) & 0x0F;
        uint8_t lo = data[i] & 0x0F;
        buf[idx++] = (hi < 10) ? ('0' + hi) : ('A' + hi - 10);
        buf[idx++] = (lo < 10) ? ('0' + lo) : ('A' + lo - 10);
        buf[idx++] = ' ';
    }

    if (idx < (int)sizeof(buf) - 2)
    {
        buf[idx++] = '\r';
        buf[idx++] = '\n';
    }

    HAL_UART_Transmit(&huart1, (uint8_t *)buf, idx, 200);
}

static void Debug_PrintStr(const char *s)
{
    if (!s)
        return;
    HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), 200);
}


static HAL_StatusTypeDef PN532_SendCommand(uint8_t *data, uint8_t len)
{
    uint8_t frame[32];
    uint8_t i;
    uint8_t checksum = 0;
    uint8_t index = 0;

    if (len > 24)
        return HAL_ERROR;

    frame[index++] = 0x00;
    frame[index++] = 0x00;
    frame[index++] = 0xFF;

    frame[index++] = len;
    frame[index++] = (uint8_t)(0x100 - len);

    for (i = 0; i < len; i++)
    {
        frame[index++] = data[i];
        checksum += data[i];
    }

    frame[index++] = (uint8_t)(0x100 - checksum);
    frame[index++] = 0x00;

    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart2,
                                                  frame,
                                                  index,
                                                  100);

    Debug_PrintHex("PN532 TX", frame, index);

    if (status == HAL_OK)
        pn532_debug_state = 1;
    else
    {
        pn532_debug_state = 5;
        pn532_debug_errors++;
    }

    return status;
}

static uint8_t PN532_ReadAck(void)
{
    uint8_t ack[6];

    debug_uart_status = HAL_UART_Receive(&huart2,
                                         ack,
                                         6,
                                         5000);

    debug_uart_error_code = huart2.ErrorCode;

    for(int i=0; i<6; i++) {
        debug_ack_buffer[i] = ack[i];
        pn532_debug_ack[i] = ack[i];
    }

    Debug_PrintHex("PN532 ACK", ack, 6);

    if (debug_uart_status != HAL_OK)
    {
        Debug_PrintStr("PN532 ACK: HAL_UART_Receive error\r\n");
        return 0;
    }

    if (memcmp(ack, PN532_ACK, 6) == 0)
    {
        pn532_debug_state = 2;
        return 1;
    }

    pn532_debug_state = 5;
    pn532_debug_errors++;
    return 0;
}

static uint8_t PN532_ReadFrame(uint8_t *data,
                               uint8_t *data_len,
                               uint32_t timeout)
{
    uint8_t byte;
    uint8_t len;
    uint8_t lcs;
    uint8_t checksum;
    uint8_t i;

    if (HAL_UART_Receive(&huart2,
                         &byte,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    while (byte != 0x00)
    {
        if (HAL_UART_Receive(&huart2,
                             &byte,
                             1,
                             timeout) != HAL_OK)
        {
            return 0;
        }
    }

    if (HAL_UART_Receive(&huart2,
                         &byte,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    if (byte != 0x00)
        return 0;

    if (HAL_UART_Receive(&huart2,
                         &byte,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    if (byte != 0xFF)
        return 0;

    if (HAL_UART_Receive(&huart2,
                         &len,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    if (len == 0)
    {
        uint8_t tmp;

        HAL_UART_Receive(&huart2,
                         &tmp,
                         1,
                         timeout);

        HAL_UART_Receive(&huart2,
                         &tmp,
                         1,
                         timeout);

        HAL_UART_Receive(&huart2,
                         &tmp,
                         1,
                         timeout);

        return 0;
    }

    if (HAL_UART_Receive(&huart2,
                         &lcs,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    if ((uint8_t)(len + lcs) != 0)
        return 0;

    if (len > PN532_MAX_FRAME)
        return 0;

    checksum = 0;

    for (i = 0; i < len; i++)
    {
        if (HAL_UART_Receive(&huart2,
                             &data[i],
                             1,
                             timeout) != HAL_OK)
        {
            return 0;
        }

        checksum += data[i];
    }

    if (HAL_UART_Receive(&huart2,
                         &byte,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    if ((uint8_t)(checksum + byte) != 0)
        return 0;

    if (HAL_UART_Receive(&huart2,
                         &byte,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    if (byte != 0x00)
        return 0;

    *data_len = len;
    memcpy((void *)pn532_debug_rx, data, len);
    pn532_debug_rx_len = len;
    pn532_debug_state = 3;

    Debug_PrintHex("PN532 RX", data, len);

    return 1;
}

static void PN532_Wakeup(void)
{
    uint8_t wakeup_packet[] = {
        0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0x03, 0xFD, 0xD4, 0x14, 0x01, 0x17, 0x00
    };

    HAL_UART_Transmit(&huart2, wakeup_packet, sizeof(wakeup_packet), 500);
    Debug_PrintHex("PN532 WAKEUP", (uint8_t *)wakeup_packet, sizeof(wakeup_packet));
    HAL_Delay(100);
}

uint8_t PN532_Init(void)
{
    PN532_Wakeup();

    uint8_t cmd[] = {
        0x00, 0x00, 0xFF,
        0x02, 0xFE,
        0xD4, 0x02,
        0x2A,
        0x00
    };

    __HAL_UART_CLEAR_OREFLAG(&huart2);

    if (HAL_UART_Transmit(&huart2, cmd, sizeof(cmd), 500) != HAL_OK)
    {
        Debug_PrintStr("PN532 Init: HAL_UART_Transmit failed\r\n");
        return 0;
    }

    uint8_t dummy_ack[6];
    if (HAL_UART_Receive(&huart2, dummy_ack, 6, 500) != HAL_OK)
    {
        Debug_PrintStr("PN532 Init: HAL_UART_Receive ACK failed\r\n");
        Debug_PrintHex("PN532 ACK got", dummy_ack, 6);
        return 0;
    }

    return 1;
}

uint8_t PN532_ReadCard(uint8_t *uid,
                              uint8_t *uid_len)
{
    uint8_t command[] =
    {
        0xD4,
        0x4A,
        0x01,
        0x00
    };

    uint8_t response[PN532_MAX_FRAME];
    uint8_t response_len;

    if (PN532_SendCommand(command,
                          sizeof(command)) != HAL_OK)
    {
        return 0;
    }

    if (!PN532_ReadAck())
    {
        return 0;
    }

    if (!PN532_ReadFrame(response,
                         &response_len,
                         1500))
    {
        return 0;
    }

    if (response_len < 3)
        return 0;

    if (response[0] != 0xD5 ||
        response[1] != 0x4B)
    {
        return 0;
    }

    if (response[2] == 0)
    {
        return 0;
    }

    if (response_len < 8)
        return 0;

    *uid_len = response[7];

    if (*uid_len > 10)
        return 0;

    if ((8 + *uid_len) > response_len)
        return 0;

    memcpy(uid,
           &response[8],
           *uid_len);

    memcpy((void *)last_valid_frame, response, response_len);
    last_valid_frame_len = response_len;

    memcpy((void *)pn532_debug_uid, uid, *uid_len);
    pn532_debug_uid_len = *uid_len;

    pn532_debug_state = 4;

    return 1;
}
