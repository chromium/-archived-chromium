#! python

import sys
import os
import os.path
import socket
import thread
import time
import httplib
import BaseHTTPServer
import SimpleHTTPServer


try:
    from cryptoIDlib.api import *
    cryptoIDlibLoaded = True
except:
    cryptoIDlibLoaded = False

if __name__ != "__main__":
    raise "This must be run as a command, not used as a module!"

#import tlslite
#from tlslite.constants import AlertDescription, Fault

#from tlslite.utils.jython_compat import formatExceptionTrace
#from tlslite.X509 import X509, X509CertChain

from tlslite.api import *

def parsePrivateKey(s):
    try:
        return parsePEMKey(s, private=True)
    except Exception, e:
        print e
        return parseXMLKey(s, private=True)


def clientTest(address, dir):

    #Split address into hostname/port tuple
    address = address.split(":")
    if len(address)==1:
        address.append("4443")
    address = ( address[0], int(address[1]) )

    def connect():
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if hasattr(sock, 'settimeout'): #It's a python 2.3 feature
            sock.settimeout(5)
        sock.connect(address)
        c = TLSConnection(sock)
        return c

    test = 0

    badFault = False

    print "Test 1 - good shared key"
    connection = connect()
    connection.handshakeClientSharedKey("shared", "key")
    connection.close()
    connection.sock.close()

    print "Test 2 - shared key faults"
    for fault in Fault.clientSharedKeyFaults + Fault.genericFaults:
        connection = connect()
        connection.fault = fault
        try:
            connection.handshakeClientSharedKey("shared", "key")
            print "  Good Fault %s" % (Fault.faultNames[fault])
        except TLSFaultError, e:
            print "  BAD FAULT %s: %s" % (Fault.faultNames[fault], str(e))
            badFault = True
        connection.sock.close()

    print "Test 3 - good SRP"
    connection = connect()
    connection.handshakeClientSRP("test", "password")
    connection.close()

    print "Test 4 - SRP faults"
    for fault in Fault.clientSrpFaults + Fault.genericFaults:
        connection = connect()
        connection.fault = fault
        try:
            connection.handshakeClientSRP("test", "password")
            print "  Good Fault %s" % (Fault.faultNames[fault])
        except TLSFaultError, e:
            print "  BAD FAULT %s: %s" % (Fault.faultNames[fault], str(e))
            badFault = True
        connection.sock.close()

    print "Test 5 - good SRP: unknown_srp_username idiom"
    def srpCallback():
        return ("test", "password")
    connection = connect()
    connection.handshakeClientUnknown(srpCallback=srpCallback)
    connection.close()
    connection.sock.close()

    print "Test 6 - good SRP: with X.509 certificate"
    connection = connect()
    connection.handshakeClientSRP("test", "password")
    assert(isinstance(connection.session.serverCertChain, X509CertChain))
    connection.close()
    connection.sock.close()

    print "Test 7 - X.509 with SRP faults"
    for fault in Fault.clientSrpFaults + Fault.genericFaults:
        connection = connect()
        connection.fault = fault
        try:
            connection.handshakeClientSRP("test", "password")
            print "  Good Fault %s" % (Fault.faultNames[fault])
        except TLSFaultError, e:
            print "  BAD FAULT %s: %s" % (Fault.faultNames[fault], str(e))
            badFault = True
        connection.sock.close()

    if cryptoIDlibLoaded:
        print "Test 8 - good SRP: with cryptoID certificate chain"
        connection = connect()
        connection.handshakeClientSRP("test", "password")
        assert(isinstance(connection.session.serverCertChain, CertChain))
        if not (connection.session.serverCertChain.validate()):
            print connection.session.serverCertChain.validate(listProblems=True)

        connection.close()
        connection.sock.close()

        print "Test 9 - CryptoID with SRP faults"
        for fault in Fault.clientSrpFaults + Fault.genericFaults:
            connection = connect()
            connection.fault = fault
            try:
                connection.handshakeClientSRP("test", "password")
                print "  Good Fault %s" % (Fault.faultNames[fault])
            except TLSFaultError, e:
                print "  BAD FAULT %s: %s" % (Fault.faultNames[fault], str(e))
                badFault = True
            connection.sock.close()

    print "Test 10 - good X509"
    connection = connect()
    connection.handshakeClientCert()
    assert(isinstance(connection.session.serverCertChain, X509CertChain))
    connection.close()
    connection.sock.close()

    print "Test 10.a - good X509, SSLv3"
    connection = connect()
    settings = HandshakeSettings()
    settings.minVersion = (3,0)
    settings.maxVersion = (3,0)
    connection.handshakeClientCert(settings=settings)
    assert(isinstance(connection.session.serverCertChain, X509CertChain))
    connection.close()
    connection.sock.close()

    print "Test 11 - X.509 faults"
    for fault in Fault.clientNoAuthFaults + Fault.genericFaults:
        connection = connect()
        connection.fault = fault
        try:
            connection.handshakeClientCert()
            print "  Good Fault %s" % (Fault.faultNames[fault])
        except TLSFaultError, e:
            print "  BAD FAULT %s: %s" % (Fault.faultNames[fault], str(e))
            badFault = True
        connection.sock.close()

    if cryptoIDlibLoaded:
        print "Test 12 - good cryptoID"
        connection = connect()
        connection.handshakeClientCert()
        assert(isinstance(connection.session.serverCertChain, CertChain))
        assert(connection.session.serverCertChain.validate())
        connection.close()
        connection.sock.close()

        print "Test 13 - cryptoID faults"
        for fault in Fault.clientNoAuthFaults + Fault.genericFaults:
            connection = connect()
            connection.fault = fault
            try:
                connection.handshakeClientCert()
                print "  Good Fault %s" % (Fault.faultNames[fault])
            except TLSFaultError, e:
                print "  BAD FAULT %s: %s" % (Fault.faultNames[fault], str(e))
                badFault = True
            connection.sock.close()

    print "Test 14 - good mutual X509"
    x509Cert = X509().parse(open(os.path.join(dir, "clientX509Cert.pem")).read())
    x509Chain = X509CertChain([x509Cert])
    s = open(os.path.join(dir, "clientX509Key.pem")).read()
    x509Key = parsePEMKey(s, private=True)

    connection = connect()
    connection.handshakeClientCert(x509Chain, x509Key)
    assert(isinstance(connection.session.serverCertChain, X509CertChain))
    connection.close()
    connection.sock.close()

    print "Test 14.a - good mutual X509, SSLv3"
    connection = connect()
    settings = HandshakeSettings()
    settings.minVersion = (3,0)
    settings.maxVersion = (3,0)
    connection.handshakeClientCert(x509Chain, x509Key, settings=settings)
    assert(isinstance(connection.session.serverCertChain, X509CertChain))
    connection.close()
    connection.sock.close()

    print "Test 15 - mutual X.509 faults"
    for fault in Fault.clientCertFaults + Fault.genericFaults:
        connection = connect()
        connection.fault = fault
        try:
            connection.handshakeClientCert(x509Chain, x509Key)
            print "  Good Fault %s" % (Fault.faultNames[fault])
        except TLSFaultError, e:
            print "  BAD FAULT %s: %s" % (Fault.faultNames[fault], str(e))
            badFault = True
        connection.sock.close()

    if cryptoIDlibLoaded:
        print "Test 16 - good mutual cryptoID"
        cryptoIDChain = CertChain().parse(open(os.path.join(dir, "serverCryptoIDChain.xml"), "r").read())
        cryptoIDKey = parseXMLKey(open(os.path.join(dir, "serverCryptoIDKey.xml"), "r").read(), private=True)

        connection = connect()
        connection.handshakeClientCert(cryptoIDChain, cryptoIDKey)
        assert(isinstance(connection.session.serverCertChain, CertChain))
        assert(connection.session.serverCertChain.validate())
        connection.close()
        connection.sock.close()

        print "Test 17 - mutual cryptoID faults"
        for fault in Fault.clientCertFaults + Fault.genericFaults:
            connection = connect()
            connection.fault = fault
            try:
                connection.handshakeClientCert(cryptoIDChain, cryptoIDKey)
                print "  Good Fault %s" % (Fault.faultNames[fault])
            except TLSFaultError, e:
                print "  BAD FAULT %s: %s" % (Fault.faultNames[fault], str(e))
                badFault = True
            connection.sock.close()

    print "Test 18 - good SRP, prepare to resume..."
    connection = connect()
    connection.handshakeClientSRP("test", "password")
    connection.close()
    connection.sock.close()
    session = connection.session

    print "Test 19 - resumption"
    connection = connect()
    connection.handshakeClientSRP("test", "garbage", session=session)
    #Don't close! -- see below

    print "Test 20 - invalidated resumption"
    connection.sock.close() #Close the socket without a close_notify!
    connection = connect()
    try:
        connection.handshakeClientSRP("test", "garbage", session=session)
        assert()
    except TLSRemoteAlert, alert:
        if alert.description != AlertDescription.bad_record_mac:
            raise
    connection.sock.close()

    print "Test 21 - HTTPS test X.509"
    address = address[0], address[1]+1
    if hasattr(socket, "timeout"):
        timeoutEx = socket.timeout
    else:
        timeoutEx = socket.error
    while 1:
        try:
            time.sleep(2)
            htmlBody = open(os.path.join(dir, "index.html")).read()
            fingerprint = None
            for y in range(2):
                h = HTTPTLSConnection(\
                        address[0], address[1], x509Fingerprint=fingerprint)
                for x in range(3):
                    h.request("GET", "/index.html")
                    r = h.getresponse()
                    assert(r.status == 200)
                    s = r.read()
                    assert(s == htmlBody)
                fingerprint = h.tlsSession.serverCertChain.getFingerprint()
                assert(fingerprint)
            time.sleep(2)
            break
        except timeoutEx:
            print "timeout, retrying..."
            pass

    if cryptoIDlibLoaded:
        print "Test 21a - HTTPS test SRP+cryptoID"
        address = address[0], address[1]+1
        if hasattr(socket, "timeout"):
            timeoutEx = socket.timeout
        else:
            timeoutEx = socket.error
        while 1:
            try:
                time.sleep(2) #Time to generate key and cryptoID
                htmlBody = open(os.path.join(dir, "index.html")).read()
                fingerprint = None
                protocol = None
                for y in range(2):
                    h = HTTPTLSConnection(\
                                 address[0], address[1],
                                 username="test", password="password",
                                 cryptoID=fingerprint, protocol=protocol)
                    for x in range(3):
                        h.request("GET", "/index.html")
                        r = h.getresponse()
                        assert(r.status == 200)
                        s = r.read()
                        assert(s == htmlBody)
                    fingerprint = h.tlsSession.serverCertChain.cryptoID
                    assert(fingerprint)
                    protocol = "urn:whatever"
                time.sleep(2)
                break
            except timeoutEx:
                print "timeout, retrying..."
                pass

    address = address[0], address[1]+1

    implementations = []
    if cryptlibpyLoaded:
        implementations.append("cryptlib")
    if m2cryptoLoaded:
        implementations.append("openssl")
    if pycryptoLoaded:
        implementations.append("pycrypto")
    implementations.append("python")

    print "Test 22 - different ciphers"
    for implementation in implementations:
        for cipher in ["aes128", "aes256", "rc4"]:

            print "Test 22:",
            connection = connect()

            settings = HandshakeSettings()
            settings.cipherNames = [cipher]
            settings.cipherImplementations = [implementation, "python"]
            connection.handshakeClientSharedKey("shared", "key", settings=settings)
            print ("%s %s" % (connection.getCipherName(), connection.getCipherImplementation()))

            connection.write("hello")
            h = connection.read(min=5, max=5)
            assert(h == "hello")
            connection.close()
            connection.sock.close()

    print "Test 23 - throughput test"
    for implementation in implementations:
        for cipher in ["aes128", "aes256", "3des", "rc4"]:
            if cipher == "3des" and implementation not in ("openssl", "cryptlib", "pycrypto"):
                continue

            print "Test 23:",
            connection = connect()

            settings = HandshakeSettings()
            settings.cipherNames = [cipher]
            settings.cipherImplementations = [implementation, "python"]
            connection.handshakeClientSharedKey("shared", "key", settings=settings)
            print ("%s %s:" % (connection.getCipherName(), connection.getCipherImplementation())),

            startTime = time.clock()
            connection.write("hello"*10000)
            h = connection.read(min=50000, max=50000)
            stopTime = time.clock()
            print "100K exchanged at rate of %d bytes/sec" % int(100000/(stopTime-startTime))

            assert(h == "hello"*10000)
            connection.close()
            connection.sock.close()

    print "Test 24 - Internet servers test"
    try:
        i = IMAP4_TLS("cyrus.andrew.cmu.edu")
        i.login("anonymous", "anonymous@anonymous.net")
        i.logout()
        print "Test 24: IMAP4 good"
        p = POP3_TLS("pop.gmail.com")
        p.quit()
        print "Test 24: POP3 good"
    except socket.error, e:
        print "Non-critical error: socket error trying to reach internet server: ", e

    if not badFault:
        print "Test succeeded"
    else:
        print "Test failed"


