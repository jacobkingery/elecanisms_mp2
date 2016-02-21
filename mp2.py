import usb.core
import time
import csv
import os.path
import sys
import cv2
import matplotlib.pyplot as plt

class Joystick:
    def __init__(self, fname=None, prompt_overwrite=True):
        self.GET_CURRENT   = 1
        self.GET_ANGLE     = 2
        self.GET_VELOCITY  = 3
        self.GET_SPEED     = 4
        self.GET_DIRECTION = 5
        self.SET_PARAMETER = 6

        self.dev = usb.core.find(idVendor = 0x6666, idProduct = 0x0003)
        if self.dev is None:
            raise ValueError('no USB device found matching idVendor = 0x6666 and idProduct = 0x0003')
        self.dev.set_configuration()

        self.parameters = [
            ['K_spring', 2],
            ['K_damper', 2],
            ['K_texture', 2],
            ['K_wall', 2],
            ['Mode', 0]
        ]
        cv2.namedWindow('Set Parameters')
        for i,parameter in enumerate(self.parameters):
            cv2.createTrackbar(parameter[0], 'Set Parameters', parameter[1], 3, self.nothing)
            self.set_parameter(parameter[1], i)

        self.field_names = ['Time', 'Current', 'Angle', 'Velocity', 'Motor_velocity']

        self.colors = ['b', 'r', 'k', 'g']
        plt.ion()

        self.fname = fname
        if self.fname:
            if prompt_overwrite and os.path.isfile(self.fname):
                raw_input('{} already exists, press Ctrl-C now to quit or Enter to overwrite.')
            with open(self.fname, 'w') as f:
                csv.DictWriter(f, fieldnames=self.field_names).writeheader()

        self.inital_time = time.time()

    def close(self):
        self.dev = None

    def nothing(self, value):
        pass

    def toWord(self, byteArray):
        val = 0
        for i,byte in enumerate(byteArray):
            val += int(byte) * 2**(8*i)
        return val

    # from http://stackoverflow.com/questions/1604464/twos-complement-in-python
    def twos_comp(self, val, bits=16):
        """compute the 2's compliment of int value val"""
        if (val & (1 << (bits - 1))) != 0: # if sign bit is set e.g., 8bit: 128-255
            val = val - (1 << bits)        # compute negative value
        return val                         # return positive value as is

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

    def get_velocity(self):
        try:
            ret = self.dev.ctrl_transfer(0xC0, self.GET_VELOCITY, 0, 0, 2)
        except usb.core.USBError:
            print "Could not send GET_VELOCITY vendor request."
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
        cv2.waitKey(1)

    def set_parameter(self, value, index):
        try:
            word = self.toWord((value, index))
            self.dev.ctrl_transfer(0x40, self.SET_PARAMETER, word, 0)
        except usb.core.USBError:
            print "Could not send SET_VALS vendor request."

    def get_readings(self):
        now = time.time() - self.inital_time
        current = self.twos_comp(self.toWord(self.get_current()))
        angle = self.twos_comp(self.toWord(self.get_angle()))
        velocity = self.twos_comp(self.toWord(self.get_velocity()))
        speed = self.toWord(self.get_speed())
        direction = -(self.toWord(self.get_direction()) or -1)  # 0 --> 1, 1 --> -1
        md_velocity = speed * direction

        readings = [now, current, angle, velocity, md_velocity]
        return dict(zip(self.field_names, readings))

    def write_readings(self, readings):
        if self.fname:
            with open(self.fname, 'a') as f:
                csv.DictWriter(f, fieldnames=self.field_names).writerows(readings)

    def plot_readings(self, readings):
        for i,key in enumerate(self.field_names[1:]):
            plt.scatter(readings['Time'], readings[key] * 0.5 if key == 'Motor_velocity' else readings[key], color=self.colors[i])
        plt.legend(self.field_names[1:], loc='lower center')
        plt.xlabel('Time (s)')
        plt.ylabel('Value')
        plt.ylim((-60000, 35000))
        plt.pause(0.01)

try:
    fname = sys.argv[1]
except IndexError:
    fname = None

joy = Joystick(fname)

readings = []
i = 0
while True:
    i += 1
    joy.update_parameters()
    r = joy.get_readings()

    # Plotting the readings slows things down significantly,
    # but can be helpful to look at when not capturing data.
    # joy.plot_readings(r)

    # Only write every 50 readings so there isn't constant file I/O
    readings.append(r)
    if i > 50:
        joy.write_readings(readings)
        readings = []
        i = 0
