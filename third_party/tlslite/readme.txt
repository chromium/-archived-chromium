
tlslite version 0.3.8                                      February 21, 2005
Trevor Perrin <trevp at trevp.net>
http://trevp.net/tlslite/
============================================================================


Table of Contents
==================
1  Introduction
2  License/Acknowledgements
3  Installation
4  Getting Started with the Command-Line Tools
5  Getting Started with the Library
6  Using TLS Lite with httplib
7  Using TLS Lite with xmlrpclib
8  Using TLS Lite with poplib or imaplib
9  Using TLS Lite with smtplib
10 Using TLS Lite with SocketServer
11 Using TLS Lite with asyncore
12 Using TLS Lite with Twisted
13 SECURITY CONSIDERATIONS
14 History
15 References


1 Introduction
===============
TLS Lite is a free python library that implements SSL v3, TLS v1, and 
TLS v1.1 [0]. TLS Lite supports non-traditional authentication methods 
such as SRP [1], shared keys [2], and cryptoIDs [3], in addition to X.509
certificates.  TLS Lite is pure python, however it can access OpenSSL [4], 
cryptlib [5], pycrypto [9], and GMPY [10] for faster crypto operations.  TLS 
Lite integrates with httplib, xmlrpclib, poplib, imaplib, smtplib,
SocketServer, asyncore, and Twisted.

API documentation is available in the 'docs' directory.

If you have questions or feedback, feel free to contact me.


2 Licenses/Acknowledgements
============================
All code here is public domain.

Thanks to Bram Cohen for his public domain Rijndael implementation.

Thanks to Edward Loper for Epydoc, which generated the API docs.


3 Installation
===============
Requirements:
  Python 2.2 or greater is required.

Options:
  - If you have cryptoIDlib [8], you can use cryptoID certificate chains for
  authentication.  CryptoIDlib is the sister library to TLS Lite; it was
  written by the same author, and has a similar interface.

  - If you have the M2Crypto [6] interface to OpenSSL, this will be used for
  fast RSA operations and fast ciphers.

  - If you have the cryptlib_py [7] interface to cryptlib, this will be used
  for random number generation and fast ciphers.  If TLS Lite can't find an
  OS-level random-number generator (i.e. /dev/urandom on UNIX or CryptoAPI on
  Windows), then you must MUST install cryptlib.

  - If you have pycrypto [9], this will be used for fast ciphers and fast RSA
  operations.

  - If you have the GMPY [10] interface to GMP, this will be used for fast RSA
  and SRP operations.

  - These modules don't need to be present at installation - you can install
  them any time.

On Windows:
  Run the installer in the 'installers' directory.
  *OR*
  Run 'setup.py install' (this only works if your system has a compiler
  available).

Anywhere else:
  - Run 'python setup.py install'

Test the Installation:
  - The 'tls.py' script should have been copied onto your path.  If not,
    you may have to copy it there manually.
  - From the distribution's ./test subdirectory, run:
      tls.py servertest localhost:4443 .
  - While the test server is waiting, run:
      tls.py clienttest localhost:4443 .

  If both say "Test succeeded" at the end, you're ready to go.

  (WARNING: Be careful running these (or any) scripts from the distribution's
  root directory.  Depending on your path, the scripts may load the local copy
  of the library instead of the installed version, with unpredictable
  results).


4 Getting Started with the Command-Line Tools
==============================================
tlslite comes with two command-line scripts: 'tlsdb.py' and 'tls.py'.  They
can be run with no arguments to see a list of commands.

'tlsdb.py' lets you manage shared key or verifier databases.  These databases
store usernames associated with either shared keys, or SRP password verifiers.
These databases are used by a TLS server when authenticating clients with
shared keys or SRP.

'tls.py' lets you run test clients and servers.  It can be used for testing
other TLS implementations, or as example code for using tlslite.  To run an
SRP server, try something like:

  tlsdb.py createsrp verifierDB
  tlsdb.py add verifierDB alice abra123cadabra 1024
  tlsdb.py add verifierDB bob swordfish 2048

  tls.py serversrp localhost:443 verifierDB

