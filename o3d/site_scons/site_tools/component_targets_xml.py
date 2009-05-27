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

"""XML output of component targets for SCons."""


import xml.dom
import SCons.Script


def TargetXMLHelp(target, source, env):
  """Generates target information in XML format.

  Args:
    target: Destination file.
    source: List of sources.  Should be empty, otherwise this will actually
        require the sources to be built first.
    env: Environment context.
  """
  env = env
  source = source    # Silence gpylint

  target_path = target[0].abspath

  xml_impl = xml.dom.getDOMImplementation()
  doc = xml_impl.createDocument(None, 'help', None)

  mode_list = doc.createElement('mode_list')
  doc.documentElement.appendChild(mode_list)
  for mode in GetTargetModes().values():
    n = doc.createElement('build_mode')
    n.setAttribute('name', mode.name)
    n.setAttribute('description', mode.description)
    mode_list.appendChild(n)

  group_list = doc.createElement('target_groups')
  doc.documentElement.appendChild(group_list)
  for group in GetTargetGroups().values():
    items = group.GetTargetNames()
    if not items:
      continue

    ngroup = doc.createElement('target_group')
    ngroup.setAttribute('name', group.name)
    group_list.appendChild(ngroup)

    for i in items:
      ntarget = doc.createElement('build_target')
      ntarget.setAttribute('name', i)
      ngroup.appendChild(ntarget)

      # Get properties for target, if any
      target = GetTargets().get(i)
      if target:
        # All modes
        for k, v in target.properties.items():
          n = doc.createElement('target_property')
          n.setAttribute('name', k)
          n.setAttribute('value', v)
          ntarget.appendChild(n)

        # Mode-specific
        for mode, mode_properties in target.mode_properties.items():
          nmode = doc.createElement('target_mode')
          nmode.setAttribute('name', mode)
          ntarget.appendChild(nmode)

          for k, v in mode_properties.items():
            n = doc.createElement('target_property')
            n.setAttribute('name', k)
            n.setAttribute('value', v)
            nmode.appendChild(n)

  f = open(target_path, 'wt')
  doc.writexml(f, encoding='UTF-8', addindent='  ', newl='\n')
  f.close()

#------------------------------------------------------------------------------


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""
  env = env     # Silence gpylint

  SCons.Script.Help('''\
  targets_xml                 Write information on the build mode's targets to
                              targets.xml.  Most useful with --mode=all.
''')

  # Add build target to generate help
  p = env.Command('$DESTINATION_ROOT/targets.xml', [],
                  env.Action(TargetXMLHelp))

  # Always build xml info if requested.
  # TODO: Is there a better way to determine the xml info is up to date?
  env.AlwaysBuild(p)
  env.Alias('targets_xml', p)
