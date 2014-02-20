#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os
import sys

import serial
from serial import SerialException

import time
import thread

import hashlib

from BaseHTTPServer import BaseHTTPRequestHandler
from BaseHTTPServer import HTTPServer
from urlparse import urlparse, parse_qs


# Serial wrapper class for sphincter
class SerialHandler:

    def __init__(self, device='./tty1', speed=9600):
        self.__ser          = serial.Serial()
        self.__ser.baudrate = speed
        self.__ser.port     = device

        if not self.__connect():
            sys.exit('Error: Sphincter not connected')

        self.__reconnecting = False
        self.state = 'LOCKED'
        thread.start_new_thread(self.__state_read_thread__, ())

    def __connect(self):
        try:
            if self.__ser.isOpen():
                self.__ser.close()

            self.__ser.open()
            return True

        except (SerialException, ValueError, OSError):
            return False

    def __reconnect(self):
        i = 1
        self.state = 'ERROR'
        self.__reconnecting = True
        while True:
            sys.stdout.write('reconnecting (' + str(i) + ')...')

            if self.__connect():
                sys.stdout.write(' success!\n')
                self.__reconnecting = False
                break

            sys.stdout.write(' failed!\n')
            i += 1
            time.sleep(5)

    def __state_read_thread(self):
        data = ''
        while True:
            try:
                data = self.__ser.readline().strip()
            except SerialException:
                self.__reconnect()
            print(data)
            self.state = data

    def __send(self, data):
        try:
            self.__ser.write(data)
            return True
        except (ValueError, SerialException):
            if not self.__reconnecting:
                self.__reconnect()
            return False

    def send_lock(self):
        return self.__send('c')

    def send_unlock(self):
        return self.__send('o')


class TokenFileHandler:

    def __init__(self, filename):
        lines = []
        self.__hashes = []

        try:
            f = open(filename)
            lines = f.readlines()
            f.close()
        except IOError:
            sys.exit('token file not found')

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

        message = 'NOT ALLOWED'

        if( param_action == 'state' ):

            message = self.server.serial_handler.state

        elif( t_handler.token_is_valid(param_token) ):

            if( param_action == 'open' ):
                success = self.server.serial_handler.send_unlock()
            elif( param_action == 'close' ):
                success = self.server.serial_handler.send_lock()

            if success:
                message = 'SUCCESS'
            else:
                message = 'FAILED'

        self.send_response(200)
        self.end_headers()
        self.wfile.write(message)

        return

    def log_message(self, format, *arg): # do nothing = turn off logging
        return


if __name__ == '__main__':

    server = SphincterServer( ('localhost', 8080), GETHandler, serial_handler=SerialHandler() )
    print 'Starting server, use <Ctrl-C> to stop'
    server.serve_forever()

