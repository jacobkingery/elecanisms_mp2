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

#define REG_ANG_ADDR    0x3FFF
#define ENC_MASK        0x3FFF
#define CUR_OFFSET      0x7FFF
#define READ_FREQ       1024
#define CTRL_FREQ       100

// USB communication encoding
#define GET_CURRENT     1
#define GET_ANGLE       2
#define GET_VELOCITY    3
#define GET_SPEED       4
#define GET_DIRECTION   5
#define SET_PARAMETER   6

// Control scheme encoding
#define SPRING      0
#define DAMPER      1
#define TEXTURE     2
#define WALL        3

// SPI pins
_PIN *ENC_SCK, *ENC_MISO, *ENC_MOSI;
_PIN *ENC_NCS;

// Readings
WORD ANG_OFFSET = (WORD) 0;
WORD ANGLE = (WORD) 0;
WORD LAST_ANGLE = (WORD) 0;
WORD UNWRAPPED_ANGLE = (WORD) 0;
int8_t WRAPS = 0;
WORD CURRENT = (WORD) 0;
WORD VELOCITY = (WORD) 0;
WORD MD_SPEED = (WORD) 0;
uint8_t MD_DIRECTION = 0;

// Control parameters
uint8_t PARAMETERS[] = {
    2,  // K_spring
    2,  // K_damper
    2,  // K_texture
    2,  // K_wall
    0   // Mode
};

// Texture parameters
uint16_t TEX_SPEED = 0x1000;
uint16_t TEX_TOLERANCE = 0x0400;
uint16_t TEX_BUMPS[] = {
    0xD000,
    0xE100,
    0x0000,
    0x1800,
    0x3000
};
uint8_t TEX_NUM_BUMPS = sizeof(TEX_BUMPS)/sizeof(TEX_BUMPS[0]);

// Wall parameters
uint16_t WALL_SPEED = 0x4000;
int16_t WALL_LOCATION = 0x2000;

