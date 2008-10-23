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

"""Software construction toolkit site_scons configuration.

This module sets up SCons for use with this toolkit.  This should contain setup
which occurs outside of environments.  If a method operates within the context
of an environment, it should instead go in a tool in site_tools and be invoked
for the target environment.
"""

import __builtin__
import sys
import SCons


# List of target groups for printing help; modified by AddTargetGroup(); used
# by BuildEnvironments().
__target_groups = {}


def _HostPlatform():
  """Returns the current host platform.

  That is, the platform we're actually running SCons on.  You shouldn't use
  this inside your SConscript files; instead, include the appropriate
  target_platform tool for your environments.  When you call
  BuildEnvironments(), only environments with the current host platform will be
  built.

  Returns:
    The host platform name - one of ('WINDOWS', 'LINUX', 'MAC').
  """

  platform_map = {
      'win32': 'WINDOWS',
      'cygwin': 'WINDOWS',
      'linux': 'LINUX',
      'linux2': 'LINUX',
      'darwin': 'MAC',
  }

  if sys.platform not in platform_map:
    print ('site_init.py warning: platform "%s" is not in platfom map.' %
           sys.platform)

  return platform_map.get(sys.platform, sys.platform)


#------------------------------------------------------------------------------


def _CheckBuildModes(build_modes, environments, host_platform):
  """Checks the build modes for the environments.

  Args:
    build_modes: List of build mode strings.
    environments: List of SCons environments.
    host_platform: Host platform string.

  Raises:
    ValueError: build groups and/or types invalid.
  """
  # Make sure the list of environments for the current host platform have
  # unique BUILD_TYPE.  This ensures they won't overwrite each others' build
  # output.  (It is ok for build types in different host platforms to overlap;
  # that is, WINDOWS and MAC can both have a 'dbg' build type.)
  all_build_types = []
  all_build_groups = {}
  build_desc = {}
  for e in environments:
    if not e.Overlap(e['HOST_PLATFORMS'], [host_platform, '*']):
      continue
    if e['BUILD_TYPE'] in all_build_types:
      raise ValueError('Multiple environments have the same BUILD_TYPE=%s' %
                       e['BUILD_TYPE'])
    else:
      all_build_types.append(e['BUILD_TYPE'])
      build_desc[e['BUILD_TYPE']] = e.get('BUILD_TYPE_DESCRIPTION')

    # Keep track of build groups and the build types which belong to them
    for g in e['BUILD_GROUPS']:
      if g not in all_build_groups:
        all_build_groups[g] = []
        # Don't allow build types and groups to share names
        if g in all_build_types:
          raise ValueError('Build group %s also specified as BUILD_TYPE.' % g)
        else:
          all_build_types.append(g)
      all_build_groups[g].append(e['BUILD_TYPE'])

  # Add help for build types
  xml_help = SCons.Script.GetOption('xml_help')
  if xml_help:
    help_mode_format = '  <build_mode name="%s"><![CDATA[%s]]></build_mode>\n'
    help_text = '<mode_list>\n'
  else:
    help_text = '''
Use --mode=type to specify the type of build to perform.  The following types
may be specified:
'''
    help_mode_format = '    %-16s %s\n'

  for build_type in all_build_types:
    if build_type not in all_build_groups:
      help_text += help_mode_format % (
          build_type, build_desc.get(build_type, ''))

  if xml_help:
    help_group_format = ('  <build_group name="%s"><![CDATA[%s]]>'
                         '</build_group>\n')
    help_text += '</mode_list>\n<group_list>\n'
  else:
    help_group_format = '    %-16s %s\n'
    help_text += '''
The following build groups may also be specified via --mode.  Build groups
build one or more of the other build types.  The available build groups are:
'''

  groups_sorted = all_build_groups.keys()
  groups_sorted.sort()
  for g in groups_sorted:
    help_text += help_group_format % (g, ','.join(all_build_groups[g]))

  if xml_help:
    help_text += '</group_list>\n'
  else:
    help_text += '''
Multiple modes may be specified, separated by commas: --mode=mode1,mode2.  If
no mode is specified, the default group will be built.  This is equivalent to
specifying --mode=default.
  '''
  SCons.Script.Help(help_text)

  # Make sure all build modes specified by the user are ones which apply to
  # the current environment.
  for mode in build_modes:
    if mode not in all_build_types and mode not in all_build_groups:
      print ('Warning: Ignoring build mode "%s", which is not defined on this '
             'platform.' % mode)


