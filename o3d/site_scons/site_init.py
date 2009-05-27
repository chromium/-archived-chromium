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

"""Software construction toolkit site_scons configuration.

This module sets up SCons for use with this toolkit.  This should contain setup
which occurs outside of environments.  If a method operates within the context
of an environment, it should instead go in a tool in site_tools and be invoked
for the target environment.
"""

import __builtin__
import sys
import SCons
import usage_log


def _HostPlatform():
  """Returns the current host platform.

  That is, the platform we're actually running SCons on.  You shouldn't use
  this inside your SConscript files; instead, include the appropriate
  target_platform tool for your environments.  When you call
  BuildEnvironments(), only environments with the current host platform will be
  built.  If for some reason you really need to examine the host platform,
  check env.Bit('host_windows') / env.Bit('host_linux') / env.Bit('host_mac').

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
  help_text = '''
Use --mode=type to specify the type of build to perform.  The following types
may be specified:
'''

  for build_type in all_build_types:
    if build_type not in all_build_groups:
      help_text += '    %-16s %s\n' % (
          build_type, build_desc.get(build_type, ''))

  help_text += '''
The following build groups may also be specified via --mode.  Build groups
build one or more of the other build types.  The available build groups are:
'''

  groups_sorted = all_build_groups.keys()
  groups_sorted.sort()
  for g in groups_sorted:
    help_text += '    %-16s %s\n' % (g, ','.join(all_build_groups[g]))

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


#------------------------------------------------------------------------------


def BuildEnvironmentSConscripts(env):
  """Evaluates SConscripts for the environment.

  Called by BuildEnvironments().
  """
  # Read SConscript for each component
  # TODO: Remove BUILD_COMPONENTS once all projects have transitioned to the
  # BUILD_SCONSCRIPTS nomenclature.
  for c in env.SubstList2('$BUILD_SCONSCRIPTS', '$BUILD_COMPONENTS'):
    # Clone the environment so components can't interfere with each other
    ec = env.Clone()

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

    # Make c_dir a string.
    c_dir = str(c_dir)

    # Use build_dir differently depending on where the SConscript is.
    if not ec.RelativePath('$TARGET_ROOT', c_dir).startswith('..'):
      # The above expression means: if c_dir is $TARGET_ROOT or anything
      # under it. Going from c_dir to $TARGET_ROOT and dropping the not fails
      # to include $TARGET_ROOT.
      # We want to be able to allow people to use addRepository to back things
      # under $TARGET_ROOT/$OBJ_ROOT with things from above the current
      # directory. When we are passed a SConscript that is already under
      # $TARGET_ROOT, we should not use build_dir.
      ec.SConscript(c_script, exports={'env': ec}, duplicate=0)
    elif not ec.RelativePath('$MAIN_DIR', c_dir).startswith('..'):
      # The above expression means: if c_dir is $MAIN_DIR or anything
      # under it. Going from c_dir to $TARGET_ROOT and dropping the not fails
      # to include $MAIN_DIR.
      # Also, if we are passed a SConscript that
      # is not under $MAIN_DIR, we should fail loudly, because it is unclear how
      # this will correspond to things under $OBJ_ROOT.
      ec.SConscript(c_script, build_dir='$OBJ_ROOT/' + c_dir,
                    exports={'env': ec}, duplicate=0)
    else:
      raise SCons.Error.UserError(
          'Bad location for a SConscript. "%s" is not under '
          '\$TARGET_ROOT or \$MAIN_DIR' % c_script)

def FilterEnvironments(environments):
  """Filters out the environments to be actually build from the specified list

  Only environments with HOST_PLATFORMS containing the platform specified by
  --host-platform (or the native host platform, if --host-platform was not
  specified) will be matched.

  Each matching environment is checked against the modes passed to the --mode
  command line argument (or 'default', if no mode(s) were specified).  If any
  of the modes match the environment's BUILD_TYPE or any of the environment's
  BUILD_GROUPS, all the BUILD_SCONSCRIPTS (and for legacy reasons,
  BUILD_COMPONENTS) in that environment will be matched.

  Args:
    environments: List of SCons environments.

  Returns:
    List of environments which were matched
  """
  # Get options
  build_modes = SCons.Script.GetOption('build_mode')
  # TODO: Remove support legacy MODE= argument, once everyone has transitioned
  # to --mode.
  legacy_mode_option = SCons.Script.ARGUMENTS.get('MODE')
  if legacy_mode_option:
    build_modes = legacy_mode_option
  build_modes = build_modes.split(',')

  # Check build modes
  _CheckBuildModes(build_modes, environments, HOST_PLATFORM)

  environments_to_evaluate = []
  for e in environments:
    if not e.Overlap(e['HOST_PLATFORMS'], [HOST_PLATFORM, '*']):
      continue      # Environment requires a host platform which isn't us

    if e.Overlap([e['BUILD_TYPE'], e['BUILD_GROUPS']], build_modes):
      environments_to_evaluate.append(e)
  return environments_to_evaluate

def BuildEnvironments(environments):
  """Build a collection of SConscripts under a collection of environments.

  The environments are subject to filtering (c.f. FilterEnvironments)

  Args:
    environments: List of SCons environments.

  Returns:
    List of environments which were actually evaluated (built).
  """
  usage_log.log.AddEntry('BuildEnvironments start')

  environments_to_evaluate = FilterEnvironments(environments)

  for e in environments_to_evaluate:
    # Make this the root environment for deferred functions, so they don't
    # execute until our call to ExecuteDefer().
    e.SetDeferRoot()

    # Defer building the SConscripts, so that other tools can do
    # per-environment setup first.
    e.Defer(BuildEnvironmentSConscripts)

    # Execute deferred functions
    e.ExecuteDefer()

  # Add help on targets.
  AddTargetHelp()

  usage_log.log.AddEntry('BuildEnvironments done')

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
  --usage-log=FILE            Write XML usage log to FILE.
'''

def SiteInitMain():
  """Main code executed in site_init."""

  # Bail out if we've been here before. This is needed to handle the case where
  # this site_init.py has been dropped into a project directory.
  if hasattr(__builtin__, 'BuildEnvironments'):
    return

  usage_log.log.AddEntry('Software Construction Toolkit site init')

  # Let people use new global methods directly.
  __builtin__.AddSiteDir = AddSiteDir
  __builtin__.FilterEnvironments = FilterEnvironments
  __builtin__.BuildEnvironments = BuildEnvironments
  # Legacy method names
  # TODO: Remove these once they're no longer used anywhere.
  __builtin__.BuildComponents = BuildEnvironments

  # Set list of default tools for component_setup
  __builtin__.component_setup_tools = [
      # Defer must be first so other tools can register environment
      # setup/cleanup functions.
      'defer',
      # Component_targets must precede component_builders so builders can
      # define target groups.
      'component_targets',
      'command_output',
      'component_bits',
      'component_builders',
      'concat_source',
      'environment_tools',
      'publish',
      'replicate',
  ]

  # Patch Tool._tool_module method to fill in an exists() method for the
  # module if it isn't present.
  # TODO: This functionality should be patched into SCons itself by changing
  # Tool.__init__().
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
      '--usage-log',
      dest='usage_log',
      nargs=1, type='string',
      action='store',
      metavar='PATH',
      help='file to write XML usage log to')

  SCons.Script.Help(_new_options_help)

  # Set up usage log
  usage_log_file = SCons.Script.GetOption('usage_log')
  if usage_log_file:
    usage_log.log.SetOutputFile(usage_log_file)

  # Set current host platform
  host_platform = SCons.Script.GetOption('host_platform')
  if not host_platform:
    host_platform = _HostPlatform()
  __builtin__.HOST_PLATFORM = host_platform

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