Then you can try connecting to the server with:

  tls.py clientsrp localhost:443 alice abra123cadabra


5 Getting Started with the Library
===================================
Using the library is simple.  Whether you're writing a client or server, there
are six steps:
1) Create a socket and connect it to the other party.
2) Construct a TLSConnection instance with the socket.
3) Call a handshake function on TLSConnection to perform the TLS handshake.
4) Check the results to make sure you're talking to the right party.
5) Use the TLSConnection to exchange data.
6) Call close() on the TLSConnection when you're done.

TLS Lite also integrates with httplib, xmlrpclib, poplib, imaplib, smtplib, 
SocketServer, asyncore, and Twisted.  When used with these, some of the steps 
are performed for you.  See the sections following this one for details.

5 Step 1 - create a socket
---------------------------
Below demonstrates a socket connection to Amazon's secure site.  It's a good
idea to set the timeout value, so if the other side fails to respond you won't
end up waiting forever.

  from socket import *
  sock = socket(AF_INET, SOCK_STREAM)
  sock.connect( ("www.amazon.com", 443) )
  sock.settimeout(10)  #Only on python 2.3 or greater

5 Step 2 - construct a TLSConnection
-------------------------------------
  from tlslite.api import *
  connection = TLSConnection(sock)

5 Step 3 - call a handshake function (client)
----------------------------------------------
If you're a client, there's several different handshake functions you can
call, depending on how you want to authenticate:

  connection.handshakeClientCert()
  connection.handshakeClientCert(certChain, privateKey)
  connection.handshakeClientSRP("alice", "abra123cadabra")
  connection.handshakeClientSharedKey("alice", "PaVBVZkYqAjCQCu6UBL2xgsnZhw")
  connection.handshakeClientUnknown(srpCallback, certCallback)

The ClientCert function without arguments is used when connecting to a site
like Amazon, which doesn't require client authentication.  The server will
authenticate with a certificate chain.

The ClientCert function can also be used to do client authentication with an
X.509 or cryptoID certificate chain.  To use cryptoID chains, you'll need the
cryptoIDlib library [8].  To use X.509 chains, you'll need some way of
creating these, such as OpenSSL (see http://www.openssl.org/docs/HOWTO/ for
details).

Below are examples of loading cryptoID and X.509 certificate chains:

  #Load cryptoID certChain and privateKey.  Requires cryptoIDlib.
  from cryptoIDlib.CertChain import CertChain
  s = open("./test/clientCryptoIDChain.xml").read()
  certChain = CertChain()
  certChain.parse(s)
  s = open("./test/clientCryptoIDKey.xml").read()
  privateKey = parseXMLKey(s, private=True)

  #Load X.509 certChain and privateKey.
  s = open("./test/clientX509Cert.pem").read()
  x509 = X509()
  x509.parse(s)
  certChain = X509CertChain([x509])
  s = open("./test/clientX509Key.pem").read()
  privateKey = parsePEMKey(s, private=True)

The SRP and SharedKey functions both do mutual authentication with a username
and password.  The difference is this: SRP is slow but safer when using low-
entropy passwords, since the SRP protocol is not vulnerable to offline
dictionary attacks.  Using shared keys is faster, but it's only safe when
used with high-entropy secrets.  In general, you should prefer SRP for human-
memorable passwords, and use shared keys only when your performance needs
outweigh the inconvenience of handling large random strings.

[WARNING: shared keys and SRP are internet-drafts; these protocols may change,
which means future versions of tlslite may not be compatible with this one.
This is less likely with SRP, more likely with shared-keys.]

The Unknown function is used when you're not sure if the server requires
client authentication.	 If the server requests SRP or certificate-based
authentication, the appropriate callback will be triggered, and you should
return a tuple containing either a (username, password) or (certChain,
privateKey), as appropriate.  Alternatively, you can return None, which will
cancel the handshake from an SRP callback, or cause it to continue without
client authentication (if the server is willing) from a certificate callback.

