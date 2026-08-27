#ifndef HUSB238A_H
#define HUSB238A_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/*
 * HUSB238A USB Power Delivery sink controller.
 *
 * The BC01 feeds its hashboard through this part: it negotiates a supply
 * voltage with the USB-C adapter and then gates VBUS through to the board.
 * Until that gate opens, the TPS546 core regulator and the EMC2302 fan
 * controller have no power and do not answer on I2C at all, so the ASIC
 * cannot be brought up.
 *
 * The BC04 source this tree came from has no USB-PD support of any kind,
 * because the BC04 is not powered this way. The register contract below was
 * recovered from the shipping BC01 firmware; see docs/BC01-USB-PD.md for
 * the evidence behind each value and for which parts are read directly
 * versus inferred.
 */

/* Observed answering on the BC01's only I2C bus, alongside the TMP75. */
#define HUSB238A_I2CADDR            0x42

/* VBUS gate control. Bit 0x20 is an active-low disable: clearing it
 * switches VBUS through, setting it cuts it. */
#define HUSB238A_REG_GATE           0x0E
#define HUSB238A_GATE_DISABLE_BIT   0x20

#define HUSB238A_REG_CONNECT_STATUS 0x63
#define HUSB238A_REG_NEGOTIATED_V   0x67

/* Adapter capability PDOs, read in sequence by the stock capability scan. */
#define HUSB238A_REG_PDO_FIRST      0x66
#define HUSB238A_REG_PDO_LAST       0x6D

/* Register the device on the shared I2C bus. */
esp_err_t husb238a_init(void);

/* True once the device has been registered and answers. */
bool husb238a_present(void);

esp_err_t husb238a_read(uint8_t reg, uint8_t *value);
esp_err_t husb238a_write(uint8_t reg, uint8_t value);

/* Log the status and capability registers. Reads only; changes nothing. */
void husb238a_dump(void);

/* Switch VBUS through to the hashboard by clearing the disable bit.
 *
 * This does not change any voltage. It passes through whatever the adapter
 * has already agreed to, so it is safe to call before negotiation is
 * implemented -- the TPS546 will simply stay off until its input reaches
 * VIN_ON, which is 11 V. */
esp_err_t husb238a_gate_open(void);

esp_err_t husb238a_gate_close(void);

#endif /* HUSB238A_H */
