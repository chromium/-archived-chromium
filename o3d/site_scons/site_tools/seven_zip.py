#!/usr/bin/python2.4
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""SCons tool for 7zip."""


import os
import shutil
import subprocess
import tempfile
import SCons.Script


def SevenZipGetFiles(env, source):
  """SCons emitter for 7zip extract.

  Examines the source 7z archive to determine the list of files which will be
  created by extract/unzip operation.
  Args:
    env: The SCons environment to get the 7zip command line from.
    source: The 7zip archive to examine.
  Returns:
    The list of filenames in the archive.
  """
  # Expand the command to list archive contents.
  cmd = env.subst('$SEVEN_ZIP l "%s"' % source)
  # Run it and capture output.
  output = subprocess.Popen(cmd, stdout=subprocess.PIPE).communicate()[0]
  # Strip off 7-line header and 3-line trailer from 7zip output.
  lines = output.split('\r\n')[7:-3]
  # Trim out just the files and their names.
  files = [i[53:] for i in lines if i[20] != 'D']
  return files


def SevenZipEmitter(target, source, env):
  """An emitter that decides what nodes are vented from a 7zip archive.

  Args:
    target: The target directory node.
    source: The source archive node.
    env: The environment in which the emit takes place.
  Returns:
    The pair (target, source) which lists the emitted targets and sources.
  """
  # Remember out dir for later.
  env['SEVEN_ZIP_OUT_DIR'] = target[0].dir
  # Get out contents
  files = SevenZipGetFiles(env, env.subst('$SOURCE', source=source))
  # Extract a layer deeper if there is only one, and it extension is 7z.
  if env.get('SEVEN_ZIP_PEEL_LAYERS', False):
    assert len(files) == 1 and os.path.splitext(files[0])[1] == '.7z'
    # Create a temporary directory.
    tmp_dir = tempfile.mkdtemp()
    # Expand the command to extract the archive to a temporary location.
    cmd = env.subst('$SEVEN_ZIP x $SOURCE -o"%s"' % tmp_dir, source=source[0])
    # Run it and swallow output.
    subprocess.Popen(cmd, stdout=subprocess.PIPE).communicate()
    # Get contents.
    inner_files = SevenZipGetFiles(env, os.path.join(tmp_dir, files[0]))
    # Make into file nodes.
    inner_files = [target[0].dir.File(i) for i in inner_files]
    # Delete temp_dir.
    shutil.rmtree(tmp_dir)
    # Decide where to extra working file to.
    working_file = env.Dir(target[0].dir.abspath +
                           '.7zip_extract').File(files[0])
    # Combine everything.
    files = [working_file] + inner_files
  else:
    # Make into file nodes.
    files = [target[0].dir.File(i) for i in files]
  # Return files as actual target.
  return (files, source)


def SevenZipGenerator(source, target, env, for_signature):
  """The generator function which decides how to extract a file."""

  # Silence lint.
  source = source
  target = target
  for_signature = for_signature

  if env.get('SEVEN_ZIP_PEEL_LAYERS', False):
    return [SCons.Script.Delete('$SEVEN_ZIP_OUT_DIR'),
            '$SEVEN_ZIP x $SOURCE -o"${TARGET.dir}"',
            '$SEVEN_ZIP x $TARGET -o"$SEVEN_ZIP_OUT_DIR"']
  else:
    return [SCons.Script.Delete('$SEVEN_ZIP_OUT_DIR'),
            '$SEVEN_ZIP x $SOURCE -o"$SEVEN_ZIP_OUT_DIR"']


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  env.Replace(
      SEVEN_ZIP='$SEVEN_ZIP_DIR/7za.exe',
      SEVEN_ZIP_ARCHIVE_OPTIONS = ['-t7z', '-mx0'],
      SEVEN_ZIP_COMPRESS_OPTIONS = ['-t7z', '-mx9'],
  )

  b = SCons.Script.Builder(generator=SevenZipGenerator,
                           emitter=SevenZipEmitter)
  env['BUILDERS']['Extract7zip'] = b

  b = SCons.Script.Builder(
      action=('cd $SOURCE && '
              '$SEVEN_ZIP a $SEVEN_ZIP_ARCHIVE_OPTIONS ${TARGET.abspath} ./'))
  env['BUILDERS']['Archive7zip'] = b

  b = SCons.Script.Builder(
      action=('cd ${SOURCE.dir} && '
              '$SEVEN_ZIP a $SEVEN_ZIP_COMPRESS_OPTIONS '
              '${TARGET.abspath} ${SOURCE.file}'))
  env['BUILDERS']['Compress7zip'] = b