WORD enc_readReg(WORD address) {
    /*
    Given an address, return the value from the encoder's register at that address
    */
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

void get_readings() {
    /*
    Get readings for current, raw angle, wraps, unwrapped angle, and velocity
    */
    // Read current pin and zero-center
    CURRENT.w = pin_read(&A[0]) - CUR_OFFSET;

    // Read the encoder, check parity, and subtract initial offset
    LAST_ANGLE = ANGLE;
    WORD result = enc_readReg((WORD) REG_ANG_ADDR);
    if (!parity(result.w)) {
        ANGLE.w = ((result.w & ENC_MASK) - ANG_OFFSET.w) & ENC_MASK;
    }

    // Check for wrapping; if the angle jumps from <~20 degrees
    // to >~340 degrees, or vice-versa, we wrapped
    if ((ANGLE.w < 0x038E) && (LAST_ANGLE.w > 0x3C71)) {
        WRAPS += 1;
    } else if ((ANGLE.w > 0x3C71) && (LAST_ANGLE.w < 0x038E)) {
        WRAPS -= 1;
    }

    // Apply wrapping to the raw angle
    uint16_t last = UNWRAPPED_ANGLE.w;
    UNWRAPPED_ANGLE.w = ANGLE.w + ENC_MASK * WRAPS;

    // Calculate velocity as (change in angle) / (time between readings)
    // Divide by 16 to avoid overflow
    VELOCITY.w = (UNWRAPPED_ANGLE.w - last) * (READ_FREQ / 16);
}

void use_spring() {
    /*
    Calculate the motor commands for the spring controller
    */
    if (UNWRAPPED_ANGLE.i > 0) {
        MD_DIRECTION = 1;
        MD_SPEED.w = UNWRAPPED_ANGLE.w;
    } else {
        MD_DIRECTION = 0;
        MD_SPEED.w = -UNWRAPPED_ANGLE.w;
    }
}

void use_damper() {
    /*
    Calculate the motor commands for the damper controller
    */
    if (VELOCITY.i > 0) {
        MD_DIRECTION = 1;
        MD_SPEED.w = VELOCITY.w;
    } else {
        MD_DIRECTION = 0;
        MD_SPEED.w = -VELOCITY.w;
    }
}

void use_texture() {
    /*
    Calculate the motor commands for the texture controller
    */
    uint8_t i;
    MD_SPEED.w = 0;
    MD_DIRECTION = 0;
    for (i = 0; i < TEX_NUM_BUMPS; ++i) {
        if (abs(TEX_BUMPS[i] - UNWRAPPED_ANGLE.w) < TEX_TOLERANCE) {
            MD_SPEED.w = TEX_SPEED;
            break;
        }
    }
}

void use_wall() {
    /*
    Calculate the motor commands for the wall controller
    */
    MD_SPEED.w = 0;
    MD_DIRECTION = 1;
    if (UNWRAPPED_ANGLE.i > WALL_LOCATION) {
        MD_SPEED.w = WALL_SPEED;
    }
}

void set_velocity() {
    /*
    Set the velocity of the motor using one of the control schemes
    */
    uint8_t mode = PARAMETERS[4];
    switch (mode) {
        case SPRING:
            use_spring();
            break;
        case DAMPER:
            use_damper();
            break;
        case TEXTURE:
            use_texture();
            break;
        case WALL:
            use_wall();
            break;
        default:
            MD_SPEED.w = 0;
            md_velocity(&md1, MD_SPEED.w, MD_DIRECTION);
            return;
    }

    // Multiply by appropriate K value and compensate for the dead zone
    MD_SPEED.w = MD_SPEED.w * PARAMETERS[mode] + 0x1000;

    // Command motor
    md_velocity(&md1, MD_SPEED.w, MD_DIRECTION);
}

void VendorRequests(void) {
    /*
    Handle USB vendor requests
    */
    switch (USB_setup.bRequest) {
        case GET_CURRENT:
            BD[EP0IN].address[0] = CURRENT.b[0];
            BD[EP0IN].address[1] = CURRENT.b[1];
            BD[EP0IN].bytecount = 2;
            BD[EP0IN].status = 0xC8;
            break;
        case GET_ANGLE:
            BD[EP0IN].address[0] = UNWRAPPED_ANGLE.b[0];
            BD[EP0IN].address[1] = UNWRAPPED_ANGLE.b[1];
            BD[EP0IN].bytecount = 2;
            BD[EP0IN].status = 0xC8;
            break;
        case GET_VELOCITY:
            BD[EP0IN].address[0] = VELOCITY.b[0];
            BD[EP0IN].address[1] = VELOCITY.b[1];
            BD[EP0IN].bytecount = 2;
            BD[EP0IN].status = 0xC8;
            break;
        case GET_SPEED:
            BD[EP0IN].address[0] = MD_SPEED.b[0];
            BD[EP0IN].address[1] = MD_SPEED.b[1];
            BD[EP0IN].bytecount = 2;
            BD[EP0IN].status = 0xC8;
            break;
        case GET_DIRECTION:
            BD[EP0IN].address[0] = MD_DIRECTION;
            BD[EP0IN].bytecount = 1;
            BD[EP0IN].status = 0xC8;
            break;
        case SET_PARAMETER:
            ;                       // This is silly, but you can't initialize
                                    // a variable right after a case statement
            uint8_t param_value = USB_setup.wValue.b[0];
            uint8_t param_index = USB_setup.wValue.b[1];
            PARAMETERS[param_index] = param_value;
            BD[EP0IN].bytecount = 0;
            BD[EP0IN].status = 0xC8;
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

    // Current measurement pin
    pin_analogIn(&A[0]);

    // SPI pin setup
    ENC_MISO = &D[1];
    ENC_MOSI = &D[0];
    ENC_SCK = &D[2];
    ENC_NCS = &D[3];
    pin_digitalOut(ENC_NCS);
    pin_set(ENC_NCS);

    // Open SPI in mode 1
    spi_open(&spi1, ENC_MISO, ENC_MOSI, ENC_SCK, 2e6, 1);

    // Motor setup
    md_velocity(&md1, 0, 0);

    // Get initial angle offset
    uint8_t unset = 1;
    while (unset) {
        ANG_OFFSET = enc_readReg((WORD) REG_ANG_ADDR);
        unset = parity(ANG_OFFSET.w);
    }
    ANG_OFFSET.w &= ENC_MASK;

    // USB setup
    InitUSB();
    while (USB_USWSTAT!=CONFIG_STATE) {
        ServiceUSB();
    }

    // Timers
    timer_setFreq(&timer2, READ_FREQ);
    timer_setFreq(&timer3, CTRL_FREQ);
    timer_start(&timer2);
    timer_start(&timer3);

    // Main loop
    while (1) {
        ServiceUSB();
        if (timer_flag(&timer2)) {
            timer_lower(&timer2);
            get_readings();
        }
        if (timer_flag(&timer3)) {
            timer_lower(&timer3);
            set_velocity();
        }
    }
}
