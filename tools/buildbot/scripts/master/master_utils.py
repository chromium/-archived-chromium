#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Set of utilities common to build masters."""

import os
import re

import chromium_config as config
import chromium_factory
import factory_commands


class MasterFactory(object):
  """Encapsulates data and methods common to both (all) master.cfg files."""

  DEFAULT_TARGET_PLATFORM = config.Master.default_platform

  # gclient additional custom deps
  CUSTOM_DEPS_DISABLE_WEBKIT = ('src/third_party/WebKit', None)
  CUSTOM_DEPS_DISABLE_V8 = ('src/v8', None)
  CUSTOM_DEPS_V8_LATEST = ('src/v8',
    'http://v8.googlecode.com/svn/branches/bleeding_edge')

  # Internal custom deps
  # Used by 'node_leak' and 'playback' tests.
  # TODO(not_me): Need to move this to new repository so we can access it from
  # the new buildbots.
  CUSTOM_DEPS_INTERNAL_DISABLE_SAVEDCACHE_DATA = (
    'src/data/saved_caches', None)

  # Memory tool.
  CUSTOM_DEPS_INTERNAL_MEMORY_TOOL = ('src/chrome/tools/memory', None)
  # Preinstalled on the trybot. Unnecessary on non-Windows platforms.
  CUSTOM_DEPS_INTERNAL_DISABLE_PLATFORM_SDK = (
    'src/third_party/platformsdk_vista_6_0', None)

  # A map used to skip dependencies when a test is not run.
  # The map key is the test name. The map value is an array containing the
  # dependencies that are not needed when this test is not run.
  NEEDED_COMPONENTS = {
    'webkit': [('src/webkit/data/layout_tests/LayoutTests', None)]}

  NEEDED_COMPONENTS_INTERNAL = {
    'memory_test':
      [('src/data/memory_test', None)],
    'page_cycler':
      [('src/data/page_cycler', None)],
    'selenium':
      [('src/data/selenium_core', None)],
    'tab_switching':
      [('src/data/tab_switching', None)],
    'ui':
      [('src/chrome/test/data/firefox2_profile/searchplugins', None),
       ('src/chrome/test/data/firefox2_searchplugins', None),
       ('src/chrome/test/data/firefox3_profile/searchplugins', None),
       ('src/chrome/test/data/firefox3_searchplugins', None),
       ('src/chrome/test/data/plugin', None),
       ('src/chrome/test/data/ssl/certs', None)],
    '^(unit|purify_chrome)$':
      [('src/chrome/test/data/osdd', None)],
    '^(webkit|test_shell)$':
      [('src/webkit/data/bmp_decoder', None),
       ('src/webkit/data/ico_decoder', None),
       ('src/webkit/data/test_shell/plugins', None),
       ('src/webkit/data/xbm_decoder', None)],
    # Unused stuff:
    'autodiscovery':
      [('src/data/autodiscovery', None)],
    'esctf':
      [('src/data/esctf', None)],
    'grit':
      [('src/tools/grit/grit/test/data', None)],
    'mozilla_js':
      [('src/data/mozilla_js_tests', None)],
  }

  def __init__(self, build_dir, svnurl, svnurl_internal=None,
               target_platform=None):
    """Sets paths to source, URLs, paths to tools, and similar information
    for use by class methods.

    Args:
      build_dir: name of the directory within the buildbot working directory
        in which the solution, Debug, and Release directories are found.  This
        will be used by default by build commands and steps, but can be
        overridden by the factories.  By default, the solution's name will be
        derived from this directory name by appending '.sln'.
      svnurl: full svn URL for which 'gclient config' will be called when
        needed for a checkout clobber.  This will be used by default by build
        steps, but can be overridden by the individual factories.
      svnurl_internal: Second url used to fetch internal dependencies
      target_platform: Target platform of the slave.
    """
    self._build_dir = build_dir
    self._svn_url = svnurl
    self._svn_url_internal = svnurl_internal

    if target_platform is None:
      target_platform = self.DEFAULT_TARGET_PLATFORM
    self._target_platform = target_platform

    if self._target_platform == 'win32':
      # Look for a solution named for its enclosing directory.
      self._solution = os.path.basename(self._build_dir) + '.sln'
    else:
      self._solution = '__solution__'

  def _RemoveUnusedComponents(self, unneeded_components, custom_deps, tests):
    """Internal function to modify a custom dependencies array."""
    for test, dependencies in unneeded_components.iteritems():
      if self._ShouldRunTest(tests, test):
        continue
      custom_deps.extend(dependencies)

  def _FixDeps(self, custom_deps, custom_deps_internal, tests):
    """Remove all dependencies for tests that aren't run."""
    self._RemoveUnusedComponents(self.NEEDED_COMPONENTS, custom_deps, tests)
    self._RemoveUnusedComponents(self.NEEDED_COMPONENTS_INTERNAL,
                                 custom_deps_internal, tests)

    # Disable what is on the internal svn server.
    custom_deps_internal.append(
        self.CUSTOM_DEPS_INTERNAL_DISABLE_SAVEDCACHE_DATA)

    if self._target_platform != 'win32':
      # Non-Windows platforms don't need the Windows SDK nor the not-yet-ported
      # memory tools.
      custom_deps_internal.extend([
        self.CUSTOM_DEPS_INTERNAL_DISABLE_PLATFORM_SDK,
        self.CUSTOM_DEPS_INTERNAL_MEMORY_TOOL])

  def _ShouldRunTest(self, tests, name):
    for test in tests:
      if re.match(name, test):
        return True
    return False

  def _AddTests(self, factory_cmd_obj, tests,
                run_crash_handler=True,
                show_perf_results=False,
                with_pageheap=False,
                perf_id=None,
                archive_webkit_results=False,
                run_single_process_ui=True):
    """Add the tests listed in 'tests' to the factory_cmd_obj."""
    # Start the crash handler process.
    if run_crash_handler:
      factory_cmd_obj.AddRunCrashHandler()

    if self._ShouldRunTest(tests, 'unit'):
      factory_cmd_obj.AddChromeUnitTests()
    if self._ShouldRunTest(tests, 'ui'):
      factory_cmd_obj.AddUITests(with_pageheap,
                                 run_single_process=run_single_process_ui)
    if self._ShouldRunTest(tests, 'interactive_ui'):
      factory_cmd_obj.AddInteractiveUITests()
    if self._ShouldRunTest(tests, 'test_shell'):
      factory_cmd_obj.AddTestShellTests()
    if self._ShouldRunTest(tests, 'node_leak'):
      factory_cmd_obj.AddNodeLeakTest('alexa_100.zip', 'alexa_100.txt')
    if self._ShouldRunTest(tests, 'webkit'):
      factory_cmd_obj.AddWebkitTests(with_pageheap,
                                     archive_results=archive_webkit_results)
    if self._ShouldRunTest(tests, 'plugin'):
      factory_cmd_obj.AddPluginTests()
    if self._ShouldRunTest(tests, 'page_cycler'):
      factory_cmd_obj.AddPageCyclerTests(show_perf_results, perf_id=perf_id)
    if self._ShouldRunTest(tests, 'page_cycler_http'):
      factory_cmd_obj.AddPageCyclerTests(show_perf_results, perf_id=perf_id,
                                         http=True)
    if self._ShouldRunTest(tests, 'startup'):
      factory_cmd_obj.AddStartupTests(show_perf_results, perf_id=perf_id)
      factory_cmd_obj.AddNewTabUITests(show_perf_results, perf_id=perf_id)
    if self._ShouldRunTest(tests, 'memory'):
      factory_cmd_obj.AddMemoryTests(show_perf_results, perf_id=perf_id)
    if self._ShouldRunTest(tests, 'tab_switching'):
      factory_cmd_obj.AddTabSwitchingTests(show_perf_results, perf_id=perf_id)
    if self._ShouldRunTest(tests, 'purify_base'):
      factory_cmd_obj.AddPurifyTest('base')
    if self._ShouldRunTest(tests, 'purify_webkit'):
      factory_cmd_obj.AddPurifyTest('test_shell')
    if self._ShouldRunTest(tests, 'purify_net'):
      factory_cmd_obj.AddPurifyTest('net')
    if self._ShouldRunTest(tests, 'purify_chrome'):
      factory_cmd_obj.AddPurifyTest('unit')
    if self._ShouldRunTest(tests, 'purify_layout'):
      factory_cmd_obj.AddPurifyTest('layout', timeout=3600)
    if self._ShouldRunTest(tests, 'selenium'):
      factory_cmd_obj.AddSeleniumTests()
    if self._ShouldRunTest(tests, 'qemu'):
      factory_cmd_obj.AddQueryQemu()
    if self._ShouldRunTest(tests, 'playback'):
      factory_cmd_obj.AddPlaybackTest('gmail', 'gmail.zip')
      factory_cmd_obj.AddPlaybackTest('maps', 'maps.zip')
      factory_cmd_obj.AddPlaybackTest('docs', 'docs.zip')
    if self._ShouldRunTest(tests, 'omnibox'):
      factory_cmd_obj.AddOmniboxTests()
    if self._ShouldRunTest(tests, 'base'):
      factory_cmd_obj.AddBaseTests()
    if self._ShouldRunTest(tests, 'net'):
      factory_cmd_obj.AddNetTests()
    if self._ShouldRunTest(tests, 'googleurl'):
      factory_cmd_obj.AddGoogleURLTests()
    # When adding a test that uses a new executable, update kill_processes.py.

  def _BuildGClientSolution(self, svnurl, custom_deps_list):
    """Returns a string describing a gclient solution.

    Args:
      svnurl: the URL from which the files should be checked out.  The last
          component of this URL will be used as the destination directory name.
      custom_deps_list: A list of tuples (name, custom_dependency) to be added
          to the gclient spec as custom deps.  Names should be strings;
          dependencies should be strings or None.
    Returns:
      A string defining the gclient solution.
    """
    name = svnurl.split('/')[-1]
    if 'chrome_webkit_merge_branch' == name:
      name = 'src'
    custom_deps = ''
    if custom_deps_list is not None:
      for dep in custom_deps_list:
        if dep[1] is None:
          dep_url = None
        else:
          dep_url = '"%s"' % dep[1]
        custom_deps += '"%s" : %s, ' % (dep[0], dep_url)
    # This must not contain any line breaks or other characters that would
    # require escaping on the command line, since it will be passed to gclient.
    spec = (
      '{ "name": "%s", '
        '"url": "%s", '
        '"custom_deps": {'
                          '%s'
                       '}'
      '}' % (name, svnurl, custom_deps)
    )
    return spec

  def _BuildGClientSpec(self, svnurl, svnurl_internal=None,
                        custom_deps=None, custom_deps_internal=None):
    """Returns a custom spec for gclient that includes the given custom deps.

    Args:
      svnurl: the URL from which the files should be checked out.  The last
          component of this URL will be used as the destination directory name.
      svnurl_internal: same as svnurl, but for the internal components.
      custom_deps: A list of tuples (name, custom_dependency) to be added
          to the gclient spec as custom deps.  Names should be strings;
          dependencies should be strings or None.
      custom_deps_internal: same as custom_deps, but for internal components.
    Returns:
      A string defining the gclient spec.
    """
    spec = 'solutions = ['
    spec += self._BuildGClientSolution(svnurl, custom_deps)
    if svnurl_internal is not None:
      spec += ','
      spec += self._BuildGClientSolution(svnurl_internal,
                                         custom_deps_internal)
    spec += ']'

    return spec

  def NewGClientFactory(self, gclient_spec):
    """Creates and returns a new factory that includes steps to kill any
    leftover processes, update the slave's scripts, and update the build
    checkout.

    Args:
      gclient_spec: this spec will be passed to gclient.
    Returns:
      A buildbot factory.
    """
    factory = chromium_factory.BuildFactory()
    factory_cmd_obj = factory_commands.FactoryCommands(
        factory,
        target_platform=self._target_platform)
    # First kill any svn.exe tasks so we can update in peace, and
    # afterwards use the checked-out script to kill everything else.
    if self._target_platform == 'win32':
      factory_cmd_obj.AddSvnKillStep()
    factory_cmd_obj.AddUpdateScriptStep()
    factory_cmd_obj.AddUpdateStep(gclient_spec)
    if self._target_platform == 'win32':
      factory_cmd_obj.AddTaskkillStep()
    return factory

  def NewBuildFactory(self, identifier, target='Release', build_dir=None,
                      solution=None, svnurl=None, svnurl_internal=None,
                      clobber=False, tests=[], with_pageheap=False,
                      archive_webkit_results=False, show_perf_results=False,
                      perf_id=None, archive_build=False, check_deps=False,
                      gclient_custom_deps=None,
                      gclient_custom_deps_internal=None, mode=None,
                      slave_type='BuilderTester', run_crash_handler=False,
                      options=None, compile_timeout=1200):
    """Returns a new build factory, including download, build, and test steps.

    Args:
      identifier: full identifier for this build.  Typically the builder name
          (e.g., 'chrome-release').
      target: Build configuration, case-sensitive; probably 'Debug' or
          'Release'
      build_dir: directory within the buildbot's working directory, the parent
          of the 'Debug' or 'Release' build output directory (e.g., 'chrome').
          If none is given, self._build_dir will be used.
      solution: solution to build.  If none is given, self._solution will be
          used.
      svnurl: URL to pass to gclient for config and checkout.  If none is
          given, self._svn_url will be used.
      svnurl_internal: same as svnurl, but for internal components. If none is
          given, self._svn_url_internal will be used.
      clobber: whether to delete the build output directory before building
      tests: list of tests to run, chosen from
               ('unit', 'ui', 'test_shell', 'webkit', 'plugin', 'page_cycler',
                'page_cycler_http', 'startup', 'selenium', 'qemu',
                'purify_webkit', 'purify_base', 'purify_net', 'purify_chrome',
                'purify_layout', 'playback', 'node_leak', 'tab_switching',
                'omnibox', 'memory, 'interactive_ui', 'base', 'net').
         The 'unit' suite includes the IPC tests.
      arhive_webkit_results: whether to archive the webkit test output
      show_perf_results: whether to add links to the test perf result graphs
      perf_id: if show_perf_results is true, perf_id is the name of the slave
          on the perf result page.
      archive_build: whether to archive the built executable and its symbols
      check_deps: whether to run a step that checks source-file dependencies
      gclient_custom_deps: a list of tuples (name, custom_dependency) to append
          to the gclient spec created from the svnurl and test list by
          _BuildGClientSpec().  Names should be strings; dependencies should
          be strings or None.
      gclient_custom_deps_internal: same as gclient_custom_deps but for
          internal components.
      mode: the mode used for the compile step.
      slave_type: The type of slave. This can be 'Builder', 'Tester',
          'BuilderTester' or 'Trybot'.
      run_crash_handler: whether to run crash_service.exe before the tests.
      options: Builder-specific options to be added to the CompileStep.
      compile_timeout: Timeout for the compile step.
    Returns:
      A buildbot factory.
    """
    if build_dir is None:
      build_dir = self._build_dir
    if solution is None:
      solution = self._solution
    if svnurl is None:
      svnurl = self._svn_url
    if svnurl_internal is None:
      svnurl_internal = self._svn_url_internal

    # Collect custom deps to remove unnecessary checkouts.
    if gclient_custom_deps is None:
      custom_deps = []
    else:
      custom_deps = gclient_custom_deps

    if gclient_custom_deps_internal is None:
      custom_deps_internal = []
    else:
      custom_deps_internal = gclient_custom_deps_internal

    self._FixDeps(custom_deps, custom_deps_internal, tests)
    if slave_type == 'Trybot' and self._target_platform == 'win32':
      # Trybot have the platform sdk preinstalled to speed up the checkout. So
      # skip it even on Windows. It's already skipped on other platforms.
      custom_deps_internal.append(
        self.CUSTOM_DEPS_INTERNAL_DISABLE_PLATFORM_SDK)
    gclient_spec = self._BuildGClientSpec(svnurl, svnurl_internal, custom_deps,
                                          custom_deps_internal)

    factory = self.NewGClientFactory(gclient_spec)
    factory_cmd_obj = factory_commands.FactoryCommands(
        factory, identifier, target, build_dir, self._target_platform)

    if check_deps:
      factory_cmd_obj.AddCheckDepsStep()

    if (slave_type == 'BuilderTester' or slave_type == 'Builder' or
        slave_type == 'Trybot'):
      factory_cmd_obj.AddCompileStep(solution, clobber, mode=mode,
                                     options=options, timeout=compile_timeout)

    # Archive the generated build.
    if archive_build:
      factory_cmd_obj.AddArchiveBuild()

    # Archive the full output directory if the machine is a builder.
    if slave_type == 'Builder':
      factory_cmd_obj.AddArchiveBuild('full')
      factory_cmd_obj.AddUploadBuild()

    # Download the full output directory if the machine is a tester.
    if slave_type == 'Tester':
      factory_cmd_obj.AddDownloadBuild()
      factory_cmd_obj.AddExtractBuild()

    run_single_process_ui = slave_type != 'Trybot'
    self._AddTests(factory_cmd_obj, tests,
                   run_crash_handler=run_crash_handler,
                   show_perf_results=show_perf_results,
                   with_pageheap=with_pageheap,
                   perf_id=perf_id,
                   archive_webkit_results=archive_webkit_results,
                   run_single_process_ui=run_single_process_ui)
    return factory

  def NewOfficialBuildFactory(self, identifier='chrome-official', svn_url=None):
    """Creates the factory used by the official builder."""

    target = 'Release'
    build_dir = self._build_dir
    solution = self._solution
    if not svn_url:
      svn_url = (config.Master.server_url +
          '/chrome/branches/chrome_official_branch/src')
    custom_deps = []
    custom_deps_internal = []
    tests = ['unit', 'ui', 'test_shell', 'page_cycler', 'startup', 'selenium']
    self._FixDeps(custom_deps, custom_deps_internal, tests)
    gclient_spec = self._BuildGClientSpec(svn_url, self._svn_url_internal,
                                          custom_deps, custom_deps_internal)
    factory = self.NewGClientFactory(gclient_spec)
    factory_cmd_obj = factory_commands.FactoryCommands(
        factory, identifier, target, build_dir, self._target_platform)

    factory_cmd_obj.AddCompileStep(solution, clobber=True, timeout=3000,
                                   mode='official')
    factory_cmd_obj.AddDifferentialBuildStep()

    self._AddTests(factory_cmd_obj, tests, with_pageheap=True)

    factory_cmd_obj.AddStageBuildStep()

    return factory

  #######
  # Submodule factories

  def _GetSubmodulesSpec(self, submodule_name, tests, custom_deps=None,
                         custom_deps_internal=None):
    """Returns the gclient spec that should be used by the submodules."""
    if custom_deps is None:
      custom_deps = []
    if custom_deps_internal is None:
      custom_deps_internal = []

    custom_deps.extend([
      self.CUSTOM_DEPS_DISABLE_V8,
      self.CUSTOM_DEPS_DISABLE_WEBKIT])
    self._FixDeps(custom_deps, custom_deps_internal, tests)
    gclient_spec = self._BuildGClientSpec(self._svn_url,
                                          self._svn_url_internal,
                                          custom_deps, custom_deps_internal)
    return gclient_spec

  def NewSubmoduleFactory(self, identifier, target='Release', options=None,
                          mode=None, modules=None):
    if modules is None:
      modules = ['base', 'net', 'sandbox']
    tests = ['unit']
    factory = self.NewGClientFactory(self._GetSubmodulesSpec('submodules',
                                                             tests))

    if 'base' in modules:
      factory_cmd_obj = factory_commands.FactoryCommands(factory, identifier,
                                                         target, 'src/base',
                                                         self._target_platform)
      factory_cmd_obj.AddCompileStep(solution='base.sln',
                                     description='compiling base',
                                     descriptionDone='compile base',
                                     options=options, mode=mode)
      if mode == 'purify':
        factory_cmd_obj.AddPurifyTest('base')
      else:
        factory_cmd_obj.AddBasicGTestTestStep('base_unittests')

    if 'net' in modules:
      factory_cmd_obj = factory_commands.FactoryCommands(factory, identifier,
                                                         target, 'src/net',
                                                         self._target_platform)
      # net does not work with mode='purify', so ignore the mode here.
      # see bug 1335595.
      factory_cmd_obj.AddCompileStep(solution='net.sln',
                                     description='compiling net',
                                     descriptionDone='compile net',
                                     options=options)
      if mode == 'purify':
        factory_cmd_obj.AddPurifyTest('net')
      else:
        factory_cmd_obj.AddBasicGTestTestStep('net_unittests')

    if 'sandbox' in modules and mode != 'purify':
      factory_cmd_obj = factory_commands.FactoryCommands(factory, identifier,
                                                         target, 'src/sandbox',
                                                          self._target_platform)
      factory_cmd_obj.AddCompileStep(solution='sandbox.sln',
                                     description='compiling sandbox',
                                     descriptionDone='compile sandbox',
                                     options=options, mode=mode)

      factory_cmd_obj.AddBasicGTestTestStep('sbox_unittests')
      factory_cmd_obj.AddBasicGTestTestStep('sbox_integration_tests')
      factory_cmd_obj.AddBasicGTestTestStep('sbox_validation_tests')

    return factory


def SplitPath(projects, path):
  """Common method to split SVN path into branch and filename sections.

  Since we have multiple projects, we announce project name as a branch
  so that schedulers can be configured to kick off builds based on project
  names.

  Args:
    projects: array containing modules we are interested in. It should
      be mapping to first directory of the change file.
    path: Base SVN path we will be polling.

  More details can be found at:
    http://buildbot.net/repos/release/docs/buildbot.html#SVNPoller.
  """
  pieces = path.split('/')
  if pieces[0] in projects:
    # announce project name as a branch
    branch = pieces.pop(0)
    return (branch, '/'.join(pieces))
  # not in projects, ignore
  return None
