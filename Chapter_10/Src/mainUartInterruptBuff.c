/*

---------------------------------------------------------------------------------------

Licenses:
- Copyright (c) 2019-2025 Packt Publishing, under the MIT License.
- Based on code copyrighted by Brian Amos, 2019, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-Second-Edition

 */

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <timers.h>
#include <Nucleo_F767ZI_GPIO.h>
#include <SEGGER_SYSVIEW.h>
#include <Nucleo_F767ZI_Init.h>
#include <stm32f7xx_hal.h>
#include <UartQuickDirtyInit.h>
#include "Uart4Setup.h"
#include <stdbool.h>
#include <string.h>

/*********************************************
 * A demonstration of a simple receive-only interrupt-driven UART driver,
 * that uses a buffer and semaphores.
 *********************************************/

#define STACK_SIZE 128

#define BAUDRATE 9600

void uartPrintOutTask( void* NotUsed);
void startUart4Traffic( TimerHandle_t xTimer );

static SemaphoreHandle_t semOkToFill = NULL;
static SemaphoreHandle_t semOkToPrintOut = NULL;

// Indicates USART2 is enabled to receive
static volatile bool rxInProgress = false;

// * The shared buffer
// * UART4 repeatedly sends the 16-byte string "data from uart4",
//   and its null-terminator
// * Make the buffer long enough to hold the string,
//   plus the null-terminator mapped to '#', plus an actual null-terminator.
#define BUFFER_LENGTH 17
static volatile uint8_t buffer[BUFFER_LENGTH];
static volatile uint8_t bufferIndex = 0;

// Diagnostic data
static volatile uint32_t semOkToFill_taken = 0;
static volatile uint32_t semOkToFill_notTaken = 0;
static volatile uint32_t semOkToPrintOut_taken = 0;
static volatile uint32_t semOkToPrintOut_notTaken = 0;

int main(void)
{
    HWInit();
    SEGGER_SYSVIEW_Conf();

    // Ensure proper priority grouping for FreeRTOS
    NVIC_SetPriorityGrouping(0);

    // Setup a timer to kick off UART traffic (flowing out of UART4 TX line
    // and into USART2 RX line) 5 seconds after the scheduler starts.
    // The transmission needs to start after the receiver is ready for data.
    TimerHandle_t oneShotHandle =
    xTimerCreate(   "startUart4Traffic",
                    5000 /portTICK_PERIOD_MS,
                    pdFALSE,
                    NULL,
                    startUart4Traffic);
    assert_param(oneShotHandle != NULL);
    xTimerStart(oneShotHandle, 0);

    // * Create the two semaphores used to coordinate access to the buffer
    //   * semOkToFill : indicates the interrupt-handler can fill the
    //                   buffer, and uartPrintOutTask cannot read from the buffer.
    //   * semOkToPrintOut : indicates uartPrintOutTask() can read from the buffer,
    //                       and the interrupt-handler cannot fill it.
    // * A semaphore is created in the ‘empty’ state, i.e., the semaphore must first be given
    //   before it can be taken
    semOkToFill = xSemaphoreCreateBinary();
    assert_param(semOkToFill != NULL);
    semOkToPrintOut = xSemaphoreCreateBinary();
    assert_param(semOkToPrintOut != NULL);

    // Create the task, making sure it has been properly created before moving on
    assert_param(xTaskCreate(uartPrintOutTask, "uartPrint", STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL) == pdPASS);

    // Start the scheduler - shouldn't return unless there's a problem
    vTaskStartScheduler();

    // If you've wound up here, there is likely an issue with over-running the freeRTOS heap
    while(1)
    {
    }
}

/**
 * Start an interrupt-driven receive.
 */
void startReceiveInt()
{
    rxInProgress = true;
    USART2->CR3 |= USART_CR3_EIE;   //enable error interrupts
    USART2->CR1 |= (USART_CR1_UE | USART_CR1_RXNEIE);
    //all 4 bits are for preemption priority -
    NVIC_SetPriority(USART2_IRQn, 6);
    NVIC_EnableIRQ(USART2_IRQn);
}

void startUart4Traffic( TimerHandle_t xTimer )
{
    SetupUart4ExternalSim(BAUDRATE);
}