def serverTest(address, dir):
    #Split address into hostname/port tuple
    address = address.split(":")
    if len(address)==1:
        address.append("4443")
    address = ( address[0], int(address[1]) )

    #Connect to server
    lsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    lsock.bind(address)
    lsock.listen(5)

    def connect():
        return TLSConnection(lsock.accept()[0])

    print "Test 1 - good shared key"
    sharedKeyDB = SharedKeyDB()
    sharedKeyDB["shared"] = "key"
    sharedKeyDB["shared2"] = "key2"
    connection = connect()
    connection.handshakeServer(sharedKeyDB=sharedKeyDB)
    connection.close()
    connection.sock.close()

    print "Test 2 - shared key faults"
    for fault in Fault.clientSharedKeyFaults + Fault.genericFaults:
        connection = connect()
        connection.fault = fault
        try:
            connection.handshakeServer(sharedKeyDB=sharedKeyDB)
            assert()
        except:
            pass
        connection.sock.close()

    print "Test 3 - good SRP"
    #verifierDB = tlslite.VerifierDB(os.path.join(dir, "verifierDB"))
    #verifierDB.open()
    verifierDB = VerifierDB()
    verifierDB.create()
    entry = VerifierDB.makeVerifier("test", "password", 1536)
    verifierDB["test"] = entry

    connection = connect()
    connection.handshakeServer(verifierDB=verifierDB)
    connection.close()
    connection.sock.close()

    print "Test 4 - SRP faults"
    for fault in Fault.clientSrpFaults + Fault.genericFaults:
        connection = connect()
        connection.fault = fault
        try:
            connection.handshakeServer(verifierDB=verifierDB)
            assert()
        except:
            pass
        connection.sock.close()

    print "Test 5 - good SRP: unknown_srp_username idiom"
    connection = connect()
    connection.handshakeServer(verifierDB=verifierDB)
    connection.close()
    connection.sock.close()

    print "Test 6 - good SRP: with X.509 cert"
    x509Cert = X509().parse(open(os.path.join(dir, "serverX509Cert.pem")).read())
    x509Chain = X509CertChain([x509Cert])
    s = open(os.path.join(dir, "serverX509Key.pem")).read()
    x509Key = parsePEMKey(s, private=True)

    connection = connect()
    connection.handshakeServer(verifierDB=verifierDB, \
                               certChain=x509Chain, privateKey=x509Key)
    connection.close()
    connection.sock.close()

    print "Test 7 - X.509 with SRP faults"
    for fault in Fault.clientSrpFaults + Fault.genericFaults:
        connection = connect()
        connection.fault = fault
        try:
            connection.handshakeServer(verifierDB=verifierDB, \
                                       certChain=x509Chain, privateKey=x509Key)
            assert()
        except:
            pass
        connection.sock.close()

    if cryptoIDlibLoaded:
        print "Test 8 - good SRP: with cryptoID certs"
        cryptoIDChain = CertChain().parse(open(os.path.join(dir, "serverCryptoIDChain.xml"), "r").read())
        cryptoIDKey = parseXMLKey(open(os.path.join(dir, "serverCryptoIDKey.xml"), "r").read(), private=True)
        connection = connect()
        connection.handshakeServer(verifierDB=verifierDB, \
                                   certChain=cryptoIDChain, privateKey=cryptoIDKey)
        connection.close()
        connection.sock.close()

        print "Test 9 - cryptoID with SRP faults"
        for fault in Fault.clientSrpFaults + Fault.genericFaults:
            connection = connect()
            connection.fault = fault
            try:
                connection.handshakeServer(verifierDB=verifierDB, \
                                           certChain=cryptoIDChain, privateKey=cryptoIDKey)
                assert()
            except:
                pass
            connection.sock.close()

    print "Test 10 - good X.509"
    connection = connect()
    connection.handshakeServer(certChain=x509Chain, privateKey=x509Key)
    connection.close()
    connection.sock.close()

    print "Test 10.a - good X.509, SSL v3"
    connection = connect()
    settings = HandshakeSettings()
    settings.minVersion = (3,0)
    settings.maxVersion = (3,0)
    connection.handshakeServer(certChain=x509Chain, privateKey=x509Key, settings=settings)
    connection.close()
    connection.sock.close()

    print "Test 11 - X.509 faults"
    for fault in Fault.clientNoAuthFaults + Fault.genericFaults:
        connection = connect()
        connection.fault = fault
        try:
            connection.handshakeServer(certChain=x509Chain, privateKey=x509Key)
            assert()
        except:
            pass
        connection.sock.close()

    if cryptoIDlibLoaded:
        print "Test 12 - good cryptoID"
        connection = connect()
        connection.handshakeServer(certChain=cryptoIDChain, privateKey=cryptoIDKey)
        connection.close()
        connection.sock.close()

        print "Test 13 - cryptoID faults"
        for fault in Fault.clientNoAuthFaults + Fault.genericFaults:
            connection = connect()
            connection.fault = fault
            try:
                connection.handshakeServer(certChain=cryptoIDChain, privateKey=cryptoIDKey)
                assert()
            except:
                pass
            connection.sock.close()

    print "Test 14 - good mutual X.509"
    connection = connect()
    connection.handshakeServer(certChain=x509Chain, privateKey=x509Key, reqCert=True)
    assert(isinstance(connection.session.serverCertChain, X509CertChain))
    connection.close()
    connection.sock.close()

    print "Test 14a - good mutual X.509, SSLv3"
    connection = connect()
    settings = HandshakeSettings()
    settings.minVersion = (3,0)
    settings.maxVersion = (3,0)
    connection.handshakeServer(certChain=x509Chain, privateKey=x509Key, reqCert=True, settings=settings)
    assert(isinstance(connection.session.serverCertChain, X509CertChain))
    connection.close()
    connection.sock.close()

    print "Test 15 - mutual X.509 faults"
    for fault in Fault.clientCertFaults + Fault.genericFaults:
        connection = connect()
        connection.fault = fault
        try:
            connection.handshakeServer(certChain=x509Chain, privateKey=x509Key, reqCert=True)
            assert()
        except:
            pass
        connection.sock.close()

    if cryptoIDlibLoaded:
        print "Test 16 - good mutual cryptoID"
        connection = connect()
        connection.handshakeServer(certChain=cryptoIDChain, privateKey=cryptoIDKey, reqCert=True)
        assert(isinstance(connection.session.serverCertChain, CertChain))
        assert(connection.session.serverCertChain.validate())
        connection.close()
        connection.sock.close()

        print "Test 17 - mutual cryptoID faults"
        for fault in Fault.clientCertFaults + Fault.genericFaults:
            connection = connect()
            connection.fault = fault
            try:
                connection.handshakeServer(certChain=cryptoIDChain, privateKey=cryptoIDKey, reqCert=True)
                assert()
            except:
                pass
            connection.sock.close()

    print "Test 18 - good SRP, prepare to resume"
    sessionCache = SessionCache()
    connection = connect()
    connection.handshakeServer(verifierDB=verifierDB, sessionCache=sessionCache)
    connection.close()
    connection.sock.close()

    print "Test 19 - resumption"
    connection = connect()
    connection.handshakeServer(verifierDB=verifierDB, sessionCache=sessionCache)
    #Don't close! -- see next test

    print "Test 20 - invalidated resumption"
    try:
        connection.read(min=1, max=1)
        assert() #Client is going to close the socket without a close_notify
    except TLSAbruptCloseError, e:
        pass
    connection = connect()
    try:
        connection.handshakeServer(verifierDB=verifierDB, sessionCache=sessionCache)
    except TLSLocalAlert, alert:
        if alert.description != AlertDescription.bad_record_mac:
            raise
    connection.sock.close()

    print "Test 21 - HTTPS test X.509"

    #Close the current listening socket
    lsock.close()

    #Create and run an HTTP Server using TLSSocketServerMixIn
    class MyHTTPServer(TLSSocketServerMixIn,
                       BaseHTTPServer.HTTPServer):
        def handshake(self, tlsConnection):
                tlsConnection.handshakeServer(certChain=x509Chain, privateKey=x509Key)
                return True
    cd = os.getcwd()
    os.chdir(dir)
    address = address[0], address[1]+1
    httpd = MyHTTPServer(address, SimpleHTTPServer.SimpleHTTPRequestHandler)
    for x in range(6):
        httpd.handle_request()
    httpd.server_close()
    cd = os.chdir(cd)

    if cryptoIDlibLoaded:
        print "Test 21a - HTTPS test SRP+cryptoID"

        #Create and run an HTTP Server using TLSSocketServerMixIn
        class MyHTTPServer(TLSSocketServerMixIn,
                           BaseHTTPServer.HTTPServer):
            def handshake(self, tlsConnection):
                    tlsConnection.handshakeServer(certChain=cryptoIDChain, privateKey=cryptoIDKey,
                                                  verifierDB=verifierDB)
                    return True
        cd = os.getcwd()
        os.chdir(dir)
        address = address[0], address[1]+1
        httpd = MyHTTPServer(address, SimpleHTTPServer.SimpleHTTPRequestHandler)
        for x in range(6):
            httpd.handle_request()
        httpd.server_close()
        cd = os.chdir(cd)

    #Re-connect the listening socket
    lsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    address = address[0], address[1]+1
    lsock.bind(address)
    lsock.listen(5)

    def connect():
        return TLSConnection(lsock.accept()[0])

    implementations = []
    if cryptlibpyLoaded:
        implementations.append("cryptlib")
    if m2cryptoLoaded:
        implementations.append("openssl")
    if pycryptoLoaded:
        implementations.append("pycrypto")
    implementations.append("python")

    print "Test 22 - different ciphers"
    for implementation in ["python"] * len(implementations):
        for cipher in ["aes128", "aes256", "rc4"]:

            print "Test 22:",
            connection = connect()

            settings = HandshakeSettings()
            settings.cipherNames = [cipher]
            settings.cipherImplementations = [implementation, "python"]

            connection.handshakeServer(sharedKeyDB=sharedKeyDB, settings=settings)
            print connection.getCipherName(), connection.getCipherImplementation()
            h = connection.read(min=5, max=5)
            assert(h == "hello")
            connection.write(h)
            connection.close()
            connection.sock.close()

    print "Test 23 - throughput test"
    for implementation in implementations:
        for cipher in ["aes128", "aes256", "3des", "rc4"]:
            if cipher == "3des" and implementation not in ("openssl", "cryptlib", "pycrypto"):
                continue

            print "Test 23:",
            connection = connect()

            settings = HandshakeSettings()
            settings.cipherNames = [cipher]
            settings.cipherImplementations = [implementation, "python"]

            connection.handshakeServer(sharedKeyDB=sharedKeyDB, settings=settings)
            print connection.getCipherName(), connection.getCipherImplementation()
            h = connection.read(min=50000, max=50000)
            assert(h == "hello"*10000)
            connection.write(h)
            connection.close()
            connection.sock.close()

    print "Test succeeded"












