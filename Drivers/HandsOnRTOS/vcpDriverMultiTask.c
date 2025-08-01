/*

---------------------------------------------------------------------------------------

Licenses:
- Copyright (c) 2019-2025 Packt Publishing, under the MIT License.
- Based on code copyrighted by Brian Amos, 2019, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-2nd-edition

 */

#include <usb_device.h>
#include "usbd_cdc.h"
#include <FreeRTOS.h>
#include <stream_buffer.h>
#include <task.h>
#include <timers.h>
#include <semphr.h>
#include <vcpDriverMultiTask.h>
#include <stdbool.h>
#include <Nucleo_F767ZI_GPIO.h>
#include <stdbool.h>

/******************************** NOTE *****************************/
/* This driver supports sending/receiving data over USB, to/from the VCP on the USB host.
 * * The driver can be used by multiple tasks, for sending.
 */

#define resetTimeInSeconds 30

/**
 * rxBuffLen should be at least as large as the length defined in usbd_cdc_if.c
 * to avoid dropped data.
 * If multiple transfers should be placed into vcom_rxStream before being read
 * by the application, the stream-buffers should be larger.
 *
 * The way usbd_cdc_if.c is written, any newly received data not fitting into the stream
 * buffer is dropped
 **/
#define rxBuffLen 1024
StreamBufferHandle_t vcom_rxStream = NULL;

// Global variable, used by the driver, and its callers
volatile uint32_t vcpResetPerformanceData = 0;

// Global handles, used within the driver
static StreamBufferHandle_t vcpTransmitStreamBuffer = NULL;
static TaskHandle_t vcpTransmitTaskHandle = NULL;
static SemaphoreHandle_t vcpTransmitCompleteSemaphore = NULL;
static SemaphoreHandle_t vcpStreamBufferSendMutex = NULL;
static SemaphoreHandle_t vcpStreamBufferIsEmptySemaphore = NULL;

// Global variable, used within the driver
// * This variable is used by multiple tasks, so operations on it must be atomic.
//   * False: 0, True: 1
volatile static uint32_t vcpTransmitTask_initialized = 0;

// Global variables, used within the driver, for performance analysis
static uint32_t test_vcpTransmitTask_transmitCount = 0;
static uint32_t test_vcpTransmitTask_minBytesReceived = VCP_DRIVER_MULTI_TASK_BUFFER_LEN;
static uint32_t test_vcpTransmitTask_maxBytesReceived = 0;
static uint32_t test_vcpTransmitTask_bytesTransmitted = 0;
static TickType_t test_vcpTransmitTask_maxTransmissionTime = 0;
static TickType_t test_vcpTransmitTask_minTransmissionTime = portMAX_DELAY;
static uint32_t test_vcpSend_waitingForSBspace = 0;
static uint32_t test_vcpSend_gotSBspace = 0;

void vcpTransmitTask( void* NotUsed);
void vcpTransmitComplete( void );
void vcpResetTimerCallBack( TimerHandle_t xTimer );

/********************************** PUBLIC FUNCTIONS *************************************/

/**
 * * Initialize the driver:
 *   * Initialize the USB peripheral and HAL-based USB stack.
 *   * Create FreeRTOS objects used by the driver: stream-buffer, semaphore, mutex,
 *     task, and timer.
 **/
void vcpInit( void )
{
    // Initialize the USB connection
    MX_USB_DEVICE_Init();

    // Create the semaphore for signaling transmit-complete
    vcpTransmitCompleteSemaphore = xSemaphoreCreateBinary();
    assert_param( vcpTransmitCompleteSemaphore != (SemaphoreHandle_t) NULL);

    // Create stream-buffers
    vcpTransmitStreamBuffer = xStreamBufferCreate( VCP_DRIVER_MULTI_TASK_BUFFER_LEN, 1 );
	vcom_rxStream  = xStreamBufferCreate( rxBuffLen, 1); 
    assert_param( vcpTransmitStreamBuffer != NULL);
	assert_param( vcom_rxStream != NULL); 

    // Create mutex used for serializing stream-buffer send
    vcpStreamBufferSendMutex = xSemaphoreCreateMutex();
    assert_param(vcpStreamBufferSendMutex != (SemaphoreHandle_t) NULL);

    // Create semaphore used for signaling that the stream-buffer was emptied
    vcpStreamBufferIsEmptySemaphore = xSemaphoreCreateBinary();
    assert_param(vcpStreamBufferIsEmptySemaphore != (SemaphoreHandle_t) NULL);

    // Create and start the timer, used for collecting performance-data
    TimerHandle_t oneShotHandle =
            xTimerCreate( "vcpResetTimerCallBack", // Name for timer
                    (resetTimeInSeconds*1000)/portTICK_PERIOD_MS, // Period of timer in ticks
                    pdFALSE,                 // Auto-reload flag
                    NULL,                    // Unique ID for timer
                    vcpResetTimerCallBack);  // Callback function
    assert_param(oneShotHandle != NULL);
    xTimerStart(oneShotHandle, 0);

    // Create task vcpTransmitTask
    // * The task gets messages from the stream-buffer,
    //   and transmits them over the USB connection.
    assert_param(xTaskCreate(vcpTransmitTask, "vcpTransmitTask", 512, NULL, (configMAX_PRIORITIES-1), &vcpTransmitTaskHandle) == pdPASS);
}


