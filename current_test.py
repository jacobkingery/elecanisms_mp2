import usb.core
import time

class CurrentTest:

    def __init__(self):
        self.GET_CURRENT = 1
        self.GET_LAST = 2
        self.dev = usb.core.find(idVendor = 0x6666, idProduct = 0x0003)
        if self.dev is None:
            raise ValueError('no USB device found matching idVendor = 0x6666 and idProduct = 0x0003')
        self.dev.set_configuration()

    def close(self):
        self.dev = None

    def get_current(self):
        try:
            ret = self.dev.ctrl_transfer(0xC0, self.GET_CURRENT, 0, 0, 2)
        except usb.core.USBError:
            print "Could not send GET_CURRENT vendor request."
        else:
            return ret

    def get_last(self):
        try:
            ret = self.dev.ctrl_transfer(0xC0, self.GET_LAST, 0, 0, 2)
        except usb.core.USBError:
            print "Could not send GET_LAST vendor request."
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

def toVoltage(val):
    return val * 3.3 / 0xFFFF

def toCurrent(val):
    return (toVoltage(val) - 1.65) / 0.75

ct = CurrentTest()
while True:
    current = toWord(ct.get_current())
    last = toWord(ct.get_last())
    print '{:016b}'.format(last), '{:016b}'.format(current)
    print '{:f}'.format(toCurrent(last)), '{:f}'.format(toCurrent(current))