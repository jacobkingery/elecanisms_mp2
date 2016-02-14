#include <p24FJ128GB206.h>
#include <stdint.h>
#include "config.h"
#include "common.h"
#include "ui.h"
#include "pin.h"
#include "spi.h"
#include "timer.h"
#include "oc.h"
#include "md.h"

#define REG_ANG_ADDR    0x3FFF
#define ENC_MASK        0x3FFF

_PIN *ENC_SCK, *ENC_MISO, *ENC_MOSI;
_PIN *ENC_NCS;

WORD OFFSET = (WORD) 0;
WORD CURRENT_ANGLE = (WORD) 0;
WORD LAST_ANGLE = (WORD) 0;
WORD UNWRAPPED_ANGLE = (WORD) 0;
int8_t WRAPS = 0;

WORD enc_readReg(WORD address) {
    WORD cmd, result;
    cmd.w = 0x4000|address.w; //set 2nd MSB to 1 for a read
    cmd.w |= parity(cmd.w)<<15; //calculate even parity for

    // Tell the sensor which register we want to read
    pin_clear(ENC_NCS);
    spi_transfer(&spi1, cmd.b[1]);
    spi_transfer(&spi1, cmd.b[0]);
    pin_set(ENC_NCS);

    // Get the reading from the sensor
    pin_clear(ENC_NCS);
    result.b[1] = spi_transfer(&spi1, 0);
    result.b[0] = spi_transfer(&spi1, 0);
    pin_set(ENC_NCS);
    return result;
}

void check_wraps() {
    // If the angle jumps from <~5 degrees to >~355 degrees, or vice-versa, we wrapped
    if ((CURRENT_ANGLE.w < 0x00E4) && (LAST_ANGLE.w > 0x3F1B)) {
        WRAPS += 1;
    } else if ((CURRENT_ANGLE.w > 0x3F1B) && (LAST_ANGLE.w < 0x00E4)) {
        WRAPS -= 1;
    }
}

WORD get_raw_angle() {
    WORD result = enc_readReg((WORD) REG_ANG_ADDR);
    // If the parity of the result is wrong, don't use the result
    if (parity(result.w)) {
        CURRENT_ANGLE = LAST_ANGLE;
    }
    // Otherwise, subtract the OFFSET and set CURRENT_ANGLE
    CURRENT_ANGLE.w = ((result.w & ENC_MASK) - (OFFSET.w & ENC_MASK)) & ENC_MASK;
}

WORD get_angle() {
    // Apply WRAPS to CURRENT_ANGLE so that we can actually know where we are
    LAST_ANGLE = CURRENT_ANGLE;
    get_raw_angle();
    check_wraps();
    UNWRAPPED_ANGLE.w = CURRENT_ANGLE.w + ENC_MASK * WRAPS;
}

int16_t main(void) {
    init_clock();
    init_timer();
    init_ui();
    init_pin();
    init_spi();
    init_oc();
    init_md();

    /* SPI setup */
    ENC_MISO = &D[1];
    ENC_MOSI = &D[0];
    ENC_SCK = &D[2];
    ENC_NCS = &D[3];
    pin_digitalOut(ENC_NCS);
    pin_set(ENC_NCS);

    // Open SPI in mode 1
    spi_open(&spi1, ENC_MISO, ENC_MOSI, ENC_SCK, 2e6, 1);

    /* Motor setup */
    md_velocity(&md1, 0, 0);

    /* Get initial angle offset */
    get_raw_angle();
    OFFSET = CURRENT_ANGLE;

    while (1) {
        get_angle();
        if (UNWRAPPED_ANGLE.i < -0x1FFF) {
            md_velocity(&md1, 0xF000, 1);
            led_on(&led2);
        } else if (UNWRAPPED_ANGLE.i > 0x1FFF) {
            md_velocity(&md1, 0xF000, 0);
            led_on(&led1);
        } else {
            md_velocity(&md1, 0, 0);
            led_off(&led1);
            led_off(&led2);
        }
    }
}
