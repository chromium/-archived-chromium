#!/usr/bin/env python
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import sys

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


def main(filename, project_to_scan):
  project_to_scan = project_to_scan.lower()
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
  if len(sys.argv) != 2 and len(sys.argv) != 3:
    print """Usage: sln_deps.py <SOLUTIONNAME>.sln [project]
  to display the dependencies of a project in human readable form.

  [project] is optional. If omited, all projects are listed."""
    sys.exit(1)

  project_to_scan = ""
  if len(sys.argv) == 3:
    project_to_scan = sys.argv[2]
  sys.exit(main(sys.argv[1], project_to_scan))
