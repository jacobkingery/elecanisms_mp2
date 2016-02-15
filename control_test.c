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
#include "usb.h"

#define GET_CURRENT     1
#define GET_ANGLE       2
#define GET_SPEED       3
#define GET_DIRECTION   4

#define REG_ANG_ADDR    0x3FFF
#define ENC_MASK        0x3FFF
#define CUR_OFFSET      0x01FF

_PIN *ENC_SCK, *ENC_MISO, *ENC_MOSI;
_PIN *ENC_NCS;

WORD OFFSET = (WORD) 0;
WORD CURRENT_ANGLE = (WORD) 0;
WORD LAST_ANGLE = (WORD) 0;
WORD UNWRAPPED_ANGLE = (WORD) 0;
int8_t WRAPS = 0;

WORD CURRENT_CURRENT = (WORD) 0;
WORD LAST_CURRENT = (WORD) 0;

WORD CURRENT_SPEED = (WORD) 0;
WORD CURRENT_DIRECTION = (WORD) 0;

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

void get_raw_angle() {
    WORD result = enc_readReg((WORD) REG_ANG_ADDR);
    // If the parity of the result is wrong, don't use the result
    if (parity(result.w)) {
        CURRENT_ANGLE = LAST_ANGLE;
    }
    // Otherwise, subtract the OFFSET and set CURRENT_ANGLE
    CURRENT_ANGLE.w = ((result.w & ENC_MASK) - (OFFSET.w & ENC_MASK)) & ENC_MASK;
}

void get_angle() {
    // Apply WRAPS to CURRENT_ANGLE so that we can actually know where we are
    LAST_ANGLE = CURRENT_ANGLE;
    get_raw_angle();
    check_wraps();
    UNWRAPPED_ANGLE.w = CURRENT_ANGLE.w + ENC_MASK * WRAPS;
}

void get_current() {
    LAST_CURRENT = CURRENT_CURRENT;
    CURRENT_CURRENT.w = (pin_read(&A[0])>>6) - CUR_OFFSET;
}

void set_velocity() {
    if (UNWRAPPED_ANGLE.i > CURRENT_CURRENT.i) {
        CURRENT_SPEED.w = UNWRAPPED_ANGLE.w - CURRENT_CURRENT.w;
        CURRENT_DIRECTION.b[0] = 0;
    } else {
        CURRENT_SPEED.w = CURRENT_CURRENT.w - UNWRAPPED_ANGLE.w;
        CURRENT_DIRECTION.b[0] = 1;
    }
    CURRENT_SPEED.w += 0x1000;
    CURRENT_SPEED.w *= 2;
    md_velocity(&md1, CURRENT_SPEED.w, CURRENT_DIRECTION.b[0]);
}

void VendorRequests(void) {
    switch (USB_setup.bRequest) {
        case GET_CURRENT:
            BD[EP0IN].address[0] = CURRENT_CURRENT.b[0];
            BD[EP0IN].address[1] = CURRENT_CURRENT.b[1];
            BD[EP0IN].bytecount = 2;    // set EP0 IN byte count to 4
            BD[EP0IN].status = 0xC8;    // send packet as DATA1, set UOWN bit
            break;
        case GET_ANGLE:
            BD[EP0IN].address[0] = UNWRAPPED_ANGLE.b[0];
            BD[EP0IN].address[1] = UNWRAPPED_ANGLE.b[1];
            BD[EP0IN].bytecount = 2;    // set EP0 IN byte count to 4
            BD[EP0IN].status = 0xC8;    // send packet as DATA1, set UOWN bit
            break;
        case GET_SPEED:
            BD[EP0IN].address[0] = CURRENT_SPEED.b[0];
            BD[EP0IN].address[1] = CURRENT_SPEED.b[1];
            BD[EP0IN].bytecount = 2;    // set EP0 IN byte count to 4
            BD[EP0IN].status = 0xC8;    // send packet as DATA1, set UOWN bit
            break;
        case GET_DIRECTION:
            BD[EP0IN].address[0] = CURRENT_DIRECTION.b[0];
            BD[EP0IN].bytecount = 1;    // set EP0 IN byte count to 4
            BD[EP0IN].status = 0xC8;    // send packet as DATA1, set UOWN bit
            break;
        default:
            USB_error_flags |= 0x01;    // set Request Error Flag
    }
}

void VendorRequestsIn(void) {
    switch (USB_request.setup.bRequest) {
        default:
            USB_error_flags |= 0x01;    // set Request Error Flag
    }
}

void VendorRequestsOut(void) {
}

int16_t main(void) {
    init_clock();
    init_timer();
    init_ui();
    init_pin();
    init_spi();
    init_oc();
    init_md();

    /* Current measurement pin */
    pin_analogIn(&A[0]);
    get_current();

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

    InitUSB();
    while (USB_USWSTAT!=CONFIG_STATE) {
        get_angle();
        get_current();
        ServiceUSB();
    }
    while (1) {
        get_angle();
        get_current();
        set_velocity();
        ServiceUSB();
    }
}