If you want more control over the handshake, you can pass in a
HandshakeSettings instance.  For example, if you're performing SRP, but you
only want to use SRP parameters of at least 2048 bits, and you only want to use
the AES-256 cipher, and you only want to allow TLS (version 3.1), not SSL
(version 3.0), you can do:

  settings = HandshakeSettings()
  settings.minKeySize = 2048
  settings.cipherNames = ["aes256"]
  settings.minVersion = (3,1)
  connection.handshakeClientSRP("alice", "abra123cadabra", settings=settings)

Finally, every TLSConnection has a session object.  You can try to resume a
previous session by passing in the session object from the old session.  If
the server remembers this old session and supports resumption, the handshake
will finish more quickly.  Otherwise, the full handshake will be done.  For
example:

  connection.handshakeClientSRP("alice", "abra123cadabra")
  .
  .
  oldSession = connection.session
  connection2.handshakeClientSRP("alice", "abra123cadabra", session=
  oldSession)

5 Step 3 - call a handshake function (server)
----------------------------------------------
If you're a server, there's only one handshake function, but you can pass it
several different parameters, depending on which types of authentication
you're willing to perform.

To perform SRP authentication, you have to pass in a database of password
verifiers.  The VerifierDB class manages an in-memory or on-disk verifier
database.

  #On-disk database (use no-arg constructor if you want an in-memory DB)
  verifierDB = VerifierDB("./test/verifierDB")

  #Open the pre-existing database (can also 'create()' a new one)
  verifierDB.open()

  #Add to the database
  verifier = VerifierDB.makeVerifier("alice", "abra123cadabra", 2048)
  verifierDB["alice"] = verifier

  #Perform a handshake using the database
  connection.handshakeServer(verifierDB=verifierDB)

To perform shared key authentication, you have to pass in a database of shared
keys.  The SharedKeyDB class manages an in-memory or on-disk shared key
database.

  sharedKeyDB = SharedKeyDB("./test/sharedkeyDB")
  sharedKeyDB.open()
  sharedKeyDB["alice"] = "PaVBVZkYqAjCQCu6UBL2xgsnZhw"
  connection.handshakeServer(sharedKeyDB=sharedKeyDB)

To perform authentication with a certificate and private key, the server must
load these as described in the previous section, then pass them in.  If the
server sets the reqCert boolean to True, a certificate chain will be requested
from the client.

  connection.handshakeServer(certChain=certChain, privateKey=privateKey,
                             reqCert=True)

You can pass in any combination of a verifier database, a shared key database,
and a certificate chain/private key.  The client will use one of them to
authenticate.  In the case of SRP and a certificate chain/private key, they
both may be used.

You can also pass in a HandshakeSettings object, as described in the last
section, for finer control over handshaking details.  Finally, the server can
maintain a SessionCache, which will allow clients to use session resumption:

  sessionCache = SessionCache()
  connection.handshakeServer(verifierDB=verifierDB, sessionCache=sessionCache)

It should be noted that the session cache, and the verifier and shared key
databases, are all thread-safe.

5 Step 4 - check the results
-----------------------------
If the handshake completes without raising an exception, authentication
results will be stored in the connection's session object.  The following
variables will be populated if applicable, or else set to None:

  connection.session.srpUsername       #string
  connection.session.sharedKeyUsername #string
  connection.session.clientCertChain   #X509CertChain or
                                       #cryptoIDlib.CertChain.CertChain
  connection.session.serverCertChain   #X509CertChain or
                                       #cryptoIDlib.CertChain.CertChain

Both types of certificate chain object support the getFingerprint() function,
but with a difference.  X.509 objects return the end-entity fingerprint, and
ignore the other certificates.  CryptoID fingerprints (aka "cryptoIDs") are
based on the root cryptoID certificate, so you have to call validate() on the
CertChain to be sure you're really talking to the cryptoID.

X.509 certificate chain objects may also be validated against a list of
trusted root certificates.  See the API documentation for details.

