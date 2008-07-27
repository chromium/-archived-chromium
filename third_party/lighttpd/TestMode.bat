@ECHO OFF
ECHO LightTPD Test mode (without log)
ECHO Press 'CTRL + C' to exit.
ECHO.
ECHO LightTPD Output:
ECHO ----------------
START /B lighttpd.exe -f conf\lighttpd-inc.conf -m lib -D
PAUSE >NUL && EXIT