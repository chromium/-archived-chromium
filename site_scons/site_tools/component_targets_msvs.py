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

"""Visual Studio solution output of component targets for SCons."""

import md5
import sys
import xml.dom
import xml.dom.minidom
import SCons


#------------------------------------------------------------------------------


def MakeGuid(name, seed='component_targets_msvs'):
  """Returns a GUID for the specified target name.

  Args:
    name: Target name.
    seed: Seed for MD5 hash.
  Returns:
    A GUID-line string calculated from the name and seed.

  This generates something which looks like a GUID, but depends only on the
  name and seed.  This means the same name/seed will always generate the same
  GUID, so that projects and solutions which refer to each other can explicitly
  determine the GUID to refer to explicitly.  It also means that the GUID will
  not change when the project for a target is rebuilt.
  """
  # Calculate a MD5 signature for the seed and name.
  d = md5.new(str(seed) + str(name)).hexdigest().upper()
  # Convert most of the signature to GUID form (discard the rest)
  guid = ('{' + d[:8] + '-' + d[8:12] + '-' + d[12:16] + '-' + d[16:20]
          + '-' + d[20:32] + '}')
  return guid

#------------------------------------------------------------------------------


def GetGuidFromVSProject(project_path):
  """Reads the GUID from a Visual Studio project file.

  Args:
    project_path: Path to the Visual Studio project file.

  Returns:
    The GUID string from the file.
  """
  doc = xml.dom.minidom.parse(project_path)
  try:
    n_root = doc.documentElement
    if n_root.nodeName != 'VisualStudioProject':
      raise SCons.Errors.UserError('%s is not a Visual Studio project.' %
                                   project_path)
    return str(n_root.attributes['ProjectGUID'].nodeValue)
  finally:
    # Clean up doc
    doc.unlink()

#------------------------------------------------------------------------------


def ComponentVSProjectBuilder(target, source, env):
  """Visual Studio project builder.

  Args:
    target: Destination file.
    source: List of sources to be added to the target.
    env: Environment context.

  Returns:
    Zero if successful.
  """
  source = source       # Silence gpylint

  target_name = env['TARGET_NAME']
  project_file = target[0].path
  project_to_main = env.RelativePath(target[0].dir, env.Dir('$MAIN_DIR'),
                                     sep='/')

  env_hammer_bat = env.Clone(VS_PROJECT_TO_MAIN_DIR=project_to_main)
  hammer_bat = env_hammer_bat.subst('$COMPONENT_VS_PROJECT_SCRIPT_PATH', raw=1)

  # Project header
  xml_impl = xml.dom.getDOMImplementation()
  doc = xml_impl.createDocument(None, 'VisualStudioProject', None)

  n_root = doc.documentElement
  n_root.setAttribute('ProjectType', 'Visual C++')
  n_root.setAttribute('Version', '8.00')
  n_root.setAttribute('Name', target_name)
  n_root.setAttribute('ProjectGUID', MakeGuid(target_name))
  n_root.setAttribute('RootNamespace', target_name)
  n_root.setAttribute('Keyword', 'MakeFileProj')

  n_platform = doc.createElement('Platforms')
  n_root.appendChild(n_platform)
  n = doc.createElement('Platform')
  n.setAttribute('Name', 'Win32')
  n_platform.appendChild(n)

  n_root.appendChild(doc.createElement('ToolFiles'))

  # One configuration per build mode supported by this target
  n_configs = doc.createElement('Configurations')
  n_root.appendChild(n_configs)

  target_path = env['TARGET_PATH']
  for mode, path in target_path.items():
    n_config = doc.createElement('Configuration')
    n_config.setAttribute('Name', '%s|Win32' % mode)
    n_config.setAttribute('OutputDirectory',
                          '$(ProjectDir)/%s/%s/out' % (mode, target_name))
    n_config.setAttribute('IntermediateDirectory',
                          '$(ProjectDir)/%s/%s/tmp' % (mode, target_name))
    n_config.setAttribute('ConfigurationType', '0')
    n_configs.appendChild(n_config)

    n_tool = doc.createElement('Tool')
    n_tool.setAttribute('Name', 'VCNMakeTool')
    n_tool.setAttribute('IncludeSearchPath', '')
    n_tool.setAttribute('ForcedIncludes', '')
    n_tool.setAttribute('AssemblySearchPath', '')
    n_tool.setAttribute('ForcedUsingAssemblies', '')
    n_tool.setAttribute('CompileAsManaged', '')
    n_tool.setAttribute('PreprocessorDefinitions', '')
    if path:
      n_tool.setAttribute(
          'Output', env.RelativePath(target[0].dir, env.Entry(path), sep='/'))
    build_cmd = '%s --mode=%s %s' % (hammer_bat, mode, target_name)
    clean_cmd = '%s --mode=%s -c %s' % (hammer_bat, mode, target_name)
    n_tool.setAttribute('BuildCommandLine', build_cmd)
    n_tool.setAttribute('CleanCommandLine', clean_cmd)
    n_tool.setAttribute('ReBuildCommandLine', clean_cmd + ' && ' + build_cmd)
    n_config.appendChild(n_tool)

  n_files = doc.createElement('Files')
  n_root.appendChild(n_files)
  # TODO(rspangler): Fill in files - at least, the .scons file invoking the
  # target.

  n_root.appendChild(doc.createElement('Globals'))

  f = open(project_file, 'wt')
  doc.writexml(f, encoding='Windows-1252', addindent='  ', newl='\n')
  f.close()

  return 0