def _AddTargetHelp():
  """Adds help for the target groups from the global __target_groups."""
  xml_help = SCons.Script.GetOption('xml_help')
  help_text = ''

  for alias, description in __target_groups.items():
    items = map(str, SCons.Script.Alias(alias)[0].sources)
    # Remove duplicates from multiple environments
    items = list(set(items))

    if items:
      colwidth = max(map(len, items)) + 2
      cols = 77 / colwidth
      if cols < 1:
          cols = 1      # If target names are really long, one per line
      rows = (len(items) + cols - 1) / cols
      items.sort()
      if xml_help:
        help_text += '<target_group name="%s">\n' % alias
        for i in items:
          help_text += '  <build_target name="%s"/>\n' % i
        help_text += '</target_group>\n'
      else:
        help_text += '\nThe following %s:' % description
        for row in range(0, rows):
          help_text += '\n  '
          for i in range(row, len(items), rows):
            help_text += '%-*s' % (colwidth, items[i])
        help_text += '\n  %s (do all of the above)\n' % alias

  SCons.Script.Help(help_text)

#------------------------------------------------------------------------------


def BuildEnvironments(environments):
  """Build a collection of SConscripts under a collection of environments.

  Only environments with HOST_PLATFORMS containing the platform specified by
  --host-platform (or the native host platform, if --host-platform was not
  specified) will be matched.

  Each matching environment is checked against the modes passed to the --mode
  command line argument (or 'default', if no mode(s) were specified).  If any
  of the modes match the environment's BUILD_TYPE or any of the environment's
  BUILD_GROUPS, all the BUILD_SCONSCRIPTS (and for legacy reasons,
  BUILD_COMPONENTS) in that environment will be built.

  Args:
    environments: List of SCons environments.

  Returns:
    List of environments which were actually evaluated (built).
  """
  # Get options
  xml_help = SCons.Script.GetOption('xml_help')
  build_modes = SCons.Script.GetOption('build_mode')
  # TODO(rspangler): Remove support legacy MODE= argument, once everyone has
  # transitioned to --mode.
  legacy_mode_option = SCons.Script.ARGUMENTS.get('MODE')
  if legacy_mode_option:
    build_modes = legacy_mode_option
  build_modes = build_modes.split(',')

  host_platform = SCons.Script.GetOption('host_platform')
  if not host_platform:
    host_platform = _HostPlatform()

  # Check build modes
  _CheckBuildModes(build_modes, environments, host_platform)

  if xml_help:
    SCons.Script.Help('<help_from_sconscripts>\n<![CDATA[\n')

  environments_to_evaluate = []
  for e in environments:
    if not e.Overlap(e['HOST_PLATFORMS'], [host_platform, '*']):
      continue      # Environment requires a host platform which isn't us

    if e.Overlap([e['BUILD_TYPE'], e['BUILD_GROUPS']], build_modes):
      environments_to_evaluate.append(e)

  for e in environments_to_evaluate:
    # Set up for deferred functions and published resources
    e._InitializeComponentBuilders()
    e._InitializeDefer()
    e._InitializePublish()

    # Read SConscript for each component
    # TODO(rspangler): Remove BUILD_COMPONENTS once all projects have
    # transitioned to the BUILD_SCONSCRIPTS nomenclature.
    for c in e.get('BUILD_COMPONENTS', []) + e.get('BUILD_SCONSCRIPTS', []):
      # Clone the environment so components can't interfere with each other
      ec = e.Clone()

      if ec.Entry(c).isdir():
        # The component is a directory, so assume it contains a SConscript
        # file.
        c_dir = ec.Dir(c)

        # Use 'build.scons' as the default filename, but if that doesn't
        # exist, fall back to 'SConscript'.
        c_script = c_dir.File('build.scons')
        if not c_script.exists():
          c_script = c_dir.File('SConscript')
      else:
        # The component is a SConscript file.
        c_script = ec.File(c)
        c_dir = c_script.dir

      ec.SConscript(c_script,
                    build_dir='$OBJ_ROOT/' + str(c_dir),
                    exports={'env': ec},
                    duplicate=0)

    # Execute deferred functions
    e._ExecuteDefer()

  if xml_help:
    SCons.Script.Help(']]>\n</help_from_sconscripts>\n')

  _AddTargetHelp()

  # End final help tag
  if xml_help:
    SCons.Script.Help('</help>\n')

  # Return list of environments actually evaluated
  return environments_to_evaluate


#------------------------------------------------------------------------------


def _ToolExists():
  """Replacement for SCons tool module exists() function, if one isn't present.

  Returns:
    True.  This enables modules which always exist not to need to include a
        dummy exists() function.
  """
  return True