To save yourself the trouble of inspecting fingerprints after the handshake,
you can pass a Checker object into the handshake function.  The checker will be
called if the handshake completes successfully.  If the other party's
certificate chain isn't approved by the checker, a subclass of
TLSAuthenticationError will be raised.  For example, to perform a handshake
with a server based on its X.509 fingerprint, do:

  try:
    checker = Checker(\
              x509Fingerprint='e049ff930af76d43ff4c658b268786f4df1296f2')
    connection.handshakeClientCert(checker=checker)
  except TLSAuthenticationError:
    print "Authentication failure"

If the handshake fails for any reason, an exception will be raised.  If the
socket timed out or was unexpectedly closed, a socket.error or
TLSAbruptCloseError will be raised.  Otherwise, either a TLSLocalAlert or
TLSRemoteAlert will be raised, depending on whether the local or remote
implementation signalled the error.  The exception object has a 'description'
member which identifies the error based on the codes in RFC 2246.  A
TLSLocalAlert also has a 'message' string that may have more details.

Example of handling a remote alert:

  try:
      [...]
  except TLSRemoteAlert, alert:
      if alert.description == AlertDescription.unknown_srp_username:
          print "Unknown user."
  [...]

Figuring out what went wrong based on the alert may require some
interpretation, particularly with remote alerts where you don't have an error
string, and where the remote implementation may not be signalling alerts
properly.  Many alerts signal an implementation error, and so should rarely be
seen in normal operation (unexpected_message, decode_error, illegal_parameter,
internal_error, etc.).

Others alerts are more likely to occur.  Below are some common alerts and
their probable causes, and whether they are signalled by the client or server.

Client bad_record_mac:
 - bad shared key password

Client handshake failure:
 - SRP parameters are not recognized by client

Client user_canceled:
 - The client might have returned None from an SRP callback.

Client insufficient_security:
 - SRP parameters are too small

Client protocol_version:
 - Client doesn't support the server's protocol version

Server protocol_version:
 - Server doesn't support the client's protocol version

Server bad_record_mac:
 - bad SRP username or password

Server unknown_srp_username
 - bad SRP username (bad_record_mac could be used for the same thing)

Server handshake_failure:
 - bad shared key username
 - no matching cipher suites

5 Step 5 - exchange data
-------------------------
Now that you have a connection, you can call read() and write() as if it were
a socket.SSL object.  You can also call send(), sendall(), recv(), and
makefile() as if it were a socket.  These calls may raise TLSLocalAlert,
TLSRemoteAlert, socket.error, or TLSAbruptCloseError, just like the handshake
functions.

Once the TLS connection is closed by the other side, calls to read() or recv()
will return an empty string.  If the socket is closed by the other side
without first closing the TLS connection, calls to read() or recv() will return
a TLSAbruptCloseError, and calls to write() or send() will return a
socket.error.

5 Step 6 - close the connection
--------------------------------
When you're finished sending data, you should call close() to close the
connection down.  When the connection is closed properly, the socket stays
open and can be used for exchanging non-secure data, the session object can be
used for session resumption, and the connection object can be re-used by
calling another handshake function.

If an exception is raised, the connection will be automatically closed; you
don't need to call close().  Furthermore, you will probably not be able to re-
use the socket, the connection object, or the session object, and you
shouldn't even try.

By default, calling close() will leave the socket open.  If you set the
connection's closeSocket flag to True, the connection will take ownership of
the socket, and close it when the connection is closed.


6 Using TLS Lite with httplib
==============================
TLS Lite comes with an HTTPTLSConnection class that extends httplib to work
over SSL/TLS connections.  Depending on how you construct it, it will do
different types of authentication.

  #No authentication whatsoever
  h = HTTPTLSConnection("www.amazon.com", 443)
  h.request("GET", "")
  r = h.getresponse()
  [...]

  #Authenticate server based on its X.509 fingerprint
  h = HTTPTLSConnection("www.amazon.com", 443,
          x509Fingerprint="e049ff930af76d43ff4c658b268786f4df1296f2")
  [...]

  #Authenticate server based on its X.509 chain (requires cryptlib_py [7])
  h = HTTPTLSConnection("www.amazon.com", 443,
          x509TrustList=[verisignCert],
          x509CommonName="www.amazon.com")
  [...]

  #Authenticate server based on its cryptoID
  h = HTTPTLSConnection("localhost", 443,
          cryptoID="dmqb6.fq345.cxk6g.5fha3")
  [...]

  #Mutually authenticate with SRP
  h = HTTPTLSConnection("localhost", 443,
          username="alice", password="abra123cadabra")
  [...]

  #Mutually authenticate with a shared key
  h = HTTPTLSConnection("localhost", 443,
          username="alice", sharedKey="PaVBVZkYqAjCQCu6UBL2xgsnZhw")
  [...]

  #Mutually authenticate with SRP, *AND* authenticate the server based
  #on its cryptoID
  h = HTTPTLSConnection("localhost", 443,
          username="alice", password="abra123cadabra",
          cryptoID="dmqb6.fq345.cxk6g.5fha3")
  [...]


