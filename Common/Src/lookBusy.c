/*
 Title:  lookBusy functions

Licenses:
- Copyright (c) 2025 Packt Publishing, under the MIT License.
- See the code-repository's license statement for more information:
  - https://github.com/PacktPublishing/Hands-On-RTOS-with-Microcontrollers-2nd-edition

*/

#include <stm32f7xx_hal.h>


/*
  FUNCTION: lookBusy()
  DESCRIPTION: Spin-loop for simulating useful processing.
  DEPENDENCIES: Use lookBusyIterationRate() to calculate lookBusy's iteration-rate.
*/
void lookBusy( uint32_t numIterations )
{
	volatile uint32_t __attribute__((unused)) dontCare = 0;
	for(int i = 0; i < numIterations; i++)
	{
		dontCare = i % 4;
	}
}


/*
   FUNCTION: lookBusyIterationRate()
   DESCRIPTION: This function returns lookBusy's iteration-rate:
   * The iterations are for lookBusy()'s for-loop.
   * The iteration-rate is measured as: iterations per 1 ms.
   DEPENDENCIES:
   * The SysTick timer is used.
     * It must be configured with a period of 1 ms, e.g., by HAL_Init.
     * Also, the timer must run on processor-clock frequency, which is the default.
   * HAL_Delay() is called, so HAL_Init() should be called before this function.
   * Interrupts are temporarily disabled, including the SysTick interrupt.
*/
uint32_t lookBusyIterationRate( void )
{
  uint32_t clockCyclesPerMilliSecond;
  uint32_t estimatedClockCyclesPerIteration;
  uint32_t timerPeriodDivisor;
  uint32_t numIterations;
  uint32_t counterStartValue;
  uint32_t counterStopValue;
  uint32_t elapsedClockCycles;
  uint32_t actualClockCyclesPerIteration;
  uint32_t iterationsPerMilliSecond;

  // How the iteration-rate is determined:
  // * (In these comments, units are shown in brackets, e.g., [iterations/ms] )
  // * The number of clock-cycles in 1 ms is determined, i.e., R [clock_cycles/ms]
  // * lookBusy() is then run for X iterations.
  // * The number of clock-cycles it took for the X iterations is measured, i.e., N [clock_cycles]
  // * The number of clock-cycles per iteration is calculated as: N/X [clock_cycles/iteration]
  // * The iteration-rate is calculated as:  R / (N/X) [iterations/ms]
  // * For calls to lookBusy(), processing done in addition to the for-loop is considered negligible.


  // Get the number of clock-cycles in 1 ms, from the SysTick timer's configuration.
  //
  // The SysTick timer counts down from the value in SysTick->LOAD
  // * The timer counts down to 0, and on the next clock-cycle the counter is set to the value in LOAD.
  // * The SysTick exception request is activated when counting from 1 to 0.
  // * For the timer-period of 1 ms, there are (SysTick->LOAD + 1) clock cycles
  // * References:
  //   * https://developer.arm.com/documentation/dui0471/m/handling-processor-exceptions/configuring-systick
  //   * "Arm Cortex-M7 Devices Generic User Guide"
  clockCyclesPerMilliSecond = (uint32_t) SysTick->LOAD + (uint32_t) 1;

  // Determine the number of iterations for lookBusy() to run
  // * How long lookBusy() runs will be measured in clock-cycles. Measurements up to 1 ms can be made.
  // * Calculate an estimate of the number of iterations lookBusy() can run in 500 us.
  // * 500 us is used to be safe, to ensure lookBusy() runs less than 1 ms.

  // Specify an estimate for the number of clock-cycles per iteration of lookBusy()
  // * There are about 20 assembly instructions per iteration.
  // * The SysTick timer's clock frequency is the same as the processor's clock frequency.
  // * The clock-cycles per assembly instruction are not specified for Cortext M7 processors,
  //   but 5 is a reasonable over-estimate.
  // * So, 100 clock-cycles per iteration would be a reasonable estimate (20 * 5)
  // * From testing the present function, the actual number was found to be around 33 clock-cycles per iteration.
  estimatedClockCyclesPerIteration = 33;
  // Used to make the calculation be for 1/2 of a ms (500 us):
  timerPeriodDivisor = 2;
  // The number of iterations in 500 ms can be calculated like this:
  //   numIterations = (clockCyclesPerMilliSecond / timerPeriodDivisor) / estimatedClockCyclesPerIteration;
  // This statement makes the same calculation, but only uses one integer divide:
  numIterations = clockCyclesPerMilliSecond / (timerPeriodDivisor * estimatedClockCyclesPerIteration);

  // Ensure no ISRs are run while lookBusy() is running
  // HAL_Delay(1) is used to avoid HAL's time-base interrupt from occurring while lookBusy() runs.
  HAL_Delay(1);
  // Disable interrupts
  __disable_irq();

  // Get the SysTick-counter's value before running lookBusy()
  counterStartValue = (uint32_t) SysTick->VAL;
  lookBusy(numIterations);
  // Get the SysTick-counter's value after running lookBusy()
  counterStopValue = (uint32_t) SysTick->VAL;

  // Enable IRQs
  __enable_irq();
  // __ISB() is a memory-barrier instruction.  It's needed after __enable_irq().
  __ISB(); // Allow pended interrupts to be recognized


  // Calculate the number of clock-cycles from counterStartValue to counterStopValue
  if (counterStartValue > counterStopValue){
    elapsedClockCycles = counterStartValue - counterStopValue;
  }
  else {
    // Use a calculation-order that does not produce overflow with uint32_t values:
    elapsedClockCycles = (clockCyclesPerMilliSecond - counterStopValue) + counterStartValue;
  }

  // Calculate the number of for-loop iterations per ms
  // * Two divides are used to ensure no overflow (vs one divide and a multiply)
  actualClockCyclesPerIteration = elapsedClockCycles / numIterations;
  iterationsPerMilliSecond = clockCyclesPerMilliSecond / actualClockCyclesPerIteration;

  return(iterationsPerMilliSecond);

}
