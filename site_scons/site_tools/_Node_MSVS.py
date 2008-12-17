#!/usr/bin/python2.4
# Copyright 2008, Google Inc.
# All rights reserved.

__doc__ = """SCons.Node.MSVS

Microsoft Visual Studio nodes.
"""

import SCons.Node.FS
import SCons.Script


"""New implementation of Visual Studio project generation for SCons."""

import md5
import os
import random


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

class _MSVSFolder(SCons.Node.Node):
  """Folder in a Visual Studio project or solution."""

  entry_type_guid = '{2150E333-8FDC-42A3-9474-1A3956D46DE8}'

  def initialize(self, path, name = None, entries = None, guid = None,
                             items = None):
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
    # For folder entries, the path is the same as the name
    self.msvs_path = path
    self.msvs_name = name or path

    self.guid = guid

    # Copy passed lists (or set to empty lists)
    self.entries = list(entries or [])
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


class _MSVSProject(SCons.Node.FS.File):
  """Visual Studio project."""

  entry_type_guid = '{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}'

  def initialize(self, path, name = None, dependencies = None, guid = None):
    """Initializes the project.

    Args:
      path: Relative path to project file.
      name: Name of project.  If None, the name will be the same as the base
          name of the project file.
      dependencies: List of other Project objects this project is dependent
          upon, if not None.
      guid: GUID to use for project, if not None.
    """
    self.msvs_path = path
    self.msvs_name = name or os.path.splitext(os.path.basename(self.name))[0]

    self.guid = guid

    # Copy passed lists (or set to empty lists)
    self.dependencies = list(dependencies or [])

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

def MSVSProject(env, item, *args, **kw):
  if not SCons.Util.is_String(item):
      return item
  item = env.subst(item)
  result = env.fs._lookup(item, None, _MSVSProject, create=1)
  result.initialize(item, *args, **kw)
  LookupAdd(item, result)
  return result

#------------------------------------------------------------------------------

def MSVSAction(target, source, env):
  target[0].Write(env)

MSVSSolutionAction = SCons.Script.Action(MSVSAction,
                                         "Generating Visual Studio solution `$TARGET' ...")

class _MSVSSolution(SCons.Node.FS.File):
  """Visual Studio solution."""

  def initialize(self, env, path, entries = None, variants = None,
                                  websiteProperties = True):
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
