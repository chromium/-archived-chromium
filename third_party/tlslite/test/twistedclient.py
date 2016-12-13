
from tlslite.api import *
import socket

s = socket.socket()

s.connect( ("localhost", 1079) )

"""
#Only use this for Echo2
s.send("000\r\n")
while 1:
    val= s.recv(100)
    print val,
    if val.endswith("000\r\n"):
        break

s.send("STARTTLS\r\n")
"""

connection = TLSConnection(s)
#connection.handshakeClientNoAuth()
connection.handshakeClientSRP("test", "password")

connection.send("abc\r\n")
print connection.recv(100),
print connection.recv(100),

connection.send("def\r\n")
print connection.recv(100),

connection.close()
connection.sock.close()