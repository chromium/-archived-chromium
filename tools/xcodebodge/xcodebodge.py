#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Commandline modification of Xcode project files
"""

import sys
import os
import optparse
import re
import tempfile
import random
import subprocess

random.seed()  # Seed the generator


# All known project build path source tree path reference types
PBX_VALID_SOURCE_TREE_TYPES =  ('"<group>"',
                                'SOURCE_ROOT',
                                '"<absolute>"',
                                'BUILT_PRODUCTS_DIR',
                                'DEVELOPER_DIR',
                                'SDKROOT',
                                'CONFIGURATION_TEMP_DIR')
# Paths with some characters appear quoted
QUOTE_PATH_RE = re.compile('\s|-|\+')

# Supported Xcode file types
EXTENSION_TO_XCODE_FILETYPE = {
  '.h'   : 'sourcecode.c.h',
  '.c'   : 'sourcecode.c.c',
  '.cpp' : 'sourcecode.cpp.cpp',
  '.cc'  : 'sourcecode.cpp.cpp',
  '.cxx' : 'sourcecode.cpp.cpp',
  '.m'   : 'sourcecode.c.objc',
  '.mm'  : 'sourcecode.c.objcpp',
}

# File types that can be added to a Sources phase
SOURCES_XCODE_FILETYPES = ( 'sourcecode.c.c',
                            'sourcecode.cpp.cpp',
                            'sourcecode.c.objc',
                            'sourcecode.c.objcpp' )

# Avoid inserting source files into these common Xcode group names. Because
# Xcode allows any names for these groups this list cannot be authoritative,
# but these are common names in the Xcode templates.
NON_SOURCE_GROUP_NAMES = ( 'Frameworks',
                           'Resources',
                           'Products',
                           'Derived Sources',
                           'Configurations',
                           'Documentation',
                           'Frameworks and Libraries',
                           'External Frameworks and Libraries',
                           'Libraries' )


def NewUUID():
  """Create a new random Xcode UUID"""
  __pychecker__ = 'unusednames=i'
  elements = []
  for i in range(24):
    elements.append(hex(random.randint(0, 15))[-1].upper())
  return ''.join(elements)

def CygwinPathClean(path):
  """Folks use Cygwin shells with standard Win32 Python which can't handle
  Cygwin paths. Run everything through cygpath if we can (conveniently
  cygpath does the right thing with normal Win32 paths).
  """
  # Look for Unix-like path with Win32 Python
  if sys.platform == 'win32' and path.startswith('/'):
    cygproc = subprocess.Popen(('cygpath', '-a', '-w', path),
                               stdout=subprocess.PIPE)
    (stdout_content, stderr_content) = cygproc.communicate()
    return stdout_content.rstrip()
  # Convert all paths to cygpaths if we're using cygwin python
  if sys.platform == 'cygwin':
    cygproc = subprocess.Popen(('cygpath', '-a', '-u', path),
                               stdout=subprocess.PIPE)
    (stdout_content, stderr_content) = cygproc.communicate()
    return stdout_content.rstrip()
  # Fallthrough for all other cases
  return path

class XcodeProject(object):
  """Class for reading/writing Xcode project files.
  This is not a general parser or representation. It is restricted to just
  the Xcode internal objects we need.

  Args:
    path: Absolute path to Xcode project file (including project.pbxproj
          filename)
  Attributes:
    path: Full path to the project.pbxproj file
    name: Project name (wrapper directory basename without extension)
    source_root_path: Absolute path for Xcode's SOURCE_ROOT
  """

  EXPECTED_PROJECT_HEADER_RE = re.compile(
    r'^// !\$\*UTF8\*\$!\n' \
     '\{\n' \
     '\tarchiveVersion = 1;\n' \
     '\tclasses = \{\n' \
     '\t\};\n' \
     '\tobjectVersion = \d+;\n' \
     '\tobjects = \{\n' \
     '\n')
  SECTION_BEGIN_RE = re.compile(r'^/\* Begin (.*) section \*/\n$')
  SECTION_END_RE = re.compile(r'^/\* End (.*) section \*/\n$')
  PROJECT_ROOT_OBJECT_RE = re.compile(
      r'^\trootObject = ([0-9A-F]{24}) /\* Project object \*/;\n$')

  def __init__(self, path):
    self.path = path
    self.name = os.path.splitext(os.path.basename(os.path.dirname(path)))[0]

    # Load project. Ideally we would use plistlib, but sadly that only reads
    # XML plists. A real parser with pyparsing
    # (http://pyparsing.wikispaces.com/) might be another option, but for now
    # we'll do the simple (?!?) thing.
    project_fh = open(self.path, 'rU')
    self._raw_content = project_fh.readlines()
    project_fh.close()

    # Store and check header
    if len(self._raw_content) < 8:
      print >> sys.stderr, ''.join(self._raw_content)
      raise RuntimeError('XcodeProject file "%s" too short' % path)
    self._header = tuple(self._raw_content[:8])
    if not self.__class__.EXPECTED_PROJECT_HEADER_RE.match(''.join(self._header)):
      print >> sys.stderr, ''.join(self._header)
      raise RuntimeError('XcodeProject file "%s" wrong header' % path)

    # Find and store tail (some projects have additional whitespace at end)
    self._tail = []
    for tail_line in reversed(self._raw_content):
      self._tail.insert(0, tail_line)
      if tail_line == '\t};\n': break

    # Ugly ugly project parsing, turn each commented section into a separate
    # set of objects. For types we don't have a custom representation for,
    # store the raw lines.
    self._section_order = []
    self._sections = {}
    parse_line_no = len(self._header)
    while parse_line_no < (len(self._raw_content) - len(self._tail)):
      section_header_match = self.__class__.SECTION_BEGIN_RE.match(
                               self._raw_content[parse_line_no])
      # Loop to next section header
      if not section_header_match:
        parse_line_no += 1
        continue

      section = section_header_match.group(1)
      self._section_order.append(section)
      self._sections[section] = []

      # Advance to first line of the section
      parse_line_no += 1

      # Read in the section, using custom classes where we need them
      section_end_match = self.__class__.SECTION_END_RE.match(
                               self._raw_content[parse_line_no])
      while not section_end_match:
        # Unhandled lines
        content = self._raw_content[parse_line_no]
        # Sections we can parse line-by-line
        if section in ('PBXBuildFile', 'PBXFileReference'):
          content = eval('%s.FromContent(content)' % section)
        # Multiline sections
        elif section in ('PBXGroup', 'PBXVariantGroup', 'PBXProject', 
                         'PBXNativeTarget', 'PBXSourcesBuildPhase'):
          # Accumulate lines
          content_lines = []
          while 1:
            content_lines.append(content)
            if content == '\t\t};\n': break
            parse_line_no += 1
            content = self._raw_content[parse_line_no]
          content = eval('%s.FromContent(content_lines)' % section)

        self._sections[section].append(content)
        parse_line_no += 1
        section_end_match = self.__class__.SECTION_END_RE.match(
                              self._raw_content[parse_line_no])
      # Validate section end
      if section_header_match.group(1) != section:
        raise RuntimeError(
          'XcodeProject parse, section "%s" ended inside section "%s"' %
          (section_end_match.group(1), section))
      # Back around parse loop

    # Sanity overall group structure
    if (not self._sections.has_key('PBXProject') or
        len(self._sections['PBXProject']) != 1):
      raise RuntimeError('PBXProject section insane')
    root_obj_parsed = self.__class__.PROJECT_ROOT_OBJECT_RE.match(
                                       self._tail[1])
    if not root_obj_parsed:
      raise RuntimeError('XcodeProject unable to parse project root object:\n%s'
                         % self._tail[1])
    if root_obj_parsed.group(1) != self._sections['PBXProject'][0].uuid:
      raise RuntimeError('XcodeProject root object does not match PBXProject')
    self._root_group_uuid = self._sections['PBXProject'][0].main_group_uuid

    # Source root
    self.source_root_path = os.path.abspath(
                              os.path.join(
                                # Directory that contains the project package
                                os.path.dirname(os.path.dirname(path)),
                                # Any relative path
                                self._sections['PBXProject'][0].project_root))

    # Build the absolute paths of the groups with these helpers
    def GroupAbsRealPath(*elements):
      return os.path.abspath(os.path.realpath(os.path.join(*elements)))
    def GroupPathRecurse(group, parent_path):
      descend = False
      if group.source_tree == '"<absolute>"':
        group.abs_path = GroupAbsRealPath(group.path)
        descend = True
      elif group.source_tree == '"<group>"':
        if group.path:
          group.abs_path = GroupAbsRealPath(parent_path, group.path)
        else:
          group.abs_path = parent_path
        descend = True
      elif group.source_tree == 'SOURCE_ROOT':
        if group.path:
          group.abs_path = GroupAbsRealPath(self.source_root_path, group.path)
        else:
          group.abs_path = GroupAbsRealPath(self.source_root_path)
        descend = True
      if descend:
        for child_uuid in group.child_uuids:
          # Try a group first
          found_uuid = False
          for other_group in self._sections['PBXGroup']:
            if other_group.uuid == child_uuid:
              found_uuid = True
              GroupPathRecurse(other_group, group.abs_path)
              break
          if self._sections.has_key('PBXVariantGroup'):
            for other_group in self._sections['PBXVariantGroup']:
              if other_group.uuid == child_uuid:
                found_uuid = True
                GroupPathRecurse(other_group, group.abs_path)
                break
          if not found_uuid:
            for file_ref in self._sections['PBXFileReference']:
              if file_ref.uuid == child_uuid:
                found_uuid = True
                if file_ref.source_tree == '"<absolute>"':
                  file_ref.abs_path = GroupAbsRealPath(file_ref.path)
                elif group.source_tree == '"<group>"':
                  file_ref.abs_path = GroupAbsRealPath(group.abs_path,
                                                       file_ref.path)
                elif group.source_tree == 'SOURCE_ROOT':
                  file_ref.abs_path = GroupAbsRealPath(self.source_root_path,
                                                       file_ref.path)
                break
          if not found_uuid:
            raise RuntimeError('XcodeProject group descent failed to find %s' %
                                child_uuid)
    self._root_group = None
    for group in self._sections['PBXGroup']:
      if group.uuid == self._root_group_uuid:
        self._root_group = group
        GroupPathRecurse(group, self.source_root_path)
    if not self._root_group:
      raise RuntimeError('XcodeProject failed to find root group by UUID')
      
  def FileContent(self):
    """Generate and return the project file content as a list of lines"""
    content = []
    content.extend(self._header[:-1])
    for section in self._section_order:
      content.append('\n/* Begin %s section */\n' % section)
      for section_content in self._sections[section]:
        content.append(str(section_content))
      content.append('/* End %s section */\n' % section)
    content.extend(self._tail)
    return content

  def Update(self):
    """Rewrite the project file in place with all updated metadata"""
    __pychecker__ = 'no-deprecated'
    # Not concerned with temp_path security here, just needed a unique name
    temp_path = tempfile.mktemp(dir=os.path.dirname(self.path))
    outfile = open(temp_path, 'w')
    outfile.writelines(self.FileContent())
    outfile.close()
    # Rename is weird on Win32, see the docs,
    os.unlink(self.path)
    os.rename(temp_path, self.path)

  def NativeTargets(self):
    """Obtain all PBXNativeTarget instances for this project

    Returns:
      List of PBXNativeTarget instances
    """
    if self._sections.has_key('PBXNativeTarget'):
      return self._sections['PBXNativeTarget']
    else:
      return []

  def NativeTargetForName(self, name):
    """Obtain the target with a given name.

    Args:
      name: Target name

    Returns:
      PBXNativeTarget instance or None
    """
    for target in self.NativeTargets():
      if target.name == name:
        return target
    return None

  def FileReferences(self):
    """Obtain all PBXFileReference instances for this project

    Returns:
      List of PBXFileReference instances
    """
    return self._sections['PBXFileReference']

  def SourcesBuildPhaseForTarget(self, target):
    """Obtain the PBXSourcesBuildPhase instance for a target. Xcode allows
    only one PBXSourcesBuildPhase per target and each target has a unique
    PBXSourcesBuildPhase.

    Args:
      target: PBXNativeTarget instance

    Returns:
      PBXSourcesBuildPhase instance
    """
    sources_uuid = None
    for i in range(len(target.build_phase_names)):
      if target.build_phase_names[i] == 'Sources':
        sources_uuid = target.build_phase_uuids[i]
        break
    if not sources_uuid:
      raise RuntimeError('Missing PBXSourcesBuildPhase for target "%s"' %
                         target.name)
    for sources_phase in self._sections['PBXSourcesBuildPhase']:
      if sources_phase.uuid == sources_uuid:
        return sources_phase
    raise RuntimeError('Missing PBXSourcesBuildPhase for UUID "%s"' %
                       sources_uuid)

  def BuildFileForUUID(self, uuid):
    """Look up a PBXBuildFile by UUID

    Args:
      uuid: UUID of the PBXBuildFile to find

    Raises:
      RuntimeError if no PBXBuildFile exists for |uuid|

    Returns:
      PBXBuildFile instance
    """
    for build_file in self._sections['PBXBuildFile']:
      if build_file.uuid == uuid:
        return build_file
    raise RuntimeError('Missing PBXBuildFile for UUID "%s"' % uuid)

  def FileReferenceForUUID(self, uuid):
    """Look up a PBXFileReference by UUID

    Args:
      uuid: UUID of the PBXFileReference to find

    Raises:
      RuntimeError if no PBXFileReference exists for |uuid|

    Returns:
      PBXFileReference instance
    """
    for file_ref in self._sections['PBXFileReference']:
      if file_ref.uuid == uuid:
        return file_ref
    raise RuntimeError('Missing PBXFileReference for UUID "%s"' % uuid)

  def RemoveSourceFileReference(self, file_ref):
    """Remove a source file's PBXFileReference from the project, cleaning up all
    PBXGroup and PBXBuildFile references to that PBXFileReference and 
    furthermore, removing any PBXBuildFiles from all PBXNativeTarget source
    lists.

    Args:
      file_ref: PBXFileReference instance

    Raises:
      RuntimeError if |file_ref| is not a source file reference in PBXBuildFile
    """
    self._sections['PBXFileReference'].remove(file_ref)
    # Clean up build files
    removed_build_files = []
    for build_file in self._sections['PBXBuildFile']:
      if build_file.file_ref_uuid == file_ref.uuid:
        if build_file.type != 'Sources':
          raise RuntimeError('Removing PBXBuildFile not of "Sources" type')
        removed_build_files.append(build_file)
    removed_build_file_uuids = []
    for build_file in removed_build_files:
      removed_build_file_uuids.append(build_file.uuid)
      self._sections['PBXBuildFile'].remove(build_file)
    # Clean up source references to the removed build files
    for source_phase in self._sections['PBXSourcesBuildPhase']:
      removal_indexes = []
      for i in range(len(source_phase.file_uuids)):
        if source_phase.file_uuids[i] in removed_build_file_uuids:
          removal_indexes.append(i)
      for removal_index in removal_indexes:
        del source_phase.file_uuids[removal_index]
        del source_phase.file_names[removal_index]
    # Clean up group references
    for group in self._sections['PBXGroup']:
      removal_indexes = []
      for i in range(len(group.child_uuids)):
        if group.child_uuids[i] == file_ref.uuid:
          removal_indexes.append(i)
      for removal_index in removal_indexes:
        del group.child_uuids[removal_index]
        del group.child_names[removal_index]

  def RelativeSourceRootPath(self, abs_path):
    """Convert a path to one relative to the project's SOURCE_ROOT if possible.
    Generally this follows Xcode semantics, that is, a path is only converted
    if it is a subpath of SOURCE_ROOT.

    Args:
      abs_path: Absolute path to convert

    Returns:
      String SOURCE_ROOT relative path if possible or None if not relative
      to SOURCE_ROOT.
    """
    if abs_path.startswith(self.source_root_path + os.path.sep):
      return abs_path[len(self.source_root_path + os.path.sep):]
    else:
      # Try to construct a relative path (bodged from ActiveState recipe
      # 302594 since we can't assume Python 2.5 with os.path.relpath()
      source_root_parts = self.source_root_path.split(os.path.sep)
      target_parts = abs_path.split(os.path.sep)
      for i in range(min(len(source_root_parts), len(target_parts))):
        if source_root_parts[i] <> target_parts[i]: break
      else:
        i += 1
      rel_parts = [os.path.pardir] * (len(source_root_parts) - i)
      rel_parts.extend(target_parts[i:])
      return os.path.join(*rel_parts)

  def RelativeGroupPath(self, abs_path):
    """Convert a path to a group-relative path if possible

    Args:
      abs_path: Absolute path to convert

    Returns:
      Parent PBXGroup instance if possible or None
    """
    needed_path = os.path.dirname(abs_path)
    possible_groups = [ g for g in self._sections['PBXGroup']
                        if g.abs_path == needed_path and 
                           not g.name in NON_SOURCE_GROUP_NAMES ]
    if len(possible_groups) < 1:
      return None
    elif len(possible_groups) == 1:
      return possible_groups[0]
    # Multiple groups match, try to find the best using some simple
    # heuristics. Does only one group contain source?
    groups_with_source = []
    for group in possible_groups:
      for child_uuid in group.child_uuids:
        try:
          self.FileReferenceForUUID(child_uuid)
        except RuntimeError:
          pass
        else:
          groups_with_source.append(group)
          break
    if len(groups_with_source) == 1:
      return groups_with_source[0]
    # Is only one _not_ the root group?
    non_root_groups = [ g for g in possible_groups
                        if g is not self._root_group ]
    if len(non_root_groups) == 1:
      return non_root_groups[0]
    # Best guess
    if len(non_root_groups):
      return non_root_groups[0]
    elif len(groups_with_source):
      return groups_with_source[0]
    else:
      return possible_groups[0]
      
  def AddSourceFile(self, path):
    """Add a source file to the project, attempting to position it
    in the GUI group heirarchy reasonably.

    NOTE: Adding a source file does not add it to any targets

    Args:
      path: Absolute path to the file to add

    Returns:
      PBXFileReference instance for the newly added source.
    """
    # Guess at file type
    root, extension = os.path.splitext(path)
    if EXTENSION_TO_XCODE_FILETYPE.has_key(extension):
      source_type = EXTENSION_TO_XCODE_FILETYPE[extension]
    else:
      raise RuntimeError('Unknown source file extension "%s"' % extension)

    # Is group-relative possible for an existing group?
    parent_group = self.RelativeGroupPath(os.path.abspath(path))
    if parent_group:
      new_file_ref = PBXFileReference(NewUUID(),
                                      os.path.basename(path),
                                      source_type,
                                      None,
                                      os.path.basename(path),
                                      '"<group>"',
                                      None)
      # Chrome tries to keep its lists name sorted, try to match
      i = 0
      while i < len(parent_group.child_uuids):
        # Only files are sorted, they keep groups at the top
        try:
          self.FileReferenceForUUID(parent_group.child_uuids[i])
          if new_file_ref.name.lower() < parent_group.child_names[i].lower():
            break
        except RuntimeError:
          pass  # Must be a child group
        i += 1
      parent_group.child_names.insert(i, new_file_ref.name)
      parent_group.child_uuids.insert(i, new_file_ref.uuid)
      self._sections['PBXFileReference'].append(new_file_ref)
      return new_file_ref

    # Group-relative failed, how about SOURCE_ROOT relative in the main group
    src_rel_path = self.RelativeSourceRootPath(os.path.abspath(path))
    if src_rel_path:
      src_rel_path = src_rel_path.replace('\\', '/')  # Convert to Unix
      new_file_ref = PBXFileReference(NewUUID(),
                                      os.path.basename(path),
                                      source_type,
                                      None,
                                      src_rel_path,
                                      'SOURCE_ROOT',
                                      None)
      self._root_group.child_uuids.append(new_file_ref.uuid)
      self._root_group.child_names.append(new_file_ref.name)
      self._sections['PBXFileReference'].append(new_file_ref)
      return new_file_ref

    # Win to Unix absolute paths probably not practical
    raise RuntimeError('Could not construct group or source PBXFileReference '
                       'for path "%s"' % path)

  def AddSourceFileToSourcesBuildPhase(self, source_ref, source_phase):
    """Add a PBXFileReference to a PBXSourcesBuildPhase, creating a new
    PBXBuildFile as needed.

    Args:
      source_ref: PBXFileReference instance appropriate for use in
                  PBXSourcesBuildPhase
      source_phase: PBXSourcesBuildPhase instance
    """
    # Prevent duplication
    for source_uuid in source_phase.file_uuids:
      build_file = self.BuildFileForUUID(source_uuid)
      if build_file.file_ref_uuid == source_ref.uuid:
        return
    # Create PBXBuildFile
    new_build_file = PBXBuildFile(NewUUID(),
                                  source_ref.name,
                                  'Sources',
                                  source_ref.uuid,
                                  '')
    self._sections['PBXBuildFile'].append(new_build_file)
    # Add to sources phase list (name sorted)
    i = 0
    while i < len(source_phase.file_names):
      if source_ref.name.lower() < source_phase.file_names[i].lower():
        break
      i += 1
    source_phase.file_names.insert(i, new_build_file.name)
    source_phase.file_uuids.insert(i, new_build_file.uuid)


class PBXProject(object):
  """Class for PBXProject data section of an Xcode project file.

  Attributes:
    uuid: Project UUID
    main_group_uuid: UUID of the top-level PBXGroup
    project_root: Relative path from project file wrapper to source_root_path
  """

  PBXPROJECT_HEADER_RE = re.compile(
      r'^\t\t([0-9A-F]{24}) /\* Project object \*/ = {\n$')
  PBXPROJECT_MAIN_GROUP_RE = re.compile(
      r'^\t\t\tmainGroup = ([0-9A-F]{24})(?: /\* .* \*/)?;\n$')
  PBXPROJECT_ROOT_RE = re.compile(
      r'^\t\t\tprojectRoot = (.*);\n$')

  @classmethod
  def FromContent(klass, content_lines):
    header_parsed = klass.PBXPROJECT_HEADER_RE.match(content_lines[0])
    if not header_parsed:
      raise RuntimeError('PBXProject unable to parse header content:\n%s'
                          % content_lines[0])
    main_group_uuid = None
    project_root = ''
    for content_line in content_lines:
      group_parsed = klass.PBXPROJECT_MAIN_GROUP_RE.match(content_line)
      if group_parsed:
        main_group_uuid = group_parsed.group(1)
      root_parsed = klass.PBXPROJECT_ROOT_RE.match(content_line)
      if root_parsed:
        project_root = root_parsed.group(1)
    if project_root.startswith('"'):
      project_root = project_root[1:-1]
    if not main_group_uuid:
      raise RuntimeError('PBXProject missing main group')
    return klass(content_lines, header_parsed.group(1),
                 main_group_uuid, project_root)

  def __init__(self, raw_lines, uuid, main_group_uuid, project_root):
    self.uuid = uuid
    self._raw_lines = raw_lines
    self.main_group_uuid = main_group_uuid
    self.project_root = project_root

  def __str__(self):
    return ''.join(self._raw_lines)


class PBXBuildFile(object):
  """Class for PBXBuildFile data from an Xcode project file.

  Attributes:
    uuid: UUID for this instance
    name: Basename of the build file
    type: 'Sources' or 'Frameworks'
    file_ref_uuid: UUID of the PBXFileReference for this file
  """

  PBXBUILDFILE_LINE_RE = re.compile(
    r'^\t\t([0-9A-F]{24}) /\* (.+) in (.+) \*/ = '
    '{isa = PBXBuildFile; fileRef = ([0-9A-F]{24}) /\* (.+) \*/; (.*)};\n$')

  @classmethod
  def FromContent(klass, content_line):
    parsed = klass.PBXBUILDFILE_LINE_RE.match(content_line)
    if not parsed:
      raise RuntimeError('PBXBuildFile unable to parse content:\n%s'
                          % content_line)
    if parsed.group(2) !=  parsed.group(5):
      raise RuntimeError('PBXBuildFile name mismatch "%s" vs "%s"' %
                         (parsed.group(2), parsed.group(5)))
    if not parsed.group(3) in ('Sources', 'Frameworks', 
                               'Resources', 'CopyFiles',
                               'Headers', 'Copy Into Framework',
                               'Rez', 'Copy Generated Headers'):
      raise RuntimeError('PBXBuildFile unknown type "%s"' % parsed.group(3))
    return klass(parsed.group(1), parsed.group(2), parsed.group(3),
                 parsed.group(4), parsed.group(6))

  def __init__(self, uuid, name, type, file_ref_uuid, raw_extras):
    self.uuid = uuid
    self.name = name
    self.type = type
    self.file_ref_uuid = file_ref_uuid
    self._raw_extras = raw_extras

  def __str__(self):
    return '\t\t%s /* %s in %s */ = ' \
           '{isa = PBXBuildFile; fileRef = %s /* %s */; %s};\n' % (
            self.uuid, self.name, self.type, self.file_ref_uuid, self.name,
            self._raw_extras)


class PBXFileReference(object):
  """Class for PBXFileReference data from an Xcode project file.

  Attributes:
    uuid: UUID for this instance
    name: Basename of the file
    file_type: current active file type (explicit or assumed)
    path: source_tree relative path (or absolute if source_tree is absolute)
    source_tree: Source tree type (see PBX_VALID_SOURCE_TREE_TYPES)
    abs_path: Absolute path to the file
  """
  PBXFILEREFERENCE_HEADER_RE = re.compile(
    r'^\t\t([0-9A-F]{24}) /\* (.+) \*/ = {isa = PBXFileReference; ')
  PBXFILEREFERENCE_FILETYPE_RE = re.compile(
    r' (lastKnownFileType|explicitFileType) = ([^\;]+); ')
  PBXFILEREFERENCE_PATH_RE = re.compile(r' path = ([^\;]+); ')
  PBXFILEREFERENCE_SOURCETREE_RE = re.compile(r' sourceTree = ([^\;]+); ')

  @classmethod
  def FromContent(klass, content_line):
    header_parsed = klass.PBXFILEREFERENCE_HEADER_RE.match(content_line)
    if not header_parsed:
      raise RuntimeError('PBXFileReference unable to parse header content:\n%s'
                          % content_line)
    type_parsed = klass.PBXFILEREFERENCE_FILETYPE_RE.search(content_line)
    if not type_parsed:
      raise RuntimeError('PBXFileReference unable to parse type content:\n%s'
                          % content_line)
    if type_parsed.group(1) == 'lastKnownFileType':
      last_known_type = type_parsed.group(2)
      explicit_type = None
    else:
      last_known_type = None
      explicit_type = type_parsed.group(2)
    path_parsed = klass.PBXFILEREFERENCE_PATH_RE.search(content_line)
    if not path_parsed:
      raise RuntimeError('PBXFileReference unable to parse path content:\n%s'
                          % content_line)
    tree_parsed = klass.PBXFILEREFERENCE_SOURCETREE_RE.search(content_line)
    if not tree_parsed:
      raise RuntimeError(
              'PBXFileReference unable to parse source tree content:\n%s'
              % content_line)
    return klass(header_parsed.group(1), header_parsed.group(2),
                 last_known_type, explicit_type, path_parsed.group(1),
                 tree_parsed.group(1), content_line)

  def __init__(self, uuid, name, last_known_file_type, explicit_file_type, 
               path, source_tree, raw_line):
    self.uuid = uuid
    self.name = name
    self._last_known_file_type = last_known_file_type
    self._explicit_file_type = explicit_file_type
    if explicit_file_type:
      self.file_type = explicit_file_type
    else:
      self.file_type = last_known_file_type
    self.path = path
    self.source_tree = source_tree
    self.abs_path = None
    self._raw_line = raw_line

  def __str__(self):
    # Raw available?
    if self._raw_line: return self._raw_line
    # Construct our own
    if self._last_known_file_type:
      print_file_type = 'lastKnownFileType = %s; ' %  self._last_known_file_type
    elif self._explicit_file_type:
      print_file_type = 'explicitFileType = %s; ' %  self._explicit_file_type
    else:
      raise RuntimeError('No known file type for stringification')
    name_attribute = ''
    if self.name != self.path:
      name_attribute = 'name = %s; ' % self.name
    print_path = self.path
    if QUOTE_PATH_RE.search(print_path):
      print_path = '"%s"' % print_path
    return '\t\t%s /* %s */ = ' \
           '{isa = PBXFileReference; ' \
           'fileEncoding = 4; ' \
           '%s' \
           '%s' \
           'path = %s; sourceTree = %s; };\n' % (
           self.uuid, self.name, print_file_type,
           name_attribute, print_path, self.source_tree)


class PBXGroup(object):
  """Class for PBXGroup data from an Xcode project file.

  Attributes:
    uuid: UUID for this instance
    name: Group (folder) name
    path: source_tree relative path (or absolute if source_tree is absolute)
    source_tree: Source tree type (see PBX_VALID_SOURCE_TREE_TYPES)
    abs_path: Absolute path to the group
    child_uuids: Ordered list of PBXFileReference UUIDs
    child_names: Ordered list of PBXFileReference names
  """

  PBXGROUP_HEADER_RE = re.compile(r'^\t\t([0-9A-F]{24}) (?:/\* .* \*/ )?= {\n$')
  PBXGROUP_FIELD_RE = re.compile(r'^\t\t\t(.*) = (.*);\n$')
  PBXGROUP_CHILD_RE = re.compile(r'^\t\t\t\t([0-9A-F]{24}) /\* (.*) \*/,\n$')

  @classmethod
  def FromContent(klass, content_lines):
    # Header line
    header_parsed = klass.PBXGROUP_HEADER_RE.match(content_lines[0])
    if not header_parsed:
      raise RuntimeError('PBXGroup unable to parse header content:\n%s'
                          % content_lines[0])
    name = None
    path = ''
    source_tree = None
    tab_width = None
    uses_tabs = None
    indent_width = None
    child_uuids = []
    child_names = []
    # Parse line by line
    content_line_no = 0
    while 1:
      content_line_no += 1
      content_line = content_lines[content_line_no]
      if content_line == '\t\t};\n': break
      if content_line == '\t\t\tisa = PBXGroup;\n': continue
      if content_line == '\t\t\tisa = PBXVariantGroup;\n': continue
      # Child groups
      if content_line == '\t\t\tchildren = (\n':
        content_line_no += 1
        content_line = content_lines[content_line_no]
        while content_line != '\t\t\t);\n':
          child_parsed = klass.PBXGROUP_CHILD_RE.match(content_line)
          if not child_parsed:
            raise RuntimeError('PBXGroup unable to parse child content:\n%s'
                                % content_line)
          child_uuids.append(child_parsed.group(1))
          child_names.append(child_parsed.group(2))
          content_line_no += 1
          content_line = content_lines[content_line_no]
        continue  # Back to top of loop on end of children
      # Other fields
      field_parsed = klass.PBXGROUP_FIELD_RE.match(content_line)
      if not field_parsed:
        raise RuntimeError('PBXGroup unable to parse field content:\n%s'
                            % content_line)
      if field_parsed.group(1) == 'name':
        name = field_parsed.group(2)
      elif field_parsed.group(1) == 'path':
        path = field_parsed.group(2)
      elif field_parsed.group(1) == 'sourceTree':
        if not field_parsed.group(2) in PBX_VALID_SOURCE_TREE_TYPES:
          raise RuntimeError('PBXGroup unknown source tree type "%s"'
                              % field_parsed.group(2))
        source_tree = field_parsed.group(2)
      elif field_parsed.group(1) == 'tabWidth':
        tab_width = field_parsed.group(2)
      elif field_parsed.group(1) == 'usesTabs':
        uses_tabs = field_parsed.group(2)
      elif field_parsed.group(1) == 'indentWidth':
        indent_width = field_parsed.group(2)
      else:
        raise RuntimeError('PBXGroup unknown field "%s"'
                           % field_parsed.group(1))
    if path and path.startswith('"'):
      path = path[1:-1]
    if name and name.startswith('"'):
      name = name[1:-1]
    return klass(header_parsed.group(1), name, path, source_tree, child_uuids,
                 child_names, tab_width, uses_tabs, indent_width)

  def __init__(self, uuid, name, path, source_tree, child_uuids, child_names,
               tab_width, uses_tabs, indent_width):
    self.uuid = uuid
    self.name = name
    self.path = path
    self.source_tree = source_tree
    self.child_uuids = child_uuids
    self.child_names = child_names
    self.abs_path = None
    # Semantically I'm not sure these aren't an error, but they 
    # appear in some projects
    self._tab_width = tab_width
    self._uses_tabs = uses_tabs
    self._indent_width = indent_width

  def __str__(self):
    if self.name:
      header_comment = '/* %s */ ' % self.name
    elif self.path:
      header_comment = '/* %s */ ' % self.path
    else:
      header_comment = ''
    if self.name:
      if QUOTE_PATH_RE.search(self.name):
        name_attribute = '\t\t\tname = "%s";\n' % self.name
      else:
        name_attribute = '\t\t\tname = %s;\n' % self.name
    else:
      name_attribute = ''
    if self.path:
      if QUOTE_PATH_RE.search(self.path):
        path_attribute = '\t\t\tpath = "%s";\n' % self.path
      else:
        path_attribute = '\t\t\tpath = %s;\n' % self.path
    else:
      path_attribute = ''
    child_lines = []
    for x in range(len(self.child_uuids)):
      child_lines.append('\t\t\t\t%s /* %s */,\n' %
                         (self.child_uuids[x], self.child_names[x]))
    children = ''.join(child_lines)
    tab_width_attribute = ''
    if self._tab_width:
      tab_width_attribute = '\t\t\ttabWidth = %s;\n' % self._tab_width
    uses_tabs_attribute = ''
    if self._uses_tabs:
      uses_tabs_attribute = '\t\t\tusesTabs = %s;\n' % self._uses_tabs
    indent_width_attribute = ''
    if self._indent_width:
      indent_width_attribute = '\t\t\tindentWidth = %s;\n' % self._indent_width
    return '\t\t%s %s= {\n' \
           '\t\t\tisa = %s;\n' \
           '\t\t\tchildren = (\n' \
           '%s' \
           '\t\t\t);\n' \
           '%s' \
           '%s' \
           '%s' \
           '\t\t\tsourceTree = %s;\n' \
           '%s' \
           '%s' \
           '\t\t};\n' % (
            self.uuid, header_comment, 
            self.__class__.__name__,
            children, 
            indent_width_attribute,
            name_attribute,
            path_attribute, self.source_tree,
            tab_width_attribute, uses_tabs_attribute)
            
            
class PBXVariantGroup(PBXGroup):
  pass


class PBXNativeTarget(object):
  """Class for PBXNativeTarget data from an Xcode project file.

  Attributes:
    name: Target name
    build_phase_uuids: Ordered list of build phase UUIDs
    build_phase_names: Ordered list of build phase names

  NOTE: We do not have wrapper classes for all build phase data types!
  """

  PBXNATIVETARGET_HEADER_RE = re.compile(
      r'^\t\t([0-9A-F]{24}) /\* (.*) \*/ = {\n$')
  PBXNATIVETARGET_BUILD_PHASE_RE = re.compile(
      r'^\t\t\t\t([0-9A-F]{24}) /\* (.*) \*/,\n$')

  @classmethod
  def FromContent(klass, content_lines):
    header_parsed = klass.PBXNATIVETARGET_HEADER_RE.match(content_lines[0])
    if not header_parsed:
      raise RuntimeError('PBXNativeTarget unable to parse header content:\n%s'
                          % content_lines[0])
    build_phase_uuids = []
    build_phase_names = []
    content_line_no = 0
    while 1:
      content_line_no += 1
      content_line = content_lines[content_line_no]
      if content_line == '\t\t};\n': break
      if content_line == '\t\t\tisa = PBXNativeTarget;\n': continue
      # Build phases groups
      if content_line == '\t\t\tbuildPhases = (\n':
        content_line_no += 1
        content_line = content_lines[content_line_no]
        while content_line != '\t\t\t);\n':
          phase_parsed = klass.PBXNATIVETARGET_BUILD_PHASE_RE.match(
                                 content_line)
          if not phase_parsed:
            raise RuntimeError(
                    'PBXNativeTarget unable to parse build phase content:\n%s'
                     % content_line)
          build_phase_uuids.append(phase_parsed.group(1))
          build_phase_names.append(phase_parsed.group(2))
          content_line_no += 1
          content_line = content_lines[content_line_no]
        break  # Don't care about the rest of the content
    return klass(content_lines, header_parsed.group(2), build_phase_uuids,
                 build_phase_names)

  def __init__(self, raw_lines, name, build_phase_uuids, build_phase_names):
    self._raw_lines = raw_lines
    self.name = name
    self.build_phase_uuids = build_phase_uuids
    self.build_phase_names = build_phase_names

  def __str__(self):
    return ''.join(self._raw_lines)


class PBXSourcesBuildPhase(object):
  """Class for PBXSourcesBuildPhase data from an Xcode project file.

  Attributes:
    uuid: UUID for this instance
    build_action_mask: Xcode magic mask constant
    run_only_for_deployment_postprocessing: deployment postprocess flag
    file_uuids: Ordered list of PBXBuildFile UUIDs
    file_names: Ordered list of PBXBuildFile names (basename)
  """

  PBXSOURCESBUILDPHASE_HEADER_RE = re.compile(
      r'^\t\t([0-9A-F]{24}) /\* Sources \*/ = {\n$')
  PBXSOURCESBUILDPHASE_FIELD_RE = re.compile(r'^\t\t\t(.*) = (.*);\n$')
  PBXSOURCESBUILDPHASE_FILE_RE = re.compile(
      r'^\t\t\t\t([0-9A-F]{24}) /\* (.*) in Sources \*/,\n$')

  @classmethod
  def FromContent(klass, content_lines):
    header_parsed = klass.PBXSOURCESBUILDPHASE_HEADER_RE.match(content_lines[0])
    if not header_parsed:
      raise RuntimeError(
              'PBXSourcesBuildPhase unable to parse header content:\n%s'
               % content_lines[0])
    # Parse line by line
    build_action_mask = None
    run_only_for_deployment_postprocessing = None
    file_uuids = []
    file_names = []
    content_line_no = 0
    while 1:
      content_line_no += 1
      content_line = content_lines[content_line_no]
      if content_line == '\t\t};\n': break
      if content_line == '\t\t\tisa = PBXSourcesBuildPhase;\n': continue
      # Files
      if content_line == '\t\t\tfiles = (\n':
        content_line_no += 1
        content_line = content_lines[content_line_no]
        while content_line != '\t\t\t);\n':
          file_parsed = klass.PBXSOURCESBUILDPHASE_FILE_RE.match(content_line)
          if not file_parsed:
            raise RuntimeError(
                    'PBXSourcesBuildPhase unable to parse file content:\n%s'
                    % content_line)
          file_uuids.append(file_parsed.group(1))
          file_names.append(file_parsed.group(2))
          content_line_no += 1
          content_line = content_lines[content_line_no]
        continue  # Back to top of loop on end of files list
      # Other fields
      field_parsed = klass.PBXSOURCESBUILDPHASE_FIELD_RE.match(content_line)
      if not field_parsed:
        raise RuntimeError(
                'PBXSourcesBuildPhase unable to parse field content:\n%s'
                % content_line)
      if field_parsed.group(1) == 'buildActionMask':
        build_action_mask = field_parsed.group(2)
      elif field_parsed.group(1) == 'runOnlyForDeploymentPostprocessing':
        run_only_for_deployment_postprocessing = field_parsed.group(2)
      else:
        raise RuntimeError('PBXSourcesBuildPhase unknown field "%s"'
                           % field_parsed.group(1))
    return klass(header_parsed.group(1), build_action_mask,
                 run_only_for_deployment_postprocessing,
                 file_uuids, file_names)

  def __init__(self, uuid, build_action_mask,
               run_only_for_deployment_postprocessing,
               file_uuids, file_names):
    self.uuid = uuid
    self.build_action_mask = build_action_mask
    self.run_only_for_deployment_postprocessing = \
          run_only_for_deployment_postprocessing
    self.file_uuids = file_uuids
    self.file_names = file_names

  def __str__(self):
    file_lines = []
    for x in range(len(self.file_uuids)):
      file_lines.append('\t\t\t\t%s /* %s in Sources */,\n' %
                         (self.file_uuids[x], self.file_names[x]))
    files = ''.join(file_lines)
    return '\t\t%s /* Sources */ = {\n' \
           '\t\t\tisa = PBXSourcesBuildPhase;\n' \
           '\t\t\tbuildActionMask = %s;\n' \
           '\t\t\tfiles = (\n' \
           '%s' \
           '\t\t\t);\n' \
           '\t\t\trunOnlyForDeploymentPostprocessing = %s;\n' \
           '\t\t};\n' % (
            self.uuid, self.build_action_mask, files,
            self.run_only_for_deployment_postprocessing)


def Usage(optparse):
  optparse.print_help()
  print '\n' \
        'Commands:\n' \
        '  list_native_targets: List Xcode "native" (source compilation)\n' \
        '    targets by name.\n' \
        '  list_target_sources: List project-relative source files in the\n' \
        '    specified Xcode "native" target.\n' \
        '  remove_source [sourcefile ...]: Remove the specified source files\n' \
        '    from every target in the project (target is ignored).\n' \
        '  add_source [sourcefile ...]: Add the specified source files\n' \
        '    to the specified target.\n'
  sys.exit(2)


def Main():
  # Use argument structure like xcodebuild commandline
  option_parser = optparse.OptionParser(
                    usage='usage: %prog -p projectname [ -t targetname ] ' \
                          '<command> [...]',
                    add_help_option=False)
  option_parser.add_option(
                  '-h', '--help', action='store_true', dest='help',
                  default=False, help=optparse.SUPPRESS_HELP)
  option_parser.add_option(
                  '-p', '--project', action='store', type='string',
                  dest='project', metavar='projectname',
                  help='Manipulate the project specified by projectname.')
  option_parser.add_option(
                  '-t', '--target', action='store', type='string',
                  dest='target', metavar='targetname',
                  help='Manipulate the target specified by targetname.')
  (options, args) = option_parser.parse_args()

  # Since we have more elaborate commands, handle help
  if options.help:
    Usage(option_parser)

  # Xcode project file
  if not options.project:
    option_parser.error('Xcode project file must be specified.')
  project_path = os.path.abspath(CygwinPathClean(options.project))
  if project_path.endswith('.xcodeproj'):
    project_path = os.path.join(project_path, 'project.pbxproj')
  if not project_path.endswith(os.sep + 'project.pbxproj'):
    option_parser.error('Invalid Xcode project file path \"%s\"' % project_path)
  if not os.path.exists(project_path):
    option_parser.error('Missing Xcode project file \"%s\"' % project_path)

  # Construct project object
  project = XcodeProject(project_path)

  # Switch on command
  if len(args) < 1:
    Usage(option_parser)

  # List native target names
  elif args[0] == 'list_native_targets':
    # List targets
    if len(args) != 1:
      option_parser.error('list_native_targets takes no arguments')
    # Ape xcodebuild output
    target_names = []
    for target in project.NativeTargets():
      target_names.append(target.name)
    print 'Information about project "%s"\n    Native Targets:\n        %s' % (
          project.name,
          '\n        '.join(target_names))

  # List files in a native target
  elif args[0] == 'list_target_sources':
    if len(args) != 1:
      option_parser.error('list_target_sources takes no arguments')
    if not options.target:
      option_parser.error('list_target_sources requires a target')
    # Validate target and get list of files
    target = project.NativeTargetForName(options.target)
    if not target:
      option_parser.error('No native target named "%s"' % options.target)
    sources_phase = project.SourcesBuildPhaseForTarget(target)
    target_files = []
    for source_uuid in sources_phase.file_uuids:
      build_file = project.BuildFileForUUID(source_uuid)
      file_ref = project.FileReferenceForUUID(build_file.file_ref_uuid)
      pretty_path = project.RelativeSourceRootPath(file_ref.abs_path)
      if pretty_path:
        target_files.append(pretty_path)
      else:
        target_files.append(file_ref.abs_path)
    # Ape xcodebuild output
    print 'Information about project "%s" target "%s"\n' \
          '    Files:\n        %s' % (project.name, options.target,
                                      '\n        '.join(target_files))

  # Remove source files
  elif args[0] == 'remove_source':
    if len(args) < 2:
      option_parser.error('remove_source needs one or more source files')
    if options.target:
      option_parser.error(
        'remove_source does not support removal from a single target')
    for source_path in args[1:]:
      source_path = CygwinPathClean(source_path)
      found = False
      for file_ref in project.FileReferences():
        # Try undecorated path, abs_path and our prettified paths
        if (file_ref.path == source_path or (
              file_ref.abs_path and (
              file_ref.abs_path == os.path.abspath(source_path) or
              project.RelativeSourceRootPath(file_ref.abs_path) == source_path))):
          # Found a matching file ref, remove it
          found = True
          project.RemoveSourceFileReference(file_ref)
      if not found:
        option_parser.error('No matching source file "%s"' % source_path)
    project.Update()

  # Add source files
  elif args[0] == 'add_source':
    if len(args) < 2:
      option_parser.error('add_source needs one or more source files')
    if not options.target:
      option_parser.error('add_source requires a target')
    # Look for the target we want to add too.
    target = project.NativeTargetForName(options.target)
    if not target:
      option_parser.error('No native target named "%s"' % options.target)
    # Get the sources build phase
    sources_phase = project.SourcesBuildPhaseForTarget(target)
    # Loop new sources
    for source_path in args[1:]:
      source_path = CygwinPathClean(source_path)
      if not os.path.exists(os.path.abspath(source_path)):
        option_parser.error('File "%s" not found' % source_path)
      # Don't generate duplicate file references if we don't need them
      source_ref = None
      for file_ref in project.FileReferences():
        # Try undecorated path, abs_path and our prettified paths
        if (file_ref.path == source_path or (
              file_ref.abs_path and (
              file_ref.abs_path == os.path.abspath(source_path) or
              project.RelativeSourceRootPath(file_ref.abs_path) == source_path))):
          source_ref = file_ref
          break
      if not source_ref:
        # Create a new source file ref
        source_ref = project.AddSourceFile(os.path.abspath(source_path))
      # Add the new source file reference to the target if its a safe type
      if source_ref.file_type in SOURCES_XCODE_FILETYPES:
        project.AddSourceFileToSourcesBuildPhase(source_ref, sources_phase)
    project.Update()
    
  # Private sanity check. On an unmodified project make sure our output is
  # the same as the input
  elif args[0] == 'parse_sanity':
    if ''.join(project.FileContent()) != ''.join(project._raw_content):
      option_parser.error('Project rewrite sanity fail "%s"' % project.path)
    
  else:
    Usage(option_parser)


if __name__ == '__main__':
  Main()