/** 
 * Return the streamBuffer handle.
 * This is wrapped in a function rather than exposed directly
 * so external modules aren't able to change where the handle points
 *
 * NOTE:	This return value will not be valid until VirtualCommInit has
 * 			been run
 */
StreamBufferHandle_t const* GetUsbRxStreamBuff( void )
{
	return &vcom_rxStream;
}


/**
 * Send data over the USB connection
 * * This function can be called by multiple tasks.
 * * For the data to be sent, this function puts it in the stream-buffer (vcpTransmitStreamBuffer).
 *   * The data is only put in the stream-buffer if all of the data fits.
 * * The task vcpTransmitTask() receives data from the stream-buffer and transmits
 *   it over the USB connection
 *
 * @param Buff: pointer to the buffer containing bytes to send
 * @param Len: number bytes to send. The maximum is the length of the stream-buffer.
 * @param ticksToWait: maximum number of ticks to wait, for vcpSend()
 * 		  * portMAX_DELAY specifies there is no maximum number
 * 		  * 0 specifies to not block
 * @returns an int32_:
 *        * If negative, the data could not be sent. An error code is specified.
 *        * Otherwise, data was sent.  Specifies the number of bytes sent, and it is
 *          equal to the input parameter Len.
 */
int32_t vcpSend(uint8_t * Buff, uint16_t Len, TickType_t ticksToWait)
{
    uint32_t bytesAvailable;
    int32_t returnValue;
    TickType_t startTickCount = 0;
    TickType_t currentTickCount = 0;
    TickType_t elapsedTickCount = 0;
    TickType_t remainingWaitingTime = 0;
    static bool resetPerformed = false;

    // Spin if the task vcpTransmitTask is not yet ready to transmit
    while (vcpTransmitTask_initialized == 0){
        vTaskDelay(1);
    }

    // * The FreeRTOS tick-counter is used to measure wait-time
    // * The FreeRTOS tick-counter's properties:
    //   * It is incremented at every FreeRTOS tick
    //   * The tick-count is stored in an unsigned int, with data type TickType_t
    //   * The tick-count's maximum value is portMAX_DELAY (defined in portmacro.h)
    //   * The tick-counter wraps from the maximum-value to 0.

    // The return-value's data-type is TickType_t
    startTickCount = xTaskGetTickCount();

    // Get the mutex, to serialize sends to the stream-buffer
    if ( xSemaphoreTake(vcpStreamBufferSendMutex, ticksToWait) == pdPASS )
    {

        // Calculate how long xSemaphoreTake() took
        // * Only needed if ticksToWait is not 0 nor portMAX_DELAY
        if ( (ticksToWait != 0) && (ticksToWait != portMAX_DELAY) ){
            currentTickCount = xTaskGetTickCount();

            if (currentTickCount >= startTickCount) {
                elapsedTickCount = currentTickCount - startTickCount;
            }
            // Else, the tick-counter wrapped
            else {
                // Use a calculation-order that does not produce overflow
                elapsedTickCount = (portMAX_DELAY - startTickCount) + currentTickCount + 1;
            }
        }

        // Determine remaining waiting-time
        if ( (ticksToWait == 0) ||
             (ticksToWait == portMAX_DELAY) ) {
            remainingWaitingTime = ticksToWait;
        }
        else if (elapsedTickCount < ticksToWait) {
            remainingWaitingTime = ticksToWait - elapsedTickCount;
        }
        else {
            remainingWaitingTime = 0;
        }

        // Reset the performance-analysis data, e.g., set counters to 0.
        // * This is done once, 30 seconds after the driver starts.
        // * When the driver's timer expires, the timer's call-back sets
        //   vcpResetPerformanceData to 1.
        if ( (resetPerformed == false) && (vcpResetPerformanceData == 1) )  {
            test_vcpSend_waitingForSBspace = 0;
            test_vcpSend_gotSBspace = 0;
            resetPerformed = true;
        }

		// * Set the semaphore vcpStreamBufferIsEmptySemaphore:
        //   * Set it to 0, if it's not 0 already.
        //
        // * This semaphore is used to signal when the stream-buffer-receive occurs in
        //   vcpTransmitTask(). That receive will empty the stream-buffer.
        // * In vcpTransmitTask(), after it receives from the stream-buffer, this semaphore
        //   is set to 1, if it isn't 1 already
        // * Later in the present function, it will attempt to take this semaphore. The
		//   semaphore-take's maximum wait-time is set to remainingWaitingTime.
        returnValue = (int32_t) uxSemaphoreGetCount(vcpStreamBufferIsEmptySemaphore);
        if (returnValue == 1) {
		    // The semaphore is 1, so call xSemaphoreTake() to set the semaphore to 0
            returnValue = (int32_t) xSemaphoreTake(vcpStreamBufferIsEmptySemaphore, 0 );
            // For testing: the semaphore should always be taken:
            assert_param(returnValue == pdPASS);
        }

        bytesAvailable = xStreamBufferSpacesAvailable(vcpTransmitStreamBuffer);

        // If there is not enough space in the stream-buffer, and there is remaining
		// waiting-time, then wait for the stream-buffer to be emptied.
        // * This is done by taking the semaphore vcpStreamBufferIsEmptySemaphore
        if ((bytesAvailable < Len) && (remainingWaitingTime > 0)) {

            test_vcpSend_waitingForSBspace++; // For performance analysis

            // * Take the semaphore that indicates vcpTransmitTask() has emptied the stream-buffer.
            //   * In taking the semaphore, specify remainingWaitingTime for the maximum wait-time parameter
            if(xSemaphoreTake(vcpStreamBufferIsEmptySemaphore, remainingWaitingTime) == pdPASS){
                // For testing: the stream-buffer should be empty.
                assert_param(xStreamBufferIsEmpty(vcpTransmitStreamBuffer));

                bytesAvailable = rxBuffLen;  // The whole stream-buffer is available
                test_vcpSend_gotSBspace++; // For performance analysis
            }
        }

        // If there is enough space in the stream-buffer, send the data.
        if (bytesAvailable >= Len){
            // * For a particular stream-buffer, if xStreamBufferSend() is called by multiple tasks,
            //   then the calls must be non-blocking. (This is according to the FreeRTOS docs.)
            // * Since vcpSend can be called by multiple tasks, this call to xStreamBufferSend() is
            //   non-blocking.
            returnValue = (int32_t) xStreamBufferSend(  vcpTransmitStreamBuffer, Buff, Len, 0 );
            // For testing: all of the data should have been sent:
            assert_param(returnValue == Len);
        }
        else {
            returnValue = VCP_SEND_CANNOT_SEND;
        }

        xSemaphoreGive(vcpStreamBufferSendMutex);

    }
    else{
        returnValue = VCP_SEND_MUTEX_NOT_AVAILABLE;
    }

    return returnValue;
}


