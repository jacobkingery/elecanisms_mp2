import usb.core
import time

class encodertest:

    def __init__(self):
        self.ENC_READ_REG = 1
        self.GET_OFFSET = 2
        self.GET_RAW_ANGLE = 3
        self.GET_ANGLE = 4
        self.dev = usb.core.find(idVendor = 0x6666, idProduct = 0x0003)
        if self.dev is None:
            raise ValueError('no USB device found matching idVendor = 0x6666 and idProduct = 0x0003')
        self.dev.set_configuration()

        # AS5048A Register Map
        self.ENC_NOP = 0x0000
        self.ENC_CLEAR_ERROR_FLAG = 0x0001
        self.ENC_PROGRAMMING_CTRL = 0x0003
        self.ENC_OTP_ZERO_POS_HI = 0x0016
        self.ENC_OTP_ZERO_POS_LO = 0x0017
        self.ENC_DIAG_AND_AUTO_GAIN_CTRL = 0x3FFD
        self.ENC_MAGNITUDE = 0x3FFE
        self.ENC_ANGLE_AFTER_ZERO_POS_ADDER = 0x3FFF

    def close(self):
        self.dev = None

    def enc_readReg(self, address):
        try:
            ret = self.dev.ctrl_transfer(0xC0, self.ENC_READ_REG, address, 0, 2)
        except usb.core.USBError:
            print "Could not send ENC_READ_REG vendor request."
        else:
            return ret

    def get_offset(self):
        try:
            ret = self.dev.ctrl_transfer(0xC0, self.GET_OFFSET, 0, 0, 2)
        except usb.core.USBError:
            print "Could not send GET_OFFSET vendor request."
        else:
            return ret

    def get_raw_angle(self):
        try:
            ret = self.dev.ctrl_transfer(0xC0, self.GET_RAW_ANGLE, 0, 0, 2)
        except usb.core.USBError:
            print "Could not send GET_RAW_ANGLE vendor request."
        else:
            return ret
    
    def get_angle(self):
        try:
            ret = self.dev.ctrl_transfer(0xC0, self.GET_ANGLE, 0, 0, 2)
        except usb.core.USBError:
            print "Could not send GET_ANGLE vendor request."
        else:
            return ret


def toWord(byteArray):
    val = 0
    for i,byte in enumerate(byteArray):
        val += int(byte) * 2**(8*i)
    return val

# from http://stackoverflow.com/questions/1604464/twos-complement-in-python
def twos_comp(val, bits):
    """compute the 2's compliment of int value val"""
    if (val & (1 << (bits - 1))) != 0: # if sign bit is set e.g., 8bit: 128-255
        val = val - (1 << bits)        # compute negative value
    return val                         # return positive value as is


myEncoderTest = encodertest()
mask = 0x3FFF  # Sets first two bits (parity and error flag) to 0
while True:
    offsetBytes = myEncoderTest.get_offset()
    rawBytes = myEncoderTest.get_raw_angle()
    angBytes = myEncoderTest.get_angle()
    print float(toWord(offsetBytes)) / mask * 360,
    print float(toWord(rawBytes)) / mask * 360,
    print float(twos_comp(toWord(angBytes), 16)) / mask * 360
