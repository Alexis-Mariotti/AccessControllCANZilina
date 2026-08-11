#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include "main.h"

// Type of request for access control
typedef enum {
    REQ_UNDEFINED = 0x00, // 00
    REQ_DOOR_SEND = 0x01, // 01 (request from the door)
    REQ_OK        = 0x02, // 10 (OK)
    REQ_NOT_OK    = 0x03  // 11 (NOT_OK)
} CAN_RequestType_t;


void CAN_Manager_Init(void);
uint8_t CAN_SendAccessRequest(uint32_t door_id, CAN_RequestType_t req_type, uint8_t *uid, uint8_t uid_len);

#endif /* CAN_MANAGER_H */
