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
#include <stream_buffer.h>
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
 * A demonstration of a receive-only stream-buffer
 * UART driver implemented through DMA
 *********************************************/

#define STACK_SIZE 128

#define BAUDRATE (9600)

void uartPrintOutTask( void* NotUsed);
void startUart4Traffic( TimerHandle_t xTimer );

static StreamBufferHandle_t streamBuffer = NULL;

static DMA_HandleTypeDef usart2DmaRx;

// NOTE: Keep buffers less than 1KB to simplify DMA implementation.
//       See MCU reference manual, section 8.3.12 for details

// * The dual-buffers used by the DMA controller and the DMA ISR:
//   * UART4 repeatedly sends the 16-byte string "data from uart4",
//     including its null-terminator.
//   * Make each buffer long enough to hold the string.
#define DMA_BUFFER_LENGTH 16
static volatile uint8_t rxData1[DMA_BUFFER_LENGTH];
static volatile uint8_t rxData2[DMA_BUFFER_LENGTH];

#define STREAM_BUFFER_SIZE  (10 * DMA_BUFFER_LENGTH)
#define MIN_RECEIVE_SIZE  DMA_BUFFER_LENGTH

// Test variables
static volatile uint32_t test_DMA1_Stream5_IRQHandler_notExpected = 0;
static volatile uint32_t test_startReceiveDMA_halDmaStartFailed = 0;
static volatile uint32_t test_uartPrintOutTask_xStreamBufferReceiveTimeout = 0;
static volatile uint32_t test_uartPrintOutTask_received = 0;
static volatile uint32_t test_uartPrintOutTask_xferNotComplete = 0;

int main(void)
{
    HWInit();
    SEGGER_SYSVIEW_Conf();

    // Ensure proper priority grouping for freeRTOS
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

    streamBuffer = xStreamBufferCreate( STREAM_BUFFER_SIZE, MIN_RECEIVE_SIZE );
    assert_param(streamBuffer != NULL);

    // Setup tasks, making sure they have been properly created before moving on
    assert_param(xTaskCreate(uartPrintOutTask, "uartPrint", STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL) == pdPASS);

    //start the scheduler - shouldn't return unless there's a problem
    vTaskStartScheduler();

    //if you've wound up here, there is likely an issue with overrunning the freeRTOS heap
    while(1)
    {
    }
}


/**
 * Sets up DMA for USART2 circular reception
 * NOTE: After returning from this function, the DMA stream
 * is enabled.  To perform additional configuration, the
 * stream might need to be disabled (i.e. when setting up
 * double buffer mode)
 */

/**
 setupUSART2DMA():
 * Enable the clock for the DMA1 controller
 * Enable the IRQ for DMA1 Stream5
 * Configure and initialize DMA stream
   * Configure the struct that specifies the DMA stream
   * Calls HAL_DMA_Init() to initialize the DMA stream
     *  HAL_DMA_Init() first calls __HAL_DMA_DISABLE()
 * Enable the DMA transfer-complete interrupt
 */
void setupUSART2DMA( void )
{
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
    // * Circular mode restarts transfers after a buffer is filled
    // * (See the MCU reference manual, section 8.3.9.)
    usart2DmaRx.Init.Mode = DMA_CIRCULAR;

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
    assert_param(HAL_DMA_Init(&usart2DmaRx) == HAL_OK);

    // DMA stream x configuration register (DMA_SxCR)
    // * TCIE: transfer-complete interrupt enable
    DMA1_Stream5->CR |= DMA_SxCR_TCIE;
}

/**
 * Start an interrupt-driven receive.
 */
