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
 * A demonstration of a simple UART driver that is: receive-only, buffer-based,
 * and DMA-driven
 *********************************************/

#define STACK_SIZE 128

#define BAUDRATE (9600)

void uartPrintOutTask( void* NotUsed);
void startUart4Traffic( TimerHandle_t xTimer );

static SemaphoreHandle_t rxDone = NULL;

static bool rxInProgress = false;

static DMA_HandleTypeDef usart2DmaRx;

// The shared buffer
// * UART4 repeatedly sends the 16-byte string "data from uart4",
//   including its null-terminator.
// * Make the buffer long enough to hold the string,
//   with its null-terminator mapped to '#', plus an added null-terminator.
// * The added null-terminator is needed to pass the buffer to SEGGER_SYSVIEW_Print.
#define STRING_LENGTH 16
#define BUFFER_LENGTH (STRING_LENGTH + 1)
static volatile uint8_t memoryBuffer[BUFFER_LENGTH];

// Test variables
static volatile uint32_t test_DMA1_Stream5_IRQHandler_notExpected = 0;
static volatile uint32_t test_startReceiveDMA_halDmaStartFailed = 0;
static volatile uint32_t test_startReceiveDMA_rxInProgressNotExpected = 0;
static volatile uint32_t test_uartPrintOutTask_xSemaphoreTakeTimeout = 0;
static volatile uint32_t test_uartPrintOutTask_received = 0;
static volatile uint32_t test_uartPrintOutTask_xferNotComplete = 0;

