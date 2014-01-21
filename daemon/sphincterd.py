#!/usr/bin/env python
# -*- coding: utf-8 -*- 

import os
import serial
import time
import threading
import SocketServer
from collections import deque
import thread

class SerialHandler(object):

    def __init__(self, device_name="/dev/tty.usbmodemfd121"):
        self.__ser = serial.Serial(device_name, 9600, timeout=1)
        self.sphincter_locked = True

        thread.start_new_thread(self.serial_state_read_thread, ())

    def serial_send_lock(self):
        self.__ser.write('c')

    def serial_send_unlock(self):
        self.__ser.write('o')
    
    def serial_state_read_thread(self):
        # wait for the arduino to reset
        time.sleep(1.5)
         
        # read from the serial until empty
        while self.__ser.inWaiting():
            print(self.__ser.readline().replace('\n', ''))
            time.sleep(0.04)

        while True:
            self.__ser.write("s")
            time.sleep(0.5)
            data = self.__ser.readline().replace('\n', '').lower()
            
            if data == "locked":
                self.sphincter_locked = True
            else:
                self.sphincter_locked = False

            time.sleep(0.5)

class ThreadedEchoRequestHandler(SocketServer.BaseRequestHandler):

    serial = SerialHandler()

    def __init__(self, request, client_address, server):
        self._command_dict = dict(
            unlock=self.__response_unlock,
            lock  =self.__response_lock,
            status=self.__response_status
        )
        
        SocketServer.BaseRequestHandler.__init__(self, request, client_address, server)
        #super(ThreadedEchoRequestHandler, self).__init__(request, client_address, server)

    def handle(self):
        # read data from the client
        data = self.request.recv(1024).lower()

        # get function accordingly to the data
        f = self._command_dict.get(data, lambda data, r: self.request.send("Unknown command: %s" % data))
        f(data, self.request)

    def __response_unlock(self, data, request):
        print ("Unlock door")
        try:
            self.serial.serial_send_unlock()
            request.send("Door unlocked")
        except Exception as e:
            pass

    def __response_lock(self, data, request):
        print ("Lock door")
        try:
            self.serial.serial_send_lock()
            request.send("Door locked")
        except Exception as e:
            pass

    def __response_status(self, data, request):
        print ("Door state")
        try:
            request.send("locked?: %s " % str(self.serial.sphincter_locked))
        except Exception as e:
            pass

class ThreadedEchoServer(SocketServer.ThreadingMixIn, SocketServer.UnixStreamServer):
    pass

if __name__ == "__main__":
    import socket
    import threading
    import signal
    signal.signal(signal.SIGINT, signal.SIG_DFL)  # react to ctrl-c


    address = './sphincter_socket'
    
    # Make sure the socket does not already exist
    try:
        os.unlink(address)
    except OSError:
        if os.path.exists(server_address):
            raise

    server = ThreadedEchoServer(address, ThreadedEchoRequestHandler)

    t = threading.Thread(target=server.serve_forever)
    #t.setDaemon(True) # don't hang on exit
    t.start()
    print 'Server loop running in thread:', t.getName()
    
