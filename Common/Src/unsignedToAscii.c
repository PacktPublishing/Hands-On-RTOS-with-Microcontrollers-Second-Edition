/*
 Title:  function unsignedToAscii()

Licenses:
- Copyright (c) 2025 Packt Publishing, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-Second-Edition

*/

#include <stm32f7xx_hal.h>

/*
  FUNCTION: unsignedToAscii()
  DESCRIPTION: convert uint32_t variable to ASCII
  PARAMTERS:
  * num : uint32_t variable to be converted
  * buffPtr : pointer for the output buffer, where the ASCII number is placed
              * buffPtr should point one byte past the end of the buffer
              * The buffer should be at least 11 bytes: 10 bytes for largest number, and 1 byte for null terminator
  RETURN:
  * pointer to the start of the ASCII number, followed by a null terminator

  NOTES: the alternative function sprintf() can be problematic in FreeRTOS, e.g., not thread-safe
 */
char *unsignedToAscii(uint32_t num, char *buffPtr)
{
    // Set null terminator in output buffer
    *--buffPtr = 0;
    // If num is zero
    if (!num){
        *--buffPtr = '0';
    }
    else {
        // Divide num by 10, until it is 0
        for (; num; num/=10) *--buffPtr = '0'+(num%10);
    }
    return buffPtr;
}
