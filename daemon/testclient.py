#!/usr/bin/python2

import socket
import sys
import random
import time

def send_message(message, sock):
    print 'sending: "%s"' % message
    sock.sendall(message)
    print "answer : %s" % sock.recv(64).strip()
    #print "answer : %s" % sock.makefile().readline().strip()
    print "-"*32

def unlock(sock):
    send_message("unlock", sock)

def lock(sock):
    send_message("lock", sock)

def status(sock):
    send_message("status", sock)

command_dict = dict(
    unlock=unlock,
    lock  =lock,
    status=status
)

# Connect the socket to the port where the server is listening
server_address = './sphincter_socket'
print >>sys.stderr, 'connecting to %s' % server_address

#try:
#    sock.connect(server_address)
#except socket.error, msg:
#    print >>sys.stderr, msg
#    sys.exit(1)


try:
    for i in range(10):

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(server_address)
        command = random.choice(list(command_dict.keys()))
        f = command_dict.get(command)
        f(sock)
        time.sleep(3)
         
finally:
    print >>sys.stderr, 'closing socket'
    sock.close()