def ComponentVSProject(self, target_name, **kwargs):
  """Visual Studio project pseudo-builder for the specified target.

  Args:
    self: Environment context.
    target_name: Name of the target.
    kwargs: Optional keyword arguments override environment variables in the
        derived environment used to create the project.

  Returns:
    A list of output nodes.
  """
  # Builder only works on Windows
  if sys.platform not in ('win32', 'cygwin'):
    return []

  # Clone environment and add keyword args
  env = self.Clone()
  for k, v in kwargs.items():
    env[k] = v

  # Save the target name
  env['TARGET_NAME'] = target_name

  # Extract the target properties and save in the environment for use by the
  # real builder.
  t = GetTargets().get(target_name)
  env['TARGET_PATH'] = {}
  if t:
    for mode, mode_properties in t.mode_properties.items():
      # Since the target path is what Visual Studio will run, use the EXE
      # property in preference to TARGET_PATH.
      target_path = mode_properties.get('EXE',
                                        mode_properties.get('TARGET_PATH'))
      env.Append(TARGET_PATH={mode: target_path})
  else:
    # No modes declared for this target.  Could be a custom alias created by
    # a SConscript, rather than a component builder.  Assume it can be built in
    # all modes, but produces no output.
    for mode in GetTargetModes():
      env.Append(TARGET_PATH={mode: None})

  # Call the real builder
  return env.ComponentVSProjectBuilder(
      '$COMPONENT_VS_PROJECT_DIR/${TARGET_NAME}', [])

#------------------------------------------------------------------------------


