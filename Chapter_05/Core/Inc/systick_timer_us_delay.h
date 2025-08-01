#ifndef INC_SYSTICK_TIMER_US_DELAY_H_
#define INC_SYSTICK_TIMER_US_DELAY_H_

/*

	Function: delay_us(microSeconds)

	Title: Delay, for the specified number of micro-seconds

	Description: The function spins for the specified number of micro-seconds

	Parameter:
	* microSeconds : specifies the delay, in micro-seconds
	* Permissible values:
	 * 0 <= microSeconds <= (999-val_read_period)
	 * val_read_period is the maximum period for the reads of SysTick->VAL, in the while-loop
	   * It's the time between executions of this instruction:  counterCurrentValue = (uint32_t) SysTick->VAL;
	 * val_read_period specifies micro-seconds, rounded up to the nearest integer

	MCU requirements:
	* ARMv7-M core, or similar core with:
	 * A SysTick timer and registers, as specified in:
		 * Reference: "Arm Cortex-M7 Devices Generic User Guide"
	 * Reads to the SysTick->VAL register must be an atomic operation
		 * Reference: "ARMv7-M Architecture Reference Manual"
	* SysTick timer configured for 1 ms period, e.g., configured by HAL_Init()

	Delay errors:
	* The actual delay is slightly longer than the input micro-seconds
		* Some causes for extra delay include:
		 * The execution time prior to the spin loop
		 * The execution time from when counterCurrentValue is last read, until when the function exits
		 * The period for the reads of SysTick->VAL, in the while-loop
	* The actual delay can also be longer if ISRs run concurrently with this function, near the end of
	  the delay.

	* This function was tested with an oscilloscope:
	 *  STM MCU STM32F767ZI
	 *  SysTick clock: 16 MHz
	 *  SYS->LOAD: 15999
	 *  Parameter values tested: 100, 500, and 900 micro-seconds
	 *  The actual delays were approximately 5 to 15 micro-seconds longer than the
			parameter value.

	Return value: None
*/

/**
   License:
   - Copyrighted (c) by PACKT, 2025, under the MIT License.
   - See the code-repository's license-statement for more information:
     - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-2nd-edition
 */

static inline void delay_us(volatile uint32_t microSeconds)
{
  uint32_t timerPeriodClockCycles;
  uint32_t counterStartValue;
  uint32_t counterTicsToWait;
  uint32_t counterCurrentValue;
  uint32_t elapsedTics;

  // The SysTick timer counts down from the value in SysTick->LOAD
  // * The timer counts down to 0, and on the next clock-cycle the counter is set to the value in LOAD.
  // * The SysTick exception request is activated when counting from 1 to 0.
  // * For the timer period of 1 ms, there are (SysTick->LOAD + 1) clock cycles
  // * References:
  //   * https://developer.arm.com/documentation/dui0471/m/handling-processor-exceptions/configuring-systick
  //   * "Arm Cortex-M7 Devices Generic User Guide"
  timerPeriodClockCycles = (uint32_t) SysTick->LOAD + (uint32_t) 1;

  // Calculate the number of timer tics for the input microSeconds
  //
  // * Get the integer-division part:
  //   * The equation, for 1 ms (units in brackets):
  //     (timerPeriodClockCycles [tics] / 1000 [micro-seconds]) * microSeconds [micro-seconds]
  //   * In the code, executing the integer division last produces a more accurate result.
  counterTicsToWait = (uint32_t) ( microSeconds * timerPeriodClockCycles ) / (uint32_t)1000;
  // * Get the remainder part:
  //   * The equation, for 1 ms (units in brackets):
  //     (timerPeriodClockCycles [tics] % 1000 [micro-seconds]) [tics] * (microSeconds [micro-seconds] / 1000 [micro-seconds])
  //   * In the code, the integer division must be executed last, since (microSeconds/1000) is typically less than one
  counterTicsToWait += (uint32_t)((timerPeriodClockCycles % (uint32_t)1000) * microSeconds) / (uint32_t)1000;

  // Get starting SysTick timer count
  counterStartValue = (uint32_t) SysTick->VAL;

  // Loop while elapsedTics is less than counterTicsToWait
  do{
	  // Get current SysTick timer count
	  counterCurrentValue = (uint32_t) SysTick->VAL;
	  // Calculate elapsedTics:  the number of counter tics since counterStartValue
	  if (counterStartValue > counterCurrentValue){
		  elapsedTics = counterStartValue - counterCurrentValue;
	  }
	  else {
	  	// The calculation order must work with uint32_t values
		  elapsedTics = (timerPeriodClockCycles - counterCurrentValue) + counterStartValue;
	  }
  } while (elapsedTics < counterTicsToWait);

}

#endif /* INC_SYSTICK_TIMER_US_DELAY_H_ */