int32_t startReceiveDMA( void )
{

    // Get the DMA peripheral ready to receive data immediately before enabling UART
    // * __HAL_DMA_DISABLE : Disables the specified DMA Stream.
    __HAL_DMA_DISABLE(&usart2DmaRx);
    // Wait for the EN bit to be set to 0.
    // * Described in ST's “Using the . . . STM32F7 Series DMA controller” (AN4031)
    while((DMA1_Stream5->CR & DMA_SxCR_EN) != 0);

    // setupUSART2DMA():
    // * Configure and initialize DMA stream
    // * Enable the DMA transfer-complete interrupt
    setupUSART2DMA();

    // DMA is configured to write to two buffers: rxData1 and rxData2.
    // * In configuring DMA:
    //   * The first buffer's address is specified in the DMA register: DMA1_Stream5->M0AR
    //   * The second buffer's address is specified in: DMA1_Stream5->M1AR
    // * DMA1_Stream5->M0AR is set to the address of rxData1, by HAL_DMA_Start()
    //   * It is called further below. The address of rxData1 is an argument.
    // * DMA1_Stream5->M1AR is set below.

    // Setup second address for double-buffered mode
    DMA1_Stream5->M1AR = (uint32_t) rxData2;

    // NOTE: HAL_DMA_Start explicitly disables double-buffer mode
    //	  	 so we'll explicitly enable double buffer mode later when
    //		 the actual transfer is started
    // HAL_DMA_Start : Starts the DMA Transfer
    // * Enables the DMA stream (sets the EN bit in the DMA_SxCR register).
    // * Parameter USART2->RDR : USART Receive Data register
    if(HAL_DMA_Start(&usart2DmaRx, (uint32_t)&(USART2->RDR), (uint32_t)rxData1, DMA_BUFFER_LENGTH) != HAL_OK)
    {
        test_startReceiveDMA_halDmaStartFailed++;
        return -1;
    }

    // Disable the stream and controller so we can setup dual buffers
    __HAL_DMA_DISABLE(&usart2DmaRx);
    // Wait for the EN bit to be set to 0.
    while((DMA1_Stream5->CR & DMA_SxCR_EN) != 0);

    // Set the double-buffer mode
    DMA1_Stream5->CR |= DMA_SxCR_DBM;

    // Re-enable the stream and controller
    __HAL_DMA_ENABLE(&usart2DmaRx);
    DMA1_Stream5->CR |= DMA_SxCR_EN;

    return 0;
}

void startUart4Traffic( TimerHandle_t xTimer )
{
    SetupUart4ExternalSim(BAUDRATE);
}

void uartPrintOutTask( void* NotUsed)
{
    #define MAX_BLOCK_TIME 100

    // * rxBufferedData will hold a copy of the stream-buffer
    //   * The stream-buffer's length is specified in DMA_BUFFER_LENGTH.
    //   * Make rxBufferedData long enough to hold the copy of the stream-buffer,
    //     plus an added null-terminator.
    //   * The added null-terminator is needed to pass rxBufferedData to SEGGER_SYSVIEW_Print.
    uint8_t rxBufferedData[DMA_BUFFER_LENGTH+1];
    uint8_t i;

    // Configure and initialize the DMA stream
    startReceiveDMA();

    // Initialize USART2, pass usart2DmaRx for use in receive
    STM_UartInit(USART2, BAUDRATE, NULL, &usart2DmaRx);

    // Control register 3 (USART_CR3)
    // * DMAR: DMA enable receiver
    // * 1: DMA mode is enabled for reception
    USART2->CR3 |= USART_CR3_DMAR_Msk;

    // Clear error flags.
    // Needed for reception to resume after being paused in the debugger.
    USART2->ICR |= ( USART_ICR_FECF | USART_ICR_PECF |
                     USART_ICR_NCF | USART_ICR_ORECF );

    // For this specific instance, we'll avoid enabling UART interrupts for errors since
    // we'll wind up with a lot of noise on the line at high baud-rates (the way the ISR is written will
    // cause a transfer to terminate if there are any errors are detected, rather than simply
    // continue with what data it can).
    //USART2->CR3 |= (USART_CR3_EIE);   //enable error interrupts
    //USART2->CR1 |= USART_CR1_RXNEIE | USART_CR1_PEIE);

    //NVIC_SetPriority(USART2_IRQn, 6);
    //NVIC_EnableIRQ(USART2_IRQn);

    // Enable the UART
    USART2->CR1 |= USART_CR1_UE;

    while(1)
    {
        uint8_t numBytes = xStreamBufferReceive( streamBuffer,
                                                 rxBufferedData,
                                                 DMA_BUFFER_LENGTH,
                                                 MAX_BLOCK_TIME );

        if(numBytes > 0)
        {
            // The string sent by UART4 has a null-terminator.
            // Replace the null-terminator with a "#".
            for(i=0; i<=(numBytes-1); i++){
                if (rxBufferedData[i]==0){
                    rxBufferedData[i]='#';
                    break;
                }
            }
            // Add a null-terminator to the end of the string (needed by SEGGER_SYSVIEW_Print)
            rxBufferedData[numBytes] = 0;

            SEGGER_SYSVIEW_PrintfHost("received: ");
            SEGGER_SYSVIEW_Print((char*)rxBufferedData);

            test_uartPrintOutTask_received++;
            if (numBytes < DMA_BUFFER_LENGTH){
                test_uartPrintOutTask_xferNotComplete++;
            }
        }
        else
        {
            // * UART4 is started 5 seconds after the scheduler is started.
            // * So, xStreamBufferReceive will timeout 50 times before
            //   UART4 starts sending data.
            test_uartPrintOutTask_xStreamBufferReceiveTimeout++;
            SEGGER_SYSVIEW_PrintfHost("timeout");
        }
    }
}

