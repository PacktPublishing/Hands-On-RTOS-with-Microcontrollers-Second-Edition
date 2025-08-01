/*

---------------------------------------------------------------------------------------

Licenses:
- Copyright (c) 2019-2025 Packt Publishing, under the MIT License.
- Based on code copyrighted by Brian Amos, 2019, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-Second-Edition

*/


#include <usb_device.h>
#include "usbd_cdc.h"
#include <FreeRTOS.h>
#include <stream_buffer.h>
#include <task.h>
#include <semphr.h>
#include <SEGGER_SYSVIEW.h>
#include <vcpDriver.h>
#include <stdbool.h>


/******************************** NOTE *****************************/
/* This driver supports transmitting data over USB, to a VCP on the USB host.
 * * Only one task can use vcpSend(), the driver's function for sending data.
 *   * vcpSend() sends the data to a stream-buffer. Only one task can do that send at a time. 
 *   * Also, vcpSend() is not designed for concurrent use by multiple tasks.
 */

// Using the same buffer-length that is defined in Drivers/HandsOnRTOS/usbd_cdc_if.c
#define BUFFER_LEN 1024
static uint8_t tempBuffer[BUFFER_LEN];

// Receiving USB data is not implemented in this driver.
// * This code is needed for usbd_cdc_if.c to compile
#define rxBuffLen 2048
StreamBufferHandle_t vcom_rxStream = NULL;

static StreamBufferHandle_t vcpTransmitStreamBuffer = NULL;
static TaskHandle_t vcpTransmitTaskHandle = NULL;
static SemaphoreHandle_t vcpTransmitCompleteSemaphore;

// Shared by tasks, so operations on the variable must be atomic.
// * False: 0, True: 1
volatile static uint32_t vcpTransmitTask_initialized = 0;

uint32_t testVcpSend_bytesDropped = 0;

void vcpTransmitTask( void* NotUsed);
void vcpTransmitComplete( void );

/********************************** PUBLIC FUNCTIONS *************************************/

/**
 * * Initialize the driver:
 *   * Initialize the USB peripheral and HAL-based USB stack.
 *   * Create FreeRTOS objects used by the driver: stream-buffer, semaphore, and task
**/
void vcpInit( void )
{
    // Initialize USB
    MX_USB_DEVICE_Init();

    // Create stream-buffer
	vcpTransmitStreamBuffer = xStreamBufferCreate( BUFFER_LEN, 1 );
	assert_param( vcpTransmitStreamBuffer != NULL);

    // Receiving USB data is not implemented in this driver.
    // * This code is needed for usbd_cdc_if.c to compile
	vcom_rxStream  = xStreamBufferCreate( rxBuffLen, 1);
	assert_param( vcom_rxStream != NULL);	

	// Create semaphore for signaling transmit-complete
	vcpTransmitCompleteSemaphore = xSemaphoreCreateBinary();
	assert_param( vcpTransmitCompleteSemaphore != (SemaphoreHandle_t) NULL);

	// Create the task.
	// * It gets messages from the stream-buffer,
	//   and transmits them over the USB connection.
	assert_param(xTaskCreate(vcpTransmitTask, "vcpTransmitTask", 256, NULL, (configMAX_PRIORITIES-1), &vcpTransmitTaskHandle) == pdPASS);
}

    // Receiving USB data is not implemented in this driver.
    // * This code is needed for usbd_cdc_if.c to compile
	StreamBufferHandle_t const* GetUsbRxStreamBuff( void )
    {
		return &vcom_rxStream;
    }

/**
 * Send data
 * * This function can only be called by one task.
 * * This function is passed a pointer to data that is to be sent.
 * * This function sends that data to the stream-buffer vcpTransmitStreamBuffer.
 *   * If there isn't enough space in the stream-buffer, the data that doesn't
 *     fit will be dropped.
 * * The task vcpTransmitTask() receives data from the stream-buffer and transmits
 *   it over USB, to the VCP on the USB host.
 *
 * @param Buff	pointer to the buffer to be sent
 * @param Len	number of bytes to send
 * @returns number of bytes placed into the stream-buffer
 */
uint32_t vcpSend(uint8_t * Buff, uint16_t Len)
{
    uint32_t numBytesCopied;

    // * Data should not be put in the stream-buffer until the task vcpTransmitTask
    //   has completed its initialized.
    // * If needed, spin until vcpTransmitTask has finished its initialization
    while (vcpTransmitTask_initialized == 0){
        vTaskDelay(1);
    }

    numBytesCopied = (uint32_t) xStreamBufferSend( vcpTransmitStreamBuffer, Buff, Len, 0);

    // * If there was not enough room in the stream-buffer, record the amount dropped.
    // * This variable is just used for testing.
	testVcpSend_bytesDropped += (uint32_t)Len - numBytesCopied;

	return numBytesCopied;
}


/********************************** PRIVATE FUNCTIONS *************************************/

/**
 * This task continually receives messages from the driver's stream-buffer (vcpTransmitStreamBuffer),
 * and it transmits the messages over USB, to the VCP on the USB host.
 *
 * This function waits for a semaphore. The semaphore is given by a callback function that
 * runs when the last transmit completes.
 *
 */
void vcpTransmitTask( void* NotUsed)
{
    uint16_t numBytes;

    USBD_CDC_HandleTypeDef *hcdc = NULL;

    // Ensure USB-initialization is completed.
    // * This code is implemented here, instead of main(), to take advantage of
    //   FreeRTOS's delay-function and avoid polling.
    while(hcdc == NULL)
    {
        // hUsbDeviceFS is a global handle, defined in Drivers/HandsOnRTOS/usb_device.c
        hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
        vTaskDelay(1);
    }

    // * Setup the callback. for when USB transmission completes.
    // * vcpTransmitComplete is a function, defined below.
    hcdc->TxCallBack = vcpTransmitComplete;

    // Ensure the USB interrupt-priority is low enough to allow for
    // FreeRTOS API calls within the ISR
    NVIC_SetPriority(OTG_FS_IRQn, 6);

    // Indicate that the task has been initialized
    vcpTransmitTask_initialized = 1;

	while(1)
	{
	    // Sending messages to SystemView will likely result in overflow, on SystemView.
		//SEGGER_SYSVIEW_PrintfHost("Waiting for stream-buffer");

		// Wait forever for data to become available in the stream-buffer vcpTransmitStreamBuffer.
		// * Up to BUFFER_LEN bytes of data will be copied into
		//   tempBuffer, when at least 1 byte is available.
		numBytes = (uint16_t) xStreamBufferReceive(	vcpTransmitStreamBuffer,
													tempBuffer,
													BUFFER_LEN,
													portMAX_DELAY);

		//SEGGER_SYSVIEW_PrintfHost("Bytes received from stream-buffer: %u ", numBytes);

        // Transmit the data
		// * hUsbDeviceFS is a global handle, defined in Drivers/HandsOnRTOS/usb_device.c
        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, tempBuffer, numBytes);
        USBD_CDC_TransmitPacket(&hUsbDeviceFS);

        // Wait for the semaphore that indicates USB transmission is done.
        xSemaphoreTake(vcpTransmitCompleteSemaphore, portMAX_DELAY);

        //SEGGER_SYSVIEW_PrintfHost("VCP transmit complete");

	}
}

//
// A call-back function that is called when the USB transmit completes.
//
void vcpTransmitComplete( void )
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	// Give the semaphore, to indicate the transmit completed
	xSemaphoreGiveFromISR(vcpTransmitCompleteSemaphore, &xHigherPriorityTaskWoken);

	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
