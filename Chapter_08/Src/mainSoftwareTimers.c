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
#include <task.h>
#include <semphr.h>
#include <timers.h>
#include <SEGGER_SYSVIEW.h>
#include <Nucleo_F767ZI_Init.h>
#include <stm32f7xx_hal.h>
#include <lookBusy.h>

#define STACK_SIZE 128

void taskStartTimers( void * argument);

void oneShotCallBack( TimerHandle_t xTimer );
void repeatCallBack( TimerHandle_t xTimer );

uint32_t iterationsPerMilliSecond;

int main(void)
{
	HWInit();
	SEGGER_SYSVIEW_Conf();
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);	//ensure proper priority grouping for FreeRTOS

    // Get the interation-rate for lookBusy()
    iterationsPerMilliSecond = lookBusyIterationRate();

	// Create the task that will start the timers.
	// Set the task's priority to be higher than the timer-task: (configTIMER_TASK_PRIORITY + 1).
	assert_param(xTaskCreate(taskStartTimers, "startTimersTask",
	                         STACK_SIZE, NULL, (configTIMER_TASK_PRIORITY + 1), NULL) == pdPASS);

	// Start the scheduler - shouldn't return unless there's a problem
	vTaskStartScheduler();

	// If you've wound up here, there is likely an issue with over-running the freeRTOS heap
	while(1)
	{
	}
}

// Task used to start the timers
void taskStartTimers( void* argument )
{
    // Indicate taskStartTimers started
    RedLed.On();
    // Spin until the user starts the SystemView app, in Record mode
    while(SEGGER_SYSVIEW_IsStarted()==0){
        lookBusy(iterationsPerMilliSecond);
    }
    RedLed.Off();

	SEGGER_SYSVIEW_PrintfHost("taskStartTimers: starting");

    //
    // Create the one-shot timer, and start it
    //

	// Start with Blue LED on - it will be turned off after one-shot fires
	BlueLed.On();
	SEGGER_SYSVIEW_PrintfHost("taskStartTimers: blue LED on");
	TimerHandle_t oneShotHandle =
		xTimerCreate(	"myOneShotTimer",			//name for timer
						2200 /portTICK_PERIOD_MS,	//period of timer in ticks
						pdFALSE,					//auto-reload flag
						NULL,						//unique ID for timer
						oneShotCallBack);			//callback function
	assert_param(oneShotHandle != NULL);

	SEGGER_SYSVIEW_PrintfHost("taskStartTimers: one-shot timer started (turns off blue LED)");
	xTimerStart(oneShotHandle, 0);


    //
    // Create the repeat-timer, and start it
    //

    TimerHandle_t repeatHandle =
        xTimerCreate(   "myRepeatTimer",            //name for timer
                        500 /portTICK_PERIOD_MS,    //period of timer in ticks
                        pdTRUE,                     //auto-reload flag
                        NULL,                       //unique ID for timer
                        repeatCallBack);            //callback function
    assert_param(repeatHandle != NULL);

    SEGGER_SYSVIEW_PrintfHost("taskStartTimers: repeating-timer started (blinks the green LED)");
    xTimerStart(repeatHandle, 0);


	// The task deletes itself
    SEGGER_SYSVIEW_PrintfHost("taskStartTimers: deleting itself");
	vTaskDelete(NULL);

    // The task never gets to here
	while(1)
	{
	}

}


void oneShotCallBack( TimerHandle_t xTimer )
{
	SEGGER_SYSVIEW_PrintfHost("oneShotCallBack:  blue LED off");
	BlueLed.Off();
}


void repeatCallBack( TimerHandle_t xTimer )
{
	static uint32_t counter = 0;

	SEGGER_SYSVIEW_PrintfHost("repeatCallBack:  toggle Green LED");
	// Toggle the green LED
	if(counter++ % 2)
	{
		GreenLed.On();
	}
	else
	{
		GreenLed.Off();
	}
}


