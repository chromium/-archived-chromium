# Chrome Buildbot slave configuration

import os
import sys

from twisted.application import service
from buildbot.slave.bot import BuildSlave

import chromium_config as config

slavename = None
password = config.Master.GetBotPassword()
host = None
port = None
basedir = None
keepalive = 600
usepty = 1
umask = None


if slavename is None:
    msg = '*** No slavename configured in %s.\n' % repr(__file__)
    sys.stderr.write(msg)
    sys.exit(1)

if password is None:
    msg = '*** No password configured in %s.\n' % repr(__file__)
    sys.stderr.write(msg)
    sys.exit(1)

if host is None:
    host = "%s.%s" % (config.Master.master_host, config.Master.master_domain)

if port is None:
    port = config.Master.slave_port

if basedir is None:
    dir, _file = os.path.split(__file__)
    basedir = os.path.abspath(dir)


application = service.Application('buildslave')
s = BuildSlave(host, port, slavename, password, basedir, keepalive, usepty,
               umask=umask)
s.setServiceParent(application)