7 Using TLS Lite with xmlrpclib
================================
TLS Lite comes with an XMLRPCTransport class that extends xmlrpclib to work
over SSL/TLS connections.  This class accepts the same parameters as
HTTPTLSConnection (see previous section), and behaves similarly.  Depending on
how you construct it, it will do different types of authentication.

  from tlslite.api import XMLRPCTransport
  from xmlrpclib import ServerProxy

  #No authentication whatsoever
  transport = XMLRPCTransport()
  server = ServerProxy("https://localhost", transport)
  server.someFunc(2, 3)
  [...]

  #Authenticate server based on its X.509 fingerprint
  transport = XMLRPCTransport(\
          x509Fingerprint="e049ff930af76d43ff4c658b268786f4df1296f2")  
  [...]


8 Using TLS Lite with poplib or imaplib
========================================
TLS Lite comes with POP3_TLS and IMAP4_TLS classes that extend poplib and
imaplib to work over SSL/TLS connections.  These classes can be constructed
with the same parameters as HTTPTLSConnection (see previous section), and 
behave similarly.

  #To connect to a POP3 server over SSL and display its fingerprint:
  from tlslite.api import *
  p = POP3_TLS("---------.net")
  print p.sock.session.serverCertChain.getFingerprint()
  [...]

  #To connect to an IMAP server once you know its fingerprint:
  from tlslite.api import *
  i = IMAP4_TLS("cyrus.andrew.cmu.edu",
          x509Fingerprint="00c14371227b3b677ddb9c4901e6f2aee18d3e45")
  [...]  
  

9 Using TLS Lite with smtplib
==============================
TLS Lite comes with an SMTP_TLS class that extends smtplib to work
over SSL/TLS connections.  This class accepts the same parameters as
HTTPTLSConnection (see previous section), and behaves similarly.  Depending 
on how you call starttls(), it will do different types of authentication.

  #To connect to an SMTP server once you know its fingerprint:
  from tlslite.api import *
  s = SMTP_TLS("----------.net")
  s.starttls(x509Fingerprint="7e39be84a2e3a7ad071752e3001d931bf82c32dc")
  [...]


10 Using TLS Lite with SocketServer
====================================
You can use TLS Lite to implement servers using Python's SocketServer
framework.  TLS Lite comes with a TLSSocketServerMixIn class.  You can combine
this with a TCPServer such as HTTPServer.  To combine them, define a new class
that inherits from both of them (with the mix-in first). Then implement the
handshake() method, doing some sort of server handshake on the connection
argument.  If the handshake method returns True, the RequestHandler will be
triggered.  Below is a complete example of a threaded HTTPS server.

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


11 Using TLS Lite with asyncore
================================
TLS Lite can be used with subclasses of asyncore.dispatcher.  See the comments
in TLSAsyncDispatcherMixIn.py for details.  This is still experimental, and
may not work with all asyncore.dispatcher subclasses.

Below is an example of combining Medusa's http_channel with
TLSAsyncDispatcherMixIn:

  class http_tls_channel(TLSAsyncDispatcherMixIn,
                         http_server.http_channel):
      ac_in_buffer_size = 16384

      def __init__ (self, server, conn, addr):
          http_server.http_channel.__init__(self, server, conn, addr)
          TLSAsyncDispatcherMixIn.__init__(self, conn)
          self.tlsConnection.ignoreAbruptClose = True
          self.setServerHandshakeOp(certChain=certChain,
                                    privateKey=privateKey)


