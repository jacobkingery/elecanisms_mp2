#include <p24FJ128GB206.h>
#include <stdint.h>
#include "config.h"
#include "common.h"
#include "usb.h"
#include "pin.h"
#include "timer.h"
#include "oc.h"
#include "md.h"

#define GET_CURRENT 1
#define GET_LAST    2

WORD CURRENT_CURRENT = (WORD) 0;
WORD LAST_CURRENT = (WORD) 0;

void VendorRequests(void) {
    switch (USB_setup.bRequest) {
        case GET_CURRENT:
            BD[EP0IN].address[0] = CURRENT_CURRENT.b[0];
            BD[EP0IN].address[1] = CURRENT_CURRENT.b[1];
            BD[EP0IN].bytecount = 2;    // set EP0 IN byte count to 4
            BD[EP0IN].status = 0xC8;    // send packet as DATA1, set UOWN bit
            break;
        case GET_LAST:
            BD[EP0IN].address[0] = LAST_CURRENT.b[0];
            BD[EP0IN].address[1] = LAST_CURRENT.b[1];
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
    init_timer();
    init_pin();
    init_oc();
    init_md();

    pin_analogIn(&A[0]);

    md_velocity(&md1, 0xF000, 1);

    InitUSB();                              // initialize the USB registers and serial interface engine
    while (USB_USWSTAT!=CONFIG_STATE) {     // while the peripheral is not configured...
        LAST_CURRENT = CURRENT_CURRENT;
        CURRENT_CURRENT.w = pin_read(&A[0]);
        ServiceUSB();                       // ...service USB requests
    }
    while (1) {
        LAST_CURRENT = CURRENT_CURRENT;
        CURRENT_CURRENT.w = pin_read(&A[0]);
        ServiceUSB();                       // service any pending USB requests
    }
}
