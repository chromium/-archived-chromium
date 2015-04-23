@ECHO OFF
openssl.exe req -config openssl.cnf -new -x509 -keyout server.pem -out server.pem -days 365 -nodes
ECHO.
ECHO Done generating self-signed certificate.
ECHO Press any key to continue...
PAUSE >NUL
EXIT