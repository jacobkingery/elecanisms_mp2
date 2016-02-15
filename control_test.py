import usb.core
import cv2
import time
import matplotlib.pyplot as plt

class CurrentTest:

    def __init__(self):
        self.GET_CURRENT   = 1
        self.GET_ANGLE     = 2
        self.GET_SPEED     = 3
        self.GET_DIRECTION = 4
        self.SET_PARAMETER = 5
        self.dev = usb.core.find(idVendor = 0x6666, idProduct = 0x0003)
        if self.dev is None:
            raise ValueError('no USB device found matching idVendor = 0x6666 and idProduct = 0x0003')
        self.dev.set_configuration()

        self.parameters = [
            ['k_theta_tau', 1],
            ['k_tau_v', 1],
            ['k_i_tau', 1]
        ]
        cv2.namedWindow('Set Parameters')
        for parameter in self.parameters:
            cv2.createTrackbar(parameter[0], 'Set Parameters', parameter[1], 5, self.nothing)

    def close(self):
        self.dev = None

    def nothing(self, value):
        pass

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

    def update_parameters(self):
        for i,parameter in enumerate(self.parameters):
            value = cv2.getTrackbarPos(parameter[0], 'Set Parameters')
            if value != parameter[1]:
                self.parameters[i][1] = value
                self.set_parameter(value, i)

    def set_parameter(self, value, index):
        try:
            word = toWord((value, index))
            self.dev.ctrl_transfer(0x40, self.SET_PARAMETER, word, 0)
        except usb.core.USBError:
            print "Could not send SET_VALS vendor request."

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
plt.ylim((-1.1, 1.1))
while True:
    ct.update_parameters()

    current = twos_comp(toWord(ct.get_current()), 16) / float(0xFFFF)
    angle = twos_comp(toWord(ct.get_angle()), 16) / float(0xFFFF)
    speed = toWord(ct.get_speed()) / float(0xFFFF)
    direction = toWord(ct.get_direction()) or -1
    now = time.time()
    plt.scatter(now, current, color='b')
    plt.scatter(now, angle, color='r')
    plt.scatter(now, speed * direction, color='g')
    plt.draw()
    plt.pause(0.01)

    cv2.waitKey(1)