void uartPrintOutTask( void* NotUsed)
{

    // Initialize USART2
    STM_UartInit(USART2, BAUDRATE, NULL, NULL);
    // Enable USART2
    startReceiveInt();

    // Allow the interrupt-handler to fill the buffer
    xSemaphoreGive(semOkToFill);

    while(1)
    {

        // Take the semaphore needed to print the buffer
        if(xSemaphoreTake(semOkToPrintOut, 100) == pdPASS)
        {
            semOkToPrintOut_taken++;
            SEGGER_SYSVIEW_Print((char*)buffer);
            // Give semaphore needed to fill the buffer
            xSemaphoreGive(semOkToFill);
        }
        else
        {
            SEGGER_SYSVIEW_PrintfHost("timeout");
            // Record diagnostic data, for testing and debugging
            if (semOkToPrintOut_taken != 0)
                semOkToPrintOut_notTaken++;
        }
    }
}


void USART2_IRQHandler( void )
{
    portBASE_TYPE xHigherPriorityTaskWoken = pdFAIL;
    portBASE_TYPE xHigherPriorityTaskWoken_give = pdFAIL;
    portBASE_TYPE xHigherPriorityTaskWoken_take = pdFAIL;

    SEGGER_SYSVIEW_RecordEnterISR();

    // First check for errors
    if( USART2->ISR & ( USART_ISR_ORE_Msk |
                        USART_ISR_NE_Msk |
                        USART_ISR_FE_Msk |
                        USART_ISR_PE_Msk ))
    {
        // Clear error flags
        USART2->ICR |= (USART_ICR_FECF |
                        USART_ICR_PECF |
                        USART_ICR_NCF |
                        USART_ICR_ORECF);
    }

    if( USART2->ISR & USART_ISR_RXNE_Msk)
    {
        // Read the data register unconditionally to clear
        // the receive-not-empty interrupt if no reception is
        // in progress
        uint8_t tempVal = (uint8_t) USART2->RDR;

        if(rxInProgress)
        {
            // * Take the semaphore semOkToFill.
            // * If the semaphore is not available, the call to xSemaphoreTakeFromISR will not block.
            if ( xSemaphoreTakeFromISR(semOkToFill, &xHigherPriorityTaskWoken_take) == pdPASS){

                semOkToFill_taken++;

                // * If the received-byte is a null-terminator, replace it with a "#".
                //
                // * UART4 sends the string "data from uart4", followed by a null terminator (0x00).
                // * The buffer is displayed by SEGGER_SYSVIEW_Print(), and its display
                //   stops at the first null-terminator in the provided string.
                // * For example, if buffer contained "usart4<0x00>data from uar<0x00>",
                //   the characters after the first null-terminator would not be displayed.
                if (tempVal == 0){
                    tempVal = '#';
                }

                buffer[bufferIndex++] = tempVal;

                // Test if the buffer is full
                if(bufferIndex == (BUFFER_LENGTH - 1)){
                    // Add null-terminator to the string (needed by SEGGER_SYSVIEW_Print)
                    buffer[bufferIndex] = 0;
                    bufferIndex = 0;

                    // Give the semaphore needed to print the buffer
                    xSemaphoreGiveFromISR(semOkToPrintOut, &xHigherPriorityTaskWoken_give);
                }
                else{
                    // Give the semaphore needed to fill the buffer
                    xSemaphoreGiveFromISR(semOkToFill, &xHigherPriorityTaskWoken_give);
                }
            }
            else {
                // * This case occurs when the semaphore semOkToFill is not available.
                // * Record diagnostic data, for testing and debugging
                semOkToFill_notTaken++;
            }
        }
    }

    // * The calls to xSemaphoreTakeFromISR and xSemaphoreGiveFromISR have a parameter
    //   *pxHigherPriorityTaskWoken.
    // * If either of them returned pdTRUE, pass that to portYIELD_FROM_ISR.
    if ( (xHigherPriorityTaskWoken_give == pdTRUE) || (xHigherPriorityTaskWoken_take == pdTRUE) ){
        xHigherPriorityTaskWoken = pdTRUE;
    }
    else{
        xHigherPriorityTaskWoken = pdFAIL;
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    SEGGER_SYSVIEW_RecordExitISR();
}
