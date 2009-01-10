#
# __COPYRIGHT__
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

__revision__ = "__FILE__ __REVISION__ __DATE__ __DEVELOPER__"

__doc__ = """SCons.Node.MSVS

New implementation of Visual Studio project generation for SCons.
"""

import md5
import os
import random
import re
import UserList
import xml.dom
import xml.dom.minidom

import SCons.Node.FS
import SCons.Script

from SCons.Debug import Trace
TODO = 0

# Initialize random number generator
random.seed()


#------------------------------------------------------------------------------
# Entry point for supplying a fixed map of GUIDs for testing.

GUIDMap = {}


#------------------------------------------------------------------------------
# Helper functions


def MakeGuid(name, seed='msvs_new'):
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
# Global look up of string names.

class LookupError(Exception):
  def __str__(self):
    string, expanded = self.args
    if string == expanded:
        return string
    else:
       return '%s (%s)' % (string, expanded)

_lookup_dict = {}

def LookupAdd(item, result):
  _lookup_dict[item] = result
  _lookup_dict[result] = result

def Lookup(item):
  """Looks up an MSVS item in the global dictionary.

  Args:
    item: A path (string) or instance for looking up.
  Returns:
    An instance from the global _lookup_dict.

  Raises an exception if the item does not exist in the _lookup_dict.
  """
  global _lookup_dict
  try:
    return _lookup_dict[item]
  except KeyError:
    return SCons.Node.FS.default_fs.Entry(item, create=False)

def LookupCreate(klass, item, *args, **kw):
  """Looks up an MSVS item, creating it if it doesn't already exist.

  Args:
    klass: The class of item being looked up, or created if it
        doesn't already exist in the global _lookup_dict.
    item: The a string (or instance) being looked up.
    *args: positional arguments passed to the klass.initialize() method.
    **kw: keyword arguments passed to the klass.initialize() method.
  Returns:
    An instance from the global _lookup_dict, or None if the item does
        not exist in the _lookup_dict.

  This raises a LookupError if the found instance doesn't match the
  requested klass.

  When creating a new instance, this populates the _lookup_dict with
  both the item and the instance itself as keys, so that looking up
  the instance will return itself.
  """
  global _lookup_dict
  result = _lookup_dict.get(item)
  if result:
    if not isinstance(result, klass):
      raise LookupError, "tried to redefine %s as a %s" % (item, klass)
    return result
  result = klass()
  result.initialize(item, *args, **kw)
  LookupAdd(item, result)
  return result


#------------------------------------------------------------------------------

class FileList(object):
  def __init__(self, entries=None):
    if isinstance(entries, FileList):
      entries = entries.entries
    self.entries = entries or []
  def __getitem__(self, i):
    return self.entries[i]
  def __setitem__(self, i, item):
    self.entries[i] = item
  def __delitem__(self, i):
    del self.entries[i]
  def __add__(self, other):
    if isinstance(other, FileList):
      return self.__class__(self.entries + other.entries)
    elif isinstance(other, type(self.entries)):
      return self.__class__(self.entries + other)
    else:
      return self.__class__(self.entries + list(other))
  def __radd__(self, other):
    if isinstance(other, FileList):
      return self.__class__(other.entries + self.entries)
    elif isinstance(other, type(self.entries)):
      return self.__class__(other + self.entries)
    else:
      return self.__class__(list(other) + self.entries)
  def __iadd__(self, other):
    if isinstance(other, FileList):
      self.entries += other.entries
    elif isinstance(other, type(self.entries)):
      self.entries += other
    else:
      self.entries += list(other)
    return self
  def append(self, item):
    return self.entries.append(item)
  def extend(self, item):
    return self.entries.extend(item)
  def index(self, item, *args):
    return self.entries.index(item, *args)
  def remove(self, item):
    return self.entries.remove(item)

def FileListWalk(top, topdown=True, onerror=None):
  """
  """
  try:
    entries = top.entries
  except AttributeError, err:
    if onerror is not None:
      onerror(err)
    return

  dirs, nondirs = [], []
  for entry in entries:
    if hasattr(entry, 'entries'):
      dirs.append(entry)
    else:
      nondirs.append(entry)

  if topdown:
    yield top, dirs, nondirs
  for entry in dirs:
    for x in FileListWalk(entry, topdown, onerror):
      yield x
  if not topdown:
    yield top, dirs, nondirs

#------------------------------------------------------------------------------

class _MSVSFolder(FileList):
  """Folder in a Visual Studio solution."""

  entry_type_guid = '{2150E333-8FDC-42A3-9474-1A3956D46DE8}'

  def initialize(self, path, name = None, entries = None, guid = None, items = None):
    """Initializes the folder.

    Args:
      path: The unique name of the folder, by which other MSVS Nodes can
          refer to it.  This is not necessarily the name that gets printed
          in the .sln file.
      name: The name of this folder as actually written in a generated
          .sln file.  The default is
      entries: List of folder entries to nest inside this folder.  May contain
          Folder or Project objects.  May be None, if the folder is empty.
      guid: GUID to use for folder, if not None.
      items: List of solution items to include in the folder project.  May be
          None, if the folder does not directly contain items.
    """
    super(_MSVSFolder, self).__init__(entries)

    # For folder entries, the path is the same as the name
    self.msvs_path = path
    self.msvs_name = name or path

    self.guid = guid

    # Copy passed lists (or set to empty lists)
    self.items = list(items or [])

  def get_guid(self):
    if self.guid is None:
      guid = GUIDMap.get(self.msvs_path)
      if not guid:
        # The GUID for the folder can be random, since it's used only inside
        # solution files and doesn't need to be consistent across runs.
        guid = MakeGuid(random.random())
      self.guid = guid
    return self.guid

  def get_msvs_path(self, sln):
      return self.msvs_name

