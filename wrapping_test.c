#include <p24FJ128GB206.h>
#include <stdint.h>
#include "config.h"
#include "common.h"
#include "ui.h"
#include "usb.h"
#include "pin.h"
#include "spi.h"

#define ENC_READ_REG    1
#define GET_OFFSET      2
#define GET_ANGLE       3

#define REG_ANG_ADDR    0x3FFF
#define ENC_MASK        0x3FFF

_PIN *ENC_SCK, *ENC_MISO, *ENC_MOSI;
_PIN *ENC_NCS;

WORD OFFSET = (WORD) 0;
WORD CURRENT_ANGLE = (WORD) 0;
WORD LAST_ANGLE = (WORD) 0;
int8_t WRAPS = 0;

void check_wraps() {
    if ((CURRENT_ANGLE.w < 0x00E4) && (LAST_ANGLE.w > 0x3F1B)) {
        WRAPS += 1;
    } else if ((CURRENT_ANGLE.w > 0x3F1B) && (LAST_ANGLE.w < 0x00E4)) {
        WRAPS -= 1;
    }
}

WORD enc_readReg(WORD address) {
    WORD cmd, result;
    cmd.w = 0x4000|address.w; //set 2nd MSB to 1 for a read
    cmd.w |= parity(cmd.w)<<15; //calculate even parity for

    pin_clear(ENC_NCS);
    spi_transfer(&spi1, cmd.b[1]);
    spi_transfer(&spi1, cmd.b[0]);
    pin_set(ENC_NCS);

    pin_clear(ENC_NCS);
    result.b[1] = spi_transfer(&spi1, 0);
    result.b[0] = spi_transfer(&spi1, 0);
    pin_set(ENC_NCS);
    return result;
}

WORD enc_getAngle() {
    WORD result = enc_readReg((WORD) REG_ANG_ADDR);
    if (parity(result.w)) {
        return  LAST_ANGLE;
    } else {
        result.i = (int16_t) ((result.w & ENC_MASK) - (OFFSET.w & ENC_MASK));
        return result;
    }
}

void VendorRequests(void) {
    WORD32 address;
    WORD result;

    switch (USB_setup.bRequest) {
        case ENC_READ_REG:
            result = enc_readReg(USB_setup.wValue);
            BD[EP0IN].address[0] = result.b[0];
            BD[EP0IN].address[1] = result.b[1];
            BD[EP0IN].bytecount = 2;     // set EP0 IN byte count to 1
            BD[EP0IN].status = 0xC8;     // send packet as DATA1, set UOWN bit
            break;
        case GET_OFFSET:
            BD[EP0IN].address[0] = OFFSET.b[0];
            BD[EP0IN].address[1] = OFFSET.b[1];
            BD[EP0IN].bytecount = 2;    // set EP0 IN byte count to 4
            BD[EP0IN].status = 0xC8;    // send packet as DATA1, set UOWN bit
            break;
        case GET_ANGLE:
            BD[EP0IN].address[0] = CURRENT_ANGLE.b[0];
            BD[EP0IN].address[1] = CURRENT_ANGLE.b[1];
            BD[EP0IN].bytecount = 2;    // set EP0 IN byte count to 4
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
    init_ui();
    init_pin();
    init_spi();

    ENC_MISO = &D[1];
    ENC_MOSI = &D[0];
    ENC_SCK = &D[2];
    ENC_NCS = &D[3];

    pin_digitalOut(ENC_NCS);
    pin_set(ENC_NCS);

    // Open SPI in mode 1
    spi_open(&spi1, ENC_MISO, ENC_MOSI, ENC_SCK, 2e6, 1);

    // Get initial angle offset
    OFFSET = CURRENT_ANGLE = enc_getAngle(0);

    InitUSB();                              // initialize the USB registers and serial interface engine
    while (USB_USWSTAT!=CONFIG_STATE) {     // while the peripheral is not configured...
        LAST_ANGLE = CURRENT_ANGLE;
        CURRENT_ANGLE = enc_getAngle(0);
        check_wraps();
        ServiceUSB();                       // ...service USB requests
    }
    while (1) {
        LAST_ANGLE = CURRENT_ANGLE;
        CURRENT_ANGLE = enc_getAngle(0);
        check_wraps();
        ServiceUSB();                       // service any pending USB requests
    }
}