12 Using TLS Lite with Twisted
===============================
TLS Lite can be used with Twisted protocols.  Below is a complete example of
using TLS Lite with a Twisted echo server.

There are two server implementations below.  Echo is the original protocol,
which is oblivious to TLS.  Echo1 subclasses Echo and negotiates TLS when the
client connects.  Echo2 subclasses Echo and negotiates TLS when the client
sends "STARTTLS".

  from twisted.internet.protocol import Protocol, Factory
  from twisted.internet import reactor
  from twisted.protocols.policies import WrappingFactory
  from twisted.protocols.basic import LineReceiver
  from twisted.python import log
  from twisted.python.failure import Failure
  import sys
  from tlslite.api import *

  s = open("./serverX509Cert.pem").read()
  x509 = X509()
  x509.parse(s)
  certChain = X509CertChain([x509])

  s = open("./serverX509Key.pem").read()
  privateKey = parsePEMKey(s, private=True)

  verifierDB = VerifierDB("verifierDB")
  verifierDB.open()

  class Echo(LineReceiver):
      def connectionMade(self):
          self.transport.write("Welcome to the echo server!\r\n")

      def lineReceived(self, line):
          self.transport.write(line + "\r\n")

  class Echo1(Echo):
      def connectionMade(self):
          if not self.transport.tlsStarted:
              self.transport.setServerHandshakeOp(certChain=certChain,
                                                  privateKey=privateKey,
                                                  verifierDB=verifierDB)
          else:
              Echo.connectionMade(self)

      def connectionLost(self, reason):
          pass #Handle any TLS exceptions here

  class Echo2(Echo):
      def lineReceived(self, data):
          if data == "STARTTLS":
              self.transport.setServerHandshakeOp(certChain=certChain,
                                                  privateKey=privateKey,
                                                  verifierDB=verifierDB)
          else:
              Echo.lineReceived(self, data)

      def connectionLost(self, reason):
          pass #Handle any TLS exceptions here

  factory = Factory()
  factory.protocol = Echo1
  #factory.protocol = Echo2

  wrappingFactory = WrappingFactory(factory)
  wrappingFactory.protocol = TLSTwistedProtocolWrapper

  log.startLogging(sys.stdout)
  reactor.listenTCP(1079, wrappingFactory)
  reactor.run()


13 Security Considerations
===========================
TLS Lite is beta-quality code.  It hasn't received much security analysis.
Use at your own risk.


14 History
===========
0.3.8 - 2/21/2005
 - Added support for poplib, imaplib, and smtplib
 - Added python 2.4 windows installer
 - Fixed occassional timing problems with test suite
0.3.7 - 10/05/2004
 - Added support for Python 2.2
 - Cleaned up compatibility code, and docs, a bit
0.3.6 - 9/28/2004
 - Fixed script installation on UNIX
 - Give better error message on old Python versions
0.3.5 - 9/16/2004
 - TLS 1.1 support
 - os.urandom() support
 - Fixed win32prng on some systems
0.3.4 - 9/12/2004
 - Updated for TLS/SRP draft 8
 - Bugfix: was setting _versioncheck on SRP 1st hello, causing problems
   with GnuTLS (which was offering TLS 1.1)
 - Removed _versioncheck checking, since it could cause interop problems
 - Minor bugfix: when cryptlib_py and and cryptoIDlib present, cryptlib
   was complaining about being initialized twice
0.3.3 - 6/10/2004
 - Updated for TLS/SRP draft 7
 - Updated test cryptoID cert chains for cryptoIDlib 0.3.1
0.3.2 - 5/21/2004
 - fixed bug when handling multiple handshake messages per record (e.g. IIS)
0.3.1 - 4/21/2004
 - added xmlrpclib integration
 - fixed hanging bug in Twisted integration
 - fixed win32prng to work on a wider range of win32 sytems
 - fixed import problem with cryptoIDlib
 - fixed port allocation problem when test scripts are run on some UNIXes
 - made tolerant of buggy IE sending wrong version in premaster secret