def MSVSFolder(env, item, *args, **kw):
  return LookupCreate(_MSVSFolder, item, *args, **kw)

#------------------------------------------------------------------------------

class MSVSConfig(object):
  """Visual Studio configuration."""
  def __init__(self, Name, config_type, tools=None, **attrs):
    """Initializes the configuration.

    Args:
      **attrs: Configuration attributes.
    """
    # Special handling for attributes that we want to make more
    # convenient for the user.
    ips = attrs.get('InheritedPropertySheets')
    if ips:
      if isinstance(ips, list):
        ips = ';'.join(ips)
      attrs['InheritedPropertySheets'] = ips.replace('/', '\\')

    self.Name = Name
    self.config_type = config_type
    self.tools = tools
    self.attrs = attrs

  def CreateElement(self, doc, project):
    """Creates an element for the configuration.

    Args:
      doc: xml.dom.Document object to use for node creation.

    Returns:
      A new xml.dom.Element for the configuration.
    """
    node = doc.createElement(self.config_type)
    node.setAttribute('Name', self.Name)
    for k, v in self.attrs.items():
      node.setAttribute(k, v)

    tools = self.tools
    if tools is None:
        tools = project.tools or []
    if not SCons.Util.is_List(tools):
      tools = [tools]
    tool_objects = []
    for t in tools:
      if not isinstance(t, MSVSTool):
        t = MSVSTool(t)
      tool_objects.append(t)
    for t in tool_objects:
        node.appendChild(t.CreateElement(doc))

    return node


class MSVSFileListBase(FileList):
  """Base class for a file list in a Visual Studio project file."""

  def CreateElement(self, doc, node_func=lambda x: x):
    """Creates an element for an MSVSFileListBase subclass.

    Args:
      doc: xml.dom.Document object to use for node creation.
      node_func: Function to use to return Nodes for objects that
          don't have a CreateElement() method of their own.

    Returns:
      A new xml.dom.Element for the MSVSFileListBase object.
    """
    node = doc.createElement(self.element_name)
    for entry in self.entries:
      if hasattr(entry, 'CreateElement'):
        n = entry.CreateElement(doc, node_func)
      else:
        n = node_func(entry)
      node.appendChild(n)
    return node


class MSVSFiles(MSVSFileListBase):
  """Files list in a Visual Studio project file."""
  element_name = 'Files'


class MSVSFilter(MSVSFileListBase):
  """Filter (that is, a virtual folder) in a Visual Studio project file."""

  element_name = 'Filter'

  def __init__(self, Name, entries=None):
    """Initializes the folder.

    Args:
      Name: Filter (folder) name.
      entries: List of filenames and/or Filter objects contained.
    """
    super(MSVSFilter, self).__init__(entries)
    self.Name = Name

  def CreateElement(self, doc, node_func=lambda x: x):
    """Creates an element for the Filter.

    Args:
      doc: xml.dom.Document object to use for node creation.
      node_func: Function to use to return Nodes for objects that
          don't have a CreateElement() method of their own.

    Returns:
      A new xml.dom.Element for the filter.
    """
    node = super(MSVSFilter, self).CreateElement(doc, node_func)
    node.setAttribute('Name', self.Name)
    return node


class MSVSTool(object):
  """Visual Studio tool."""

  def __init__(self, Name, **attrs):
    """Initializes the tool.

    Args:
      Name: Tool name.
      **attrs: Tool attributes.
    """
    self.Name = Name
    self.attrs = attrs

  def CreateElement(self, doc):
    """Creates an element for the tool.

    Args:
      doc: xml.dom.Document object to use for node creation.

    Returns:
      A new xml.dom.Element for the tool.
    """
    node = doc.createElement('Tool')
    node.setAttribute('Name', self.Name)
    for k, v in self.attrs.items():
      node.setAttribute(k, v)
    return node

  def _format(self):
    """Formats a tool specification for debug printing"""
    xml_impl = xml.dom.getDOMImplementation()
    doc = xml_impl.createDocument(None, 'VisualStudioProject', None)
    return self.CreateElement(doc).toprettyxml()

  def diff(self, other):
    for key, value in self.attrs.items():
      if other.attrs[key] == value:
        del self.attrs[key]


class MSVSToolFile(object):
  """Visual Studio tool file specification."""

  def __init__(self, node, **attrs):
    """Initializes the tool.

    Args:
      node: Node for the Tool File
      **attrs: Tool File attributes.
    """
    self.node = node

  def CreateElement(self, doc, project):
      result = doc.createElement('ToolFile')
      result.setAttribute('RelativePath', project.get_rel_path(self.node))
      return result


#------------------------------------------------------------------------------

def MSVSAction(target, source, env):
  target[0].Write(env)

