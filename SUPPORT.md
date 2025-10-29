## Hands-On RTOS with Microcontrollers, Second Edition

This repo, and how to install it, are described in the book, in chapter 3.<br>
Additional information is provided here on how to select the FreeRTOS version.

The repo has two branches, and each contains all of the book's example-programs.<br>
The branch **main** uses FreeRTOS version 10.2.1 (released in 2019).<br>
The branch **FreeRTOSv11-1-0** uses FreeRTOS version 11.1.0 (released in 2025).

We recommend readers use the branch *main*, for these reasons:
* *main* was used in developing the book.
* The example-programs' performance-data more closely matches what is shown in the book.
* For the FreeRTOS API-calls that are used, they work the same as in FreeRTOS version 11.1.0.

Also, the branch *main* has an example-program with FreeRTOS version 11.1.0. It is named *FreeRTOS_project*, and it's described in the book, in Appendix C. Readers who would like to experiment with a more recent FreeRTOS can use it.<br>
(The branch *FreeRTOSv11-1-0* also has the example-program *FreeRTOS_project*.)

To use the branch *main*, just follow the book's instructions for installing the repo, in chapter 3.  (The branch *main* is the repo's default branch.)

To use the branch *FreeRTOSv11-1-0*, follow the installation instructions in chapter 3.<br>
However, after cloning the repo, change the local-copy to use the branch *FreeRTOSv11-1-0*.<br>
The git command is:  `git checkout FreeRTOSv11-1-0`.