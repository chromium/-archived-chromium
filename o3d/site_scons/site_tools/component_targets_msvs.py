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

"""Visual Studio solution output of component targets for SCons."""

import copy
import md5
import os
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


def GetGuidAndNameFromVSProject(project_path):
  """Reads the GUID from a Visual Studio project file.

  Args:
    project_path: Path to the Visual Studio project file.

  Returns:
    The GUID string from the file.
    The project name from the file.
  """
  doc = xml.dom.minidom.parse(project_path)
  try:
    n_root = doc.documentElement
    if n_root.nodeName != 'VisualStudioProject':
      raise SCons.Errors.UserError('%s is not a Visual Studio project.' %
                                   project_path)
    return (
        str(n_root.attributes['ProjectGUID'].nodeValue),
        str(n_root.attributes['Name'].nodeValue),
    )
  finally:
    # Clean up doc
    doc.unlink()

#------------------------------------------------------------------------------


class VSProjectWriter(object):
  """Visual Studio XML project writer."""

  def __init__(self, project_path):
    """Initializes the project.

    Args:
      project_path: Path to the project file.
    """
    self.project_path = project_path
    self.doc = None

  def Create(self, name):
    """Creates the project document.

    Args:
      name: Name of the project.
    """
    self.name = name
    self.guid = MakeGuid(name)

    # Create XML doc
    xml_impl = xml.dom.getDOMImplementation()
    self.doc = xml_impl.createDocument(None, 'VisualStudioProject', None)

    # Add attributes to root element
    self.n_root = self.doc.documentElement
    self.n_root.setAttribute('ProjectType', 'Visual C++')
    self.n_root.setAttribute('Version', '8.00')
    self.n_root.setAttribute('Name', self.name)
    self.n_root.setAttribute('ProjectGUID', self.guid)
    self.n_root.setAttribute('RootNamespace', self.name)
    self.n_root.setAttribute('Keyword', 'MakeFileProj')

    # Add platform list
    n_platform = self.doc.createElement('Platforms')
    self.n_root.appendChild(n_platform)
    n = self.doc.createElement('Platform')
    n.setAttribute('Name', 'Win32')
    n_platform.appendChild(n)

    # Add empty ToolFiles section
    self.n_root.appendChild(self.doc.createElement('ToolFiles'))

    # Add configurations section
    self.n_configs = self.doc.createElement('Configurations')
    self.n_root.appendChild(self.n_configs)

    # Add files section
    self.n_files = self.doc.createElement('Files')
    self.n_root.appendChild(self.n_files)

    # Add empty Globals section
    self.n_root.appendChild(self.doc.createElement('Globals'))

  def AddConfig(self, name, attrs, tool_attrs):
    """Adds a configuration to the project.

    Args:
      name: Configuration name.
      attrs: Dict of configuration attributes.
      tool_attrs: Dict of tool attributes.
    """
    # Add configuration node
    n_config = self.doc.createElement('Configuration')
    n_config.setAttribute('Name', '%s|Win32' % name)
    n_config.setAttribute('ConfigurationType', '0')
    for k, v in attrs.items():
      n_config.setAttribute(k, v)
    self.n_configs.appendChild(n_config)

    # Add tool node
    n_tool = self.doc.createElement('Tool')
    n_tool.setAttribute('Name', 'VCNMakeTool')
    n_tool.setAttribute('IncludeSearchPath', '')
    n_tool.setAttribute('ForcedIncludes', '')
    n_tool.setAttribute('AssemblySearchPath', '')
    n_tool.setAttribute('ForcedUsingAssemblies', '')
    n_tool.setAttribute('CompileAsManaged', '')
    n_tool.setAttribute('PreprocessorDefinitions', '')
    for k, v in tool_attrs.items():
      n_tool.setAttribute(k, v)
    n_config.appendChild(n_tool)

  def _WalkFolders(self, folder_dict, parent):
    """Recursively walks the folder tree.

    Args:
      folder_dict: Dict of folder entries.  Entry is
           either subdir_name:subdir_dict or relative_path_to_file:None.
      parent: Parent node (folder node for that dict)
    """
    entries = folder_dict.keys()
    entries.sort()
    for e in entries:
      if folder_dict[e]:
        # Folder
        n_subfolder = self.doc.createElement('Filter')
        n_subfolder.setAttribute('Name', e)
        parent.appendChild(n_subfolder)
        self._WalkFolders(folder_dict[e], n_subfolder)
      else:
        # File
        n_file = self.doc.createElement('File')
        n_file.setAttribute('RelativePath', e)
        parent.appendChild(n_file)

  def AddFiles(self, name, files_dict):
    """Adds files to the project.

    Args:
      name: Name of the folder.  If None, files/folders will be added directly
        to the files list.
      files_dict: A dict of files / folders.

    Within the files_dict:
      * A file entry is relative_path:None
      * A folder entry is folder_name:files_dict, where files_dict is another
        dict of this form.
    """
    # Create subfolder if necessary
    if name:
      n_folder = self.doc.createElement('Filter')
      n_folder.setAttribute('Name', name)
      self.n_files.appendChild(n_folder)
    else:
      n_folder = self.n_files

    # Recursively add files to the folder
    self._WalkFolders(files_dict, n_folder)

  def Write(self):
    """Writes the project file."""
    f = open(self.project_path, 'wt')
    self.doc.writexml(f, encoding='Windows-1252', addindent='  ', newl='\n')
    f.close()

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

  vsp = VSProjectWriter(project_file)
  vsp.Create(target_name)

  # Add configuration per build mode supported by this target
  target_path = env['TARGET_PATH']
  for mode, path in target_path.items():
    attrs = {}
    attrs['OutputDirectory'] = '$(ProjectDir)/%s/%s/out' % (mode, target_name)
    attrs['IntermediateDirectory'] = ('$(ProjectDir)/%s/%s/tmp' %
                                      (mode, target_name))

    tool_attrs = {}
    if path:
      tool_attrs['Output'] = env.RelativePath(target[0].dir,
                                              env.Entry(path), sep='/')
    build_cmd = '%s --mode=%s %s' % (hammer_bat, mode, target_name)
    clean_cmd = '%s --mode=%s -c %s' % (hammer_bat, mode, target_name)
    tool_attrs['BuildCommandLine'] = build_cmd
    tool_attrs['CleanCommandLine'] = clean_cmd
    tool_attrs['ReBuildCommandLine'] = clean_cmd + ' && ' + build_cmd

    vsp.AddConfig(mode, attrs, tool_attrs)

  # TODO: Fill in files - at least, the .scons file invoking the target.

  # Write project
  vsp.Write()
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


