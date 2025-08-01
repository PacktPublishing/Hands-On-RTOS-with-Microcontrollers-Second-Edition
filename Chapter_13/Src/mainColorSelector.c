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
#include <queue.h>
#include <Nucleo_F767ZI_GPIO.h>
#include <SEGGER_SYSVIEW.h>
#include <Nucleo_F767ZI_Init.h>
#include <stm32f7xx_hal.h>
#include "vcpDriverMultiTask.h"
#include <string.h>
#include <stdio.h>
#include <pwmImplementation.h>
#include <ledCmdExecutor.h>
#include <CRC32.h>

// 128 * 4 = 512 bytes
// (recommended min stack size per task)
#define STACK_SIZE 128

void frameDecoder( void* NotUsed);

/**
 * The ledCmdQueue is used to pass data from the frameDecoder task to the
 * LedCmdExecution task.
 */
QueueHandle_t ledCmdQueue = NULL;

int main(void)
{
	HWInit();
	PWMInit();
	vcpInit();
	SEGGER_SYSVIEW_Conf();
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);	// Ensure proper priority grouping for freeRTOS

	// Create a queue for LedCmd structs, capable of holding up to 4 commands
	ledCmdQueue = xQueueCreate(4, sizeof(LedCmd));
	assert_param(ledCmdQueue != NULL);

	/**
	 * Create a variable that will store the arguments
	 * passed to the LED command executor.
	 * This allows the logic in ledCmdExecution to be completely
	 * independent of the underlying implementations.
	 * This is a static variable so it isn't placed on the stack.  We're
	 * using aggregate initialization to guarantee that the iPWM* members won't
	 * change over time.
	 */
	static CmdExecArgs ledTaskArgs;
	ledTaskArgs.ledCmdQueue = ledCmdQueue;
	ledTaskArgs.redPWM = &RedPWM;
	ledTaskArgs.greenPWM = &GreenPWM;
	ledTaskArgs.bluePWM = &BluePWM;

	// Setup tasks, making sure they have been properly created before moving on
	assert_param(xTaskCreate(frameDecoder, "frameDecoder", 256, NULL, configMAX_PRIORITIES-2, NULL) == pdPASS);
	assert_param(xTaskCreate(LedCmdExecution, "cmdExec", 256, &ledTaskArgs, configMAX_PRIORITIES-2, NULL) == pdPASS);

	// Start the scheduler - shouldn't return unless there's a problem
	vTaskStartScheduler();

	// If you've wound up here, there is likely an issue with over-running the FreeRTOS heap
	while(1)
	{
	}
}


/**
 * The frameDecoder task is responsible for watching the stream buffer
 * receiving data from the PC over USB.
 * It validates incoming data, populates the
 * data structure used by the LED command-executor
 * and pushes commands into the command-executor's queue.
 *
 * The incoming frame consists of a delimiter byte with value 0x02 and then 4 bytes
 * representing the command number, red duty cycle , green duty cycle, and blue duty cycle.
 * To ensure the frame has been correctly detected and
 * received, a 32 bit CRC must be validated before pushing the RGB values into
 * the queue (it comes across the wire little endian).
 *
 * <STX> <Cmd> <red> <green> <blue> <CRC LSB> <CRC> <CRC> <CRC MSB>
 *
 * STX is ASCII start of text (0x02)
 */
void frameDecoder( void* NotUsed)
{
	LedCmd incomingCmd;

#define FRAME_LEN 9
	uint8_t frame[FRAME_LEN];

	while(1)
	{
		memset(frame, 0, FRAME_LEN);

		// Since this is the only task receiving from the stream-buffer, we don't
		// need to acquire a mutex before accessing it.
		// * If more than one task was to be receiving, vcom_rxStream would require
		//   protection from a mutex.
		// * Also, the calls to xStreamBufferReceive() would have to use a maximum
		//   wait-time of 0.

		// First, ensure we received STX (0x02)
		while(frame[0] != 0x02)
		{
			xStreamBufferReceive(	*GetUsbRxStreamBuff(),
									frame,
									1,
									portMAX_DELAY);
		}

		// Now receive the rest of the frame (hopefully)
		// This won't return until the remainder has been received
		xStreamBufferReceive(	*GetUsbRxStreamBuff(),
								&frame[1],
								FRAME_LEN-1,
								portMAX_DELAY);

		// If the frame is intact, store it into the command
		// and send it to the ledCmdQueue
		if(CheckCRC(frame, FRAME_LEN))
		{
			// Populate the command with current values
			incomingCmd.cmdNum = frame[1];
			incomingCmd.red = frame[2]/255.0 * 100;
			incomingCmd.green = frame[3]/255.0 * 100;
			incomingCmd.blue = frame[4]/255.0 * 100;

			// Push the command to the queue.
			// Wait up to 100 ticks and then drop it if not added...
			xQueueSend(ledCmdQueue, &incomingCmd, 100);
		}
	}
}
