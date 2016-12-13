from SocketServer import *
from BaseHTTPServer import *
from SimpleHTTPServer import *
from tlslite.api import *

s = open("./serverX509Cert.pem").read()
x509 = X509()
x509.parse(s)
certChain = X509CertChain([x509])

s = open("./serverX509Key.pem").read()
privateKey = parsePEMKey(s, private=True)

sessionCache = SessionCache()

class MyHTTPServer(ThreadingMixIn, TLSSocketServerMixIn, HTTPServer):
    def handshake(self, tlsConnection):
        try:
            tlsConnection.handshakeServer(certChain=certChain,
                                          privateKey=privateKey,
                                          sessionCache=sessionCache)
            tlsConnection.ignoreAbruptClose = True
            return True
        except TLSError, error:
            print "Handshake failure:", str(error)
            return False

httpd = MyHTTPServer(('localhost', 443), SimpleHTTPRequestHandler)
httpd.serve_forever()