int main(void)
{
	BaseType_t retVal;
	HWInit();
	SEGGER_SYSVIEW_Conf();

	// Ensure proper priority-grouping for FreeRTOS
	NVIC_SetPriorityGrouping(0);

    // Setup a timer to kick off UART traffic (flowing out of UART4 TX line
    // and into USART2 RX line) 5 seconds after the scheduler starts.
    // The transmission needs to start after the receiver is ready for data.
	TimerHandle_t oneShotHandle =
	xTimerCreate(	"startUart4Traffic",
					5000 /portTICK_PERIOD_MS,
					pdFALSE,
					NULL,
					startUart4Traffic);
	assert_param(oneShotHandle != NULL);
	xTimerStart(oneShotHandle, 0);

    // Create the semaphore
	rxDone = xSemaphoreCreateBinary();
	assert_param(rxDone != NULL);

    // Setup tasks, making sure they have been properly created before moving on
	retVal = xTaskCreate(uartPrintOutTask, "uartPrint", STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
	assert_param(retVal == pdPASS);

	// Start the scheduler - shouldn't return unless there's a problem
	vTaskStartScheduler();

	// If you've wound up here, there is likely an issue with over-running the FreeRTOS heap
	while(1)
	{
	}
}


/**
 setupUSART2DMA():
 * Enable the clock for the DMA1 controller
 * Enable the IRQ for DMA1 Stream5
 * Configure and initialize DMA stream
   * Configure the struct that specifies the DMA stream
   * Calls HAL_DMA_Init() to initialize the DMA stream
     *  HAL_DMA_Init() first calls __HAL_DMA_DISABLE()
 * Enable the DMA transfer-complete interrupt
 * Enable USART2's DMA-mode, for reception

 */
void setupUSART2DMA( void )
{
	HAL_StatusTypeDef retVal;

	// __HAL_RCC_DMA1_CLK_ENABLE : AHB1 Peripheral Clock Enable
	// * Prior to calling HAL_DMA_Init() the clock must be enabled for DMA through this macro.
	__HAL_RCC_DMA1_CLK_ENABLE();
	
	NVIC_SetPriority(DMA1_Stream5_IRQn, 6);
	NVIC_EnableIRQ(DMA1_Stream5_IRQn);

	// The DMA stream is configured by filling out usart2DmaRx
	memset(&usart2DmaRx, 0, sizeof(usart2DmaRx));

	// Specify stream 5
    usart2DmaRx.Instance = DMA1_Stream5;
    // Specify channel 4
    usart2DmaRx.Init.Channel = DMA_CHANNEL_4;
    // Specify the data-transfer is from the peripheral to memory
    usart2DmaRx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    // Specify not to use the stream’s FIFO queue.
    // * This is referred to as “direct mode”.
    // * After each single data transfer from the peripheral to the
    //   FIFO, the data is immediately stored into the memory
    //   destination.
    usart2DmaRx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    // Specify the stream’s priority, relative to other streams
    // in the DMA controller.
    usart2DmaRx.Init.Priority = DMA_PRIORITY_HIGH;
    // Specifies the DMA mode.
    // * Normal mode specifies the data-transfer occurs once
    //   (vs circular mode).
    // * Normal mode also specifies the DMA flow-controller is used
    //   (vs the peripheral flow-controller).
    usart2DmaRx.Init.Mode = DMA_NORMAL;

    //
    // Peripheral-transfer configuration
    //

    // DMA Peripheral data size.
    // * Specifies one byte.
    usart2DmaRx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    // Specify that the peripheral address is not incremented
    // after each transfer.
    usart2DmaRx.Init.PeriphInc = DMA_PINC_DISABLE;
    // Specify the DMA controller should generate a “single transfer”.
    // * Each DMA request generates a data transfer of a byte.
    // * Since direct-mode is used, “single transfer” must be used.
    usart2DmaRx.Init.PeriphBurst = DMA_PBURST_SINGLE;

    //
    // Memory-transfer configuration
    //

    // DMA memory data-size
    // * Specifies one byte.
    usart2DmaRx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    // Enable memory incrementing.
    // * The DMA controller will increment the memory address
    //   after a transfer
    usart2DmaRx.Init.MemInc = DMA_MINC_ENABLE;
    // Specify the DMA controller should generate a “single transfer”.
    // * Each DMA request generates a data transfer of a byte.
    // * Since direct-mode is used, “single transfer” must be used.
    usart2DmaRx.Init.MemBurst = DMA_MBURST_SINGLE;


	// HAL_DMA_Init : Initialize the DMA stream specified in usart2DmaRx, and create the
    // associated handle.
	retVal = HAL_DMA_Init(&usart2DmaRx);
	assert_param(retVal == HAL_OK);

	// DMA stream x configuration register (DMA_SxCR)
	// * TCIE: transfer-complete interrupt enable
	DMA1_Stream5->CR |= DMA_SxCR_TCIE;

    // Control register 3 (USART_CR3)
    // * DMAR: DMA enable receiver
    // * 1: DMA mode is enabled for reception
    USART2->CR3 |= USART_CR3_DMAR_Msk;

}

/**
 * Start an interrupt-driven receive.
 */
int32_t startReceiveDMA( volatile uint8_t * Buffer, uint_fast16_t Len )
{
	if(!rxInProgress)
	{
		rxInProgress = true;

		// __HAL_DMA_DISABLE : Disables the specified DMA Stream.
		__HAL_DMA_DISABLE(&usart2DmaRx);
		// Wait for the EN bit to be set to 0.
		// * Described in ST's “Using the . . . STM32F7 Series DMA controller” (AN4031)
		while((DMA1_Stream5->CR & DMA_SxCR_EN) != 0);

		// setupUSART2DMA():
		// * Configure and initialize DMA stream
		// * Enable the DMA transfer-complete interrupt
		setupUSART2DMA();

        // Enable DMA for USART2-reception
        // * Get the DMA peripheral ready to receive data immediately before enabling UART
        //   so there is no chance of overrun.
        // * The DMA stream-enable bit must be toggled before a transfer will properly restart.

		// HAL_DMA_Start : Starts the DMA Transfer
		// * Enables the DMA stream (sets the EN bit in the DMA_SxCR register).
		// * Parameter USART2->RDR : USART Receive Data register
		if(HAL_DMA_Start(&usart2DmaRx, (uint32_t)&(USART2->RDR), (uint32_t) Buffer, Len) != HAL_OK)
		{
		    test_startReceiveDMA_halDmaStartFailed++;
			return -1;
		}


		// Clear error flags.
		// Needed for reception to resume after being paused in the debugger.
		USART2->ICR |= (USART_ICR_FECF | USART_ICR_PECF |
		            	USART_ICR_NCF | USART_ICR_ORECF);
						
		// For this specific instance, we'll avoid enabling UART interrupts for errors since
		// we'll wind up with a lot of noise on the line at high baud-rates (the way the ISR is written will
		// cause a transfer to terminate if there are any errors are detected, rather than simply
		// continue with what data it can).
        //USART2->CR3 |= (USART_CR3_EIE);	//enable error interrupts
		//USART2->CR1 |= USART_CR1_RXNEIE | USART_CR1_PEIE);

		//NVIC_SetPriority(USART2_IRQn, 6);
		//NVIC_EnableIRQ(USART2_IRQn);

        // Enable the UART
        USART2->CR1 |= (USART_CR1_UE);

		return 0;
	}

	test_startReceiveDMA_rxInProgressNotExpected++;
	return -1;
}

void startUart4Traffic( TimerHandle_t xTimer )
{
	SetupUart4ExternalSim(BAUDRATE);
}

void stopReceiveDMA( void )
{
	rxInProgress = false;

	// HAL_DMA_Abort : Aborts the DMA Transfer
	HAL_DMA_Abort(&usart2DmaRx);

}

void uartPrintOutTask( void* NotUsed)
{
    uint8_t i;
    memset((void*)memoryBuffer, 0, BUFFER_LENGTH);

	// Configure and initialize the DMA stream
	setupUSART2DMA();

	// Initialize USART2, pass usart2DmaRx for use in receive
	STM_UartInit(USART2, BAUDRATE, NULL, &usart2DmaRx);

	while(1)
	{
	    // Start the DMA transfer for each iteration of the loop.
	    // * After the DMA transfer completes, it must be restarted.
		startReceiveDMA(memoryBuffer, (uint_fast16_t)(STRING_LENGTH));

		// Take the semaphore. The timeout is 100 ticks.
		if(xSemaphoreTake(rxDone, 100) == pdPASS)
		{
			// NDTR is the remaining number of data items to be transferred,
			// 0 signals completion
			if(DMA1_Stream5->NDTR == 0)
			{
			    test_uartPrintOutTask_received++;

			    // The string sent by UART4 has a null-terminator.
			    // Replace the null-terminator with a "#".
			    for(i=0;i<=(STRING_LENGTH-1);i++){
			        if (memoryBuffer[i]==0){
			            memoryBuffer[i]='#';
			            break;
			        }
			    }
			    // Add a null-terminator to the end of the string (needed by SEGGER_SYSVIEW_Print)
			    memoryBuffer[BUFFER_LENGTH - 1] = 0;

			    SEGGER_SYSVIEW_Print("received: ");
				SEGGER_SYSVIEW_Print((char*)memoryBuffer);
			}
			else
			{
			    test_uartPrintOutTask_xferNotComplete++;
			}
		}
		else
		{
		    // Stop the DMA transfer if timeout in xSemaphoreTake
	        stopReceiveDMA();
            // * UART4 is started 5 seconds after the scheduler is started.
            // * So, xSemaphoreTake will timeout 50 times before
            //   UART4 starts sending data.
	        test_uartPrintOutTask_xSemaphoreTakeTimeout++;
			SEGGER_SYSVIEW_PrintfHost("timeout");
		}
	}
}

/**
 * Given the DMA setup performed by setupUSART2DMA,
 * this ISR will only execute when a DMA transfer is complete
 */
void DMA1_Stream5_IRQHandler(void)
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	SEGGER_SYSVIEW_RecordEnterISR();

	// DMA high interrupt status register (DMA_HISR)
	// * TCIF[7:4]: stream x transfer-complete interrupt-flag
	if(rxInProgress && (DMA1->HISR & DMA_HISR_TCIF5))
	{
		rxInProgress = false;

		// DMA high interrupt flag clear register (DMA_HIFCR)
		// * CTCIF[7:4]: stream x clear transfer-complete interrupt-flag
		//   * Writing 1 to this bit clears the corresponding TCIFx flag in
		//     the DMA_HISR register.
		DMA1->HIFCR |= DMA_HIFCR_CTCIF5;

		xSemaphoreGiveFromISR(rxDone, &xHigherPriorityTaskWoken);
	}
	else
	{
	    test_DMA1_Stream5_IRQHandler_notExpected++;
	}

	SEGGER_SYSVIEW_RecordExitISR();
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

