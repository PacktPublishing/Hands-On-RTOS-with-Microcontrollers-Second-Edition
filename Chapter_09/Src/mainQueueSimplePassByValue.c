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
 * two tasks with pass-by-value.
 *********************************************/

#define STACK_SIZE 128

/**
 * Setup a simple enum to specify the state of a single LED, or all LEDs
 *
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

} LED_CMDS;

void recvTask( void* NotUsed );
void sendingTask( void* NotUsed );

// This is a handle for the queue that will be used by recvTask and sendingTask
static QueueHandle_t ledCmdQueue = NULL;

int main(void)
{
	HWInit();
	SEGGER_SYSVIEW_Conf();
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);	//ensure proper priority grouping for freeRTOS

	// Setup tasks, making sure they have been properly created before moving on
	assert_param(xTaskCreate(recvTask, "recvTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL) == pdPASS);
	assert_param(xTaskCreate(sendingTask, "sendingTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL) == pdPASS);

	// Create a queue that can store 1 uint8_t, using ledCmdQueue to point to it
	ledCmdQueue = xQueueCreate(1, sizeof(uint8_t));
	assert_param(ledCmdQueue != NULL);

	// Start the scheduler - shouldn't return unless there's a problem
	vTaskStartScheduler();

	// If you've wound up here, there is likely an issue with over-running the freeRTOS heap
	while(1)
	{
	}
}

/**
 * The receive task takes items from the queue
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
 * The sending task puts items in the queue
 */
void sendingTask( void* NotUsed )
{
    uint8_t ledCmd;
    
	while(1)
	{
		for(int i = 0; i < 8; i++)
		{
			ledCmd = (LED_CMDS) i;
			xQueueSend(ledCmdQueue, &ledCmd, portMAX_DELAY);
			
            vTaskDelay(200/portTICK_PERIOD_MS);
		}
	}
}

