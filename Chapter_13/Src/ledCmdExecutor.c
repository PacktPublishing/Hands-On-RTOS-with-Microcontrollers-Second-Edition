#include <ledCmdExecutor.h>
#include <stdbool.h>

/**
 * Sets all 3 duty cycles for red, green, and blue LED's.
 * This function assumes that all pwmInstances have been
 * properly initialized.
 *
 * @param Args CmdExecArgs passed to the task
 * @param RedDuty red LED duty-cycle 0-100%
 * @param GreenDuty green LED duty-cycle 0-100%
 * @param BlueDuty blue LED duty-cycle 0-100%
 */
void setDutyCycles( const CmdExecArgs* Args, float RedDuty,
					float GreenDuty, float BlueDuty)
{
	Args->redPWM->SetDutyCycle(RedDuty);
	Args->greenPWM->SetDutyCycle(GreenDuty);
	Args->bluePWM->SetDutyCycle(BlueDuty);
}


/**
 * Args is CmdExecArgs
 */
void LedCmdExecution( void* Args )
{
	// The next command pulled from the queue will be placed into nextCmd
	LED_CMD_NUM currCmdNum = CMD_ALL_OFF;
	bool blinkingLedsOn = false;	// Tracks the current blinking state
	LedCmd nextLedCmd;

	// Ensure Args isn't NULL before dereference
	while(Args == NULL);
	CmdExecArgs args = *(CmdExecArgs*)Args;

	// iPWM pointers and the queue handle can't be NULL.
	// Stop here if they are, so we don't dereference a null pointer. . .
	while(args.redPWM == NULL);
	while(args.bluePWM == NULL);
	while(args.greenPWM == NULL);
	while(args.ledCmdQueue == NULL);

	while(1)
	{
		if(xQueueReceive(args.ledCmdQueue, &nextLedCmd, 250) == pdTRUE)
		{
			switch(nextLedCmd.cmdNum)
			{
				case CMD_SET_INTENSITY:
					currCmdNum = CMD_SET_INTENSITY;
					setDutyCycles(&args, nextLedCmd.red, nextLedCmd.green, nextLedCmd.blue);
					break;
				case CMD_BLINK:
					currCmdNum = CMD_BLINK;
					blinkingLedsOn = true;
					setDutyCycles(&args, nextLedCmd.red, nextLedCmd.green, nextLedCmd.blue);
					break;
				case CMD_ALL_OFF:
					currCmdNum = CMD_ALL_OFF;
					setDutyCycles(&args, 0, 0, 0);
					break;
				case CMD_ALL_ON:
					currCmdNum = CMD_ALL_ON;
					setDutyCycles(&args, 100, 100, 100);
					break;
			}
		}
		else if (currCmdNum == CMD_BLINK)
		{
			// If there is no new command and we should be blinking
			if(blinkingLedsOn)
			{
				blinkingLedsOn = false;
				setDutyCycles(&args, 0, 0, 0);
			}
			else
			{
				blinkingLedsOn = true;
				setDutyCycles(&args, nextLedCmd.red, nextLedCmd.green, nextLedCmd.blue);
			}
		}
	}
}
