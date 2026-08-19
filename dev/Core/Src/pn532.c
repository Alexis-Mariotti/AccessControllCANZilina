/* pn532.c - PN532 HSU implementation moved out of main.c */
#include "pn532.h"
#include <string.h>

/* extern UART handles from main.c */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

// ToDo: remove after figured out how to wakeup the PN532
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

    if (debug_uart_status != HAL_OK)
    {
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

    return 1;
}





void PN532_wakeup(UART_HandleTypeDef *huart)
{
	// reset the PN532 by toggling the RST pin
    HAL_GPIO_WritePin(PN532_RST_GPIO_Port, PN532_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(PN532_RST_GPIO_Port, PN532_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(400);
    HAL_GPIO_WritePin(PN532_RST_GPIO_Port, PN532_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10); // Pause indispensable post-reset

    // Sending a wakeup frame (20 bytes of 0x55) to the PN532
    uint8_t wakeup_buffer[20];
    memset(wakeup_buffer, PN532_WAKEUP_FRAME, sizeof(wakeup_buffer));

    HAL_UART_Transmit(huart, wakeup_buffer, sizeof(wakeup_buffer), 100);

    HAL_Delay(10);
}

uint8_t PN532_Init(void)
{
	PN532_wakeup(&huart2);

    // empty the UART RX buffer to ensure no residual data from previous operations
    __HAL_UART_CLEAR_OREFLAG(&huart2);
    uint8_t dummy;
    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET) {
        HAL_UART_Receive(&huart2, &dummy, 1, 10);
    }

    // Frame: D4 02 (GetFirmwareVersion)
    uint8_t cmd[] = {
        0x00, 0x00, 0xFF, // Preamble + Start Code
        0x02, 0xFE,       // LEN (2) + LCS (FE)
        0xD4, 0x02,       // TFI + Command Code (GetFirmwareVersion)
        0x2A,             // DCS (Checksum)
        0x00              // Postambule
    };

    if (HAL_UART_Transmit(&huart2, cmd, sizeof(cmd), 500) != HAL_OK)
    {
        return 0;
    }

    // read the ACK frame
    uint8_t ack[6];
    if (HAL_UART_Receive(&huart2, ack, 6, 1000) != HAL_OK) // Augmenté à 1000ms pour être sûr
    {
        return 0;
    }

    // validate the ACK frame
    const uint8_t PN532_ACK_LOCAL[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    if (memcmp(ack, PN532_ACK_LOCAL, 6) != 0)
    {
        return 0;
    }

    // reading the response frame for GetFirmwareVersion
    uint8_t fw_response[32];
    uint8_t fw_len;
    PN532_ReadFrame(fw_response, &fw_len, 1000);

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
