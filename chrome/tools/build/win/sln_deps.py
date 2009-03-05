#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import optparse

__version__ = "1.0"

# Constants used by Visual Studio.
UNKNOWN_GUID = "{00000000-0000-0000-0000-000000000000}"
FOLDER_GUID = "{2150E333-8FDC-42A3-9474-1A3956D46DE8}"
C_PROJECT_GUID = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"
CSHARP_PROJECT_GUID = "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}"

# A project defines it name, its guid and its dependencies.
class Project:
  def __init__(self):
    self.name = ""
    self.type = UNKNOWN_GUID
    self.deps = []

  def __str__(self):
    return self.name


def ScanSlnFile(filename):
  """Scan a Visual Studio .sln and extract the project dependencies."""
  try:
    sln = open(filename, "r")
  except IOError:
    sys.stderr.write("Unable to open " + filename + " for reading.\n")
    return 1
  projects = {}
  project = None
  while 1:
    line = sln.readline().strip()
    if not line:
      break

    if line.startswith('Project("{'):
      # Project definition line looks like
      # Project("$TypeGuid") = "$ProjectName", "$ProjectPath", "$ProjectGuid"$
      items = line.split('"')
      project = Project()
      project.name = items[3]
      project.path = items[5]
      project.guid = items[7]
      project.type = items[1]
      projects[items[7]] = project

    # Start of a dependency group.
    if line == "ProjectSection(ProjectDependencies) = postProject":
      line = sln.readline().strip()
      # End of a dependency group.
      while line and line != "EndProjectSection":
        project.deps.append(line[:len(project.guid)])
        line = sln.readline().strip()

  # We are done parsing.
  sln.close()
  return projects


def main(filename, project_to_scan, reverse):
  """Displays the project's dependencies."""
  project_to_scan = project_to_scan.lower()

  projects = ScanSlnFile(filename)

  if reverse:
    # Inverse the dependencies map so they are displayed in the reverse order.
    # First, create a copy of the map.
    projects_reversed = copy.deepcopy(projects)
    for project_reversed in projects_reversed.itervalues():
      project_reversed.deps = []
    # Then, assign reverse dependencies.
    for project in projects.itervalues():
      for dep in project.deps:
        projects_reversed[dep].deps.append(project.guid)
    projects = projects_reversed

  # Print the results.
  for project in projects.itervalues():
    if project.type == FOLDER_GUID:
      continue
    if project_to_scan and project.name.lower() != project_to_scan:
      continue
    print project.name
    deps_name = [projects[d].name for d in project.deps]
    print "\n".join(str("  " + name) for name in sorted(deps_name,
                                                        key=str.lower))


if __name__ == '__main__':
  usage = "usage: %prog [options] solution [project]"

  description = ("Display the dependencies of a project in human readable"
                 " form. [project] is optional. If omited, all projects are"
                 " listed.")

  option_parser = optparse.OptionParser(usage = usage,
                                        version="%prog " + __version__,
                                        description = description)
  option_parser.add_option("-r",
                           "--reverse",
                           dest="reverse",
                           action="store_true",
                           default=False,
                           help="Display the reverse dependencies")
  options, args = option_parser.parse_args()
  if len(args) != 1 and len(args) != 2:
    option_parser.error("incorrect number of arguments")

  project_to_scan = ""
  if len(args) == 2:
    project_to_scan = args[1]
  main(args[0], project_to_scan, options.reverse)

