/*

---------------------------------------------------------------------------------------

Licenses:
- Copyright (c) 2019-2025 Packt Publishing, under the MIT License.
- Based on code copyrighted by Brian Amos, 2019, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-Second-Edition

 */
#ifndef DRIVERS_HANDSONRTOS_VCPDRIVER_H_
#define DRIVERS_HANDSONRTOS_VCPDRIVER_H_
#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <FreeRTOS.h>
#include <stream_buffer.h>

void vcpInit( void );

uint32_t vcpSend(uint8_t * Buff, uint16_t Len);

#ifdef __cplusplus
 }
#endif
#endif /* DRIVERS_HANDSONRTOS_VCPDRIVER_H_ */
