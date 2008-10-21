#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Copies new contents of a source directory to destination directory.

This script has been tested only on Linux. Currently it runs on the buildmaster
as a cron job.
"""

import datetime
import errno
import logging
import optparse
import os
import shutil

class OptionParser (optparse.OptionParser):
  """ Class to make command line options with no default value, mandatory.
  """
  def check_required (self, opt):
    option = self.get_option(opt)

    # Assumes the option's 'default' is set to None!
    if getattr(self.values, option.dest) is None:
      self.error("%s option not supplied" % option)


def copytree(src, dst, return_if_dst_exist=True):
  """Recursively copy a directory tree.

  shutil.copytree() complains if the destination directory already exists.
  This method will take the new files under source directory, copy them to
  destination directory and skip the ones that already exists (if
  return_if_dst_exist is set to False).

  Args:
    src: source directory
    dst: destination directory
    return_if_dst_exist: return if destination directory exists
  """
  logging.debug("Copying directory from %s to %s" % (src, dst))
  if not os.path.isdir(src):
    logging.error("The source directory specified doesn't exist "
                 "(or is not a directory): %s" % src)
    return

  if os.path.exists(dst):
    if not os.path.isdir(dst) or return_if_dst_exist:
      logging.debug("The destination already exists: %s" % dst)
      return
  else:
    os.mkdir(dst)

  children = os.listdir(src)
  for child in children:
    child_src = os.path.join(src, child)
    child_dst = os.path.join(dst, child)
    try:
      if os.path.isdir(child_src):
        copytree(child_src, child_dst, return_if_dst_exist=False)
      elif os.path.exists(child_dst):
        logging.debug("Destination file already exists %s" % (child_dst))
      else:
        logging.debug("Copying file from %s to %s" % (child_src, child_dst))
        shutil.copyfile(child_src, child_dst)
    except (IOError, os.error), ex:
      logging.error("Exception thrown while copying %s to %s: %s" %
                    (srcname, dstname, ex))


def writePid(pidfile):
  """Write process id of current process to file specified by pidfile.
  """
  f = open(pidfile, "w")
  f.write(str(os.getpid()))
  f.close()


def lockPidFile(pidfile, runtime):
  """Create a lock file for the current job to prevent multiple instances.

  This method create a lock file for the current process and does following
  checks:
  - if file doesn't exist we can safely create a lock file and return true
  - if file already exists but the process with the process id given inside the
  file doesn't exist, we can override the file and return true
  - if file exists, the process with the process id given inside the file also
  exists but the file is older than runtime seconds, kill the process, write
  a new lock file and return true
  - Otherwise do not alter the lock file and return false.
  Args:
    pidfle: path to the lock file
    runtime: the maximum time in seconds that an instance of current process is
    allowed to run
  Returns:
    True: if the lock file was created
    False: if currently an instance is running and lock file was not updated
  """
  logging.debug("Trying to look for pid file %s" % pidfile)
  try:
    f = open(pidfile, "r")
  except IOError, ex:
    if ex[0] == errno.ENOENT:  # No such file or directory
      logging.info("No previous job found, locking pid file %s" % pidfile)
      writePid(pidfile)
      return True
    else:
      raise # unknown error so raise

  try:
    pid = int(f.read())
    os.kill(pid, 0)
  except OSError, ex:
    f.close()
    if ex[0] == errno.ESRCH: # No such process
      logging.info("Bogus lock file found, ignoring and locking new pid file")
      writePid(pidfile)
      return True
    else:
      raise

  now = datetime.datetime.now()
  lastmodified = datetime.datetime.fromtimestamp(
                     os.fstat(f.fileno()).st_mtime)
  f.close()
  if runtime and (now - lastmodified) > datetime.timedelta(seconds=runtime):
    try:
      os.kill(pid, 9)
    except OSError, exc:
      if ex[0] != errno.ESRCH: # No such process
        raise
    logging.info("Killed previous job with pid %d as it exceeded "
                 "maximum runtime %d" % (pid, runtime))
    writePid(pidfile)
    return True
  else:
    logging.warn("Job still running with pid %d" % pid)
    return False


def main(options, args):
  """Copy the directory tree.

  Args:
    options: a dictionary of command line options
    args: should be empty as this script only takes options and no arguments
  """
  # setup logging
  log_level = logging.INFO
  if options.verbose:
    log_level = logging.DEBUG
  logging.basicConfig(level=log_level,
                      format='%(asctime)s %(filename)s:%(lineno)-3d'
                             ' %(levelname)s %(message)s',
                      datefmt='%y%m%d %H:%M:%S')

  logging.debug(options)

  if not lockPidFile(options.pidfile, int(options.runtime)):
    logging.debug("Skipping copy")
    return

  copytree(options.src, options.dst, return_if_dst_exist=False)
  logging.debug("Done copying all the files.")
  os.remove(options.pidfile)
  logging.info("Removed pid file successfully and all done.")


if '__main__' == __name__:
  parser = OptionParser()
  parser.add_option("-s", "--src", default=None, help="source directory")
  parser.add_option("-d", "--dst", default=None, help="destination directory")
  parser.add_option("-v", "--verbose", action="store_true",
                    default=False, help="include debug level logging")
  parser.add_option("-p", "--pidfile", default="/tmp/copytree.pid",
                    help="pid file name to lock so that only one instance runs")
  parser.add_option("-t", "--runtime", default=0,
                    help="The maximum time this script is allowed to run")
  (options, args) = parser.parse_args()
  parser.check_required("-s")
  parser.check_required("-d")

  main(options, args)
