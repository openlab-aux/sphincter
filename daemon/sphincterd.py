#!/usr/bin/env python2
# -*- coding: utf-8 -*- 

import os
import serial

import time
import thread

import hashlib

from BaseHTTPServer import BaseHTTPRequestHandler
from urlparse import urlparse, parse_qs

from BaseHTTPServer import HTTPServer


class SerialHandler(object):

    def __init__(self, device, speed):
        self.__ser = serial.Serial(device, speed, timeout=1)
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
            print(self.__ser.readline().strip())
            time.sleep(0.04)

        while True:
            self.__ser.write("s")
            time.sleep(0.5)
            data = self.__ser.readline().strip()

            self.sphincter_locked = data == 'LOCKED'
            
            time.sleep(0.5)


class TokenFileHandler:

    def __init__(self, filename):
        lines = []
        self.__hashes = []

        try:
            f = open(filename)
            lines = f.readlines()
            f.close()
        except IOError:
            print 'token file not found'
            return

        for line in lines:
            self.__hashes.append( line.split(':')[1].rstrip() )

    def token_is_valid(self,token):
        return hashlib.sha256(token).hexdigest() in self.__hashes


class SphincterServer(HTTPServer):

    def __init__(self, *args, **kwargs):
        serial_handler = None
        try:
            self.serial_handler = kwargs['serial_handler']
            del(kwargs['serial_handler'])
        except KeyError:
            raise Exception("Need serial_handler argument")
        HTTPServer.__init__(self, *args, **kwargs)


class GETHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        query_fields = parse_qs( urlparse(self.path).query )

        param_token  = None
        param_action = None
        
        try:
            param_action = query_fields['action'][0]
            param_token  = query_fields['token'][0]
        except KeyError:
            pass

        t_handler = TokenFileHandler('table')

        message = 'failed'

        if( param_action == 'state' ):

            if( self.server.serial_handler.sphincter_locked ):
                message = 'locked'
            else:
                message = 'unlocked'

        elif( t_handler.token_is_valid(param_token) ):
            
            if( param_action == 'open' ):
                self.server.serial_handler.serial_send_unlock()
            elif( param_action == 'close' ):
                self.server.serial_handler.serial_send_lock()

            message = 'success'

        self.send_response(200)
        self.end_headers()
        self.wfile.write(message)

        return

    def log_message(self, format, *arg): # do nothing = turn off loggin
        return
        

if __name__ == '__main__':

    server = SphincterServer( ('localhost', 8080), GETHandler, serial_handler=SerialHandler('/dev/sphincter', 9600) )
    print 'Starting server, use <Ctrl-C> to stop'
    server.serve_forever()

