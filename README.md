# tegra-eeprom-tool

Tool for working with identification EEPROMs on NVIDIA
Jetson/Tegra hardware, as documented in
[Jetson TX1-TX2 Module EEPROM Layout](https://developer.nvidia.com/embedded/dlc/tx1-tx2-module-eeprom-layout).
Works with EEPROMs directly accessible through an EEPROM driver, or via userspace I2C transactions.

**WARNING** This tool provides both read **and write** access to the EEPROMs. Use with caution.

For writing to the EEPROMs, you must have CONFIG_AT24 in your kernel configuration
and an entry in the device tree for the eeprom.  Direct eeprom writes using the I2C
user-space driver have proven unreliable and are not supported.
