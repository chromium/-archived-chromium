#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a simple HTTP server used for testing Chrome.

It supports several test URLs, as specified by the handlers in TestPageHandler.
It defaults to living on localhost:8888.
It can use https if you specify the flag --https=CERT where CERT is the path
to a pem file containing the certificate and private key that should be used.
To shut it down properly, visit localhost:8888/kill.
"""

import base64
import BaseHTTPServer
import cgi
import md5
import optparse
import os
import re
import SocketServer
import sys
import time
import tlslite
import tlslite.api
import pyftpdlib.ftpserver

SERVER_HTTP = 0 
SERVER_FTP = 1

debug_output = sys.stderr
def debug(str):
  debug_output.write(str + "\n")
  debug_output.flush()

class StoppableHTTPServer(BaseHTTPServer.HTTPServer):
  """This is a specialization of of BaseHTTPServer to allow it
  to be exited cleanly (by setting its "stop" member to True)."""

  def serve_forever(self):
    self.stop = False
    self.nonce = None
    while not self.stop:
      self.handle_request()
    self.socket.close()

class HTTPSServer(tlslite.api.TLSSocketServerMixIn, StoppableHTTPServer):
  """This is a specialization of StoppableHTTPerver that add https support."""

  def __init__(self, server_address, request_hander_class, cert_path):
    s = open(cert_path).read()
    x509 = tlslite.api.X509()
    x509.parse(s)
    self.cert_chain = tlslite.api.X509CertChain([x509])
    s = open(cert_path).read()
    self.private_key = tlslite.api.parsePEMKey(s, private=True)

    self.session_cache = tlslite.api.SessionCache()
    StoppableHTTPServer.__init__(self, server_address, request_hander_class)

  def handshake(self, tlsConnection):
    """Creates the SSL connection."""
    try:
      tlsConnection.handshakeServer(certChain=self.cert_chain,
                                    privateKey=self.private_key,
                                    sessionCache=self.session_cache)
      tlsConnection.ignoreAbruptClose = True
      return True
    except tlslite.api.TLSError, error:
      print "Handshake failure:", str(error)
      return False

class TestPageHandler(BaseHTTPServer.BaseHTTPRequestHandler):

  def __init__(self, request, client_address, socket_server):
    self._get_handlers = [
      self.KillHandler,
      self.NoCacheMaxAgeTimeHandler,
      self.NoCacheTimeHandler,
      self.CacheTimeHandler,
      self.CacheExpiresHandler,
      self.CacheProxyRevalidateHandler,
      self.CachePrivateHandler,
      self.CachePublicHandler,
      self.CacheSMaxAgeHandler,
      self.CacheMustRevalidateHandler,
      self.CacheMustRevalidateMaxAgeHandler,
      self.CacheNoStoreHandler,
      self.CacheNoStoreMaxAgeHandler,
      self.CacheNoTransformHandler,
      self.DownloadHandler,
      self.DownloadFinishHandler,
      self.EchoHeader,
      self.EchoAllHandler,
      self.FileHandler,
      self.RealFileWithCommonHeaderHandler,
      self.RealBZ2FileWithCommonHeaderHandler,
      self.AuthBasicHandler,
      self.AuthDigestHandler,
      self.SlowServerHandler,
      self.ContentTypeHandler,
      self.ServerRedirectHandler,
      self.ClientRedirectHandler,
      self.DefaultResponseHandler]
    self._post_handlers = [
      self.EchoTitleHandler,
      self.EchoAllHandler,
      self.EchoHandler] + self._get_handlers

    self._mime_types = { 'gif': 'image/gif',  'jpeg' : 'image/jpeg', 'jpg' : 'image/jpeg' }
    self._default_mime_type = 'text/html'

    BaseHTTPServer.BaseHTTPRequestHandler.__init__(self, request, client_address, socket_server)

  def GetMIMETypeFromName(self, file_name):
    """Returns the mime type for the specified file_name. So far it only looks
    at the file extension."""

    (shortname, extension) = os.path.splitext(file_name)
    if len(extension) == 0:
      # no extension.
      return self._default_mime_type

    return self._mime_types.get(extension, self._default_mime_type)

  def KillHandler(self):
    """This request handler kills the server, for use when we're done"
    with the a particular test."""

    if (self.path.find("kill") < 0):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=0')
    self.end_headers()
    self.wfile.write("Time to die")
    self.server.stop = True

    return True

  def NoCacheMaxAgeTimeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and no caching requested."""

    if (self.path.find("/nocachetime/maxage") != 0):
      return False

    self.send_response(200)
    self.send_header('Cache-Control', 'max-age=0')
    self.send_header('Content-type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True

  def NoCacheTimeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and no caching requested."""

    if (self.path.find("/nocachetime") != 0):
      return False

    self.send_response(200)
    self.send_header('Cache-Control', 'no-cache')
    self.send_header('Content-type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True

  def CacheTimeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for one minute."""

    if self.path.find("/cachetime") != 0:
      return False

    self.send_response(200)
    self.send_header('Cache-Control', 'max-age=60')
    self.send_header('Content-type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True

  def CacheExpiresHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and set the page to expire on 1 Jan 2099."""

    if (self.path.find("/cache/expires") != 0):
      return False

    self.send_response(200)
    self.send_header('Expires', 'Thu, 1 Jan 2099 00:00:00 GMT')
    self.send_header('Content-type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True

  def CacheProxyRevalidateHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for 60 seconds"""

    if (self.path.find("/cache/proxy-revalidate") != 0):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=60, proxy-revalidate')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True

  def CachePrivateHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for 5 seconds."""

    if (self.path.find("/cache/private") != 0):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=5, private')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True

  def CachePublicHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for 5 seconds."""

    if (self.path.find("/cache/public") != 0):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=5, public')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True

  def CacheSMaxAgeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow for caching."""

    if (self.path.find("/cache/s-maxage") != 0):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'public, s-maxage = 60, max-age = 0')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True

  def CacheMustRevalidateHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow caching."""

    if (self.path.find("/cache/must-revalidate") != 0):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'must-revalidate')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True

  def CacheMustRevalidateMaxAgeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow caching event though max-age of 60
    seconds is specified."""

    if (self.path.find("/cache/must-revalidate/max-age") != 0):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=60, must-revalidate')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True


  def CacheNoStoreHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow the page to be stored."""

    if (self.path.find("/cache/no-store") != 0):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'no-store')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True

  def CacheNoStoreMaxAgeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow the page to be stored even though max-age
    of 60 seconds is specified."""

    if (self.path.find("/cache/no-store/max-age") != 0):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=60, no-store')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True


  def CacheNoTransformHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow the content to transformed during
    user-agent caching"""

    if (self.path.find("/cache/no-transform") != 0):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'no-transform')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' % time.time())

    return True

  def EchoHeader(self):
    """This handler echoes back the value of a specific request header."""

    if self.path.find("/echoheader") != 0:
      return False

    query_char = self.path.find('?')
    if query_char != -1:
      header_name = self.path[query_char+1:]

    self.send_response(200)
    self.send_header('Content-type', 'text/plain')
    self.send_header('Cache-control', 'max-age=60000')
    # insert a vary header to properly indicate that the cachability of this
    # request is subject to value of the request header being echoed.
    if len(header_name) > 0:
      self.send_header('Vary', header_name)
    self.end_headers()

    if len(header_name) > 0:
      self.wfile.write(self.headers.getheader(header_name))

    return True

  def EchoHandler(self):
    """This handler just echoes back the payload of the request, for testing
    form submission."""

    if self.path.find("/echo") != 0:
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    length = int(self.headers.getheader('content-length'))
    request = self.rfile.read(length)
    self.wfile.write(request)
    return True

  def EchoTitleHandler(self):
    """This handler is like Echo, but sets the page title to the request."""

    if self.path.find("/echotitle") != 0:
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    length = int(self.headers.getheader('content-length'))
    request = self.rfile.read(length)
    self.wfile.write('<html><head><title>')
    self.wfile.write(request)
    self.wfile.write('</title></head></html>')
    return True

  def EchoAllHandler(self):
    """This handler yields a (more) human-readable page listing information
    about the request header & contents."""

    if self.path.find("/echoall") != 0:
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head><style>'
      'pre { border: 1px solid black; margin: 5px; padding: 5px }'
      '</style></head><body>'
      '<div style="float: right">'
      '<a href="http://localhost:8888/echo">back to referring page</a></div>'
      '<h1>Request Body:</h1><pre>')

    if self.command == 'POST':
      length = int(self.headers.getheader('content-length'))
      qs = self.rfile.read(length)
      params = cgi.parse_qs(qs, keep_blank_values=1)

      for param in params:
        self.wfile.write('%s=%s\n' % (param, params[param][0]))

    self.wfile.write('</pre>')

    self.wfile.write('<h1>Request Headers:</h1><pre>%s</pre>' % self.headers)

    self.wfile.write('</body></html>')
    return True

  def DownloadHandler(self):
    """This handler sends a downloadable file with or without reporting
    the size (6K)."""

    if self.path.startswith("/download-unknown-size"):
      send_length = False
    elif self.path.startswith("/download-known-size"):
      send_length = True
    else:
      return False

    #
    # The test which uses this functionality is attempting to send
    # small chunks of data to the client.  Use a fairly large buffer
    # so that we'll fill chrome's IO buffer enough to force it to
    # actually write the data.
    # See also the comments in the client-side of this test in
    # download_uitest.cc
    #
    size_chunk1 = 35*1024
    size_chunk2 = 10*1024

    self.send_response(200)
    self.send_header('Content-type', 'application/octet-stream')
    self.send_header('Cache-Control', 'max-age=0')
    if send_length:
      self.send_header('Content-Length', size_chunk1 + size_chunk2)
    self.end_headers()

    # First chunk of data:
    self.wfile.write("*" * size_chunk1)
    self.wfile.flush()

    # handle requests until one of them clears this flag.
    self.server.waitForDownload = True
    while self.server.waitForDownload:
      self.server.handle_request()

    # Second chunk of data:
    self.wfile.write("*" * size_chunk2)
    return True

  def DownloadFinishHandler(self):
    """This handler just tells the server to finish the current download."""

    if not self.path.startswith("/download-finish"):
      return False

    self.server.waitForDownload = False
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=0')
    self.end_headers()
    return True

  def FileHandler(self):
    """This handler sends the contents of the requested file.  Wow, it's like
    a real webserver!"""

    prefix='/files/'
    if not self.path.startswith(prefix):
      return False

    file = self.path[len(prefix):]
    entries = file.split('/');
    path = os.path.join(self.server.data_dir, *entries)

    if not os.path.isfile(path):
      print "File not found " + file + " full path:" + path
      self.send_error(404)
      return True

    f = open(path, "rb")
    data = f.read()
    f.close()

    # If file.mock-http-headers exists, it contains the headers we
    # should send.  Read them in and parse them.
    headers_path = path + '.mock-http-headers'
    if os.path.isfile(headers_path):
      f = open(headers_path, "r")

      # "HTTP/1.1 200 OK"
      response = f.readline()
      status_code = re.findall('HTTP/\d+.\d+ (\d+)', response)[0]
      self.send_response(int(status_code))

      for line in f:
        # "name: value"
        name, value = re.findall('(\S+):\s*(.*)', line)[0]
        self.send_header(name, value)
      f.close()
    else:
      # Could be more generic once we support mime-type sniffing, but for
      # now we need to set it explicitly.
      self.send_response(200)
      self.send_header('Content-type', self.GetMIMETypeFromName(file))
      self.send_header('Content-Length', len(data))
    self.end_headers()

    self.wfile.write(data)

    return True

  def RealFileWithCommonHeaderHandler(self):
    """This handler sends the contents of the requested file without the pseudo
    http head!"""

    prefix='/realfiles/'
    if not self.path.startswith(prefix):
      return False

    file = self.path[len(prefix):]
    path = os.path.join(self.server.data_dir, file)

    try:
      f = open(path, "rb")
      data = f.read()
      f.close()

      # just simply set the MIME as octal stream
      self.send_response(200)
      self.send_header('Content-type', 'application/octet-stream')
      self.end_headers()

      self.wfile.write(data)
    except:
      self.send_error(404)

    return True

  def RealBZ2FileWithCommonHeaderHandler(self):
    """This handler sends the bzip2 contents of the requested file with
     corresponding Content-Encoding field in http head!"""

    prefix='/realbz2files/'
    if not self.path.startswith(prefix):
      return False

    parts = self.path.split('?')
    file = parts[0][len(prefix):]
    path = os.path.join(self.server.data_dir, file) + '.bz2'

    if len(parts) > 1:
      options = parts[1]
    else:
      options = ''

    try:
      self.send_response(200)
      accept_encoding = self.headers.get("Accept-Encoding")
      if accept_encoding.find("bzip2") != -1:
        f = open(path, "rb")
        data = f.read()
        f.close()
        self.send_header('Content-Encoding', 'bzip2')
        self.send_header('Content-type', 'application/x-bzip2')
        self.end_headers()
        if options == 'incremental-header':
          self.wfile.write(data[:1])
          self.wfile.flush()
          time.sleep(1.0)
          self.wfile.write(data[1:])
        else:
          self.wfile.write(data)
      else:
        """client do not support bzip2 format, send pseudo content
        """
        self.send_header('Content-type', 'text/html; charset=ISO-8859-1')
        self.end_headers()
        self.wfile.write("you do not support bzip2 encoding")
    except:
      self.send_error(404)

    return True

  def AuthBasicHandler(self):
    """This handler tests 'Basic' authentication.  It just sends a page with
    title 'user/pass' if you succeed."""

    if not self.path.startswith("/auth-basic"):
      return False

    username = userpass = password = b64str = ""

    auth = self.headers.getheader('authorization')
    try:
      if not auth:
        raise Exception('no auth')
      b64str = re.findall(r'Basic (\S+)', auth)[0]
      userpass = base64.b64decode(b64str)
      username, password = re.findall(r'([^:]+):(\S+)', userpass)[0]
      if password != 'secret':
        raise Exception('wrong password')
    except Exception, e:
      # Authentication failed.
      self.send_response(401)
      self.send_header('WWW-Authenticate', 'Basic realm="testrealm"')
      self.send_header('Content-type', 'text/html')
      self.end_headers()
      self.wfile.write('<html><head>')
      self.wfile.write('<title>Denied: %s</title>' % e)
      self.wfile.write('</head><body>')
      self.wfile.write('auth=%s<p>' % auth)
      self.wfile.write('b64str=%s<p>' % b64str)
      self.wfile.write('username: %s<p>' % username)
      self.wfile.write('userpass: %s<p>' % userpass)
      self.wfile.write('password: %s<p>' % password)
      self.wfile.write('You sent:<br>%s<p>' % self.headers)
      self.wfile.write('</body></html>')
      return True

    # Authentication successful.  (Return a cachable response to allow for
    # testing cached pages that require authentication.)
    if_none_match = self.headers.getheader('if-none-match')
    if if_none_match == "abc":
      self.send_response(304)
      self.end_headers()
    else:
      self.send_response(200)
      self.send_header('Content-type', 'text/html')
      self.send_header('Cache-control', 'max-age=60000')
      self.send_header('Etag', 'abc')
      self.end_headers()
      self.wfile.write('<html><head>')
      self.wfile.write('<title>%s/%s</title>' % (username, password))
      self.wfile.write('</head><body>')
      self.wfile.write('auth=%s<p>' % auth)
      self.wfile.write('</body></html>')

    return True

  def AuthDigestHandler(self):
    """This handler tests 'Digest' authentication.  It just sends a page with
    title 'user/pass' if you succeed."""

    if not self.path.startswith("/auth-digest"):
      return False

    # Periodically generate a new nonce.  Technically we should incorporate
    # the request URL into this, but we don't care for testing.
    nonce_life = 10
    stale = False
    if not self.server.nonce or (time.time() - self.server.nonce_time > nonce_life):
      if self.server.nonce:
        stale = True
      self.server.nonce_time = time.time()
      self.server.nonce = \
          md5.new(time.ctime(self.server.nonce_time) + 'privatekey').hexdigest()

    nonce = self.server.nonce
    opaque = md5.new('opaque').hexdigest()
    password = 'secret'
    realm = 'testrealm'

    auth = self.headers.getheader('authorization')
    pairs = {}
    try:
      if not auth:
        raise Exception('no auth')
      if not auth.startswith('Digest'):
        raise Exception('not digest')
      # Pull out all the name="value" pairs as a dictionary.
      pairs = dict(re.findall(r'(\b[^ ,=]+)="?([^",]+)"?', auth))

      # Make sure it's all valid.
      if pairs['nonce'] != nonce:
        raise Exception('wrong nonce')
      if pairs['opaque'] != opaque:
        raise Exception('wrong opaque')

      # Check the 'response' value and make sure it matches our magic hash.
      # See http://www.ietf.org/rfc/rfc2617.txt
      hash_a1 = md5.new(':'.join([pairs['username'], realm, password])).hexdigest()
      hash_a2 = md5.new(':'.join([self.command, pairs['uri']])).hexdigest()
      if 'qop' in pairs and 'nc' in pairs and 'cnonce' in pairs:
        response = md5.new(':'.join([hash_a1, nonce, pairs['nc'],
            pairs['cnonce'], pairs['qop'], hash_a2])).hexdigest()
      else:
        response = md5.new(':'.join([hash_a1, nonce, hash_a2])).hexdigest()

      if pairs['response'] != response:
        raise Exception('wrong password')
    except Exception, e:
      # Authentication failed.
      self.send_response(401)
      hdr = ('Digest '
             'realm="%s", '
             'domain="/", '
             'qop="auth", '
             'algorithm=MD5, '
             'nonce="%s", '
             'opaque="%s"') % (realm, nonce, opaque)
      if stale:
        hdr += ', stale="TRUE"'
      self.send_header('WWW-Authenticate', hdr)
      self.send_header('Content-type', 'text/html')
      self.end_headers()
      self.wfile.write('<html><head>')
      self.wfile.write('<title>Denied: %s</title>' % e)
      self.wfile.write('</head><body>')
      self.wfile.write('auth=%s<p>' % auth)
      self.wfile.write('pairs=%s<p>' % pairs)
      self.wfile.write('You sent:<br>%s<p>' % self.headers)
      self.wfile.write('We are replying:<br>%s<p>' % hdr)
      self.wfile.write('</body></html>')
      return True

    # Authentication successful.
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head>')
    self.wfile.write('<title>%s/%s</title>' % (pairs['username'], password))
    self.wfile.write('</head><body>')
    self.wfile.write('auth=%s<p>' % auth)
    self.wfile.write('pairs=%s<p>' % pairs)
    self.wfile.write('</body></html>')

    return True

  def SlowServerHandler(self):
    """Wait for the user suggested time before responding. The syntax is
    /slow?0.5 to wait for half a second."""
    if not self.path.startswith("/slow"):
      return False
    query_char = self.path.find('?')
    wait_sec = 1.0
    if query_char >= 0:
      try:
        wait_sec = int(self.path[query_char + 1:])
      except ValueError:
        pass
    time.sleep(wait_sec)
    self.send_response(200)
    self.send_header('Content-type', 'text/plain')
    self.end_headers()
    self.wfile.write("waited %d seconds" % wait_sec)
    return True

  def ContentTypeHandler(self):
    """Returns a string of html with the given content type.  E.g.,
    /contenttype?text/css returns an html file with the Content-Type
    header set to text/css."""
    if not self.path.startswith('/contenttype'):
      return False
    query_char = self.path.find('?')
    content_type = self.path[query_char + 1:].strip()
    if not content_type:
      content_type = 'text/html'
    self.send_response(200)
    self.send_header('Content-Type', content_type)
    self.end_headers()
    self.wfile.write("<html>\n<body>\n<p>HTML text</p>\n</body>\n</html>\n");
    return True

  def ServerRedirectHandler(self):
    """Sends a server redirect to the given URL. The syntax is
    '/server-redirect?http://foo.bar/asdf' to redirect to 'http://foo.bar/asdf'"""

    test_name = "/server-redirect"
    if not self.path.startswith(test_name):
      return False

    query_char = self.path.find('?')
    if query_char < 0 or len(self.path) <= query_char + 1:
      self.sendRedirectHelp(test_name)
      return True
    dest = self.path[query_char + 1:]

    self.send_response(301)  # moved permanently
    self.send_header('Location', dest)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head>')
    self.wfile.write('</head><body>Redirecting to %s</body></html>' % dest)

    return True;

  def ClientRedirectHandler(self):
    """Sends a client redirect to the given URL. The syntax is
    '/client-redirect?http://foo.bar/asdf' to redirect to 'http://foo.bar/asdf'"""

    test_name = "/client-redirect"
    if not self.path.startswith(test_name):
      return False

    query_char = self.path.find('?');
    if query_char < 0 or len(self.path) <= query_char + 1:
      self.sendRedirectHelp(test_name)
      return True
    dest = self.path[query_char + 1:]

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head>')
    self.wfile.write('<meta http-equiv="refresh" content="0;url=%s">' % dest)
    self.wfile.write('</head><body>Redirecting to %s</body></html>' % dest)

    return True

  def DefaultResponseHandler(self):
    """This is the catch-all response handler for requests that aren't handled
    by one of the special handlers above.
    Note that we specify the content-length as without it the https connection
    is not closed properly (and the browser keeps expecting data)."""

    contents = "Default response given for path: " + self.path
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header("Content-Length", len(contents))
    self.end_headers()
    self.wfile.write(contents)
    return True

  def do_GET(self):
    for handler in self._get_handlers:
      if handler():
        return

  def do_POST(self):
    for handler in self._post_handlers:
      if handler():
        return

  # called by the redirect handling function when there is no parameter
  def sendRedirectHelp(self, redirect_name):
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><body><h1>Error: no redirect destination</h1>')
    self.wfile.write('Use <pre>%s?http://dest...</pre>' % redirect_name)
    self.wfile.write('</body></html>')