def ComponentVSSolutionBuilder(target, source, env):
  """Visual Studio solution builder.

  Args:
    target: Destination file.
    source: List of sources to be added to the target.
    env: Environment context.

  Returns:
    Zero if successful.
  """
  source = source       # Silence gpylint

  solution_file = target[0].path
  projects = env['SOLUTION_PROJECTS']
  folders = env['SOLUTION_FOLDERS']

  f = open(solution_file, 'wt')

  f.write('Microsoft Visual Studio Solution File, Format Version 9.00\n')
  f.write('# Visual Studio 2005\n')

  # Projects generated by ComponentVSSolution()
  for p in projects:
    project_file = env.File(
        '$COMPONENT_VS_PROJECT_DIR/%s$COMPONENT_VS_PROJECT_SUFFIX' % p)
    f.write('Project("%s") = "%s", "%s", "%s"\nEndProject\n' % (
        '{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}',    # External makefile GUID
        p,                                           # Project name
        env.RelativePath(target[0].dir, project_file),   # Path to project file
        MakeGuid(p),                                 # Project GUID
    ))

  # Projects generated elsewhere
  for p in source:
    f.write('Project("%s") = "%s", "%s", "%s"\nEndProject\n' % (
        # TODO(rspangler): What if this project isn't type external makefile?
        # How to tell what type it is?
        '{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}',    # External makefile GUID
        p,                                           # Project name
        env.RelativePath(target[0].dir, p),          # Path to project file
        GetGuidFromVSProject(p.abspath),             # Project GUID
    ))

  # Folders from build groups
  for folder in folders:
    f.write('Project("%s") = "%s", "%s", "%s"\nEndProject\n' % (
        '{2150E333-8FDC-42A3-9474-1A3956D46DE8}',    # Solution folder GUID
        folder,                                      # Folder name
        folder,                                      # Folder name (again)
        # Use a different seed so the folder won't get the same GUID as a
        # project.
        MakeGuid(folder, seed='folder'),             # Project GUID
    ))

  f.write('\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n')
  for mode in GetTargetModes():
    f.write('\t\t%s|Win32 = %s|Win32\n' % (mode, mode))
  f.write('\tEndGlobalSection\n')

  f.write('\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n')
  for p in projects:
    for mode in GetTargetModes():
      f.write('\t\t%s.%s|Win32.ActiveCfg = %s|Win32\n' % (
          MakeGuid(p),    # Project GUID
          mode,           # Solution build configuration
          mode,           # Project build config for that solution config
      ))
      t = GetTargets().get(p)
      if t and mode in t.mode_properties:
        # Target can be built in this mode
        f.write('\t\t%s.%s|Win32.Build.0 = %s|Win32\n' % (
            MakeGuid(p),    # Project GUID
            mode,           # Solution build configuration
            mode,           # Project build config for that solution config
        ))
  f.write('\tEndGlobalSection\n')

  f.write('\tGlobalSection(SolutionProperties) = preSolution\n')
  f.write('\t\tHideSolutionNode = FALSE\n')
  f.write('\tEndGlobalSection\n')

  if folders:
    f.write('\tGlobalSection(NestedProjects) = preSolution\n')
    for p, folder in projects.items():
      f.write('\t\t%s = %s\n' % (MakeGuid(p), MakeGuid(folder, seed='folder')))
    f.write('\tEndGlobalSection\n')

  f.write('EndGlobal\n')
  f.close()

  return 0


