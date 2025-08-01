/*

---------------------------------------------------------------------------------------

Licenses:
- Copyright (c) 2019-2025 Packt Publishing, under the MIT License.
- Based on code copyrighted by Brian Amos, 2019, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-2nd-edition

 */
#ifndef DRIVERS_HANDSONRTOS_VCPDRIVER_MULTI_TASK_H_
#define DRIVERS_HANDSONRTOS_VCPDRIVER_MULTI_TASK_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <FreeRTOS.h>
#include <stream_buffer.h>

void vcpInit( void );

StreamBufferHandle_t const* GetUsbRxStreamBuff( void );

int32_t vcpSend(uint8_t * Buff, uint16_t Len, TickType_t ticksToWait);

extern volatile uint32_t vcpResetPerformanceData;

// Using the same buffer-length as in usbd_cdc_if.c
#define VCP_DRIVER_MULTI_TASK_BUFFER_LEN 1024

#define VCP_SEND_CANNOT_SEND -1
#define VCP_SEND_MUTEX_NOT_AVAILABLE -2

#ifdef __cplusplus
}
#endif
#endif /* DRIVERS_HANDSONRTOS_VCPDRIVER_MULTI_TASK_H_ */
