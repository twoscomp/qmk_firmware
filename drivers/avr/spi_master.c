/*  Copyright 2020
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <avr/io.h>

#include "spi_master.h"
#include "quantum.h"
#include "timer.h"

#if defined(__AVR_ATmega16U2__) || defined(__AVR_ATmega32U2__) || defined(__AVR_ATmega16U4__) || defined(__AVR_ATmega32U4__) || defined(__AVR_AT90USB646__) || defined(__AVR_AT90USB647__) || defined(__AVR_AT90USB1286__) || defined(__AVR_AT90USB1287__)
#    define SPI_SCK_PIN B1
#    define SPI_MOSI_PIN B2
#    define SPI_MISO_PIN B3
#elif defined(__AVR_ATmega32A__)
#    define SPI_SCK_PIN B7
#    define SPI_MOSI_PIN B5
#    define SPI_MISO_PIN B6
#elif defined(__AVR_ATmega328P__)
#    define SPI_SCK_PIN B5
#    define SPI_MOSI_PIN B3
#    define SPI_MISO_PIN B4
#endif

static pin_t   currentSlavePin    = NO_PIN;
static uint8_t currentSlaveConfig = 0;
static bool    currentSlave2X     = false;

void spi_init(void) {
    writePinHigh(SPI_SS_PIN);
    setPinOutput(SPI_SCK_PIN);
    setPinOutput(SPI_MOSI_PIN);
    setPinInput(SPI_MISO_PIN);

    SPCR = (_BV(SPE) | _BV(MSTR));
}

void spi_start(pin_t slavePin, bool lsbFirst, uint8_t mode, uint8_t divisor) {
    if (currentSlavePin == NO_PIN && slavePin != NO_PIN) {
        if (lsbFirst) {
            currentSlaveConfig |= _BV(DORD);
        }

        switch (mode) {
            case 1:
                currentSlaveConfig |= _BV(CPHA);
                break;
            case 2:
                currentSlaveConfig |= _BV(CPOL);
                break;
            case 3:
                currentSlaveConfig |= (_BV(CPOL) | _BV(CPHA));
                break;
        }

        uint8_t roundedDivisor = 1;
        while (roundedDivisor < divisor) {
            roundedDivisor <<= 1;
        }

        switch (roundedDivisor) {
            case 16:
                currentSlaveConfig |= _BV(SPR0);
                break;
            case 64:
                currentSlaveConfig |= _BV(SPR1);
                break;
            case 128:
                currentSlaveConfig |= (_BV(SPR1) | _BV(SPR0));
                break;
            case 2:
                currentSlave2X = true;
                break;
            case 8:
                currentSlave2X = true;
                currentSlaveConfig |= _BV(SPR0);
                break;
            case 32:
                currentSlave2X = true;
                currentSlaveConfig |= _BV(SPR1);
                break;
        }

        SPSR |= currentSlaveConfig;
        currentSlavePin = slavePin;
        setPinOutput(currentSlavePin);
        writePinLow(currentSlavePin);
    }
}

spi_status_t spi_write(uint8_t data, uint16_t timeout) {
    SPDR = data;

    uint16_t timeout_timer = timer_read();
    while (!(SPSR & _BV(SPIF))) {
        if ((timeout != SPI_TIMEOUT_INFINITE) && ((timer_read() - timeout_timer) >= timeout)) {
            return SPI_STATUS_TIMEOUT;
        }
    }

    return SPDR;
}

spi_status_t spi_read(uint16_t timeout) {
    SPDR = 0x00;  // Dummy

    uint16_t timeout_timer = timer_read();
    while (!(SPSR & _BV(SPIF))) {
        if ((timeout != SPI_TIMEOUT_INFINITE) && ((timer_read() - timeout_timer) >= timeout)) {
            return SPI_STATUS_TIMEOUT;
        }
    }

    return SPDR;
}

spi_status_t spi_transmit(const uint8_t *data, uint16_t length, uint16_t timeout) {
    spi_status_t status = SPI_STATUS_ERROR;

    for (uint16_t i = 0; i < length; i++) {
        status = spi_write(data[i], timeout);
    }

    return status;
}

spi_status_t spi_receive(uint8_t *data, uint16_t length, uint16_t timeout) {
    spi_status_t status = SPI_STATUS_ERROR;

    for (uint16_t i = 0; i < length; i++) {
        status = spi_read(timeout);

        if (status > 0) {
            data[i] = status;
        }
    }

    return (status < 0) ? status : SPI_STATUS_SUCCESS;
}

void spi_stop(void) {
    if (currentSlavePin != NO_PIN) {
        setPinOutput(currentSlavePin);
        writePinHigh(currentSlavePin);
        currentSlavePin = NO_PIN;
        SPCR &= ~(currentSlaveConfig);
        currentSlaveConfig = 0;
        SPSR               = 0;
        currentSlave2X     = false;
    }
}
