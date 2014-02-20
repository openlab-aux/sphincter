#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import sys
import smtplib
import string
import random
import hashlib
import re

from smtplib import SMTP_SSL as SMTP
from email.MIMEText import MIMEText


def gen_token(size=32):
    chars = string.ascii_uppercase + string.digits + string.lowercase
    token = string.join((random.choice(chars) for x in range(size)), '')
    thash = hashlib.sha256(token).hexdigest()

    return [token, thash]


def write_hash(filename, identifier, token_hash):
    try:
        f = open(filename, 'a')
        f.write(identifier + ':' + token_hash + '\n')
        f.close
    except IOError:
        sys.exit('Error: Could not write into ' + filename)


def send_token(token, address, server='', username='', password=''):

    content = "Dein Token fuer den Zugang zum OpenLab ist da:\n\n"
    content += "\t" + token

    sender = 'donotreply@openlab-augsburg.de'

    try:
        msg = MIMEText(content, 'plain')
        msg['Subject'] = 'Dein OpenLab-Zugang'
        msg['From']    = sender

        conn = SMTP(server)
        conn.set_debuglevel(False)
        conn.login(username, password)
        try:
            conn.sendmail(sender, [address], msg.as_string())
        finally:
            conn.close()

    except Exception, exc:
        sys.exit( "mail failed; %s" % str(exc) )


if __name__ == "__main__":

    try:
        address = sys.argv[1]
        filename = sys.argv[2]
    except IndexError:
        sys.exit('Usage: gentoken <email> <tokenfile>')

    if not re.match(r'[^@]+@[^@]+\.[^@]+', address):
        sys.exit('"'+address+'" is not a valid email address')

    token = gen_token()

    send_token(token[0], address)
    print('Token was successfully sent to ' + address)
    write_hash(filename, address, token[1])
    print('Hash written into ' + filename)
