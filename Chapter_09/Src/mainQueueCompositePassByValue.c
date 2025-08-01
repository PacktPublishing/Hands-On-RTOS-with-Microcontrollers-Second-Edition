/**

---------------------------------------------------------------------------------------

Licenses:
- Copyright (c) 2019-2025 Packt Publishing, under the MIT License.
- Based on code copyrighted by Brian Amos, 2019, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-2nd-edition

 */

#include <FreeRTOS.h>
#include <Nucleo_F767ZI_GPIO.h>
#include <queue.h>
#include <SEGGER_SYSVIEW.h>
#include <Nucleo_F767ZI_Init.h>
#include <stm32f7xx_hal.h>

/*********************************************
 * A simple demonstration of using queues across
 * multiple tasks with pass by value.
 * This time, a large struct is copied into
 * the queue.
 *********************************************/

#define STACK_SIZE 128

/**
 * Define a larger structure to also specify the number of
 * milliseconds this state should last.
 * Notice the difference in the queue's item-size.
 */
typedef struct
{
    uint8_t redLEDState : 1;    // Specify this variable as 1 bit wide
    uint8_t blueLEDState : 1;   // Specify this variable as 1 bit wide
    uint8_t greenLEDState : 1;  // Specify this variable as 1 bit wide
    uint32_t msDelayTime;   // Minimum number of ms to remain in this state
} LedStates_t;

void recvTask( void* NotUsed );
void sendingTask( void* NotUsed );

// This is a handle for the queue that will be used by
// recvTask and sendingTask
static QueueHandle_t ledCmdQueue = NULL;

int main(void)
{
    HWInit();
    SEGGER_SYSVIEW_Conf();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4); //ensure proper priority grouping for freeRTOS

    // Setup tasks, making sure they have been properly created before moving on
    assert_param(xTaskCreate(recvTask, "recvTask", STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1), NULL) == pdPASS);
    assert_param(xTaskCreate(sendingTask, "sendingTask", STACK_SIZE, NULL, (configMAX_PRIORITIES - 1), NULL) == pdPASS);

    /**
     * Create a queue that can store up to 8 copies of the struct.
     * Using sizeof allows us to modify the struct and have the queue-item's
     * storage sized at compile time
     */
    ledCmdQueue = xQueueCreate(8, sizeof(LedStates_t));
    assert_param(ledCmdQueue != NULL);

    // Start the scheduler - shouldn't return unless there's a problem
    vTaskStartScheduler();

    // If you've wound up here, there is likely an issue with over-running the freeRTOS heap
    while(1)
    {
    }
}

/**
 * This receive task watches the queue for a new command added to it
 */
void recvTask( void* NotUsed )
{
    LedStates_t nextCmd;

    while(1)
    {
        xQueueReceive(ledCmdQueue, &nextCmd, portMAX_DELAY);

        if(nextCmd.redLEDState == 1)
            RedLed.On();
        else
            RedLed.Off();

        if(nextCmd.blueLEDState == 1)
            BlueLed.On();
        else
            BlueLed.Off();

        if(nextCmd.greenLEDState == 1)
            GreenLed.On();
        else
            GreenLed.Off();

        vTaskDelay(nextCmd.msDelayTime/portTICK_PERIOD_MS);
    }
}

/**
 * sendingTask modifies a single nextStates variable
 * and passes it to the queue.
 * Each time the variable is passed to the queue, its
 * value is copied into the queue, which is allowed to
 * fill to capacity.
 */
void sendingTask( void* NotUsed )
{
    // A single instance of nextStates is defined here
    LedStates_t nextStates;

    while(1)
    {
        nextStates.redLEDState = 1;
        nextStates.greenLEDState = 1;
        nextStates.blueLEDState = 1;
        nextStates.msDelayTime = 500;
        xQueueSend(ledCmdQueue, &nextStates, portMAX_DELAY);

        nextStates.greenLEDState = 0;  // Turn-off the green LED
        xQueueSend(ledCmdQueue, &nextStates, portMAX_DELAY);

        nextStates.blueLEDState = 0;  // Turn-off the blue LED
        xQueueSend(ledCmdQueue, &nextStates, portMAX_DELAY);

        nextStates.redLEDState = 0;  // Turn-off the red LED
        nextStates.msDelayTime = 1000;
        xQueueSend(ledCmdQueue, &nextStates, portMAX_DELAY);
    }

}

