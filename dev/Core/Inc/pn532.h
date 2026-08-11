/* pn532.h - PN532 HSU interface separated from main.c */
#ifndef PN532_H
#define PN532_H

#include "main.h"
#include <stdint.h>

#define PN532_MAX_FRAME 64

/* Public API */
uint8_t PN532_Init(void);
uint8_t PN532_ReadCard(uint8_t *uid, uint8_t *uid_len);

/* Debug / status variables (defined in pn532.c) */
extern volatile uint8_t pn532_debug_state;
extern volatile uint8_t pn532_debug_ack[6];
extern volatile uint8_t pn532_debug_rx[PN532_MAX_FRAME];
extern volatile uint8_t pn532_debug_rx_len;
extern volatile uint8_t pn532_debug_uid[10];
extern volatile uint8_t pn532_debug_uid_len;
extern volatile uint32_t pn532_debug_errors;
extern volatile uint8_t last_valid_frame[PN532_MAX_FRAME];
extern volatile uint8_t last_valid_frame_len;
extern volatile HAL_StatusTypeDef debug_uart_status;
extern volatile uint32_t debug_uart_error_code;
extern volatile uint8_t debug_ack_buffer[6];

#endif /* PN532_H */
