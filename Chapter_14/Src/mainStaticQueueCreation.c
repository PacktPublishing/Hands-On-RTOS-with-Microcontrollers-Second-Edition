/*

---------------------------------------------------------------------------------------

Licenses:
- Copyright (c) 2019-2025 Packt Publishing, under the MIT License.
- Based on code copyrighted by Brian Amos, 2019, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-Second-Edition

 */

#include <FreeRTOS.h>
#include <Nucleo_F767ZI_GPIO.h>
#include <queue.h>
#include <Nucleo_F767ZI_Init.h>
#include <stm32f7xx_hal.h>

/*********************************************
 * A simple demonstration of creating a queue
 * using static memory-allocation, instead of the
 * FreeRTOS heap.
 *
 * heap_1 is used with this program.
 *********************************************/

// Recommended min stack size per task
// (128 * 4 = 512 bytes)
#define STACK_SIZE 128

/**
 * Setup a simple enum to command the state of a single LED
 *
 * NOTE: only 1 LED can be commanded at a time using this method.
 * THE NUMERIC VALUES ARE IMPORTANT! These will be used by
 * sendingTask to increment the LED states
 */
typedef enum
{
	ALL_OFF	= 	0,
	RED_ON	= 	1,
	RED_OFF = 	2,
	BLUE_ON = 	3,
	BLUE_OFF= 	4,
	GREEN_ON = 	5,
	GREEN_OFF = 6,
	ALL_ON	=	7

}LED_CMDS;

void recvTask( void* NotUsed );
void sendingTask( void* NotUsed );

// This is a handle for the queue that will be used by
// recvTask and sendingTask
static QueueHandle_t ledCmdQueue = NULL;

/**
 * The queue data-structure is defined here, as a static variable
 * so it can be passed into xQueueCreateStatic.  This additional
 * variable is required because there is no dynamic-allocation
 * in xQueueCreateStatic.
 */
static StaticQueue_t queueDataStructure;

/**
 *	Define the number of elements in the queue.  This
 *	definition exists to ensure we allocate the same number
 *	of elements in the queueStorage array as we pass into
 *	xQueueCreateStatic
 */
#define LED_CMD_QUEUE_LEN 2

/**
 * Storage for the actual queue is also required to be passed
 * into xQueueCreate. This static variable will exist
 * the entire duration of the program
 */
static uint8_t queueStorage[LED_CMD_QUEUE_LEN];

int main(void)
{
	HWInit();
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);	// Ensure proper priority grouping for freeRTOS
	BaseType_t retVal;

	// Setup tasks, making sure they have been properly created before moving on
	retVal = xTaskCreate(recvTask, "recvTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
	assert_param(retVal == pdPASS);

	retVal = xTaskCreate(sendingTask, "sendingTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
	assert_param(retVal == pdPASS);

	// Create a queue that can store 2 uint8_t's.
	// Use ledCmdQueue to point to the queue.
	ledCmdQueue = xQueueCreateStatic(LED_CMD_QUEUE_LEN, sizeof(uint8_t), queueStorage, &queueDataStructure );
	assert_param(ledCmdQueue != NULL);

	// Start the scheduler - shouldn't return unless there's a problem
	vTaskStartScheduler();

	// If you've wound up here, there is likely an issue with over-running the freeRTOS heap
	while(1)
	{
	}
}

/**
 * This receive task watches a queue for a new ledCmd to be added to it
 */
void recvTask( void* NotUsed )
{
	uint8_t nextCmd = 0;

	while(1)
	{
	    xQueueReceive(ledCmdQueue, &nextCmd, portMAX_DELAY);

        switch(nextCmd)
        {
            case ALL_OFF:
                RedLed.Off();
                GreenLed.Off();
                BlueLed.Off();
            break;
            case GREEN_ON:
                GreenLed.On();
            break;
            case GREEN_OFF:
                GreenLed.Off();
            break;
            case RED_ON:
                RedLed.On();
            break;
            case RED_OFF:
                RedLed.Off();
            break;
            case BLUE_ON:
                BlueLed.On();
            break;
            case BLUE_OFF:
                BlueLed.Off();
            break;
            case ALL_ON:
                GreenLed.On();
                RedLed.On();
                BlueLed.On();
            break;
        }

	}
}

/**
 * sendingTask loops through
 */
void sendingTask( void* NotUsed )
{
	while(1)
	{
		for(int i = 0; i < 8; i++)
		{
			uint8_t ledCmd = (LED_CMDS) i;
			xQueueSend(ledCmdQueue, &ledCmd, portMAX_DELAY);
			vTaskDelay(500/portTICK_PERIOD_MS);
		}
	}
}