def _ToolModule(self):
  """Thunk for SCons.Tool.Tool._tool_module to patch in exists() function.

  Returns:
    The module from the original SCons.Tool.Tool._tool_module call, with an
        exists() method added if it wasn't present.
  """
  module = self._tool_module_orig()
  if not hasattr(module, 'exists'):
    module.exists = _ToolExists

  return module

#------------------------------------------------------------------------------


def AddSiteDir(site_dir):
  """Adds a site directory, as if passed to the --site-dir option.

  Args:
    site_dir: Site directory path to add, relative to the location of the
        SConstruct file.

  This may be called from the SConscript file to add a local site scons
  directory for a project.  This does the following:
     * Adds site_dir/site_scons to sys.path.
     * Imports site_dir/site_init.py.
     * Adds site_dir/site_scons to the SCons tools path.
  """
  # Call the same function that SCons does for the --site-dir option.
  SCons.Script.Main._load_site_scons_dir(
      SCons.Node.FS.get_default_fs().SConstruct_dir, site_dir)


def AddTargetGroup(target_group, description):
  """Adds a target group, used for printing help.

  Args:
    target_group: Name of target group.  This should be the name of an alias
        which points to other aliases for the specific targets.
    description: Description of the target group.
  """

  __target_groups[target_group] = description

#------------------------------------------------------------------------------


_new_options_help = '''
Additional options for SCons:

  --mode=MODE                 Specify build mode (see below).
  --host-platform=PLATFORM    Force SCons to use PLATFORM as the host platform,
                              instead of the actual platform on which SCons is
                              run.  Useful for examining the dependency tree
                              which would be created, but not useful for
                              actually running the build because it'll attempt
                              to use the wrong tools for your actual platform.
  --site-path=DIRLIST         Comma-separated list of additional site
                              directory paths; each is processed as if passed
                              to --site-dir.
  --xml-help                  Print help in XML format.
'''

def SiteInitMain():
  """Main code executed in site_init."""

  # Bail out if we've been here before. This is needed to handle the case where
  # this site_init.py has been dropped into a project directory.
  if hasattr(__builtin__, 'BuildEnvironments'):
    return

  # Let people use new global methods directly.
  __builtin__.AddSiteDir = AddSiteDir
  __builtin__.BuildEnvironments = BuildEnvironments
  __builtin__.AddTargetGroup = AddTargetGroup
  # Legacy method names
  # TODO(rspangler): Remove these once they're no longer used anywhere.
  __builtin__.BuildComponents = BuildEnvironments


  # Set list of default tools for component_setup
  __builtin__.component_setup_tools = [
      'command_output',
      'component_bits',
      'component_builders',
      'concat_source',
      'defer',
      'environment_tools',
      'publish',
      'replicate',
  ]

  # Patch Tool._tool_module method to fill in an exists() method for the
  # module if it isn't present.
  # TODO(sgk): This functionality should be patched into SCons itself by
  # changing Tool.__init__().
  SCons.Tool.Tool._tool_module_orig = SCons.Tool.Tool._tool_module
  SCons.Tool.Tool._tool_module = _ToolModule

  # Add our options
  SCons.Script.AddOption(
      '--mode', '--build-mode',
      dest='build_mode',
      nargs=1, type='string',
      action='store',
      metavar='MODE',
      default='default',
      help='build mode(s)')
  SCons.Script.AddOption(
      '--host-platform',
      dest='host_platform',
      nargs=1, type='string',
      action='store',
      metavar='PLATFORM',
      help='build mode(s)')
  SCons.Script.AddOption(
      '--site-path',
      dest='site_path',
      nargs=1, type='string',
      action='store',
      metavar='PATH',
      help='comma-separated list of site directories')
  SCons.Script.AddOption(
      '--xml-help',
      dest='xml_help',
      action='store_true',
      help='print help in XML format')

  if SCons.Script.GetOption('xml_help'):
    SCons.Script.Help('<?xml version="1.0" encoding="UTF-8" ?>\n<help>\n')
  else:
    SCons.Script.Help(_new_options_help)

  # Check for site path.  This is a list of site directories which each are
  # processed as if they were passed to --site-dir.
  site_path = SCons.Script.GetOption('site_path')
  if site_path:
    for site_dir in site_path.split(','):
      AddSiteDir(site_dir)

  # Since our site dir was specified on the SCons command line, SCons will
  # normally only look at our site dir.  Add back checking for project-local
  # site_scons directories.
  if not SCons.Script.GetOption('no_site_dir'):
    SCons.Script.Main._load_site_scons_dir(
        SCons.Node.FS.get_default_fs().SConstruct_dir, None)

# Run main code
SiteInitMain()
