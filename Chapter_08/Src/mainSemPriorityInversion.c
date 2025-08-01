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
#include <lookBusy.h>

#define STACK_SIZE 128

static void blinkTwice( LED* led );

void TaskA( void * argument);
void TaskB( void* argumet );
void TaskC( void* argumet );

// Create storage for a pointer to a semaphore
SemaphoreHandle_t semPtr = NULL;

uint32_t iterationsPerMilliSecond;

int main(void)
{
    HWInit();
    SEGGER_SYSVIEW_Conf();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4); //ensure proper priority grouping for freeRTOS

    // Get the iteration-rate for lookBusy()
    iterationsPerMilliSecond = lookBusyIterationRate();

    // Create a semaphore using the FreeRTOS Heap
    semPtr = xSemaphoreCreateBinary();
    assert_param(semPtr != NULL);

    assert_param(xTaskCreate(TaskA, "TaskA", STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL) == pdPASS);
    assert_param(xTaskCreate(TaskB, "TaskB", STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL) == pdPASS);
    assert_param(xTaskCreate(TaskC, "TaskC", STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL) == pdPASS);

    // Initial "give" of the semaphore
    xSemaphoreGive(semPtr);

    // Start the scheduler - shouldn't return unless there's a problem
    vTaskStartScheduler();

    // If you've wound up here, there is likely an issue with over-running the freeRTOS heap
    while(1)
    {
    }
}

/**
 * Task A is the highest priority task in the system.
 * It periodically takes the semaphore with a 200ms timeout.
 * If the semaphore is taken within 200 ms, the red LED is turned off,
   and blinkTwice() is called to blink the green LED.
 * If the semaphore is not taken within 200ms, the red LED is turned on
   (after TaskA has been given context)
 * At the end of each while-loop iteration, vTaskDelay is called.
 */
void TaskA( void* argument )
{
    uint32_t receivedCounter = 0;
    uint32_t timedoutCounter = 0;

    while(1)
    {

        // 'take' the semaphore with a 200ms timeout
        SEGGER_SYSVIEW_PrintfHost("attempt to take semPtr");
        if(xSemaphoreTake(semPtr, 200/portTICK_PERIOD_MS) == pdPASS)
        {
            RedLed.Off();
            receivedCounter++;
            // In calling SEGGER_SYSVIEW_PrintfHost, the space between the %u and the closing
            // quote appears to be a necessary work-around for a bug in the API.
            SEGGER_SYSVIEW_PrintfHost("received semPtr: %u ", receivedCounter);
            blinkTwice(&GreenLed);
            xSemaphoreGive(semPtr);
        }
        else
        {
            // This code is called when the semaphore wasn't taken in time
            timedoutCounter++;
            SEGGER_SYSVIEW_PrintfHost("FAILED to receive semPtr in time: %u ", timedoutCounter);
            RedLed.On();
        }
        // Wait for a bit to let other tasks run
        // StmRand will return a random number between 5 and 30 (inclusive).
        vTaskDelay(StmRand(5,30));
    }
}

/**
 * This medium priority task just wakes up periodically and wastes time.
 */
void TaskB( void* argument )
{
    uint32_t counter = 0;
    uint32_t spinTime;
    uint32_t i;

    while(1)
    {
        counter++;
        SEGGER_SYSVIEW_PrintfHost("starting iteration %u ", counter);
        vTaskDelay(StmRand(10,25));

        // Each for-loop iteration takes 1 ms of processing time.
        // The for-loop, as a whole, will use 30-75 ms of processing time.
        spinTime = StmRand(30,75);
        for (i=0; i<spinTime; i++){
            lookBusy(iterationsPerMilliSecond);
        }
    }
}

/**
 * The lowest priority task in the system.
 * It is the same as TaskA, except:
   * It calls blinkTwice() to blink the blue LED
   * It does not call vTaskDelay().
 */
void TaskC( void* argument )
{
    uint32_t receivedCounter = 0;
    uint32_t timedoutCounter = 0;

    while(1)
    {
        // 'take' the semaphore with a 200mS timeout
        SEGGER_SYSVIEW_PrintfHost("attempt to take semPtr");
        if(xSemaphoreTake(semPtr, 200/portTICK_PERIOD_MS) == pdPASS)
        {
            RedLed.Off();
            receivedCounter++;
            SEGGER_SYSVIEW_PrintfHost("received semPtr: %u ", receivedCounter);
            blinkTwice(&BlueLed);
            xSemaphoreGive(semPtr);
        }
        else
        {
            // This code is called when the semaphore wasn't taken in time
            timedoutCounter++;
            SEGGER_SYSVIEW_PrintfHost("FAILED to receive semPtr in time: %u ", timedoutCounter);
            RedLed.On();
        }
    }
}

/**
 * Blink the desired LED twice
 */
static void blinkTwice( LED* led )
{
    for(uint32_t i = 0; i < 2; i++)
    {
        led->On();
        vTaskDelay(25/portTICK_PERIOD_MS);
        led->Off();
        vTaskDelay(25/portTICK_PERIOD_MS);
    }
}
