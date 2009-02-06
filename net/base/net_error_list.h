// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the list of network errors.

// An asynchronous IO operation is not yet complete.  This usually does not
// indicate a fatal error.  Typically this error will be generated as a
// notification to wait for some external notification that the IO operation
// finally completed.
NET_ERROR(IO_PENDING, -1)

// A generic failure occured.
NET_ERROR(FAILED, -2)

// An operation was aborted (due to user action).
NET_ERROR(ABORTED, -3)

// An argument to the function is incorrect.
NET_ERROR(INVALID_ARGUMENT, -4)

// The handle or file descriptor is invalid.
NET_ERROR(INVALID_HANDLE, -5)

// The file or directory cannot be found.
NET_ERROR(FILE_NOT_FOUND, -6)

// An operation timed out.
NET_ERROR(TIMED_OUT, -7)

// The file is too large.
NET_ERROR(FILE_TOO_BIG, -8)

// An unexpected error.  This may be caused by a programming mistake or an
// invalid assumption.
NET_ERROR(UNEXPECTED, -9)

// Permission to access a resource was denied.
NET_ERROR(ACCESS_DENIED, -10)

// The operation failed because of unimplemented functionality.
NET_ERROR(NOT_IMPLEMENTED, -11)

// There were not enough resources to complete the operation.
NET_ERROR(INSUFFICIENT_RESOURCES, -12)

// A connection was closed (corresponding to a TCP FIN).
NET_ERROR(CONNECTION_CLOSED, -100)

// A connection was reset (corresponding to a TCP RST).
NET_ERROR(CONNECTION_RESET, -101)

// A connection attempt was refused.
NET_ERROR(CONNECTION_REFUSED, -102)

// A connection timed out as a result of not receiving an ACK for data sent.
// This can include a FIN packet that did not get ACK'd.
NET_ERROR(CONNECTION_ABORTED, -103)

// A connection attempt failed.
NET_ERROR(CONNECTION_FAILED, -104)

// The host name could not be resolved.
NET_ERROR(NAME_NOT_RESOLVED, -105)

// The Internet connection has been lost.
NET_ERROR(INTERNET_DISCONNECTED, -106)

// An SSL protocol error occurred.
NET_ERROR(SSL_PROTOCOL_ERROR, -107)

// The IP address or port number is invalid (e.g., cannot connect to the IP
// address 0 or the port 0).
NET_ERROR(ADDRESS_INVALID, -108)

// The IP address is unreachable.  This usually means that there is no route to
// the specified host or network.
NET_ERROR(ADDRESS_UNREACHABLE, -109)

// The server requested a client certificate for SSL client authentication.
NET_ERROR(SSL_CLIENT_AUTH_CERT_NEEDED, -110)

// A tunnel connection through the proxy could not be established.
NET_ERROR(TUNNEL_CONNECTION_FAILED, -111)

// No SSL protocol versions are enabled.
NET_ERROR(NO_SSL_VERSIONS_ENABLED, -112)

// The client and server don't support a common SSL protocol version or
// cipher suite.
NET_ERROR(SSL_VERSION_OR_CIPHER_MISMATCH, -113)

// The server requested a renegotiation (rehandshake).
NET_ERROR(SSL_RENEGOTIATION_REQUESTED, -114)

// Certificate error codes
//
// The values of certificate error codes must be consecutive.

// The server responded with a certificate whose common name did not match
// the host name.  This could mean:
//
// 1. An attacker has redirected our traffic to his server and is
//    presenting a certificate for which he knows the private key.
//
// 2. The server is misconfigured and responding with the wrong cert.
//
// 3. The user is on a wireless network and is being redirected to the
//    network's login page.
//
// 4. The OS has used a DNS search suffix and the server doesn't have
//    a certificate for the abbreviated name in the address bar.
//
NET_ERROR(CERT_COMMON_NAME_INVALID, -200)

