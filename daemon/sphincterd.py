#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os
import serial
from serial import SerialException

import time
import thread

import hashlib

from BaseHTTPServer import BaseHTTPRequestHandler
from urlparse import urlparse, parse_qs

from BaseHTTPServer import HTTPServer


class SerialHandler(object):

    def __init__(self, device, speed):

        self.__device = device
        self.__speed = speed

        if not self.__connect():
            sys.exit('Error: sphincter not connected?')

        self.sphincter_locked = True
        thread.start_new_thread(self.state_read_thread, ())

    def __connect(self):
        try:
            self.__ser = serial.Serial(self.__device, self.__speed)
        except:
            return False
        return True

    def __reconnect(self):
        while not self.__connect():
            time.sleep(10000)

    def send_lock(self):
        try:
            self.__ser.write('c')
        except:
            self.__reconnect()

    def send_unlock(self):
        try:
            self.__ser.write('o')
        except:
            self.__reconnect()

    def state_read_thread(self):
        while True:
            try:
                data = self.__ser.readline().strip()
            except:
                self.__reconnect()
            print(data)
            self.sphincter_locked = data == 'LOCKED'



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
            self.__hashes.append( line.split(':')[1].strip() )

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

        param_token  = ''
        param_action = ''

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
                self.server.serial_handler.send_unlock()
            elif( param_action == 'close' ):
                self.server.serial_handler.send_lock()

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