0.3.0 - 3/20/2004
 - added API docs thanks to epydoc
 - added X.509 path validation via cryptlib
 - much cleaning/tweaking/re-factoring/minor fixes
0.2.7 - 3/12/2004
 - changed Twisted error handling to use connectionLost()
 - added ignoreAbruptClose
0.2.6 - 3/11/2004
 - added Twisted errorHandler
 - added TLSAbruptCloseError
 - added 'integration' subdirectory
0.2.5 - 3/10/2004
 - improved asynchronous support a bit
 - added first-draft of Twisted support
0.2.4 - 3/5/2004
 - cleaned up asyncore support
 - added proof-of-concept for Twisted
0.2.3 - 3/4/2004
 - added pycrypto RSA support
 - added asyncore support
0.2.2 - 3/1/2004
 - added GMPY support
 - added pycrypto support
 - added support for PEM-encoded private keys, in pure python
0.2.1 - 2/23/2004
 - improved PRNG use (cryptlib, or /dev/random, or CryptoAPI)
 - added RSA blinding, to avoid timing attacks
 - don't install local copy of M2Crypto, too problematic
0.2.0 - 2/19/2004
 - changed VerifierDB to take per-user parameters
 - renamed tls_lite -> tlslite
0.1.9 - 2/16/2004
 - added post-handshake 'Checker'
 - made compatible with Python 2.2
 - made more forgiving of abrupt closure, since everyone does it:
   if the socket is closed while sending/recv'ing close_notify,
   just ignore it.
0.1.8 - 2/12/2004
 - TLSConnections now emulate sockets, including makefile()
 - HTTPTLSConnection and TLSMixIn simplified as a result
0.1.7 - 2/11/2004
 - fixed httplib.HTTPTLSConnection with multiple requests
 - fixed SocketServer to handle close_notify
 - changed handshakeClientNoAuth() to ignore CertificateRequests
 - changed handshakeClient() to ignore non-resumable session arguments
0.1.6 - 2/10/2004
 - fixed httplib support
0.1.5 - 2/09/2004
 - added support for httplib and SocketServer
 - added support for SSLv3
 - added support for 3DES
 - cleaned up read()/write() behavior
 - improved HMAC speed
0.1.4 - 2/06/2004
 - fixed dumb bug in tls.py
0.1.3 - 2/05/2004
 - change read() to only return requested number of bytes
 - added support for shared-key and in-memory databases
 - added support for PEM-encoded X.509 certificates
 - added support for SSLv2 ClientHello
 - fixed shutdown/re-handshaking behavior
 - cleaned up handling of missing_srp_username
 - renamed readString()/writeString() -> read()/write()
 - added documentation
0.1.2 - 2/04/2004
 - added clienttest/servertest functions
 - improved OpenSSL cipher wrappers speed
 - fixed server when it has a key, but client selects plain SRP
 - fixed server to postpone errors until it has read client's messages
 - fixed ServerHello to only include extension data if necessary
0.1.1 - 2/02/2004
 - fixed close_notify behavior
 - fixed handling of empty application data packets
 - fixed socket reads to not consume extra bytes
 - added testing functions to tls.py
0.1.0 - 2/01/2004
 - first release


15 References
==============
[0] http://www.ietf.org/html.charters/tls-charter.html
[1] http://www.trevp.net/tls_srp/draft-ietf-tls-srp-07.html
[2] http://www.ietf.org/internet-drafts/draft-ietf-tls-sharedkeys-02.txt
[3] http://www.trevp.net/cryptoID/
[4] http://www.openssl.org/
[5] http://www.cs.auckland.ac.nz/~pgut001/cryptlib/
[6] http://sandbox.rulemaker.net/ngps/m2/
[7] http://trevp.net/cryptlibConverter/
[8] http://www.trevp.net/cryptoID/
[9] http://www.amk.ca/python/code/crypto.html
[10] http://gmpy.sourceforge.net/

