if len(sys.argv) == 1 or (len(sys.argv)==2 and sys.argv[1].lower().endswith("help")):
    print ""
    print "Version: 0.3.8"
    print ""
    print "RNG: %s" % prngName
    print ""
    print "Modules:"
    if cryptlibpyLoaded:
        print "  cryptlib_py : Loaded"
    else:
        print "  cryptlib_py : Not Loaded"
    if m2cryptoLoaded:
        print "  M2Crypto    : Loaded"
    else:
        print "  M2Crypto    : Not Loaded"
    if pycryptoLoaded:
        print "  pycrypto    : Loaded"
    else:
        print "  pycrypto    : Not Loaded"
    if gmpyLoaded:
        print "  GMPY        : Loaded"
    else:
        print "  GMPY        : Not Loaded"
    if cryptoIDlibLoaded:
        print "  cryptoIDlib : Loaded"
    else:
        print "  cryptoIDlib : Not Loaded"
    print ""
    print "Commands:"
    print ""
    print "  clientcert      <server> [<chain> <key>]"
    print "  clientsharedkey <server> <user> <pass>"
    print "  clientsrp       <server> <user> <pass>"
    print "  clienttest      <server> <dir>"
    print ""
    print "  serversrp       <server> <verifierDB>"
    print "  servercert      <server> <chain> <key> [req]"
    print "  serversrpcert   <server> <verifierDB> <chain> <key>"
    print "  serversharedkey <server> <sharedkeyDB>"
    print "  servertest      <server> <dir>"
    sys.exit()