MSVSProjectAction = SCons.Script.Action(MSVSAction,
                                        "Generating Visual Studio project `$TARGET' ...")

class _MSVSProject(SCons.Node.FS.File):
  """Visual Studio project."""

  entry_type_guid = '{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}'
  initialized = False

  def initialize(self, env, path, name = None,
                                 dependencies = None,
                                 guid = None,
                                 buildtargets = [],
                                 files = [],
                                 root_namespace = None,
                                 relative_path_prefix = '',
                                 tools = None,
                                 configurations = None,
                                 **attrs):
    """Initializes the project.

    Args:
      path: Relative path to project file.
      name: Name of project.  If None, the name will be the same as the base
          name of the project file.
      dependencies: List of other Project objects this project is dependent
          upon, if not None.
      guid: GUID to use for project, if not None.
      buildtargets: List of target(s) being built by this project.
      files: List of source files for the project.  This will be
          supplemented by any source files of buildtargets.
      root_namespace: The value of the RootNamespace attribute of the
          project, if not None.  The default is to use the same
          string as the name.
      relative_path_prefix: A prefix to be appended to the beginning of
          every file name in the list.  The canonical use is to specify
          './' to make files explicitly relative to the local directory.
      tools: A list of MSVSTool objects or strings representing
          tools to be used to build this project.  This will be used
          for any configurations that don't provide their own
          per-configuration tool list.
      configurations: A list of MSVSConfig objects representing
          configurations built by this project.
    """
    if name is None:
      if buildtargets:
        name = os.path.splitext(buildtargets[0].name)[0]
      else:
        name = os.path.splitext(os.path.basename(path))[0]
    if not root_namespace:
        root_namespace or name

    if self.initialized:
      # TODO(sgk):  fill in
      if self.msvs_name != name:
        pass
      if self.root_namespace != root_namespace:
        pass
      if self.relative_path_prefix != relative_path_prefix:
        pass
      if self.guid != guid:
        pass
      #if self.env != env:
      #  pass
    else:
      self.buildtargets = []
      self.configurations = []
      self.dependencies = []
      self.file_configurations = {}
      self.files = MSVSFiles([])
      self.tool_files = []
      self.file_lists = []
      self.initialized = True

    self.attrs = attrs
    self.env = env
    self.guid = guid
    self.msvs_name = name
    self.msvs_path = path
    self.relative_path_prefix = relative_path_prefix
    self.root_namespace = root_namespace or self.msvs_name
    self.tools = tools

    self.buildtargets.extend(buildtargets)
    self.configurations.extend(configurations or [])
    self.dependencies.extend(list(dependencies or []))
    self.AddFiles(files)

    env.Command(self, [], MSVSProjectAction)

  def args2nodes(self, entries):
    result = []
    for entry in entries:
      if SCons.Util.is_String(entry):
        entry = self.env.File(entry)
        result.append(entry)
      elif hasattr(entry, 'entries'):
        entry.entries = self.args2nodes(entry.entries)
        result.append(entry)
      elif isinstance(entry, (list, UserList.UserList)):
        result.extend(self.args2nodes(entry))
      elif hasattr(entry, 'sources') and entry.sources:
        result.extend(entry.sources)
      else:
        result.append(entry.srcnode())
    return result

  def FindFile(self, node):
    try:
      flat_file_dict = self.flat_file_dict
    except AttributeError:
      flat_file_dict = {}
      file_list = self.files[:]
      while file_list:
        entry = file_list.pop(0)
        if not isinstance(entry, (list, UserList.UserList)):
          entry = [entry]
        for f in entry:
          if hasattr(f, 'entries'):
            file_list.extend(f.entries)
          else:
            flat_file_dict[f] = True
            flat_file_dict[f.srcnode()] = True
            if hasattr(f, 'sources'):
              for s in f.sources:
                flat_file_dict[s] = True
                flat_file_dict[s.srcnode()] = True
      self.flat_file_dict = flat_file_dict

    return flat_file_dict.get(node)

  def get_guid(self):
    if self.guid is None:
      guid = GUIDMap.get(self.msvs_path)
      if not guid:
        # Set GUID from path
        # TODO(rspangler): This is fragile.
        # 1. We can't just use the project filename sans path, since there
        #    could be multiple projects with the same base name (for example,
        #    foo/unittest.vcproj and bar/unittest.vcproj).
        # 2. The path needs to be relative to $SOURCE_ROOT, so that the project
        #    GUID is the same whether it's included from base/base.sln or
        #    foo/bar/baz/baz.sln.
        # 3. The GUID needs to be the same each time this builder is invoked,
        #    so that we don't need to rebuild the solution when the
        #    project changes.
        # 4. We should be able to handle pre-built project files by reading the
        #    GUID from the files.
        guid = MakeGuid(self.msvs_path)
      self.guid = guid
    return self.guid

  def get_msvs_path(self, sln):
      return sln.rel_path(self).replace('/', '\\')

  def get_rel_path(self, node):
      result = self.relative_path_prefix + self.rel_path(node)
      return result.replace('/', '\\')

  def AddConfig(self, Name, tools=None, **attrs):
    """Adds a configuration to the parent node.

    Args:
      Name: The name of the configuration.
      tools: List of tools (strings or Tool objects); may be None.
      **attrs: Configuration attributes.
    """
    if tools is None:
      # No tool list specifically for this configuration,
      # use the Project's as a default.
      tools = self.tools
    if not attrs.has_key('ConfigurationType'):
      # No ConfigurationType specifically for this configuration,
      # use the Project's as a default.
      try:
        attrs['ConfigurationType'] = self.attrs['ConfigurationType']
      except KeyError:
        pass
    if attrs.has_key('InheritedPropertySheets'):
      ips = attrs['InheritedPropertySheets']
      attrs['InheritedPropertySheets'] = self.env.subst(ips)
    c = MSVSConfig(Name, 'Configuration', tools=tools, **attrs)
    self.configurations.append(c)

  def AddFiles(self, files):
    """Adds files to the project.

    Args:
      files: A list of Filter objects and/or relative paths to files.

    This makes a copy of the file/filter tree at the time of this call.  If you
    later add files to a Filter object which was passed into a previous call
    to AddFiles(), it will not be reflected in this project.
    """
    self.file_lists.append(self.args2nodes(files))

  def _FilesToSourceFiles(self, files):
    file_list = files[:]
    result = []
    while file_list:
      entry = file_list.pop(0)
      if not isinstance(entry, (list, UserList.UserList)):
        entry = [entry]
      for f in entry:
        if hasattr(f, 'entries'):
          self._FilesToSourceFiles(f.entries)
          result.append(f)
        else:
          if f.sources:
            flist = f.sources
          else:
            flist = [f]
          for x in flist:
            result.append(x.srcnode())
    files[:] = result

  def _MergeFiles(self, dest_list, src_list):
    for f in src_list:
      if f not in dest_list:
        dest_list.append(f)
        continue
      #if hasattr(f, 'entries'):
      #  self._FilesToSourceFiles(f.entries)

  def AddFileConfig(self, path, Name, tools=None, **attrs):
    """Adds a configuration to a file.

    Args:
      path: Relative path to the file.
      Name: Name of configuration to add.
      tools: List of tools (strings or MSVSTool objects); may be None.
      **attrs: Configuration attributes.

    Raises:
      ValueError: Relative path does not match any file added via AddFiles().
    """
    # Store as the VariantDir node, not as the source node.
    node = self.env.File(path)
    c = MSVSConfig(Name, 'FileConfiguration', tools=tools, **attrs)
    config_list = self.file_configurations.get(node)
    if config_list is None:
        config_list = []
        self.file_configurations[node] = config_list
    config_list.append(c)

  def AddToolFile(self, path):
    """Adds a tool file to the project.

    Args:
      path: Relative path from project to tool file.
    """
    tf = MSVSToolFile(self.env.File(path))
    self.tool_files.append(tf)

  def Create(self):
    """Creates the project document.

    Args:
      name: Name of the project.
      guid: GUID to use for project, if not None.
    """
    # Create XML doc
    xml_impl = xml.dom.getDOMImplementation()
    self.doc = xml_impl.createDocument(None, 'VisualStudioProject', None)

    # Add attributes to root element
    root = self.doc.documentElement
    root.setAttribute('ProjectType', 'Visual C++')
    root.setAttribute('Version', '8.00')
    root.setAttribute('Name', self.msvs_name)
    root.setAttribute('ProjectGUID', self.get_guid())
    root.setAttribute('RootNamespace', self.root_namespace)
    root.setAttribute('Keyword', 'Win32Proj')

    # Add platform list
    platforms = self.doc.createElement('Platforms')
    root.appendChild(platforms)
    n = self.doc.createElement('Platform')
    n.setAttribute('Name', 'Win32')
    platforms.appendChild(n)

    # Add tool files section
    tool_files = self.doc.createElement('ToolFiles')
    root.appendChild(tool_files)
    for tf in self.tool_files:
        tool_files.appendChild(tf.CreateElement(self.doc, self))

    # Add configurations section
    configs = self.doc.createElement('Configurations')
    root.appendChild(configs)
    for c in self.configurations:
        configs.appendChild(c.CreateElement(self.doc, self))

    # Add empty References section
    root.appendChild(self.doc.createElement('References'))

    # Add files section
    root.appendChild(self.files.CreateElement(self.doc, self.CreateFileElement))

    # Add empty Globals section
    root.appendChild(self.doc.createElement('Globals'))

  def CreateFileElement(self, file):
    """Create a DOM node for the specified file.

    Args:
      file: The file Node being considered.

    Returns:
      A DOM Node for the File, with a relative path to the current
      project object, and any file configurations attached to the
      project.
    """

    node = self.doc.createElement('File')
    node.setAttribute('RelativePath', self.get_rel_path(file))
    for c in self.file_configurations.get(file, []):
      node.appendChild(c.CreateElement(self.doc, self))
    return node

  def VCCLCompilerTool(self, args):
    default_attrs = {
      'BufferSecurityCheck' : "false",
      'CompileAs' : 0,                          # default
      'DebugInformationFormat' : 0,             # TODO(???)
      'DisableSpecificWarnings' : [],
      'EnableFiberSafeOptimizations' : "false",
      'EnableFunctionLevelLinking' : "false",
      'EnableIntrinsicFunctions' : "false",
      'FavorSizeOrSpeed' : 0,                   # favorNone
      'InlineFunctionExpansion' : 1,            # expandDisable
      'MinimalRebuild' : "false",
      'OmitFramePointers' : "false",
      'Optimization' : 1,                       # optimizeDisabled TODO(???)
      'PreprocessorDefinitions' : [],
      'RuntimeLibrary' : TODO,
      'RuntimeTypeInfo' : "false",
      'StringPooling' : "false",
      'SuppressStartupBanner' : "false",
      'WarningAsError' : "false",
      'WarningLevel' : 1,                       # warningLevel_1
      'WholeProgramOptimization' : "false",
    }

    tool = MSVSTool('VCCLCompilerTool', **default_attrs)
    attrs = tool.attrs

    for arg in args:
      if arg in ('/c',):
        continue
      if arg.startswith('/Fo'):
        continue
      if arg.startswith('/D'):
        attrs['PreprocessorDefinitions'].append(arg[2:])
      elif arg == '/EH':
        attrs['ExceptionHandling'] = 0
      elif arg == '/GF':
        attrs['StringPooling'] = "true"
      elif arg == '/GL':
        attrs['WholeProgramOptimization'] = "true"
      elif arg == '/GM':
        attrs['MinimalRebuild'] = "true"
      elif arg == '/GR-':
        attrs['RuntimeTypeInfo'] = "true"
      elif arg == '/Gs':
        attrs['BufferSecurityCheck'] = "true"
      elif arg == '/Gs-':
        attrs['BufferSecurityCheck'] = "false"
      elif arg == '/GT':
        attrs['EnableFiberSafeOptimizations'] = "true"
      elif arg == '/Gy':
        attrs['EnableFunctionLevelLinking'] = "true"
      elif arg == '/MD':
        attrs['RuntimeLibrary'] = 1             # rtMultiThreadedDebug
      elif arg == '/MDd':
        attrs['RuntimeLibrary'] = 2             # rtMultiThreadedDebugDLL
      elif arg == '/MT':
        attrs['RuntimeLibrary'] = 0             # rtMultiThreaded
      elif arg == '/MTd':
        attrs['RuntimeLibrary'] = 3             # rtMultiThreadedDLL
      elif arg == '/nologo':
        attrs['SuppressStartupBanner'] = "true"
      elif arg == '/O1':
        attrs['InlineFunctionExpansion'] = 4    # optimizeMinSpace
      elif arg == '/O2':
        attrs['InlineFunctionExpansion'] = 3    # optimizeMaxSpeed
      elif arg == '/Ob1':
        attrs['InlineFunctionExpansion'] = 2    # expandOnlyInline
      elif arg == '/Ob2':
        attrs['InlineFunctionExpansion'] = 0    # expandAnySuitable
      elif arg == '/Od':
        attrs['Optimization'] = 0
      elif arg == '/Oi':
        attrs['EnableIntrinsicFunctions'] = "true"
      elif arg == '/Os':
        attrs['FavorSizeOrSpeed'] = 1           # favorSize
      elif arg == '/Ot':
        attrs['FavorSizeOrSpeed'] = 2           # favorSpeed
      elif arg == '/Ox':
        attrs['Optimization'] = 2               # optimizeFull
      elif arg == '/Oy':
        attrs['OmitFramePointers'] = "true"
      elif arg == '/Oy-':
        attrs['TODO'] = "true"
      elif arg in ('/Tc', '/TC'):
        attrs['CompileAs'] = 1 # compileAsC
      elif arg in ('/Tp', '/TP'):
        attrs['CompileAs'] = 2 # compileAsCPlusPlus
      elif arg == '/WX':
        attrs['WarnAsError'] = "true"
      elif arg.startswith('/W'):
        attrs['WarningLevel'] = int(arg[2:])    # 0 through 4
      elif arg.startswith('/wd'):
        attrs['DisableSpecificWarnings'].append(str(arg[3:]))
      elif arg == '/Z7':
        attrs['DebugInformationFormat'] = 3 # debugOldSytleInfo TODO(???)
      elif arg == '/Zd':
        attrs['DebugInformationFormat'] = 0 # debugDisabled
      elif arg == '/Zi':
        attrs['DebugInformationFormat'] = 2 # debugEnabled TODO(???)
      elif arg == '/ZI':
        attrs['DebugInformationFormat'] = 1 # debugEditAndContinue TODO(???)

    cppdefines = attrs['PreprocessorDefinitions']
    if cppdefines:
      attrs['PreprocessorDefinitions'] = ';'.join(cppdefines)
    warnings = attrs['DisableSpecificWarnings']
    if warnings:
      warnings = SCons.Util.uniquer(warnings)
      attrs['DisableSpecificWarnings'] = ';'.join(warnings)

    return tool

  def VCLibrarianTool(self, args):
    default_attrs = {
      'LinkTimeCodeGeneration' : "false",
      'SuppressStartupBanner' : "false",
    }

    tool = MSVSTool('VCLibrarianTool', **default_attrs)
    attrs = tool.attrs

    for arg in args:
      if arg.startswith('/OUT'):
        continue
      if arg == '/ltcg':
        attrs['LinkTimeCodeGeneration'] = "true"
      elif arg == '/nologo':
        attrs['SuppressStartupBanner'] = "true"

    return tool

  def VCLinkerTool(self, args):
    default_attrs = {
      'LinkIncremental' : "false",
      'LinkTimeCodeGeneration' : "false",
      'EnableCOMDATFolding' : TODO,
      'OptimizeForWindows98' : TODO,
      'OptimizeReferences' : TODO,
      'Profile' : "false",
      'SuppressStartupBanner' : "false",
    }

    tool = MSVSTool('VCLinkerTool', **default_attrs)
    attrs = tool.attrs

    for arg in args:
      if arg == '':
        continue
      if arg == '/INCREMENTAL':
        attrs['LinkIncremental'] = "true"
      elif arg == '/INCREMENTAL:NO':
        attrs['LinkIncremental'] = "false"
      elif arg == '/LTCG':
        attrs['LinkTimeCodeGeneration'] = "true"
      elif arg == '/nologo':
        attrs['SuppressStartupBanner'] = "true"
      elif arg == '/OPT:NOICF':
        attrs['EnableCOMDATFolding'] = 2 #
      elif arg == '/OPT:NOWIN98':
        attrs['OptimizeForWindows98'] = 1 #
      elif arg == '/OPT:REF':
        attrs['OptimizeReferences'] = 2 #
      elif arg == '/PROFILE':
        attrs['Profile'] = "true"

    return tool

  command_to_tool_map = {
    'cl' : 'VCCLCompilerTool',
    'cl.exe' : 'VCCLCompilerTool',
    'lib' : 'VCLibrarianTool',
    'lib.exe' : 'VCLibrarianTool',
    'link' : 'VCLinkerTool',
    'link.exe' : 'VCLinkerTool',
  }

  def cl_to_tool(self, args):
    command = os.path.basename(args[0])
    method_name = self.command_to_tool_map.get(command)
    if not method_name:
      return None
    return getattr(self, method_name)(args[1:])

  def _AddFileConfigurationDifferences(self, target, source, base_env, file_env, name):
    """Adds a per-file configuration.

    Args:
      target: The target being built from the source.
      source: The source to which the file configuration is being added.
      base_env: The base construction environment for the project.
          Differences from this will go into the FileConfiguration
          in the project file.
      file_env: The construction environment for the target, containing
          the per-target settings.
    """
    executor = target.get_executor()
    base_cl = map(str, base_env.subst_list(executor)[0])
    file_cl = map(str, file_env.subst_list(executor)[0])
    if base_cl == file_cl:
      return

    base_tool = self.cl_to_tool(base_cl)
    file_tool = self.cl_to_tool(file_cl)

    file_tool.diff(base_tool)

    self.AddFileConfig(source, name, tools=[file_tool])

  def _AddFileConfigurations(self, env):
    """Adds per-file configurations for the buildtarget's sources.

    Args:
      env: The base construction environment for the project.
    """
    if not self.buildtargets:
      return

    for bt in self.buildtargets:
      executor = bt.get_executor()
      build_env = bt.get_build_env()
      bt_cl = map(str, build_env.subst_list(executor)[0])
      tool = self.cl_to_tool(bt_cl)
      default_tool = self.cl_to_tool([bt_cl[0]])
      if default_tool:
        tool.diff(default_tool)
      else:
        print "no tool for %r" % bt_cl[0]
      for t in bt.sources:
        e = t.get_build_env()
        additional_files = SCons.Util.UniqueList()
        for s in t.sources:
          s = env.arg2nodes([s])[0].srcnode()
          if not self.FindFile(s):
            additional_files.append(s)
          if not build_env is e:
            # TODO(sgk):  This test may be bogus, but it works for now.
            # We're trying to figure out if the file configuration
            # differences need to be added one per build target, or one
            # per configuration for the entire project.  The assumption
            # is that if the number of buildtargets configured matches
            # the number of project configurations, that we use those
            # in preference to the project configurations.
            if len(self.buildtargets) == len(self.configurations):
              self._AddFileConfigurationDifferences(t, s, build_env, e, e.subst('$MSVSCONFIGURATIONNAME'))
            else:
              for config in self.configurations:
                self._AddFileConfigurationDifferences(t, s, build_env, e, config.Name)
        self._MergeFiles(self.files, additional_files)

  def Write(self, env):
    """Writes the project file."""
    for flist in self.file_lists:
      self._FilesToSourceFiles(flist)
      self._MergeFiles(self.files, flist)
    for k, v in self.file_configurations.items():
      self.file_configurations[str(k)] = v
      k = self.env.File(k).srcnode()
      self.file_configurations[k] = v
      self.file_configurations[str(k)] = v
    self._AddFileConfigurations(env)

    self.Create()

    f = open(str(self), 'wt')
    f.write(self.formatMSVSProjectXML(self.doc))
    f.close()

  # Methods for formatting XML as nearly identically to Microsoft's
  # .vcproj output as we can practically make it.
  #
  # The general design here is copied from:
  #
  #     Bruce Eckels' MindView, Inc:  12-09-04 XML Oddyssey
  #     http://www.mindview.net/WebLog/log-0068
  #
  # Eckels' implementation broke up long tag definitions for readability,
  # in much the same way that .vcproj does, but we've modified things
  # for .vcproj quirks (like some tags *always* terminating with </Tag>,
  # even when empty).

  encoding = 'Windows-1252'

  def formatMSVSProjectXML(self, xmldoc):
      xmldoc = xmldoc.toprettyxml("", "\n", encoding=self.encoding)
      # Remove trailing whitespace from each line:
      xmldoc = "\n".join(
          [line.rstrip() for line in xmldoc.split("\n")])
      # Remove all empty lines before opening '<':
      while xmldoc.find("\n\n<") != -1:
          xmldoc = xmldoc.replace("\n\n<", "\n<")
      dom = xml.dom.minidom.parseString(xmldoc)
      xmldoc = dom.toprettyxml("\t", "", encoding=self.encoding)
      xmldoc = xmldoc.replace('?><', '?>\n<')
      xmldoc = self.reformatLines(xmldoc)
      return xmldoc

  def reformatLines(self, xmldoc):
      result = []
      for line in [line.rstrip() for line in xmldoc.split("\n")]:
          if line.lstrip().startswith("<"):
              result.append(self.reformatLine(line) + "\n")
          else:
              result.append(line + "\n")
      return ''.join(result)

  # Keyword order for specific tags.
  #
  # Listed keywords will come first and in the specified order.
  # Any unlisted keywords come after, in whatever order they appear
  # in the input config.  In theory this means we would only *have* to
  # list the keywords that we care about, but in practice we'll probably
  # want to nail down Visual Studio's order to make sure we match them
  # as nearly as possible.

  order = {
      'Configuration' : [
          'Name',
          'ConfigurationType',
          'InheritedPropertySheets',
      ],
      'FileConfiguration' : [
          'Name',
          'ExcludedFromBuild',
      ],
      'Tool' : [
          'Name',
          'DisableSpecificWarnings',

          'PreprocessorDefinitions',
          'UsePrecompiledHeader',
          'PrecompiledHeaderThrough',
          'ForcedIncludeFiles',
      ],
      'VisualStudioProject' : [
          'ProjectType',
          'Version',
          'Name',
          'ProjectGUID',
          'RootNamespace',
          'Keyword',
      ],
  }

  force_closing_tag = [
      'File',
      'Globals',
      'References',
      'ToolFiles'
  ]

  oneLiner = re.compile("(\s*)<(\w+)(.*)>")
  keyValuePair = re.compile('\w+="[^"]*?"')
  def reformatLine(self, line):
      """Reformat an xml tag to put each key-value
      element on a single indented line, for readability"""
      matchobj = self.oneLiner.match(line.rstrip())
      if not matchobj:
          return line
      baseIndent, tag, rest = matchobj.groups()
      slash = ''
      if rest[-1:] == '/':
          slash = '/'
          rest = rest[:-1]
      result = [baseIndent + '<' + tag]
      indent = baseIndent + "\t"
      pairs = self.keyValuePair.findall(rest)
      for key in self.order.get(tag, []):
          for p in [ p for p in pairs if p.startswith(key+'=') ]:
              result.append("\n" + indent + p)
              pairs.remove(p)
      for pair in pairs:
          result.append("\n" + indent + pair)
      result = [''.join(result).rstrip()]

      if tag in self.force_closing_tag:
          # These force termination with </Tag>, so translate slash.
          if rest:
              result.append("\n")
              result.append(indent)
          result.append(">")
          if slash:
              result.append("\n")
              result.append(baseIndent + "</" + tag + ">")
      else:
          if rest:
              result.append("\n")
              if slash:
                  result.append(baseIndent)
              else:
                  result.append(indent)
          result.append(slash + ">")

      return ''.join(result)

