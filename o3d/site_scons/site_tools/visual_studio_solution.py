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

"""Visual Studio solution file generation tool for SCons."""


import sys
import SCons.Script


def Solution(env, solution_name,
             environments,
             exclude_pattern=None,
             extra_build_targets=None):
  """Builds an MSVS solution containing all projects underneath build/win.

  Args:
    solution_name: Name of the solution.
    environments: List of environments for variants. Only the first one
      will be used to build the solutions/projects.
    exclude_pattern: Files matching this pattern will not be added to the
      projects and solution.
    extra_build_targets: Dict of extra build targets, indexed by target
      name.  Each extra build target will be given
      its own empty project.
  """

  # Provide an empty dict for the set of extra targets, by default.
  if extra_build_targets is None:
    extra_build_targets = dict()

  # Fail if not on windows for now.
  if sys.platform not in ['win32', 'cygwin']:
    print ('*** Solution file generation skipped '
           '(not supported on this platform).')
    return

  # Add in the msvs tool.
  env.Tool('msvs')

  # Pick out variants
  variants = [e['BUILD_TYPE'] for e in environments]
  # Pick out build targets
  build_targets = [e.subst('$TARGET_ROOT') for e in environments]
  # pick out sources, headers, and resources
  sources, headers, resources, others = env.GatherInputs(
      [env.Dir('$DESTINATION_ROOT')],
      ['.+\\.(c|cc|m|mm|cpp)$',  # source files
       '.+\\.(h|hh|hpp)$',       # header files
       '.+\\.(rc)$',             # resource files
       '.*'],                    # all other files
      exclude_pattern=exclude_pattern,
  )
  # Build main Visual Studio Project file
  project_list = env.MSVSProject(target=solution_name +
                                 env['MSVSPROJECTSUFFIX'],
                                 srcs=sources + headers + others + resources,
                                 incs=[],
                                 misc=[],
                                 resources=[],
                                 auto_build_solution=0,
                                 MSVSCLEANCOM='hammer.bat -c MODE=all',
                                 MSVSBUILDCOM='hammer.bat MODE=all',
                                 MSVSREBUILD='hammer.bat -c MODE=all;'
                                     'hammer.bat MODE=all',
                                 buildtarget=build_targets,
                                 variant=variants)
  # Collect other projects
  for e in extra_build_targets:
    # Explicitly create a node for target, so SCons will expand env variables.
    build_target = env.File(extra_build_targets[e])
    # Create an empty project that only has a build target.
    project_list += env.MSVSProject(target='projects/' + e + '/' + e +
                                    env['MSVSPROJECTSUFFIX'],
                                    srcs=[],
                                    incs=[],
                                    resources=[],
                                    misc=[],
                                    auto_build_solution=0,
                                    MSVSCLEANCOM='rem',
                                    MSVSBUILDCOM='rem',
                                    MSVSREBUILD='rem',
                                    buildtarget=build_target,
                                    variant=variants[0])

  # Build Visual Studio Solution file.
  solution = env.MSVSSolution(target=solution_name + env['MSVSSOLUTIONSUFFIX'],
                              projects=project_list,
                              variant=variants)
  # Explicitly add dependencies.
  env.Depends(solution, project_list)

  return solution


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Add in the gather_inputs tool.
  env.Tool('gather_inputs')

  # Add a method to generate a combined solution file.
  env.AddMethod(Solution, 'Solution')
