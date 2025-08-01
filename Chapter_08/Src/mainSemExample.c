/**

---------------------------------------------------------------------------------------

Licenses:
- Copyright (c) 2019-2025 Packt Publishing, under the MIT License.
- Based on code copyrighted by Brian Amos, 2019, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-2nd-edition

 */


#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <SEGGER_SYSVIEW.h>
#include <Nucleo_F767ZI_GPIO.h>
#include <Nucleo_F767ZI_Init.h>
#include <stm32f7xx_hal.h>

#define STACK_SIZE 128

void GreenTaskA( void * argument);
void BlueTaskB( void* argumet );

// Create storage for a pointer to a semaphore
SemaphoreHandle_t semPtr = NULL;

int main(void)
{
	HWInit();
	SEGGER_SYSVIEW_Conf();
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);	//ensure proper priority grouping for freeRTOS

	// Create a semaphore using the FreeRTOS Heap
	semPtr = xSemaphoreCreateBinary();
    // Ensure the pointer is valid (semaphore created successfully)
	assert_param(semPtr != NULL);

	// Create TaskA as a higher priority than TaskB.  In this example, this isn't strictly necessary since the tasks
	// spend nearly all of their time blocked
	assert_param(xTaskCreate(GreenTaskA, "GreenTaskA", STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL) == pdPASS);

	// Using an assert to ensure proper task creation
	assert_param(xTaskCreate(BlueTaskB, "BlueTaskB", STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL) == pdPASS);

	// Start the scheduler - shouldn't return unless there's a problem
	vTaskStartScheduler();

	// If you've wound up here, there is likely an issue with over-running the freeRTOS heap
	while(1)
	{
	}
}

/**
 * GreenTaskA periodically 'gives' semaphorePtr
 * NOTES:
 * - This semaphore isn't "given" to any task specifically
 * - Giving the semaphore doesn't prevent GreenTaskA from continuing to run.
 * - Note the green LED continues to blink at all times
 */
void GreenTaskA( void* argument )
{
	uint_fast8_t count = 0;
	while(1)
	{
		// Every 5 times through the loop, give the semaphore
		if(++count >= 5)
		{
			count = 0;
			SEGGER_SYSVIEW_PrintfHost("GreenTaskA gives semPtr");
			xSemaphoreGive(semPtr);
		}
		GreenLed.On();
		vTaskDelay(100/portTICK_PERIOD_MS);
		GreenLed.Off();
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}

/**
 * BlueTaskB waits to receive semPtr, then triple-blinks the blue LED
 */
void BlueTaskB( void* argument )
{
	while(1)
	{
		// 'take' the semaphore with no timeout.
	    // * In our system, FreeRTOSConfig.h specifies "#define INCLUDE_vTaskSuspend 1".
	    // * So, in xSemaphoreTake, portMAX_DELAY specifies an indefinite wait.
		SEGGER_SYSVIEW_PrintfHost("BlueTaskB attempts to take semPtr");
		if(xSemaphoreTake(semPtr, portMAX_DELAY) == pdPASS)
		{
			SEGGER_SYSVIEW_PrintfHost("BlueTaskB received semPtr");
			// Triple-blink the Blue LED
			for(uint_fast8_t i = 0; i < 3; i++)
			{
				BlueLed.On();
				vTaskDelay(50/portTICK_PERIOD_MS);
				BlueLed.Off();
				vTaskDelay(50/portTICK_PERIOD_MS);
			}
		}
//		else
//		{
//			This is the code that would be executed if we timed-out waiting for
//			the semaphore to be given.
//		}
	}
}