def MSVSProject(env, item, *args, **kw):
  if not SCons.Util.is_String(item):
      return item
  item = env.subst(item)
  result = env.fs._lookup(item, None, _MSVSProject, create=1)
  result.initialize(env, item, *args, **kw)
  LookupAdd(item, result)
  return result

#------------------------------------------------------------------------------

MSVSSolutionAction = SCons.Script.Action(MSVSAction,
                                         "Generating Visual Studio solution `$TARGET' ...")

class _MSVSSolution(SCons.Node.FS.File):
  """Visual Studio solution."""

  def initialize(self, env, path, entries = None, variants = None, websiteProperties = True):
    """Initializes the solution.

    Args:
      path: Path to solution file.
      entries: List of entries in solution.  May contain Folder or Project
          objects.  May be None, if the folder is empty.
      variants: List of build variant strings.  If none, a default list will
          be used.
    """
    self.msvs_path = path
    self.websiteProperties = websiteProperties

    # Copy passed lists (or set to empty lists)
    self.entries = list(entries or [])

    if variants:
      # Copy passed list
      self.variants = variants[:]
    else:
      # Use default
      self.variants = ['Debug|Win32', 'Release|Win32']
    # TODO(rspangler): Need to be able to handle a mapping of solution config
    # to project config.  Should we be able to handle variants being a dict,
    # or add a separate variant_map variable?  If it's a dict, we can't
    # guarantee the order of variants since dict keys aren't ordered.

    env.Command(self, [], MSVSSolutionAction)

  def Write(self, env):
    """Writes the solution file to disk.

    Raises:
      IndexError: An entry appears multiple times.
    """
    r = []
    errors = []

    def lookup_subst(item, env=env, errors=errors):
      if SCons.Util.is_String(item):
        lookup_item = env.subst(item)
      else:
        lookup_item = item
      try:
        return Lookup(lookup_item)
      except SCons.Errors.UserError:
        raise LookupError(item, lookup_item)

    # Walk the entry tree and collect all the folders and projects.
    all_entries = []
    entries_to_check = self.entries[:]
    while entries_to_check:
      # Pop from the beginning of the list to preserve the user's order.
      entry = entries_to_check.pop(0)
      try:
        entry = lookup_subst(entry)
      except LookupError, e:
        errors.append("Could not look up entry `%s'." % e)
        continue

      # A project or folder can only appear once in the solution's folder tree.
      # This also protects from cycles.
      if entry in all_entries:
        #raise IndexError('Entry "%s" appears more than once in solution' %
        #                 e.name)
        continue

      all_entries.append(entry)

      # If this is a folder, check its entries too.
      if isinstance(entry, _MSVSFolder):
        entries_to_check += entry.entries

    # Header
    r.append('Microsoft Visual Studio Solution File, Format Version 9.00\n')
    r.append('# Visual Studio 2005\n')

    # Project entries
    for e in all_entries:
      r.append('Project("%s") = "%s", "%s", "%s"\n' % (
          e.entry_type_guid,          # Entry type GUID
          e.msvs_name,                # Folder name
          e.get_msvs_path(self),      # Folder name (again)
          e.get_guid(),               # Entry GUID
      ))

      # TODO(rspangler): Need a way to configure this stuff
      if self.websiteProperties:
        r.append('\tProjectSection(WebsiteProperties) = preProject\n'
                '\t\tDebug.AspNetCompiler.Debug = "True"\n'
                '\t\tRelease.AspNetCompiler.Debug = "False"\n'
                '\tEndProjectSection\n')

      if isinstance(e, _MSVSFolder):
        if e.items:
          r.append('\tProjectSection(SolutionItems) = preProject\n')
          for i in e.items:
            i = i.replace('/', '\\')
            r.append('\t\t%s = %s\n' % (i, i))
          r.append('\tEndProjectSection\n')

      if isinstance(e, _MSVSProject):
        if e.dependencies:
          r.append('\tProjectSection(ProjectDependencies) = postProject\n')
          for d in e.dependencies:
            try:
              d = lookup_subst(d)
            except LookupError, e:
              errors.append("Could not look up dependency `%s'." % e)
            else:
              r.append('\t\t%s = %s\n' % (d.get_guid(), d.get_guid()))
          r.append('\tEndProjectSection\n')

      r.append('EndProject\n')

    # Global section
    r.append('Global\n')

    # Configurations (variants)
    r.append('\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n')
    for v in self.variants:
      r.append('\t\t%s = %s\n' % (v, v))
    r.append('\tEndGlobalSection\n')

    # Sort config guids for easier diffing of solution changes.
    config_guids = []
    for e in all_entries:
      if isinstance(e, _MSVSProject):
        config_guids.append(e.get_guid())
    config_guids.sort()

    r.append('\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n')
    for g in config_guids:
      for v in self.variants:
        r.append('\t\t%s.%s.ActiveCfg = %s\n' % (
            g,              # Project GUID
            v,              # Solution build configuration
            v,              # Project build config for that solution config
        ))

        # Enable project in this solution configuratation
        r.append('\t\t%s.%s.Build.0 = %s\n' % (
            g,              # Project GUID
            v,              # Solution build configuration
            v,              # Project build config for that solution config
        ))
    r.append('\tEndGlobalSection\n')

    # TODO(rspangler): Should be able to configure this stuff too (though I've
    # never seen this be any different)
    r.append('\tGlobalSection(SolutionProperties) = preSolution\n')
    r.append('\t\tHideSolutionNode = FALSE\n')
    r.append('\tEndGlobalSection\n')

    # Folder mappings
    # TODO(rspangler): Should omit this section if there are no folders
    folder_mappings = []
    for e in all_entries:
      if not isinstance(e, _MSVSFolder):
        continue        # Does not apply to projects, only folders
      for subentry in e.entries:
        try:
          subentry = lookup_subst(subentry)
        except LookupError, e:
          errors.append("Could not look up subentry `%s'." % subentry)
        else:
          folder_mappings.append((subentry.get_guid(), e.get_guid()))
    folder_mappings.sort()
    r.append('\tGlobalSection(NestedProjects) = preSolution\n')
    for fm in folder_mappings:
        r.append('\t\t%s = %s\n' % fm)
    r.append('\tEndGlobalSection\n')

    r.append('EndGlobal\n')

    if errors:
      errors = ['Errors while generating solution file:'] + errors
      raise SCons.Errors.UserError, '\n\t'.join(errors)

    f = open(self.path, 'wt')
    f.write(''.join(r))
    f.close()

def MSVSSolution(env, item, *args, **kw):
  if not SCons.Util.is_String(item):
      return item
  item = env.subst(item)
  result = env.fs._lookup(item, None, _MSVSSolution, create=1)
  result.initialize(env, item, *args, **kw)
  LookupAdd(item, result)
  return result

import __builtin__

__builtin__.MSVSConfig = MSVSConfig
__builtin__.MSVSFilter = MSVSFilter
__builtin__.MSVSProject = MSVSProject
__builtin__.MSVSSolution = MSVSSolution
__builtin__.MSVSTool = MSVSTool