class SourceWalker(object):
  """Iterator for walking a node tree and collecting sources.

  This is depth-first, children are visited before the parent.  The object
  can be initialized with any node, and returns the next node on the descent
  with each Next() call.

  This class does not get caught in node cycles caused, for example, by C
  header file include loops.

  Based on SCons.Node.Walker.
  """

  def __init__(self, node, seen, print_interval = 1000):
    """Initializes the source walker.

    Args:
      node: Node to start walk from.
      seen: Set to add seen nodes to, if not None.
      print_interval: Interval in nodes examined at which to print a progress
          indicator.
    """
    self.interval = print_interval
    # Put node on stack
    self.stack = [node]
    # Scan for node's children, then copy them to node.wkids
    node.wkids = copy.copy(node.children(scan=1))
    # Keep track of nodes we've seen (returned)
    if seen is None:
      seen = set()
    self.seen = seen
    # Add node to history for cycle detection
    self.history = set()
    self.history.add(node)
    # We've seen one node now
    self.nodes_examined = 1
    self.unique_nodes = 1


  def Next(self):
    """Get the next node for this walk of the tree.

    Returns:
      The next node, or None if no more nodes.

    This function is intentionally iterative, not recursive, to sidestep any
    issues of stack size limitations.
    """
    while self.stack:
      if self.stack[-1].wkids:
        # Node has children we haven't examined, so iterate into the first
        # child
        node = self.stack[-1].wkids.pop(0)
        if not self.stack[-1].wkids:
          # No more children of this node
          self.stack[-1].wkids = None
        self.nodes_examined += 1
        if self.interval and not self.nodes_examined % self.interval:
          self.PrintProgress()
        if (node not in self.history) and (node not in self.seen):
          # Haven't hit a cycle or a node we've already seen
          node.wkids = copy.copy(node.children(scan=1))
          self.stack.append(node)
          self.history.add(node)
      else:
        # Coming back from iterating, so return the next node on the stack.
        node = self.stack.pop()
        self.history.remove(node)
        self.seen.add(node)
        self.unique_nodes += 1
        return node
    return None

  def PrintProgress(self):
    """Prints a progress indicator."""
    print '    Examined %d nodes, found %d unique...' % (
        self.nodes_examined, self.unique_nodes)

  def WalkAll(self):
    """Walks all nodes in the the tree."""
    while self.Next():
      pass
    if self.interval:
      self.PrintProgress()


