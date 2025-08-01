/*

---------------------------------------------------------------------------------------

Licenses:
- Copyright (c) 2019-2025 Packt Publishing, under the MIT License.
- Based on code copyrighted by Brian Amos, 2019, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-Second-Edition

 */

#include <FreeRTOS.h>
#include <task.h>
#include <Nucleo_F767ZI_GPIO.h>
#include <SEGGER_SYSVIEW.h>
#include <Nucleo_F767ZI_Init.h>
#include <stm32f7xx_hal.h>
#include "vcpDriver.h"
#include <string.h>
#include <stdio.h>
#include <unsignedToAscii.h>

/* This program uses the driver vcpDriver, to send data over USB.
   * A single task calls the driver, to send data over USB.
 */

#define STACK_SIZE 128

void sendDataTask( void* NotUsed);

int main(void)
{
    // Initialize the hardware and HAL
    HWInit();

    // Initialize USB, and the driver vcpDriver
    vcpInit();

    SEGGER_SYSVIEW_Conf();

    // Ensure proper priority grouping for FreeRTOS
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    // Setup the task that sends data, making sure it has been properly created before moving on
    assert_param(xTaskCreate(sendDataTask, "sendDataTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL) == pdPASS);

    // Start the scheduler - shouldn't return unless there's a problem
    vTaskStartScheduler();

    // If you've wound up here, there is likely an issue with over-running the FreeRTOS heap
    while(1)
    {
    }
}

void sendDataTask( void* NotUsed)
{

    #define STRING_BUFFER_SIZE 25
    char stringBuffer[STRING_BUFFER_SIZE];
    #define NUM_BUFFER_SIZE 11
    char numBuffer[NUM_BUFFER_SIZE];
    char * numPtr;

    uint32_t messageACount = 0;
    uint32_t messageBCount = 0;

    while(1)
    {
        // Create the message to send:  message_A: <messageAcount>
        numPtr = unsignedToAscii( ++messageACount, (numBuffer+NUM_BUFFER_SIZE) );
        strcpy(stringBuffer, "message_A: ");
        strcpy(stringBuffer+strlen(stringBuffer), numPtr);
        strcpy(stringBuffer+strlen(stringBuffer), "\n");

        //SEGGER_SYSVIEW_PrintfHost(stringBuffer);
        vcpSend((uint8_t*) stringBuffer, strlen(stringBuffer));

        // Create the message to send:  message_B: <messageBcount>
        numPtr = unsignedToAscii( ++messageBCount, (numBuffer+NUM_BUFFER_SIZE) );
        strcpy(stringBuffer, "message_B: ");
        strcpy(stringBuffer+strlen(stringBuffer), numPtr);
        strcpy(stringBuffer+strlen(stringBuffer), "\n");

        //SEGGER_SYSVIEW_PrintfHost(stringBuffer);
        vcpSend((uint8_t*) stringBuffer, strlen(stringBuffer));

        vTaskDelay(2);
    }
}
