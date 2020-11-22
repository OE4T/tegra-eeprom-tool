# tegra-eeprom-tool

Library and tools for working with identification EEPROMs on NVIDIA
Jetson/Tegra hardware, as documented in
[Jetson TX1-TX2 Module EEPROM Layout](https://developer.nvidia.com/embedded/dlc/tx1-tx2-module-eeprom-layout).
Works with EEPROMs directly accessible through an EEPROM driver (for read/write access), or via
userspace I2C transactions (for reads only).

# libtegra-eeprom library

This shared library implements functions for determining the SoC type and I2C address of the
SoC's module (CVM) EEPROM (the `cvm` API), functions for reading/writing an ID EEPROM (the
`eeprom` API), and a function for parsing the part number in the CVM EEPROM that is used
as a board specification for bootloader upgrades (the `boardspec` API).

# tegra-eeprom-tool

This tool provides a CLI for getting (and setting) information in an identification EEPROM.
It can be used interactively, using **libedit** to provide command editing and history,
or in "one-shot" mode by specifying a single command on the comand line.

# tegra-boardspec

This tool displays the board specification that serves as the basis for determining compatibility
of individual components in a bootloader update payload.


**WARNING** This package provides both read **and write** access to the EEPROMs. Use with caution.

For writing to the EEPROMs, you must have CONFIG_AT24 in your kernel configuration
and an entry in the device tree for the eeprom.  Direct eeprom writes using the I2C
user-space driver have proven unreliable and are not supported.