def ComponentVSSourceProjectBuilder(target, source, env):
  """Visual Studio source project builder.

  Args:
    target: Destination file.
    source: List of sources to be added to the target.
    env: Environment context.

  Returns:
    Zero if successful.
  """
  source = source       # Silence gpylint

  target_name = env['PROJECT_NAME']
  project_file = target[0].path
  project_dir = target[0].dir

  # Get list of suffixes to include
  suffixes = env.SubstList2('$COMPONENT_VS_SOURCE_SUFFIXES')

  # Convert source folders to absolute paths
  folders = []
  for f in env['COMPONENT_VS_SOURCE_FOLDERS']:
    # (folder name, folder abspath, dict of contents)
    folders.append((f[0], env.Dir(f[1]).abspath, {}))

  # TODO: Additional enhancements:
  # * Should be able to specify paths in folder name (i.e., foo/bar) and
  #   create the nested folder nodes ('foo' and 'bar')
  # * Should be tolerant of a folder being specified more than once with
  #   the same name (probably necessary to support nested folder nodes anyway)
  # Can probably accomplish both of those by creating a parent fodler dict and
  # calling WalkFolders() only once.
  # Create a temporary solution alias to point to all the targets, so we can
  # make a single call to SourceWalker()
  tmp_alias = env.Alias('vs_source_project_' + target_name,
      map(env.Alias, env['COMPONENT_VS_SOURCE_TARGETS']))

  # Scan all targets and add unique nodes to set of sources
  print '  Scanning dependency tree ...'
  all_srcs = set()
  walker = SourceWalker(tmp_alias[0], all_srcs)
  walker.WalkAll()

  # Walk all sources and build directory trees
  print '  Building source tree...'
  for n in all_srcs:
    if not hasattr(n, 'rfile'):
      continue  # Not a file
    n = n.rfile()
    if not hasattr(n, 'isfile') or not n.isfile():
      continue  # Not a file
    if n.has_builder():
      continue  # Not a leaf node
    if n.suffix not in suffixes:
      continue  # Not a file type we include

    path = n.abspath
    for f in folders:
      if path.startswith(f[1]):
        if f[0] is None:
          # Folder name of None is a filter
          break
        relpath = path[len(f[1]) + 1:].split(os.sep)
        folder_dict = f[2]
        # Recursively add subdirs
        for pathseg in relpath[:-1]:
          if pathseg not in folder_dict:
            folder_dict[pathseg] = {}
          folder_dict = folder_dict[pathseg]
        # Add file to last subdir.  No dict, since this isn't a subdir
        folder_dict[env.RelativePath(project_dir, path)] = None
        break

  print '  Writing project file...'

  vsp = VSProjectWriter(project_file)
  vsp.Create(target_name)

  # One configuration for all build modes
  vsp.AddConfig('all', {}, {})

  # Add files
  for f in folders:
    if f[0] is None:
      continue  # Skip filters
    vsp.AddFiles(f[0], f[2])

  vsp.Write()
  return 0