cmd = sys.argv[1].lower()

class Args:
    def __init__(self, argv):
        self.argv = argv
    def get(self, index):
        if len(self.argv)<=index:
            raise SyntaxError("Not enough arguments")
        return self.argv[index]
    def getLast(self, index):
        if len(self.argv)>index+1:
            raise SyntaxError("Too many arguments")
        return self.get(index)

args = Args(sys.argv)

def reformatDocString(s):
    lines = s.splitlines()
    newLines = []
    for line in lines:
        newLines.append("  " + line.strip())
    return "\n".join(newLines)

try:
    if cmd == "clienttest":
        address = args.get(2)
        dir = args.getLast(3)
        clientTest(address, dir)
        sys.exit()

    elif cmd.startswith("client"):
        address = args.get(2)

        #Split address into hostname/port tuple
        address = address.split(":")
        if len(address)==1:
            address.append("4443")
        address = ( address[0], int(address[1]) )

        def connect():
            #Connect to server
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            if hasattr(sock, "settimeout"):
                sock.settimeout(5)
            sock.connect(address)

            #Instantiate TLSConnections
            return TLSConnection(sock)

        try:
            if cmd == "clientsrp":
                username = args.get(3)
                password = args.getLast(4)
                connection = connect()
                start = time.clock()
                connection.handshakeClientSRP(username, password)
            elif cmd == "clientsharedkey":
                username = args.get(3)
                password = args.getLast(4)
                connection = connect()
                start = time.clock()
                connection.handshakeClientSharedKey(username, password)
            elif cmd == "clientcert":
                certChain = None
                privateKey = None
                if len(sys.argv) > 3:
                    certFilename = args.get(3)
                    keyFilename = args.getLast(4)

                    s1 = open(certFilename, "rb").read()
                    s2 = open(keyFilename, "rb").read()

                    #Try to create cryptoID cert chain
                    if cryptoIDlibLoaded:
                        try:
                            certChain = CertChain().parse(s1)
                            privateKey = parsePrivateKey(s2)
                        except:
                            certChain = None
                            privateKey = None

                    #Try to create X.509 cert chain
                    if not certChain:
                        x509 = X509()
                        x509.parse(s1)
                        certChain = X509CertChain([x509])
                        privateKey = parsePrivateKey(s2)

                connection = connect()
                start = time.clock()
                connection.handshakeClientCert(certChain, privateKey)
            else:
                raise SyntaxError("Unknown command")

        except TLSLocalAlert, a:
            if a.description == AlertDescription.bad_record_mac:
                if cmd == "clientsharedkey":
                    print "Bad sharedkey password"
                else:
                    raise
            elif a.description == AlertDescription.user_canceled:
                print str(a)
            else:
                raise
            sys.exit()
        except TLSRemoteAlert, a:
            if a.description == AlertDescription.unknown_srp_username:
                if cmd == "clientsrp":
                    print "Unknown username"
                else:
                    raise
            elif a.description == AlertDescription.bad_record_mac:
                if cmd == "clientsrp":
                    print "Bad username or password"
                else:
                    raise
            elif a.description == AlertDescription.handshake_failure:
                print "Unable to negotiate mutually acceptable parameters"
            else:
                raise
            sys.exit()

        stop = time.clock()
        print "Handshake success"
        print "  Handshake time: %.4f seconds" % (stop - start)
        print "  Version: %s.%s" % connection.version
        print "  Cipher: %s %s" % (connection.getCipherName(), connection.getCipherImplementation())
        if connection.session.srpUsername:
            print "  Client SRP username: %s" % connection.session.srpUsername
        if connection.session.sharedKeyUsername:
            print "  Client shared key username: %s" % connection.session.sharedKeyUsername
        if connection.session.clientCertChain:
            print "  Client fingerprint: %s" % connection.session.clientCertChain.getFingerprint()
        if connection.session.serverCertChain:
            print "  Server fingerprint: %s" % connection.session.serverCertChain.getFingerprint()
        connection.close()
        connection.sock.close()

    elif cmd.startswith("server"):
        address = args.get(2)

        #Split address into hostname/port tuple
        address = address.split(":")
        if len(address)==1:
            address.append("4443")
        address = ( address[0], int(address[1]) )

        verifierDBFilename = None
        sharedKeyDBFilename = None
        certFilename = None
        keyFilename = None
        sharedKeyDB = None
        reqCert = False

        if cmd == "serversrp":
            verifierDBFilename = args.getLast(3)
        elif cmd == "servercert":
            certFilename = args.get(3)
            keyFilename = args.get(4)
            if len(sys.argv)>=6:
                req = args.getLast(5)
                if req.lower() != "req":
                    raise SyntaxError()
                reqCert = True
        elif cmd == "serversrpcert":
            verifierDBFilename = args.get(3)
            certFilename = args.get(4)
            keyFilename = args.getLast(5)
        elif cmd == "serversharedkey":
            sharedKeyDBFilename = args.getLast(3)
        elif cmd == "servertest":
            address = args.get(2)
            dir = args.getLast(3)
            serverTest(address, dir)
            sys.exit()

        verifierDB = None
        if verifierDBFilename:
            verifierDB = VerifierDB(verifierDBFilename)
            verifierDB.open()

        sharedKeyDB = None
        if sharedKeyDBFilename:
            sharedKeyDB = SharedKeyDB(sharedKeyDBFilename)
            sharedKeyDB.open()

        certChain = None
        privateKey = None
        if certFilename:
            s1 = open(certFilename, "rb").read()
            s2 = open(keyFilename, "rb").read()

            #Try to create cryptoID cert chain
            if cryptoIDlibLoaded:
                try:
                    certChain = CertChain().parse(s1)
                    privateKey = parsePrivateKey(s2)
                except:
                    certChain = None
                    privateKey = None

            #Try to create X.509 cert chain
            if not certChain:
                x509 = X509()
                x509.parse(s1)
                certChain = X509CertChain([x509])
                privateKey = parsePrivateKey(s2)



        #Create handler function - performs handshake, then echos all bytes received
        def handler(sock):
            try:
                connection = TLSConnection(sock)
                settings = HandshakeSettings()
                connection.handshakeServer(sharedKeyDB=sharedKeyDB, verifierDB=verifierDB, \
                                           certChain=certChain, privateKey=privateKey, \
                                           reqCert=reqCert, settings=settings)
                print "Handshake success"
                print "  Version: %s.%s" % connection.version
                print "  Cipher: %s %s" % (connection.getCipherName(), connection.getCipherImplementation())
                if connection.session.srpUsername:
                    print "  Client SRP username: %s" % connection.session.srpUsername
                if connection.session.sharedKeyUsername:
                    print "  Client shared key username: %s" % connection.session.sharedKeyUsername
                if connection.session.clientCertChain:
                    print "  Client fingerprint: %s" % connection.session.clientCertChain.getFingerprint()
                if connection.session.serverCertChain:
                    print "  Server fingerprint: %s" % connection.session.serverCertChain.getFingerprint()

                s = ""
                while 1:
                    newS = connection.read()
                    if not newS:
                        break
                    s += newS
                    if s[-1]=='\n':
                        connection.write(s)
                        s = ""
            except TLSLocalAlert, a:
                if a.description == AlertDescription.unknown_srp_username:
                    print "Unknown SRP username"
                elif a.description == AlertDescription.bad_record_mac:
                    if cmd == "serversrp" or cmd == "serversrpcert":
                        print "Bad SRP password for:", connection.allegedSrpUsername
                    else:
                        raise
                elif a.description == AlertDescription.handshake_failure:
                    print "Unable to negotiate mutually acceptable parameters"
                else:
                    raise
            except TLSRemoteAlert, a:
                if a.description == AlertDescription.bad_record_mac:
                    if cmd == "serversharedkey":
                        print "Bad sharedkey password for:", connection.allegedSharedKeyUsername
                    else:
                        raise
                elif a.description == AlertDescription.user_canceled:
                    print "Handshake cancelled"
                elif a.description == AlertDescription.handshake_failure:
                    print "Unable to negotiate mutually acceptable parameters"
                elif a.description == AlertDescription.close_notify:
                    pass
                else:
                    raise

        #Run multi-threaded server
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(address)
        sock.listen(5)
        while 1:
            (newsock, cliAddress) = sock.accept()
            thread.start_new_thread(handler, (newsock,))


    else:
        print "Bad command: '%s'" % cmd
except TLSRemoteAlert, a:
    print str(a)
    raise