def MakeDataDir():
  if options.data_dir:
    if not os.path.isdir(options.data_dir):
      print 'specified data dir not found: ' + options.data_dir + ' exiting...'
      return None
    my_data_dir = options.data_dir
  else:
    # Create the default path to our data dir, relative to the exe dir.
    my_data_dir = os.path.dirname(sys.argv[0])
    my_data_dir = os.path.join(my_data_dir, "..", "..", "..", "..",
                                   "test", "data")

    #TODO(ibrar): Must use Find* funtion defined in google\tools
    #i.e my_data_dir = FindUpward(my_data_dir, "test", "data")

  return my_data_dir

def main(options, args):
  # redirect output to a log file so it doesn't spam the unit test output
  logfile = open('testserver.log', 'w')
  sys.stderr = sys.stdout = logfile

  port = options.port

  if options.server_type == SERVER_HTTP:
    if options.cert:
      # let's make sure the cert file exists.
      if not os.path.isfile(options.cert):
        print 'specified cert file not found: ' + options.cert + ' exiting...'
        return
      server = HTTPSServer(('127.0.0.1', port), TestPageHandler, options.cert)
      print 'HTTPS server started on port %d...' % port
    else:
      server = StoppableHTTPServer(('127.0.0.1', port), TestPageHandler)
      print 'HTTP server started on port %d...' % port

    server.data_dir = MakeDataDir()

  # means FTP Server
  else:
    my_data_dir = MakeDataDir()

    def line_logger(msg):
      if (msg.find("kill") >= 0):
        server.stop = True
        print 'shutting down server'
        sys.exit(0)

    # Instantiate a dummy authorizer for managing 'virtual' users
    authorizer = pyftpdlib.ftpserver.DummyAuthorizer()

    # Define a new user having full r/w permissions and a read-only
    # anonymous user
    authorizer.add_user('chrome', 'chrome', my_data_dir, perm='elradfmw')

    authorizer.add_anonymous(my_data_dir)

    # Instantiate FTP handler class
    ftp_handler = pyftpdlib.ftpserver.FTPHandler
    ftp_handler.authorizer = authorizer
    pyftpdlib.ftpserver.logline = line_logger

    # Define a customized banner (string returned when client connects)
    ftp_handler.banner = "pyftpdlib %s based ftpd ready." % pyftpdlib.ftpserver.__ver__

    # Instantiate FTP server class and listen to 127.0.0.1:port
    address = ('127.0.0.1', port)
    server = pyftpdlib.ftpserver.FTPServer(address, ftp_handler)
    print 'FTP server started on port %d...' % port

  try:
    server.serve_forever()
  except KeyboardInterrupt:
    print 'shutting down server'
    server.stop = True

if __name__ == '__main__':
  option_parser = optparse.OptionParser()
  option_parser.add_option("-f", '--ftp', action='store_const',
                           const=SERVER_FTP, default=SERVER_HTTP,
                           dest='server_type',
                           help='FTP or HTTP server default HTTP')
  option_parser.add_option('', '--port', default='8888', type='int',
                           help='Port used by the server')
  option_parser.add_option('', '--data-dir', dest='data_dir',
                           help='Directory from which to read the files')
  option_parser.add_option('', '--https', dest='cert',
                           help='Specify that https should be used, specify '
                           'the path to the cert containing the private key '
                           'the server should use')
  options, args = option_parser.parse_args()

  sys.exit(main(options, args))
  