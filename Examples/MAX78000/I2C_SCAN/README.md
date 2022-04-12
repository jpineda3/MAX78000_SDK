## Description

This example uses the I2C Master to find the addresses of any I2C Slave devices connected to the same bus as I2C1.

*** NOTE ***: This example is only intended to be run on the MAX78000 Featherboard.

## Setup

-   Connect a USB cable between the PC and the CN1 (USB/PWR) connector.
-   Open an terminal application on the PC and connect to the EV kit's console UART at 115200, 8-N-1.
-   Connect I2C1 (SCL - P0.16, SDA - P0.17) to I2C bus.

## Expected Output

The Console UART of the device will output these messages:

```
******** I2C SLAVE ADDRESS SCANNER *********

This example finds the addresses of any I2C Slave devices connected to the
same bus as I2C1 (SCL - P0.16, SDA - P0.17).
-->I2C Master Initialization Complete
-->Scanning started
.................
Found slave ID 024; 0x18
................
Found slave ID 040; 0x28
..............
Found slave ID 054; 0x36
..........................
Found slave ID 080; 0x50
.......................................
-->Scan finished. 4 devices found
```
