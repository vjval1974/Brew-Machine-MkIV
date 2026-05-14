'use strict';

// PCF8574 IO-expander 7-bit I2C addresses from the original I2C-IO.h.
// 8-bit addresses in the C code (0x70, 0x72, ...) become 7-bit on Linux
// (0x38, 0x39, ...).

module.exports.PCF_PORTS = {
  U: 0x38, // orig I2C_SLAVE_ADDRESS0 / PORTU
  V: 0x39, // orig I2C_SLAVE_ADDRESS1 / PORTV
  W: 0x3a, // orig I2C_SLAVE_ADDRESS2 / PORTW
  X: 0x3b, // orig I2C_SLAVE_ADDRESS3 / PORTX
  Y: 0x3c, // orig I2C_SLAVE_ADDRESS4 / PORTY
};