// The server responded with a certificate that, by our clock, appears to
// either not yet be valid or to have expired.  This could mean:
//
// 1. An attacker is presenting an old certificate for which he has
//    managed to obtain the private key.
//
// 2. The server is misconfigured and is not presenting a valid cert.
//
// 3. Our clock is wrong.
//
NET_ERROR(CERT_DATE_INVALID, -201)

// The server responded with a certificate that is signed by an authority
// we don't trust.  The could mean:
//
// 1. An attacker has substituted the real certificate for a cert that
//    contains his public key and is signed by his cousin.
//
// 2. The server operator has a legitimate certificate from a CA we don't
//    know about, but should trust.
//
// 3. The server is presenting a self-signed certificate, providing no
//    defense against active attackers (but foiling passive attackers).
//
NET_ERROR(CERT_AUTHORITY_INVALID, -202)

// The server responded with a certificate that contains errors.
// This error is not recoverable.
//
// MSDN describes this error as follows:
//   "The SSL certificate contains errors."
//
NET_ERROR(CERT_CONTAINS_ERRORS, -203)

// The certificate has no mechanism for determining if it is revoked.  In
// effect, this certificate cannot be revoked.
NET_ERROR(CERT_NO_REVOCATION_MECHANISM, -204)

// Revocation information for the security certificate for this site is not
// available.  This could mean:
//
// 1. An attacker has compromised the private key in the certificate and is
//    blocking our attempt to find out that the cert was revoked.
//
// 2. The certificate is unrevoked, but the revocation server is busy or
//    unavailable.
//
NET_ERROR(CERT_UNABLE_TO_CHECK_REVOCATION, -205)

// The server responded with a certificate has been revoked.
// We have the capability to ignore this error, but it is probably not the
// thing to do.
NET_ERROR(CERT_REVOKED, -206)

// The server responded with a certificate that is invalid.
// This error is not recoverable.
//
// MSDN describes this error as follows:
//   "The SSL certificate is invalid."
//
NET_ERROR(CERT_INVALID, -207)

// Add new certificate error codes here.
//
// Update the value of CERT_END whenever you add a new certificate error
// code.

// The value immediately past the last certificate error code.
NET_ERROR(CERT_END, -208)

// The URL is invalid.
NET_ERROR(INVALID_URL, -300)

// The scheme of the URL is disallowed.
NET_ERROR(DISALLOWED_URL_SCHEME, -301)

// The scheme of the URL is unknown.
NET_ERROR(UNKNOWN_URL_SCHEME, -302)

// Attempting to load an URL resulted in too many redirects.
NET_ERROR(TOO_MANY_REDIRECTS, -310)

// Attempting to load an URL resulted in an unsafe redirect (e.g., a redirect
// to file:// is considered unsafe).
NET_ERROR(UNSAFE_REDIRECT, -311)

// Attempting to load an URL with an unsafe port number.  These are port
// numbers that correspond to services, which are not robust to spurious input
// that may be constructed as a result of an allowed web construct (e.g., HTTP
// looks a lot like SMTP, so form submission to port 25 is denied).
NET_ERROR(UNSAFE_PORT, -312)

// The server's response was invalid.
NET_ERROR(INVALID_RESPONSE, -320)

// Error in chunked transfer encoding.
NET_ERROR(INVALID_CHUNKED_ENCODING, -321)

// The server did not support the request method.
NET_ERROR(METHOD_NOT_SUPPORTED, -322)

// The response was 407 (Proxy Authentication Required), yet we did not send
// the request to a proxy.
NET_ERROR(UNEXPECTED_PROXY_AUTH, -323)

// The server closed the connection without sending any data.
NET_ERROR(EMPTY_RESPONSE, -324)

// The headers section of the response is too large.
NET_ERROR(RESPONSE_HEADERS_TOO_BIG, -325)
    
// The PAC requested by HTTP did not have a valid status code (non-200).
NET_ERROR(PAC_STATUS_NOT_OK, -326)

// The cache does not have the requested entry.
NET_ERROR(CACHE_MISS, -400)

// The server's response was insecure (e.g. there was a cert error).
NET_ERROR(INSECURE_RESPONSE, -501)

