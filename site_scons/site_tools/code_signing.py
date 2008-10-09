#!/usr/bin/python2.4
# Copyright 2008, Google Inc.
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

"""Code signing build tool.

This module sets up code signing.
It is used as follows:
  env = Environment(tools = ["code_signing"])
To sign an EXE/DLL do:
  env.SignedBinary('hello_signed.exe', 'hello.exe',
                   CERTIFICATE_FILE='bob.pfx',
                   CERTIFICATE_PASSWORD='123',
                   TIMESTAMP_SERVER='')
If no certificate file is specified, copying instead of signing will occur.
If an empty timestamp server string is specified, there will be no timestamp.
"""

import SCons.Script


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  env.Replace(
      # Path to Microsoft signtool.exe
      SIGNTOOL='$VC80_DIR/common7/tools/bin/signtool.exe',
      # No certificate by default.
      CERTIFICATE_PATH='',
      # No certificate password by default.
      CERTIFICATE_PASSWORD='',
      # The default timestamp server.
      TIMESTAMP_SERVER='http://timestamp.verisign.com/scripts/timestamp.dll',
  )

  # Setup Builder for Signing
  env['BUILDERS']['SignedBinary'] = SCons.Script.Builder(
      generator=SignedBinaryGenerator,
      emitter=SignedBinaryEmitter)


def SignedBinaryEmitter(target, source, env):
  """Add the signing certificate (if any) to the source dependencies."""
  if env['CERTIFICATE_PATH']:
    source.append(env['CERTIFICATE_PATH'])
  return target, source


def SignedBinaryGenerator(source, target, env, for_signature):
  """A builder generator for code signing."""
  source = source                # Silence gpylint.
  target = target                # Silence gpylint.
  for_signature = for_signature  # Silence gpylint.

  # Alway copy and make writable.
  commands = [
      SCons.Script.Copy('$TARGET', '$SOURCE'),
      SCons.Script.Chmod('$TARGET', 0755),
  ]

  # Only do signing if there is a certificate path.
  if env['CERTIFICATE_PATH']:
    # The command used to do signing (target added on below).
    signing_cmd = '$SIGNTOOL sign /f "$CERTIFICATE_PATH"'
    # Add certificate password if any.
    if env['CERTIFICATE_PASSWORD']:
      signing_cmd += ' /p "$CERTIFICATE_PASSWORD"'
    # Add timestamp server if any.
    if env['TIMESTAMP_SERVER']:
      signing_cmd += ' /t "$TIMESTAMP_SERVER"'
    # Add in target name
    signing_cmd += ' $TARGET'
    # Add the signing to the list of commands to perform.
    commands.append(signing_cmd)

  return commands