def ComponentVSSourceProject(self, project_name, target_names, **kwargs):
  """Visual Studio source project pseudo-builder.

  Args:
    self: Environment context.
    project_name: Name of the project.
    target_names: List of target names to include source for.
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

  # Save the project name and targets
  env['PROJECT_NAME'] = project_name
  env['COMPONENT_VS_SOURCE_TARGETS'] = target_names

  # Call the real builder
  return env.ComponentVSSourceProjectBuilder(
      '$COMPONENT_VS_PROJECT_DIR/${PROJECT_NAME}', [])

#------------------------------------------------------------------------------


def FindSources(env, dest, source, suffixes=None):
  """Recursively finds sources and adds them to the destination set.

  Args:
    env: Environment context.
    dest: Set to add source nodes to.
    source: Source file(s) to find.  May be a string, Node, or a list of
        mixed strings or Nodes.  Strings will be passed through env.Glob() to
        evaluate wildcards.  If a source evaluates to a directory, the entire
        directory will be recursively added.
    suffixes: List of suffixes to include.  If not None, only files with these
        suffixes will be added to dest.
  """
  for source_entry in env.Flatten(source):
    if type(source_entry) == str:
      # Search for matches for each source entry
      source_nodes = env.Glob(source_entry)
    else:
      # Source entry is already a file or directory node; no need to glob it
      source_nodes = [source_entry]
    for s in source_nodes:
      if str(s.__class__) == 'SCons.Node.FS.Dir':
        # Recursively search subdir.  Since glob('*') doesn't match dot files,
        # also glob('.*').
        FindSources(env, dest, [s.abspath + '/*', s.abspath + '/.*'], suffixes)
      elif suffixes and s.suffix in suffixes:
        dest.add(s)


def ComponentVSDirProjectBuilder(target, source, env):
  """Visual Studio directory project builder.

  Args:
    target: Destination file.
    source: List of sources to be added to the target.
    env: Environment context.

  Returns:
    Zero if successful.
  """
  source = source       # Silence gpylint

  target_name = env['PROJECT_NAME']
  project_file = target[0].path
  project_dir = target[0].dir

  # Convert source folders to absolute paths
  folders = []
  for f in env['COMPONENT_VS_SOURCE_FOLDERS']:
    # (folder name, folder abspath, dict of contents)
    folders.append((f[0], env.Dir(f[1]).abspath, {}))

  # Recursively scan source directories
  print '  Scanning directories for source...'
  all_srcs = set()
  FindSources(env, all_srcs, env['PROJECT_SOURCES'],
              suffixes=env.SubstList2('$COMPONENT_VS_SOURCE_SUFFIXES'))

  # Walk all sources and build directory trees
  print '  Building source tree...'
  for n in all_srcs:
    # Map addRepository'd source to its real location.
    path = n.rfile().abspath
    for f in folders:
      if path.startswith(f[1]):
        if f[0] is None:
          # Folder name of None is a filter
          break
        relpath = path[len(f[1]) + 1:].split(os.sep)
        folder_dict = f[2]
        # Recursively add subdirs
        for pathseg in relpath[:-1]:
          if pathseg not in folder_dict:
            folder_dict[pathseg] = {}
          folder_dict = folder_dict[pathseg]
        # Add file to last subdir.  No dict, since this isn't a subdir
        folder_dict[env.RelativePath(project_dir, path)] = None
        break

  print '  Writing project file...'

  vsp = VSProjectWriter(project_file)
  vsp.Create(target_name)

  # One configuration for all build modes
  vsp.AddConfig('all', {}, {})

  # Add files
  for f in folders:
    if f[0] is None:
      continue  # Skip filters
    vsp.AddFiles(f[0], f[2])

  vsp.Write()
  return 0


def ComponentVSDirProject(self, project_name, source, **kwargs):
  """Visual Studio directory project pseudo-builder.

  Args:
    self: Environment context.
    project_name: Name of the project.
    source: List of source files and/or directories.
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

  # Save the project name and sources
  env['PROJECT_NAME'] = project_name
  env['PROJECT_SOURCES'] = source

  # Call the real builder
  return env.ComponentVSDirProjectBuilder(
      '$COMPONENT_VS_PROJECT_DIR/${PROJECT_NAME}', [])

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

  # Scan externally-generated projects
  external_projects = []
  for p in source:
    guid, name = GetGuidAndNameFromVSProject(p.abspath)
    external_projects.append((p, name, guid))

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
  for p, name, guid in external_projects:
    f.write('Project("%s") = "%s", "%s", "%s"\nEndProject\n' % (
        # TODO: What if this project isn't type external makefile?
        # How to tell what type it is?
        '{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}',    # External makefile GUID
        name,                                        # Project name
        env.RelativePath(target[0].dir, p),          # Path to project file
        guid,                                        # Project GUID
    ))

  # Folders from build groups
  # TODO: Currently no way to add external project (specified in sources) to a
  # solution folder.
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

  # Determine which projects should be enabled
  # TODO: This is somewhat clunky.  DEFAULT_TARGETS is global, and what we
  # really need is something mode-specific.  In theory we could make this a
  # mode-specific dict rather than a list, but that'd also be a pain to
  # populate.
  # These variable names are also getting REALLY long.  Perhaps we should
  # define shorter ones (with the default value redirecting to the longer
  # ones for legacy compatibility).
  enable_projects = env.SubstList2('$COMPONENT_VS_ENABLED_PROJECTS')

  f.write('\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n')

  # Projects generated by ComponentVSSolution()
  for p in projects:
    for mode in GetTargetModes():
      f.write('\t\t%s.%s|Win32.ActiveCfg = %s|Win32\n' % (
          MakeGuid(p),    # Project GUID
          mode,           # Solution build configuration
          mode,           # Project build config for that solution config
      ))

      t = GetTargets().get(p)

      # Determine if project should be enabled in this mode
      enabled = t and mode in t.mode_properties
      if enable_projects and p not in enable_projects:
        # Enable list specified, but target isn't in it
        # TODO: Since we env.Default(scons-out) elsewhere, this likely causes
        # all projects to be disabled by default.  But that's realistically
        # better than enabling them all...
        enabled = False

      if enabled:
        # Target can be built in this mode
        f.write('\t\t%s.%s|Win32.Build.0 = %s|Win32\n' % (
            MakeGuid(p),    # Project GUID
            mode,           # Solution build configuration
            mode,           # Project build config for that solution config
        ))

  # Projects generated elsewhere
  for p, name, guid in external_projects:
    for mode in GetTargetModes():
      f.write('\t\t%s.%s|Win32.ActiveCfg = %s|Win32\n' % (
          guid,           # Project GUID
          mode,           # Solution build configuration
          mode,           # Project build config for that solution config
      ))

      if name in enable_projects or not enable_projects:
        # Build target in this mode
        f.write('\t\t%s.%s|Win32.Build.0 = %s|Win32\n' % (
            guid,           # Project GUID
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
    solution_name: Name of the solution.
    target_names: Names of targets or target groups to include in the solution.
        This will automatically build projects for them.
    projects: List of aditional projects not generated by this solution to
        include in the solution.
    kwargs: Optional keyword arguments override environment variables in the
        derived environment used to create the solution.

  Returns:
    The list of output nodes.
  """
  # TODO: Should have option to build source project also.  Perhaps select
  # using a --source_project option, since it needs to use gather_inputs
  # to scan the DAG and will blow up the null build time.
  # TODO: Should also have autobuild_projects option.  If false, don't build
  # them.
  # TODO: Should also be able to specify a target group directly
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

  # Save the default targets list as an environment variable
  env['COMPONENT_VS_SCONS_DEFAULT_TARGETS'] = SCons.Script.DEFAULT_TARGETS

  # Expand target_names into project names, and create project-to-folder
  # mappings
  project_names = {}
  folders = []
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
  # TODO: Should really defer the rest of the work above, since otherwise we
  # can't build a solution which has a target to rebuild itself.
  env.Alias('all_solutions', env.Alias(solution_name, out_nodes))

  # TODO: To rebuild the solution properly, need to override its project
  # configuration so it only has '--mode=all' (or some other way of setting the
  # subset of modes which it should use to rebuild itself).  Rebuilding with
  # the property below will strip it down to just the current build mode, which
  # isn't what we want.
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
  env.AddMethod(ComponentVSDirProject)
  env.AddMethod(ComponentVSProject)
  env.AddMethod(ComponentVSSolution)
  env.AddMethod(ComponentVSSourceProject)

  # If not on Windows, don't do anything else
  if sys.platform not in ('win32', 'cygwin'):
    return

  # Include tools we need
  env.Tool('gather_inputs')

  env.SetDefault(
      COMPONENT_VS_PROJECT_DIR='$COMPONENT_VS_SOLUTION_DIR/projects',
      COMPONENT_VS_PROJECT_SCRIPT_NAME = 'hammer.bat',
      COMPONENT_VS_PROJECT_SCRIPT_PATH = (
          '$$(ProjectDir)/$VS_PROJECT_TO_MAIN_DIR/'
          '$COMPONENT_VS_PROJECT_SCRIPT_NAME'),
      COMPONENT_VS_PROJECT_SUFFIX='.vcproj',

      COMPONENT_VS_SOLUTION_DIR='$DESTINATION_ROOT/solution',
      COMPONENT_VS_SOLUTION_SUFFIX='.sln',
      COMPONENT_VS_ENABLED_PROJECTS=['$COMPONENT_VS_SCONS_DEFAULT_TARGETS'],

      COMPONENT_VS_SOURCE_SUFFIXES=['$CPPSUFFIXES', '.rc', '.scons'],
      COMPONENT_VS_SOURCE_FOLDERS=[('source', '$MAIN_DIR')],
  )

  AddTargetGroup('all_solutions', 'solutions can be built')

  # Add builders
  vcprojaction = SCons.Script.Action(ComponentVSProjectBuilder, varlist=[
      'COMPONENT_VS_PROJECT_SCRIPT_PATH',
      'TARGET_NAME',
      'TARGET_PATH',
  ])
  vcprojbuilder = SCons.Script.Builder(
      action=vcprojaction,
      suffix='$COMPONENT_VS_PROJECT_SUFFIX')

  source_vcproj_action = SCons.Script.Action(
      ComponentVSSourceProjectBuilder, varlist=[
          'COMPONENT_VS_SOURCE_FOLDERS',
          'COMPONENT_VS_SOURCE_SUFFIXES',
          'COMPONENT_VS_SOURCE_TARGETS',
      ])
  source_vcproj_builder = SCons.Script.Builder(
      action=source_vcproj_action,
      suffix='$COMPONENT_VS_PROJECT_SUFFIX')

  dir_vcproj_action = SCons.Script.Action(
      ComponentVSDirProjectBuilder, varlist=[
          'COMPONENT_VS_SOURCE_FOLDERS',
          'COMPONENT_VS_SOURCE_SUFFIXES',
          'PROJECT_SOURCES',
      ])
  dir_vcproj_builder = SCons.Script.Builder(
      action=dir_vcproj_action,
      suffix='$COMPONENT_VS_PROJECT_SUFFIX')

  slnaction = SCons.Script.Action(ComponentVSSolutionBuilder, varlist=[
      'COMPONENT_VS_ENABLED_PROJECTS',
      'SOLUTION_FOLDERS',
      'SOLUTION_PROJECTS',
      'SOLUTION_TARGETS',
  ])
  slnbuilder = SCons.Script.Builder(
      action=slnaction,
      suffix='$COMPONENT_VS_SOLUTION_SUFFIX')

  env.Append(BUILDERS={
      'ComponentVSDirProjectBuilder': dir_vcproj_builder,
      'ComponentVSProjectBuilder': vcprojbuilder,
      'ComponentVSSolutionBuilder': slnbuilder,
      'ComponentVSSourceProjectBuilder': source_vcproj_builder,
  })