/**
 * Given the DMA setup performed by setupUSART2DMA
 * this ISR will only execute when a DMA transfer is complete.
 *
 * Each time a transfer is completed, the DMA controller will
 * automatically start writing to the opposite buffer.
 * For example the controller will initially be writing to the buffer
 * specified in DMA1_Stream5->M0AR.
 * After this buffer is filled, the DMA controller will start writing
 * to the buffer in DMA1_Stream5->M1AR.
 *
 */
void DMA1_Stream5_IRQHandler(void)
{
    uint16_t numWritten = 0;
    volatile uint8_t* dmaBufferPtr = NULL;
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
    SEGGER_SYSVIEW_RecordEnterISR();

    // DMA high interrupt status register (DMA_HISR)
    // * TCIF[7:4]: stream x transfer-complete interrupt-flag
    if(DMA1->HISR & DMA_HISR_TCIF5)
    {

        // Copy data from the correct memory-buffer into the freeRTOS stream buffer.
        // * The DMA_SxCR_CT bit indicates the "Current Target"
        //   which is either DMA_SxM0AR or DMA_SxM1AR.
        // * See the MCU reference manual, section 8.5.5 for details
        //   on the CT bit of the DMA_SxCR register.
        if(DMA1_Stream5->CR & DMA_SxCR_CT)
        {
            dmaBufferPtr = rxData1;
        }
        else
        {
            dmaBufferPtr = rxData2;
        }

        // Send the buffer to the stream-buffer.
        numWritten = xStreamBufferSendFromISR(	streamBuffer,
                                                (uint8_t*) dmaBufferPtr,
                                                DMA_BUFFER_LENGTH,
                                                &xHigherPriorityTaskWoken);

        // OH NO - not everything was written to the stream
        // Spin, to identify the error.
        while(numWritten != DMA_BUFFER_LENGTH);

        // DMA high interrupt flag clear register (DMA_HIFCR)
        // * CTCIF[7:4]: stream x clear transfer-complete interrupt-flag
        //   * Writing 1 to this bit clears the corresponding TCIFx flag in
        //     the DMA_HISR register.
        DMA1->HIFCR |= DMA_HIFCR_CTCIF5;
    }
    else
    {
        test_DMA1_Stream5_IRQHandler_notExpected++;
    }

    SEGGER_SYSVIEW_RecordExitISR();
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
