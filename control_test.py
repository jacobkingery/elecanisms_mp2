import usb.core
import time
import matplotlib.pyplot as plt

class CurrentTest:

    def __init__(self):
        self.GET_CURRENT   = 1
        self.GET_ANGLE     = 2
        self.GET_SPEED     = 3
        self.GET_DIRECTION = 4
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

    def get_angle(self):
        try:
            ret = self.dev.ctrl_transfer(0xC0, self.GET_ANGLE, 0, 0, 2)
        except usb.core.USBError:
            print "Could not send GET_ANGLE vendor request."
        else:
            return ret

    def get_speed(self):
        try:
            ret = self.dev.ctrl_transfer(0xC0, self.GET_SPEED, 0, 0, 2)
        except usb.core.USBError:
            print "Could not send GET_SPEED vendor request."
        else:
            return ret

    def get_direction(self):
        try:
            ret = self.dev.ctrl_transfer(0xC0, self.GET_DIRECTION, 0, 0, 1)
        except usb.core.USBError:
            print "Could not send GET_DIRECTION vendor request."
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
    return toVoltage(val) / 0.75

def toAngle(val):
    return float(twos_comp(val, 16)) / 0x3FFF * 360

ct = CurrentTest()
plt.ion()
plt.figure()
# plt.ylim((-3.3, 3.3))
while True:
    # try:
    current = twos_comp(toWord(ct.get_current()), 16) / float(0xFFFF)
    angle = twos_comp(toWord(ct.get_angle()), 16) / float(0xFFFF)
    speed = toWord(ct.get_speed()) / float(0xFFFF)
    direction = toWord(ct.get_direction()) / float(0xFFFF)
    now = time.time()
    plt.scatter(now, current, color='b')
    plt.scatter(now, angle, color='r')
    plt.scatter(now, speed, color='g')
    plt.scatter(now, direction*50000, color='k')
    plt.draw()
    plt.pause(0.01)
    # except:
        # print 'error'
        # pass
