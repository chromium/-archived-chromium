
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
