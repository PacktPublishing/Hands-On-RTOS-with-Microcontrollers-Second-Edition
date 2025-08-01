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
#include <stdint.h>

#define STACK_SIZE 128

void GreenTaskA( void * argument);
void BlueTaskB( void* argumet );

volatile uint32_t flag = 0;

int main(void)
{
	HWInit();
	SEGGER_SYSVIEW_Conf();
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);	// Ensure proper priority grouping for freeRTOS

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
 * TaskA periodically sets 'flag' to 1, which signals TaskB to run
 *
 */
void GreenTaskA( void* argument )
{
	uint_fast8_t count = 0;
	while(1)
	{
		// Every 5 times through the loop, set the flag
		if(++count >= 5)
		{
			count = 0;
			SEGGER_SYSVIEW_PrintfHost("Task A (green LED) sets flag");
			flag = 1;	// Set 'flag' to 1 to "signal" BlueTaskB to run
		}
		GreenLed.On();
		vTaskDelay(100/portTICK_PERIOD_MS);
		GreenLed.Off();
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}

/**
 * Wait until flag != 0 then triple blink the Blue LED
 */
void BlueTaskB( void* argument )
{
	while(1)
	{
	    SEGGER_SYSVIEW_PrintfHost("Task B (Blue LED) starts polling on flag");

		// Repeatedly poll on flag.  As soon as it is non-zero,
		// blink the blue LED 3 times
	    while(!flag){}
        flag = 0;

		SEGGER_SYSVIEW_PrintfHost("Task B (Blue LED) received flag");

		// Triple blink the Blue LED
		for(uint_fast8_t i = 0; i < 3; i++)
		{
			BlueLed.On();
			vTaskDelay(50/portTICK_PERIOD_MS);
			BlueLed.Off();
			vTaskDelay(50/portTICK_PERIOD_MS);
		}
	}
}
