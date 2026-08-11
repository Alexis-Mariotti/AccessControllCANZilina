#include "can_manager.h"

/* Ensure the CAN handle is visible in this translation unit */
extern CAN_HandleTypeDef hcan;

// Global variable to maintain the rolling counter across CAN messages ( 6 bits)
static uint8_t rolling_counter = 0;

/**
 * @brief Init and start CAN controller
 */
void CAN_Manager_Init(void)
{
    CAN_FilterTypeDef sFilterConfig;

    /* Configure CAN filter to accept ALL standard messages (for testing)
     * FilterBank 0: ID mask mode, accept messages with any standard ID
     */
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;        /* Accept any ID */
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;    /* Mask: 0 = don't care about any bits */
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;

    if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_CAN_Start(&hcan) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief Build the payload and send a CAN message for access control request
 *
 * We use a specific format for the payload
 * Format for Byte 0 :
 * - Bits [7:6] : Request type (01 for a request from the door, 10 for OK, 11 for NOT_OK, 00 undefined)
 * - Bits [5:0] : Rolling counter (0 to 63)
 * Bytes [1 to 7] : card's UID
 */
uint8_t CAN_SendAccessRequest(uint32_t door_id, CAN_RequestType_t req_type, uint8_t *uid, uint8_t uid_len)
{
    CAN_TxHeaderTypeDef TxHeader;
    uint32_t TxMailbox;
    uint8_t payload[8] = {0};


    payload[0] = ((req_type & 0x03) << 6) | (rolling_counter & 0x3F);

    // Rolling counter is incremented
    rolling_counter = (rolling_counter + 1) & 0x3F;

    // Copy the UID into the payload, ensuring we don't exceed 7 bytes
    uint8_t copy_len = (uid_len > 7) ? 7 : uid_len;
    for (uint8_t i = 0; i < copy_len; i++)
    {
        payload[1 + i] = uid[i];
    }

    // Configure the CAN header
    TxHeader.StdId = door_id;        // ID of the door (11 bits)
    TxHeader.ExtId = 0x00;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.DLC = 8;                // 8 bytes of data
    TxHeader.TransmitGlobalTime = DISABLE;

    // Check if there is a free mailbox to send the message
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0)
    {
        if (HAL_CAN_AddTxMessage(&hcan, &TxHeader, payload, &TxMailbox) != HAL_OK)
        {
            return 0; // Sending failed
        }
        return 1; // Successfully sent
    }

    return 0; // No free mailbox available
}
