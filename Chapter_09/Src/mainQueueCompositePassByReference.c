/**

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
#include <SEGGER_SYSVIEW.h>
#include <Nucleo_F767ZI_Init.h>
#include <stm32f7xx_hal.h>

/*********************************************
 * A simple demonstration of using queues across
 * multiple tasks with pass by reference.
 * This time, a pointer to a struct
 * is copied into the queue
 *********************************************/


#define STACK_SIZE 128

// Maximum length of the message struct
#define MAX_MSG_LEN 256

/**
 * Define a larger structure to also specify a message
 */
typedef struct
{
	uint32_t redLEDState : 1;
	uint32_t blueLEDState : 1;
	uint32_t greenLEDState : 1;
	uint32_t msDelayTime;		// Minimum number of ms to remain in this state
	char message[MAX_MSG_LEN];	// An array for storing strings of up to 256 chars
} LedStates_t;

/**
 * Below, we define two instances of the LedStates_t struct.  Since these are globals
 * they will always be available for use.
 */
// Red ON, blue OFF, green OFF, 1000ms delay, msg
static const LedStates_t ledState1 = {1, 0, 0, 1000, "The quick brown fox jumped over the lazy dog. Only the Red LED is on."};

// Red OFF, blue ON, green OFF, 1000ms delay, msg
static const LedStates_t ledState2 = {0, 1, 0, 1000, "Another string. Only the Blue LED is on"};

void recvTask( void* NotUsed );
void sendingTask( void* NotUsed );

// This is a handle for the queue that will be used by
// recvTask and sendingTask
static QueueHandle_t ledCmdQueue = NULL;

int main(void)
{
	HWInit();
	SEGGER_SYSVIEW_Conf();
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);	//ensure proper priority grouping for freeRTOS

	// Setup tasks, making sure they have been properly created before moving on
	assert_param(xTaskCreate(recvTask, "recvTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL) == pdPASS);
	assert_param(xTaskCreate(sendingTask, "sendingTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL) == pdPASS);

	/**
	 * Create a queue that can store up to 8 LedStates_t struct POINTERS.
	 * It is very important to note that each element of this queue only
	 * contains enough space for storing a POINTER to LedStates_t, not
	 * the entire struct.
	 *
	 * In Ozone's Global Data view, take a look at ledCmdQueue->uxItemSize.
	 * Notice how it differs from the value in mainQueueLargeCompositePassByValue.c
	 *
	 */
	ledCmdQueue = xQueueCreate(8, sizeof(LedStates_t*));
	assert_param(ledCmdQueue != NULL);

	// Start the scheduler - shouldn't return unless there's a problem
	vTaskStartScheduler();

	// if you've wound up here, there is likely an issue with over-running the freeRTOS heap
	while(1)
	{
	}
}

/**
 * recvTask receives items from queue, of type LedStates_t pointer.
 */
void recvTask( void* NotUsed )
{
	LedStates_t* nextCmd;

	while(1)
	{
		xQueueReceive(ledCmdQueue, &nextCmd, portMAX_DELAY);

        if(nextCmd->redLEDState == 1)
            RedLed.On();
        else
            RedLed.Off();

        if(nextCmd->blueLEDState == 1)
            BlueLed.On();
        else
            BlueLed.Off();

        if(nextCmd->greenLEDState == 1)
            GreenLed.On();
        else
            GreenLed.Off();

		vTaskDelay(nextCmd->msDelayTime/portTICK_PERIOD_MS);
	}
}

/**
 * sendingTask sends items to the queue, of type LedStates_t pointer.
 *
 */
void sendingTask( void* NotUsed )
{
	const LedStates_t * state1Ptr = &ledState1;
	const LedStates_t * state2Ptr = &ledState2;

	while(1)
	{
		xQueueSend(ledCmdQueue, &state1Ptr, portMAX_DELAY);
		xQueueSend(ledCmdQueue, &state2Ptr, portMAX_DELAY);
	}
}

