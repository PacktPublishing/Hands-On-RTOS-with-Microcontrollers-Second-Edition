/**

 * TITLE:  Chapter 7 example-program: failed task-creation
 *
 * - This is an example program for the book:
 *   "Hands On RTOS with Microcontrollers" (2nd edition)

 ----------------------------------------------------

 Licenses:
 * Copyright (c) 2019-2025 Packt Publishing, under the MIT License.
 * - Based on code copyrighted by Brian Amos, 2019, under the MIT License.
 * - See the code-repository's license statement for more information:
 * - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-2nd-edition

 */

#include <FreeRTOS.h>
#include <Nucleo_F767ZI_Init.h>
#include <stm32f7xx_hal.h>
#include <Nucleo_F767ZI_GPIO.h>
#include <task.h>
#include <SEGGER_SYSVIEW.h>
#include <lookBusy.h>

/**
 * 	Function prototypes
 */
void GreenTask(void *argument);
void BlueTask(void *argument);
void RedTask(void *argument);

// The address of blueTaskHandle will be passed to xTaskCreate.
// This global variable will be used by RedTask to delete BlueTask.
TaskHandle_t blueTaskHandle;

// In FreeRTOS, the stack size is specified in words.
// * Our processor's word size is 4 bytes
// * (128 * 4) = 512 bytes
#define STACK_SIZE 128

// Define the stack and the the task control block (TCB) for RedTask to use
static StackType_t RedTaskStack[STACK_SIZE];
static StaticTask_t RedTaskTCB;

uint32_t iterationsPerMilliSecond;

int main(void)
{
    HWInit();
    SEGGER_SYSVIEW_Conf();


    // Get the interation-rate for lookBusy()
    iterationsPerMilliSecond = lookBusyIterationRate();

    // If the task isn't created successfully, main() will spin in the infinite while-loop.
    if (xTaskCreate(GreenTask, "GreenTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL) != pdPASS){ while(1); }

    // Using an assert to ensure proper task creation
    assert_param(xTaskCreate(BlueTask, "BlueTask", STACK_SIZE *100, NULL, tskIDLE_PRIORITY + 1, &blueTaskHandle) == pdPASS);

    // xTaskCreateStatic returns the task handle.
    // The function always passes because the function's memory was statically allocated.
    xTaskCreateStatic(	RedTask, "RedTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, RedTaskStack, &RedTaskTCB);

    // Start the scheduler. It should not return unless there's a problem.
    vTaskStartScheduler();

    // If you've wound up here, there is likely an issue with over-running the FreeRTOS heap
    while(1)
    {

    }
}

void GreenTask(void *argument)
{
    // Indicate GreenTask started
    BlueLed.On();
    // Spin until the user starts the SystemView app, in Record mode
    while(SEGGER_SYSVIEW_IsStarted()==0){
        lookBusy(iterationsPerMilliSecond);
    }
    BlueLed.Off();

    SEGGER_SYSVIEW_PrintfHost("GreenTask started");

    GreenLed.On();
    vTaskDelay(1500 / portTICK_PERIOD_MS);
    GreenLed.Off();

    SEGGER_SYSVIEW_PrintfHost("GreenTask is deleting itself");
    // A task can delete itself by passing NULL to vTaskDelete
    vTaskDelete(NULL);

    // The task never gets to here
    GreenLed.On();
}

void BlueTask( void* argument )
{
    while(1)
    {
        SEGGER_SYSVIEW_PrintfHost("BlueTask is starting a loop iteration");
        BlueLed.On();
        vTaskDelay(200 / portTICK_PERIOD_MS);
        BlueLed.Off();
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void RedTask( void* argument )
{
    uint8_t firstIteration = 1;
    uint32_t i;

    while(1)
    {
        SEGGER_SYSVIEW_PrintfHost("RedTask is starting a loop iteration");
        // Spin for 1 second of processor time
        for (i=0;i<1000;i++){
            lookBusy(iterationsPerMilliSecond);
        }

        SEGGER_SYSVIEW_PrintfHost("RedTask is turning-on the red LED");
        RedLed.On();
        vTaskDelay(500/ portTICK_PERIOD_MS);
        RedLed.Off();
        vTaskDelay(500/ portTICK_PERIOD_MS);

        if(firstIteration == 1)
        {
            SEGGER_SYSVIEW_PrintfHost("RedTask is deleting BlueTask");
            // Tasks can delete one-another by passing the desired TaskHandle_t to vTaskDelete
            vTaskDelete(blueTaskHandle);
            firstIteration = 0;
        }
    }
}