/********************************** PRIVATE FUNCTIONS *************************************/

/**
 * FreeRTOS task that takes data out of the stream-buffter (vcpTransmitStreamBuffer), and
 * transmits it over USB, to the VCP on the USB host.
 *
 * This function waits for a semaphore. The semaphore is given by a callback function that
 * runs when the prior transmit completes.
 *
 */
void vcpTransmitTask( void* NotUsed)
{
    // Declare as static, so it's not on the stack
    static uint8_t tempBuffer[VCP_DRIVER_MULTI_TASK_BUFFER_LEN];
	
    USBD_CDC_HandleTypeDef *hcdc = NULL;
    uint16_t numBytes;
    TickType_t startTransmission, transmissionTime;
    bool resetPerformed = false;
    BaseType_t returnValue;

    // If necessary, wait for USB to finish initialization.
    // * The wait is implemented here to take advantage of FreeRTOS's delay-function,
    //   to avoid polling.
    // * This is the highest-priority task, so it will run first.
    while(hcdc == NULL)
    {
        // hUsbDeviceFS is a global handle, defined in Drivers/HandsOnRTOS/usb_device.c
        hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
        vTaskDelay(1);
    }

    // * Setup our own callback, which is called when USB transmission is complete.
    // * vcpTransmitComplete is a function, defined below
    hcdc->TxCallBack = vcpTransmitComplete;

    // Ensure the USB interrupt-priority is low enough to allow for
    // FreeRTOS API calls within the ISR
    NVIC_SetPriority(OTG_FS_IRQn, 6);

    // Indicate data can now be transmitted
    vcpTransmitTask_initialized = 1;

    // Turn-on the green user-LED, to show the driver is running
    GreenLed.On();

    while(1)
    {
        // Receive from the stream-buffer vcpTransmitStreamBuffer.
        // * All of the stream-buffer's data will be received
        // * The maximum wait-time is unlimited
        numBytes = (uint16_t) xStreamBufferReceive( vcpTransmitStreamBuffer,
                                                    tempBuffer,
                                                    VCP_DRIVER_MULTI_TASK_BUFFER_LEN,
                                                    portMAX_DELAY);

        // For testing: the stream-buffer should be empty.
        assert_param(xStreamBufferIsEmpty(vcpTransmitStreamBuffer));

        // Indicate the stream-buffer receive occurred
        // * This semaphore is used by vcpSend()
        // * If the semaphore is not 1, set it to 1
        returnValue = (int32_t) uxSemaphoreGetCount(vcpStreamBufferIsEmptySemaphore);
        if (returnValue == 0) {
            returnValue = xSemaphoreGive(vcpStreamBufferIsEmptySemaphore);
            // For testing: the semaphore should always been given here.
            assert_param(returnValue == pdPASS);
        }

        // Reset the performance-analysis data, e.g., set counters to 0.
        // * This is done once, 30 seconds after the driver starts.
        // * When the driver's timer expires, the timer's call-back sets
        //   vcpResetPerformanceData to 1.
        if ( (resetPerformed == false) && (vcpResetPerformanceData == 1) )  {
            test_vcpTransmitTask_transmitCount = 0;
            test_vcpTransmitTask_minBytesReceived =  VCP_DRIVER_MULTI_TASK_BUFFER_LEN;
            test_vcpTransmitTask_maxBytesReceived =  0;
            test_vcpTransmitTask_bytesTransmitted =  0;
            test_vcpTransmitTask_maxTransmissionTime = 0;
            test_vcpTransmitTask_minTransmissionTime = portMAX_DELAY;
            resetPerformed = true;
        }

        // Record data for performance analysis

        test_vcpTransmitTask_transmitCount++;
        test_vcpTransmitTask_bytesTransmitted += numBytes;

        // Record the min and max bytes-received, for performance analysis.
        if (numBytes < test_vcpTransmitTask_minBytesReceived){
            test_vcpTransmitTask_minBytesReceived = numBytes;
        }
        if (numBytes > test_vcpTransmitTask_maxBytesReceived){
            test_vcpTransmitTask_maxBytesReceived = numBytes;
        }

        startTransmission = xTaskGetTickCount();

        // Transmit the data
        // * hUsbDeviceFS is a global handle, defined in Drivers/HandsOnRTOS/usb_device.c
        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, tempBuffer, numBytes);
        USBD_CDC_TransmitPacket(&hUsbDeviceFS);

        // Wait for the semaphore that indicates transmission is done.
        xSemaphoreTake(vcpTransmitCompleteSemaphore, portMAX_DELAY);

        // Record the min and max transmission-time, for performance analysis.
		// * This calculation assumes the FreeRTOS tick-counter has not wrapped
        transmissionTime =  xTaskGetTickCount() - startTransmission;
        if ( transmissionTime > test_vcpTransmitTask_maxTransmissionTime ){
            test_vcpTransmitTask_maxTransmissionTime = transmissionTime;
        }
        if ( transmissionTime < test_vcpTransmitTask_minTransmissionTime ){
            test_vcpTransmitTask_minTransmissionTime = transmissionTime;
        }

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


//
// The call-back function that is called when the driver's
// one-shot timer expires.
//
void vcpResetTimerCallBack( TimerHandle_t xTimer )
{
    // Signal the tasks to reset their performance data
    vcpResetPerformanceData = 1;
    BlueLed.On();
}