def ComponentVSSolution(self, solution_name, target_names, projects=None,
                        **kwargs):
  """Visual Studio solution pseudo-builder.

  Args:
    self: Environment context.
    solution_name: Name of the target.
    target_names: Names of targets or target groups to include in the solution.
        This will automatically build projects for them.
    projects: List of aditional projects not generated by this solution to
        include in the solution.
    kwargs: Optional keyword arguments override environment variables in the
        derived environment used to create the solution.

  Returns:
    The list of output nodes.
  """
  # TODO(rspangler): Should have option to build source project also.  Perhaps
  # select using a --source_project option, since it needs to use gather_inputs
  # to scan the DAG and will blow up the null build time.
  # TODO(rspangler): Should also have autobuild_projects option.  If false,
  # don't build them.
  # TODO(rspangler): Should also be able to specify a target group directly
  # (i.e. 'run_all_tests')

  # Builder only works on Windows
  if sys.platform not in ('win32', 'cygwin'):
    return []

  # Clone environment and add keyword args
  env = self.Clone()
  for k, v in kwargs.items():
    env[k] = v

  # Save the target name
  env['SOLUTION_NAME'] = solution_name

  # Get list of targets to make projects for.  At this point we haven't
  # determined whether they're groups or targets.
  target_names = env.SubstList2(target_names)
  env['SOLUTION_TARGETS'] = target_names

  project_names = {}
  folders = []
  # Expand target_names into project names, and create project-to-folder
  # mappings
  if target_names:
    # Expand target_names into project names
    for target in target_names:
      if target in GetTargetGroups():
        # Add target to folders
        folders.append(target)
        # Add all project_names in the group
        for t in GetTargetGroups()[target].GetTargetNames():
          project_names[t] = target
      elif target in GetTargets():
        # Just a target name
        project_names[target] = None
      else:
        print 'Warning: ignoring unknown target "%s"' % target
  else:
    # No target names specified, so use all projects
    for t in GetTargets():
      project_names[t] = None

  env['SOLUTION_FOLDERS'] = folders
  env['SOLUTION_PROJECTS'] = project_names

  # Call the real builder
  out_nodes = env.ComponentVSSolutionBuilder(
      '$COMPONENT_VS_SOLUTION_DIR/${SOLUTION_NAME}', projects or [])

  # Call the real builder for the projects we generate
  for p in project_names:
    out_nodes += env.ComponentVSProject(p)

  # Add the solution target
  # TODO(rspangler): Should really defer the rest of the work above, since
  # otherwise we can't build a solution which has a target to rebuild itself.
  env.Alias('all_solutions', env.Alias(solution_name, out_nodes))

  # TODO(rspangler): To rebuild the solution properly, need to override its
  # project configuration so it only has '--mode=all' (or some other way of
  # setting the subset of modes which it should use to rebuild itself).
  # Rebuilding with the property below will strip it down to just the current
  # build mode, which isn't what we want.
  # Let component_targets know this target is available in the current mode
  #self.SetTargetProperty(solution_name, TARGET_PATH=out_nodes[0])

  return out_nodes

#------------------------------------------------------------------------------


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Add pseudo-builders to set up the project and solution builders.  These
  # need to be available on all platforms so that SConscripts which reference
  # them will work.
  env.AddMethod(ComponentVSProject)
  env.AddMethod(ComponentVSSolution)

  # If not on Windows, don't do anything else
  if sys.platform not in ('win32', 'cygwin'):
    return

  # Include tools we need
  env.Tool('gather_inputs')

  env.SetDefault(
      COMPONENT_VS_SOLUTION_DIR='$DESTINATION_ROOT/solution',
      COMPONENT_VS_PROJECT_DIR='$COMPONENT_VS_SOLUTION_DIR/projects',
      COMPONENT_VS_SOLUTION_SUFFIX='.sln',
      COMPONENT_VS_PROJECT_SUFFIX='.vcproj',
      COMPONENT_VS_PROJECT_SCRIPT_NAME = 'hammer.bat',
      COMPONENT_VS_PROJECT_SCRIPT_PATH = (
          '$$(ProjectDir)/$VS_PROJECT_TO_MAIN_DIR/'
          '$COMPONENT_VS_PROJECT_SCRIPT_NAME'),
  )

  AddTargetGroup('all_solutions', 'solutions can be built')

  # Add builders
  vcprojaction = SCons.Script.Action(ComponentVSProjectBuilder, varlist=[
      'TARGET_NAME',
      'TARGET_PATH',
      'COMPONENT_VS_PROJECT_SCRIPT_PATH',
  ])
  vcprojbuilder = SCons.Script.Builder(
      action=vcprojaction,
      suffix='$COMPONENT_VS_PROJECT_SUFFIX')

  slnaction = SCons.Script.Action(ComponentVSSolutionBuilder, varlist=[
      'SOLUTION_TARGETS',
      'SOLUTION_FOLDERS',
      'SOLUTION_PROJECTS',
  ])
  slnbuilder = SCons.Script.Builder(
      action=slnaction,
      suffix='$COMPONENT_VS_SOLUTION_SUFFIX')

  env.Append(BUILDERS={
      'ComponentVSProjectBuilder': vcprojbuilder,
      'ComponentVSSolutionBuilder': slnbuilder,
  })
