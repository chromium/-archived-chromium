# -*- makefile -*-

# This is a simple makefile which lives in a buildmaster/buildslave
# directory (next to the buildbot.tac file). It allows you to start/stop the
# master or slave by doing 'make start' or 'make stop'.

# The 'reconfig' target will tell a buildmaster to reload its config file.

# On the Mac, the buildbot is started via the launchd mechanism as a
# LaunchAgent to give the slave a proper Mac UI environment for tests.  In
# order for this to work, the plist must be present and loaded by launchd, and
# the user must be logged in to the UI.  The plist is loaded by launchd at user
# login (and the job may have been initially started at that time too).  Our
# Mac build slaves are all set up this way, and have auto-login enabled, so
# "make start" should just work and do the right thing.
#
# When using launchd to start the job, it also needs to be used to stop the
# job.  Otherwise, launchd might try to restart the job when stopped manually
# by SIGTERM.  Using SIGHUP for reconfig is safe with launchd.
#
# Because it's possible to have more than one slave on a machine (for testing),
# this tests to make sure that the slave is in the known slave location,
# /b/slave, which is what the LaunchAgent operates on.
USE_LAUNCHD := \
  $(shell [ -f ~/Library/LaunchAgents/org.chromium.buildbot.slave.plist ] && \
          [ "$$(pwd -P)" = "/b/slave" ] && \
          echo 1)

start:
ifneq ($(USE_LAUNCHD),1)
	python2.4 run_slave.py --no_save -y buildbot.tac
else
	launchctl start org.chromium.buildbot.slave
endif

stop:
ifneq ($(USE_LAUNCHD),1)
	kill `cat twistd.pid`
else
	launchctl stop org.chromium.buildbot.slave
endif

reconfig:
	kill -HUP `cat twistd.pid`

log:
	tail -f twistd.log
