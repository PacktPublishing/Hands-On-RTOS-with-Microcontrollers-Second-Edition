/*

---------------------------------------------------------------------------------------

Licenses:
- Copyright (c) 2019-2025 Packt Publishing, under the MIT License.
- Based on code copyrighted by Brian Amos, 2019, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-2nd-edition

 */

#include <FreeRTOS.h>
#include <task.h>
#include <Nucleo_F767ZI_GPIO.h>
#include <Nucleo_F767ZI_Init.h>
#include <stm32f7xx_hal.h>
#include <string.h>
#include <stdio.h>
#include <vcpDriverMultiTask.h>
#include <stdbool.h>
#include <unsignedToAscii.h>


/* This driver supports sending data over USB, to the VCP on the USB host.
 * * This driver can be used by multiple tasks.
 */

#define STACK_SIZE 512

void sendDataTask( void* Number);

int main(void)
{
    // Initialize the hardware and HAL
    HWInit();

    // Initialize the USB VCP connection, and driver.
    vcpInit();

    // Ensure proper priority grouping for FreeRTOS
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    // Setup tasks, making sure they have been properly created before moving on
    assert_param(xTaskCreate(sendDataTask, "sendDataTask #1", STACK_SIZE, (void*)1, tskIDLE_PRIORITY + 2, NULL) == pdPASS);
    assert_param(xTaskCreate(sendDataTask, "sendDataTask #2", STACK_SIZE, (void*)2, tskIDLE_PRIORITY + 2, NULL) == pdPASS);

    // Start the scheduler - shouldn't return unless there's a problem
    vTaskStartScheduler();

    // If you've wound up here, there is likely an issue with over-running the FreeRTOS heap
    while(1)
    {
    }
}

void sendDataTask( void* Number)
{
    // Make the send-buffer the same length as the driver's stream-buffer.
    #define SEND_BUFFER_LEN  VCP_DRIVER_MULTI_TASK_BUFFER_LEN
    // Define two buffers, one for each task
    static char sendBuffers[2][SEND_BUFFER_LEN] = {0};

    static uint32_t sendCount[2] = {0};

    #define NUM_BUFFER_SIZE 11
    char numBuffer[NUM_BUFFER_SIZE];
    char * numPtr;

    // Performance-analysis data
    static uint32_t test_sendDataTask_sendError[2];
    static uint32_t test_sendDataTask_mutexError[2];
    static uint32_t test_sendDataTask_maxSendTime[2];
    static uint32_t test_sendDataTask_minSendTime[2];

    bool resetPerformed = false;
    int32_t sendResult;
    TickType_t startSend, sendTime;

    uint32_t taskNumber;
    taskNumber = (uint32_t) Number;

    char * sendBufferPtr;
    sendBufferPtr = sendBuffers[taskNumber-1];

    // In the message prefix, set the task number
    // * "task #1: " or "task #2: "
    char messagePrefix[] = "task #_: ";
    messagePrefix[6] = '0'+ taskNumber;

    while(1)
    {

        // Reset the performance-analysis data, e.g., set the counters to 0.
        // * This is done once, 30 seconds after the driver starts.
        // * When the driver's timer expires, the timer's call-back sets
        //   vcpResetPerformanceData to 1.
        // * vcpResetPerformanceData is defined in Drivers/HandsOnRTOS/vcpDriverMultiTask.c
        if ( (resetPerformed == false) && (vcpResetPerformanceData == 1) )  {
            sendCount[taskNumber-1] = 0;
            test_sendDataTask_sendError[taskNumber-1] = 0;
            test_sendDataTask_mutexError[taskNumber-1] = 0;
            test_sendDataTask_maxSendTime[taskNumber-1] = 0;
            test_sendDataTask_minSendTime[taskNumber-1] = portMAX_DELAY;
            resetPerformed = true;
        }

        sendCount[taskNumber-1]++;

        // Create the message to send:
        // * "task #<task_number>: <task_message_count><new_line><hex_zeros>"
        // * Example: "task #1: 1234567890\n\0x00..."
        // * The message is created at the beginning of the send-buffer.
        // * The message is up to 21 bytes, including the null-terminator.
        // * In the send-buffer, the data after the message is hex zeros.
        memset( sendBufferPtr, 0, VCP_DRIVER_MULTI_TASK_BUFFER_LEN );
        numPtr = unsignedToAscii( sendCount[taskNumber-1], (numBuffer+NUM_BUFFER_SIZE) );
        strcpy( sendBufferPtr, messagePrefix);
        strcpy( (sendBufferPtr+strlen(sendBufferPtr)), numPtr);
        strcpy( (sendBufferPtr+strlen(sendBufferPtr)), "\n");

        // Needed for calculating send-time
        startSend = xTaskGetTickCount();

        // Call the driver, to send the data over USB
		// * The second argument specifies how many bytes to send.
		// * The third argument specifies the maximum wait-time
        sendResult = vcpSend((uint8_t*) sendBufferPtr, 100, 100);

        // Record data for performance analysis

        // Record the min and max send-time, for performance analysis.
		// * This calculation assumes the FreeRTOS tick-counter has not wrapped		
        sendTime =  xTaskGetTickCount() - startSend;
        if ( sendTime > test_sendDataTask_maxSendTime[taskNumber-1] ){
            test_sendDataTask_maxSendTime[taskNumber-1] = sendTime;
        }
        if ( sendTime < test_sendDataTask_minSendTime[taskNumber-1] ){
            test_sendDataTask_minSendTime[taskNumber-1] = sendTime;
        }
        // Record errors returned from vcpSend
        if (sendResult == VCP_SEND_CANNOT_SEND){
            test_sendDataTask_sendError[taskNumber-1]++;
        }
        else if (sendResult == VCP_SEND_MUTEX_NOT_AVAILABLE){
            test_sendDataTask_mutexError[taskNumber-1]++;
        }

        // Delays, to avoid back-to-back sends.
        // Comment-out this code for back-to-back sends.
        if (taskNumber == 1){
            vTaskDelay(1);
        }
        else{
            vTaskDelay(2);
        }

    }  // END OF: while(1)
}
