#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Set of utilities to add commands to a buildbot factory."""

import os

from buildbot.steps import shell
from buildbot.steps import transfer

import chromium_config as config
import chromium_step
import chromium_utils

from log_parser import archive_command
from log_parser import cl_command
from log_parser import gtest_command
from log_parser import process_log
from log_parser import retcode_command
from log_parser import webkit_test_command


class FactoryCommands(object):
  """Encapsulates data and methods to add commands to a buildbot factory."""

  # --------------------------------------------------------------------------
  # PERF TEST SETTINGS
  # In each mapping below, the first key is the target and the second is the
  # perf_id. The value is the directory name in the results URL.

  # Base URL for performance test results.
  PERF_BASE_URL = config.Master.perf_base_url
  PERF_REPORT_URL_SUFFIX = config.Master.perf_report_url_suffix

  # Directory in which to save perf output data files.
  PERF_OUTPUT_DIR = config.Master.perf_output_dir

  # Configuration of PageCycler tests.
  PAGECYCLER_TEST_MAPPINGS = {
    'Release': {
      'chromium-rel-xp': 'xp-release',
      'chromium-rel-jsc': 'xp-release-jsc',
      'chromium-rel-xp-dual': 'xp-release-dual-core',
      'chromium-rel-xp-single': 'xp-release-single-core',
      'chromium-rel-vista-dual': 'vista-release-dual-core',
      'chromium-rel-vista-single': 'vista-release-single-core',
    }
  }

  # Configuration of Startup tests.
  STARTUP_TEST_MAPPINGS = {
    'Release': {
      'chromium-rel-xp': 'xp-release/startup',
      'chromium-rel-jsc': 'xp-release-jsc/startup',
      'chromium-rel-xp-dual': 'xp-release-dual-core/startup',
      'chromium-rel-xp-single': 'xp-release-single-core/startup',
      'chromium-rel-vista-dual': 'vista-release-dual-core/startup',
      'chromium-rel-vista-single': 'vista-release-single-core/startup',
    }
  }

  # Configuration of NewTabUIStartup tests.
  NEW_TAB_UI_COLD_TEST_MAPPINGS = {
    'Release': {
      'chromium-rel-xp': 'xp-release/new-tab-ui-cold',
      'chromium-rel-jsc': 'xp-release-jsc/new-tab-ui-cold',
      'chromium-rel-xp-dual': 'xp-release-dual-core/new-tab-ui-cold',
      'chromium-rel-xp-single': 'xp-release-single-core/new-tab-ui-cold',
      'chromium-rel-vista-dual': 'vista-release-dual-core/new-tab-ui-cold',
      'chromium-rel-vista-single': 'vista-release-single-core/new-tab-ui-cold',
    }
  }
  NEW_TAB_UI_WARM_TEST_MAPPINGS = {
    'Release': {
      'chromium-rel-xp': 'xp-release/new-tab-ui-warm',
      'chromium-rel-jsc': 'xp-release-jsc/new-tab-ui-warm',
      'chromium-rel-xp-dual': 'xp-release-dual-core/new-tab-ui-warm',
      'chromium-rel-xp-single': 'xp-release-single-core/new-tab-ui-warm',
      'chromium-rel-vista-dual': 'vista-release-dual-core/new-tab-ui-warm',
      'chromium-rel-vista-single': 'vista-release-single-core/new-tab-ui-warm',
    }
  }

  # Configuration of TabSwitching tests.
  TAB_SWITCHING_TEST_MAPPINGS = {
    'Release': {
      'chromium-rel-xp': 'xp-release/tab-switching',
      'chromium-rel-jsc': 'xp-release-jsc/tab-switching',
      'chromium-rel-xp-dual': 'xp-release-dual-core/tab-switching',
      'chromium-rel-xp-single': 'xp-release-single-core/tab-switching',
      'chromium-rel-vista-dual': 'vista-release-dual-core/tab-switching',
      'chromium-rel-vista-single': 'vista-release-single-core/tab-switching',
    }
  }

  # Configuration of Memory tests.
  MEMORY_TEST_MAPPINGS = {
    'Release': {
      'chromium-rel-xp': 'xp-release/memory',
      'chromium-rel-jsc': 'xp-release-jsc/memory',
      'chromium-rel-xp-dual': 'xp-release-dual-core/memory',
      'chromium-rel-xp-single': 'xp-release-single-core/memory',
      'chromium-rel-vista-dual': 'vista-release-dual-core/memory',
      'chromium-rel-vista-single': 'vista-release-single-core/memory',
    }
  }


  def __init__(self, factory=None, identifier=None, target=None,
               build_dir=None, target_platform=None):
    """Initializes the SlaveCommands class.

    Args:
      factory: Factory to configure
      identifier: full identifier for this build.  Typically the builder name
          (e.g., 'chrome-release').
      target: Build configuration, case-sensitive; probably 'Debug' or
          'Release'
      build_dir: name of the directory within the buildbot working directory
        in which the solution, Debug, and Release directories are found.
      target_platform: Slave's OS.
    """

    self._factory = factory
    self._identifier = identifier
    self._target = target
    self._build_dir = build_dir
    self._target_platform = target_platform

    if self._target_platform == 'win32':
      self._perl = 'perl.exe'
      # Steps run using a separate copy of python.exe, so it can be killed at
      # the start of a build. But the kill_processes (taskkill) step has to use
      # the original python.exe, or it kills itself.
      # Starting from e.g. C:\b\slave\chrome-release\build, find
      # c:\b\slave\chrome-release\build\third_party\python_24.
      # os.path.join() doesn't work here because cmd.exe needs '\'.
      python_path = '\\'.join(['src', 'third_party', 'python_24'])
      self._python = '\\'.join([python_path, 'python_slave.exe'])
      self._taskkill_python = '\\'.join([python_path, 'python.exe'])
    else:
      self._perl = 'perl'
      self._python = 'python'
      self._taskkill_python = 'python'

    self._working_dir = 'build'
    self._repository_root = 'src'

    # Where to point waterfall links for builds and test results.
    self._archive_url = config.Master.archive_url

    # Purify isn't in the script_dir, it's out on its own.
    self._purify_tool = os.path.join('src', 'tools', 'purify',
                                     'chrome_tests.py')

    # These tools aren't in the script_dir either.
    # TODO(pamg): For consistency, move them into the script_dir if possible.
    self._check_deps_tool = os.path.join('src', 'tools', 'checkdeps',
                                         'checkdeps.py')
    self._debugger_test_tool = os.path.join('test', 'debugger',
                                            'debugger_unittests.py')

    # Starting from e.g. C:\b\slave\chrome-release\build, find
    # C:\b\scripts\slave.
    self._script_dir = os.path.join('..', '..', '..', 'scripts', 'slave')

    self._kill_tool = os.path.join(self._script_dir, 'kill_processes.py')
    self._differential_installer_tool = os.path.join(
        self._script_dir,
        'differential_installer.py')
    self._crash_handler_tool = os.path.join(self._script_dir,
                                            'run_crash_handler.py')
    self._compile_tool = os.path.join(self._script_dir, 'compile.py')
    self._archive_tool = os.path.join(self._script_dir, 'archive_build.py')
    self._test_tool = os.path.join(self._script_dir, 'runtest.py')
    self._crash_dump_tool = os.path.join(self._script_dir,
                                         'archive_crash_dumps.py')
    self._layout_test_tool = os.path.join(self._script_dir,
                                          'layout_test_wrapper.py')
    self._layout_archive_tool = os.path.join(self._script_dir,
                                             'archive_layout_test_results.py')
    self._node_leak_test_tool = os.path.join(self._script_dir,
                                             'node_leak_test_wrapper.py')
    self._qemu_tool = os.path.join(self._script_dir, '..', 'private',
                                   'query_qemu_results.py')
    self._playback_tool = os.path.join(self._script_dir, 'playback_tests.py')
    self._extract_tool = os.path.join(self._script_dir, 'extract_build.py')

    # chrome_staging directory, relative to the build directory.
    self._staging_dir = os.path.join('..', 'chrome_staging')

    # The _update_scripts_command will be run in the _update_scripts_dir to
    # udpate the slave's own script checkout.
    self._update_scripts_dir = '..'
    self._update_scripts_command = [chromium_utils.GetGClientCommand(),
                                    'sync', '--verbose']

  #######
  # Generic commands

  def GetTestCommand(self, executable, arg_list=None):
    """Returns a command list to call the _test_tool on the given executable,
    passing the arg_list, if any, to that executable under test.
    """
    cmd = [self._python, self._test_tool,
           '--target', self._target,
           '--build-dir', self._build_dir,
           executable]
    if arg_list is not None:
      cmd.extend(arg_list)
    return cmd


  def AddTestStep(self, command_class, test_name, timeout, test_command,
                  test_description='', workdir=None, env=None):
    """Adds a step to the factory to run a test.

    Args:
      command_class: the command type to run, such as shell.ShellCommand or
          gtest_command.GTestCommand
      test_name: a string describing the test, used to build its logfile name
          and its descriptions in the waterfall display
      timeout: the buildbot timeout for the test, in seconds.  If it doesn't
          produce any output to stdout or stderr for this many seconds,
          buildbot will cancel it and call it a failure.
      test_command: the command list to run
      test_description: an auxiliary description to be appended to the
        test_name in the buildbot display; for example, ' (single process)'
      workdir: directory where the test executable will be launched. If None,
        step will use default directory.
      env: dictionary with environmental variable key value pairs that will be
        set or overridden before launching the test executable. Does not do
        anything if 'env' is None.
    """
    self._factory.addStep(
        command_class,
        name=test_name,
        timeout=timeout,
        workdir=workdir,
        env=env,
        description='running %s%s' % (test_name, test_description),
        descriptionDone='%s%s' % (test_name, test_description),
        command=test_command)

  def AddBasicGTestTestStep(self, test_name, timeout=300, arg_list=None):
    """Adds a step to the factory to run a simple GTest test with standard
    defaults.  The executable name must be test_name plus '.exe' on
    Windows, or else just the test name.
    """
    if self._target_platform == 'win32':
      test_name = test_name + '.exe'

    self.AddTestStep(gtest_command.GTestCommand, test_name, timeout,
                     test_command=self.GetTestCommand(test_name,
                                                      arg_list=arg_list))

  def AddSvnKillStep(self):
    """Adds a step to the factory to kill svn.exe."""
    self._factory.addStep(shell.ShellCommand, description='svnkill',
                          timeout=60,
                          workdir='',  # The build subdir may not exist yet.
                          command=[r'%WINDIR%\system32\taskkill.exe',
                                   '/f', '/im', 'svn.exe',
                                   '||', 'set', 'ERRORLEVEL=0'])

  def AddUpdateScriptStep(self):
    """Adds a step to the factory to update the script folder."""
    self._factory.addStep(shell.ShellCommand,
                          description='update scripts',
                          timeout=60,
                          workdir=self._update_scripts_dir,
                          command=self._update_scripts_command)

  def AddUpdateStep(self, gclient_spec):
    """Adds a step to the factory to update the workspace."""
    self._factory.addStep(chromium_step.GClient,
                          gclient_spec=gclient_spec,
                          workdir=self._working_dir,
                          mode='update',
                          retry=(60*5, 4),  # Try 4+1=5 more times, 5 min apart
                          timeout=60*5,     # svn timeout is 2 min; we allow 5
                          rm_timeout=60*15) # The step can take a long time.

  def AddTaskkillStep(self):
    """Adds a step to kill the running processes before a build"""
    self._factory.addStep(shell.ShellCommand, description='taskkill',
                          timeout=60,
                          workdir='',  # Doesn't really matter where we are.
                          command=[self._taskkill_python, self._kill_tool])

  #######
  # Archive, Extract, Upload and Download commands.

  def GetArchiveBuildCommand(self, mode):
    """Returns a command list to call the _archive_tool to archive the build
    in the given archive mode ('dev' or 'official').
    """
    return [self._python, self._archive_tool,
            '--target', self._target,
            '--build-dir', self._build_dir,
            '--mode', mode]

  def AddArchiveStep(self, data_description, timeout, base_url, link_text,
                     command):
    """Adds a step to the factory f to archive a build or other data.

    Args:
      data_description: a string describing the object being archived, used to
          build the step name (for its log file) and its descriptions in the
          waterfall display.  For example, 'build' or 'layout test results'.
      timeout: the buildbot timeout for the step, in seconds.  If it doesn't
          produce any output to stdout or stderr for this many seconds,
          buildbot will cancel it and call it a failure.
      base_url: the URL at which to archive the data, passed to the
          ArchiveCommand
      link_text: a string to use in the waterfall display as the link text
          pointing to the archived data
      command: the command list to run
    """
    step_name = 'archive_%s' % data_description
    step_name.replace(' ', '_')
    self._factory.addStep(archive_command.ArchiveCommand,
                          name=step_name,
                          timeout=timeout,
                          description='archiving %s' % data_description,
                          descriptionDone='archived %s' % data_description,
                          base_url=base_url,
                          link_text=link_text,
                          command=command)

  def AddArchiveBuild(self, mode='dev', show_url=True):
    """Adds a step to the factory to archive a build."""
    if show_url:
      url = '%s/%s/%s' %  (self._archive_url, 'snapshots', self._identifier)
      text = 'download'
    else:
      url = None
      text = None

    self.AddArchiveStep(data_description='build',
                        timeout=600,
                        base_url=url,
                        link_text=text,
                        command=self.GetArchiveBuildCommand(mode=mode))

  def GetExtractCommand(self):
    """Returns the command to call the extract tool."""
    return [self._python, self._extract_tool,
            '--build-dir', self._build_dir,
            '--target', self._target]

  def AddExtractBuild(self, timeout=240):
    """Adds a step to the factory to extract a build."""
    self.AddTestStep(shell.ShellCommand,
                     timeout=timeout,
                     test_name='extract build',
                     test_command=self.GetExtractCommand())

  def GetMasterZip(self):
    """Returns the relative path to the build output zip file on the master,
    relative to the master.xxx base directory.
    """
    zip_name = '%s.zip' % self._identifier
    return os.path.join('build_output', zip_name)

  def AddUploadBuild(self):
    """Adds a step to upload the target folder.
    """
    zip_src = os.path.join(self._staging_dir, 'full-build-win32.zip')
    zip_dest = self.GetMasterZip()
    self._factory.addStep(transfer.FileUpload,
                          name='Upload Build',
                          blocksize=640*1024,
                          slavesrc=zip_src,
                          masterdest=zip_dest)

  def AddDownloadBuild(self):
    """Adds a step to download the target folder.
    """
    zip_src = self.GetMasterZip()
    zip_dest = 'full-build-win32.zip'
    self._factory.addStep(transfer.FileDownload,
                          haltOnFailure=True,
                          blocksize=640*1024,
                          name='Download Build',
                          mastersrc=zip_src,
                          slavedest=zip_dest)

  #######
  # Build steps

  def GetBuildCommand(self, clobber, solution, mode, options=None):
    """Returns a command list to call the _compile_tool in the given build_dir,
    optionally clobbering the build (that is, deleting the build directory)
    first.
    """
    cmd = [self._python, self._compile_tool]
    cmd.extend(['--solution', solution,
                '--target', self._target,
                '--build-dir', self._build_dir])
    if mode is not None:
      cmd.extend(['--mode', mode])
    if clobber:
      cmd.append('--clobber')
    if options:
      cmd.extend(options)
    return cmd

  def GetCheckDepsCommand(self):
    """Returns a command list to check dependencies."""
    return [self._python, self._check_deps_tool,
            '--root', self._repository_root]

  def AddCompileStep(self, solution, clobber=False, description='compiling',
                     descriptionDone='compile', timeout=600, mode=None,
                     options=None):
    """Adds a step to the factory to compile the solution.

    Args:
      solution: the solution file to build
      clobber: if True, clobber the build (that is, delete the build
          directory) before building
      description: for the waterfall
      descriptionDone: for the waterfall
      timeout: if no output is received in this many seconds, the compile step
          will be killed
      mode: if given, this will be passed as the --mode option to the compile
          command
      options: list of additional options to pass to the compile command
    """
    self._factory.addStep(cl_command.CLCommand,
                          enable_warnings=0,
                          timeout=timeout,
                          description=description,
                          descriptionDone=descriptionDone,
                          command=self.GetBuildCommand(clobber,
                                                       solution,
                                                       mode,
                                                       options))

  def AddCheckDepsStep(self, timeout=300):
    """Adds a step to the factory to validate inter-project dependencies."""
    self._factory.addStep(shell.ShellCommand,
                          timeout=timeout,
                          description='checking deps',
                          descriptionDone='check deps',
                          command=self.GetCheckDepsCommand())

  #######
  # Performance steps.

  def _CreatePerformanceStepClass(
      self, log_processor_class, report_link=None, output_dir=None,
      command_class=chromium_step.ProcessLogShellStep):
    """Returns ProcessLogShellStep class.

    Args:
      log_processor_class: class that will be used to process logs. Normally
        should be a subclass of process_log.PerformanceLogProcessor.
      report_link: URL that will be used as a link to results. If None,
        result won't be written into file.
      output_dir: directory where the log processor will write the results.
      command_class: command type to run for this step. Normally this will be
        chromium_step.ProcessLogShellStep.
    """
    # We create a log-processor class using
    # chromium_utils.InitializePartiallyWithArguments, which uses function
    # currying to create classes that have preset constructor arguments.
    # This serves two purposes:
    # 1. Allows the step to instantiate its log processor without any
    #    additional parameters;
    # 2. Creates a unique log processor class for each individual step, so
    # they can keep state that won't be shared between builds
    log_processor_class = chromium_utils.InitializePartiallyWithArguments(
        log_processor_class, report_link=report_link, output_dir=output_dir)
    # Similarly, we need to allow buildbot to create the step itself without
    # using additional parameters, so we create a step class that already
    # knows which log_processor to use.
    return chromium_utils.InitializePartiallyWithArguments(command_class,
                                                         log_processor_class)

  def GetPerfTestCommand(self, perf_id, show_results, mappings,
                         log_processor_class, **kwargs):
    """Selects the right build step for the specified perf test."""
    report_link = None
    output_dir = None
    if show_results and self._target in mappings:
      mapping = mappings[self._target]
      dir_name = mapping.get(perf_id)
      if dir_name:
        report_link = "%s/%s/%s" % (self.PERF_BASE_URL, dir_name,
                                    self.PERF_REPORT_URL_SUFFIX)
        output_dir = '%s/%s/' % (self.PERF_OUTPUT_DIR, dir_name)
      else:
        raise Exception, ('There is no mapping for identifier %s in %s' %
                            (perf_id, self._target))

    return self._CreatePerformanceStepClass(
        log_processor_class, report_link=report_link, output_dir=output_dir)

  def GetPageCyclerTestCommand(self, perf_id, test_name, show_results,
                               **kwargs):
    """Selects the right build step for the specified page-cycler test.

    Args:
      perf_id: full identifier for this build.  Typically the builder name
          (e.g., 'chrome-release').
      test_name: name of the individual test type, such as 'moz' or 'intl1'
      show_results: whether to display a link to the results graph on the
        waterfall page
      kwargs: additional keyword arguments that may be used by subclasses
    """
    report_link = None
    output_dir = None
    if show_results and self._target in self.PAGECYCLER_TEST_MAPPINGS:
      mapping = self.PAGECYCLER_TEST_MAPPINGS[self._target]
      dir_name = mapping.get(perf_id)
      if dir_name:
        if kwargs.get('http', None):
          test_name += '-http'
        report_link = (self.PERF_BASE_URL +
                       '/%s/%s/' % (dir_name, test_name) +
                       self.PERF_REPORT_URL_SUFFIX)
        output_dir = '%s/%s/%s' % (self.PERF_OUTPUT_DIR, dir_name, test_name)
      else:
        raise Exception, ('There is no mapping for identifier %s in %s' %
                            (perf_id, self._target))

    return self._CreatePerformanceStepClass(
        process_log.GraphingPageCyclerLogProcessor,
        report_link=report_link,
        output_dir=output_dir)

  def GetStartupTestCommand(self, perf_id, show_results, **kwargs):
    """Selects the right build step for the specified startup test."""
    return self.GetPerfTestCommand(perf_id, show_results,
                                   self.STARTUP_TEST_MAPPINGS,
                                   process_log.GraphingLogProcessor)

  def GetMemoryTestCommand(self, perf_id, show_results, **kwargs):
    """Selects the right build step for the specified memory test."""
    return self.GetPerfTestCommand(perf_id, show_results,
                                   self.MEMORY_TEST_MAPPINGS,
                                   process_log.GraphingLogProcessor)

  def GetNewTabUITestCommand(self, perf_id, show_results,
                             mappings, **kwargs):
    """Selects the right build step for the specified new tab UI test.
    """
    return self.GetPerfTestCommand(perf_id, show_results, mappings,
                                   process_log.GraphingLogProcessor)

  def GetTabSwitchingTestCommand(self, perf_id, show_results,
                                 **kwargs):
    """Selects the right build step for the specified tab switching test.
    """
    return self.GetPerfTestCommand(perf_id, show_results,
                                   self.TAB_SWITCHING_TEST_MAPPINGS,
                                   process_log.GraphingLogProcessor)

  def GetPageCyclerCommand(self, test_name, http):
    """Returns a command list to call the _test_tool on the page_cycler
    executable, with the appropriate GTest filter and additional arguments.
    """
    cmd = [self._python, self._test_tool,
           '--target', self._target,
           '--build-dir', self._build_dir]
    if http:
      test_type = 'Http'
      cmd.extend(['--with-httpd', os.path.join('src', 'data', 'page_cycler')])
    else:
      test_type = 'File'
    cmd.extend(['page_cycler_tests.exe',
                '--gtest_filter=PageCycler*.%s%s' % (test_name, test_type)])
    return cmd

  def AddPageCyclerTests(self, show_results, perf_id=None, timeout=600,
                         http=False):
    """Adds a step to the factory to run the page-cycler tests.

    Args:
      http: If True, launch an httpd server and run the Httpd tests. Otherwise,
          run the File tests.  (These names are used in the GTest filters and
          correspond to test names in page_cycler_tests.exe.)

    The meaning of the other arguments depends on the implementation of a
    subclass of this class; by default they have no effect.
    """
    c_moz = self.GetPageCyclerTestCommand(perf_id, 'moz', show_results,
                                          http=http)
    c_intl1 = self.GetPageCyclerTestCommand(perf_id, 'intl1', show_results,
                                            http=http)
    c_intl2 = self.GetPageCyclerTestCommand(perf_id, 'intl2', show_results,
                                            http=http)
    c_bloat = self.GetPageCyclerTestCommand(perf_id, 'bloat', show_results,
                                            http=http)
    c_dhtml = self.GetPageCyclerTestCommand(perf_id, 'dhtml', show_results,
                                            http=http)

    if http:
      name_extension = '-http'
    else:
      name_extension = ''

    # moz data set
    self.AddTestStep(command_class=c_moz,
                     test_name='page_cycler_moz%s' % name_extension,
                     timeout=timeout,
                     test_command=self.GetPageCyclerCommand('Moz', http))

    # intl1/intl2 data sets (only run in file:// release mode for now)
    if self._target == 'Release' and not http:
      self.AddTestStep(command_class=c_intl1,
                       test_name='page_cycler_intl1%s' % name_extension,
                       timeout=timeout,
                       test_command=self.GetPageCyclerCommand('Intl1', http))

      self.AddTestStep(command_class=c_intl2,
                       test_name='page_cycler_intl2%s' % name_extension,
                       timeout=timeout,
                       test_command=self.GetPageCyclerCommand('Intl2', http))

    # bloat data set (only run in http release mode for now)
    if self._target == 'Release' and http:
      self.AddTestStep(command_class=c_bloat,
                       test_name='page_cycler_bloat%s' % name_extension,
                       timeout=timeout,
                       test_command=self.GetPageCyclerCommand('Bloat', http))

    # dhtml data set (only run in file:// release mode for now)
    if self._target == 'Release' and not http:
      self.AddTestStep(command_class=c_dhtml,
                       test_name='page_cycler_dhtml%s' % name_extension,
                       timeout=timeout,
                       test_command=self.GetPageCyclerCommand('Dhtml', http))

  def AddStartupTests(self, show_results, perf_id=None, timeout=300):
    """Adds a step to the factory to run the startup test."""
    c = self.GetStartupTestCommand(perf_id, show_results)
    # We don't want to run the Reference tests in debug mode
    # because we don't use the data and it seems to be
    # flaky.
    if self._target == 'Release':
      options = ['--gtest_filter=Startup*.*']
    else:
      options = ['--gtest_filter=StartupTest.*']
    self.AddTestStep(command_class=c,
                     test_name='startup_test',
                     timeout=timeout,
                     test_command=self.GetTestCommand('startup_tests.exe',
                                                      options))

  def AddMemoryTests(self, show_results, perf_id=None, timeout=300):
    """Adds a step to the factory to run the memory test."""
    c = self.GetMemoryTestCommand(perf_id, show_results)
    self.AddTestStep(command_class=c,
                     test_name='memory_test',
                     timeout=timeout,
                     test_command=self.GetTestCommand('memory_test.exe'))

  def AddNewTabUITests(self, show_results, perf_id=None, timeout=300):
    """Adds a step to the factory to run the new tab test."""
    c = self.GetNewTabUITestCommand(perf_id, show_results,
                                    self.NEW_TAB_UI_COLD_TEST_MAPPINGS)
    self.AddTestStep(
        command_class=c,
        timeout=timeout,
        test_name='new_tab_ui_cold_test',
        test_command=self.GetTestCommand(
            'startup_tests.exe',
            ['--gtest_filter=NewTabUIStartupTest.*Cold']))

    c = self.GetNewTabUITestCommand(perf_id, show_results,
                                    self.NEW_TAB_UI_WARM_TEST_MAPPINGS)
    self.AddTestStep(
        command_class=c,
        timeout=timeout,
        test_name='new_tab_ui_warm_test',
        test_command=self.GetTestCommand(
            'startup_tests.exe',
            ['--gtest_filter=NewTabUIStartupTest.*Warm']))

  def AddTabSwitchingTests(self, show_results, perf_id=None, timeout=300):
    """Adds a step to the factory to run the tab switching test."""
    c = self.GetTabSwitchingTestCommand(perf_id, show_results)
    options = ['--gtest_filter=TabSwitchingUITest.*', '-enable-logging',
               '-dump-histograms-on-exit']
    self.AddTestStep(command_class=c,
                     timeout=timeout,
                     test_name='tab_switching_test',
                     test_command=self.GetTestCommand('tab_switching_test.exe',
                                                      options))

  #######
  # Chrome tests

  def GetQemuCommand(self):
    """Returns a command list to call the _qemu_tool to report QEMU results."""
    return [self._python, self._qemu_tool,
            '--build-dir', self._build_dir,
            '--known-crash-list', 'known-crashes.txt']

  def AddQueryQemu(self, timeout=240):
    """Adds a step to the factory to query the QEMU distributed-testing
    results.
    """
    self.AddTestStep(shell.ShellCommand,
                     timeout=timeout,
                     test_name='distributed_reliability_tests',
                     test_command=self.GetQemuCommand())

  def AddDebuggerTestStep(self, timeout=60):
    """Adds a step to the factory to run the debugger test."""
    self.AddTestStep(shell.ShellCommand,
                     timeout=timeout,
                     test_name='debugger_test',
                     test_command=self.GetDebuggerTestCommand())

  def GetDebuggerTestCommand(self):
    """Returns a command list to call the _debugger_test_tool.
    """
    cmd = [self._python, os.path.join(self._build_dir,
                                      self._debugger_test_tool),
           '--build_dir', os.path.join(self._build_dir, self._target)]
    return cmd

  def AddChromeUnitTests(self):
    """Adds a step to the factory to run all the unit tests."""
    # When adding a test, update kill_processes.py.
    if self._target_platform == 'win32':
      self.AddBasicGTestTestStep('ipc_tests')
      self.AddBasicGTestTestStep('installer_unittests')
      if self._target == 'Release':
        self.AddBasicGTestTestStep('mini_installer_test')

    self.AddBasicGTestTestStep('unit_tests')

    if 'jsc' not in self._identifier and self._target_platform == 'win32':
      self.AddDebuggerTestStep()

  def AddUITests(self, with_pageheap, run_single_process=True):
    """Adds a step to the factory to run the UI tests in both regular and
    single-process modes.  If with_pageheap is True, page-heap checking will be
    enabled for chrome.exe.
    """
    pageheap_description = ''
    options = []
    if with_pageheap:
      pageheap_description = ' (--enable-pageheap)'
      options = ['--enable-pageheap']

    self.AddTestStep(
        gtest_command.GTestCommand,
        'ui_tests',
        timeout=300,
        test_description=pageheap_description,
        test_command=self.GetTestCommand('ui_tests.exe', options))

    if run_single_process:
      options = ['--single-process']
      self.AddTestStep(
          gtest_command.GTestCommand,
          'ui_tests_singleproc',
          timeout=300,
          test_description=' (--single-process)',
          test_command=self.GetTestCommand('ui_tests.exe', options))

  def AddInteractiveUITests(self):
    """Adds a step to the factory to run the interactive UI tests."""
    self.AddBasicGTestTestStep('interactive_ui_tests')

  def AddOmniboxTests(self):
    """Adds a step to the factory to run the Omnibox tests.
    """
    # This is not a UI unit test.  Rather, it is a long-running test that
    # scores Omnibox results.  It's part of ui_tests.exe because it uses much
    # of the UI test framework, so putting it there made more sense than
    # creating a new project.  This test needs a special command-line switch
    # in order to run, which prevents it from running by default.
    options = ['--gtest_filter=OmniboxTest.Measure',
               '--run_omnibox_test']
    self.AddTestStep(
        gtest_command.GTestCommand,
        'omnibox_tests',
        timeout=300,
        test_command=self.GetTestCommand('ui_tests.exe', options))

  def AddPluginTests(self):
    """Adds a step to the factory to run the plugin tests."""
    self.AddBasicGTestTestStep('plugin_tests')

  def AddBaseTests(self):
    """Adds a step to the factory to run the base tests.  Only works for
    Hammer builds because chrome.sln doesn't build base_unittests."""
    self.AddBasicGTestTestStep('base_unittests')

  def AddNetTests(self):
    """Adds a step to the factory to run the net tests.  Only works for
    Hammer builds because chrome.sln doesn't build net_unittests."""
    self.AddBasicGTestTestStep('net_unittests')

  def AddGoogleURLTests(self):
    """Adds a step to the factory to run the googleurl tests."""
    self.AddBasicGTestTestStep('googleurl_unittests')

  def AddSeleniumTests(self, timeout=300):
    """Adds a step to the factory to run the Selenium tests."""
    self.AddTestStep(
        shell.ShellCommand,
        timeout=timeout,
        test_name='selenium_tests',
        test_command=self.GetTestCommand('selenium_tests.exe'))

  #######
  # Purify Tests

  def GetPurifyCommand(self, test_name):
    """Returns a command list to call the _purify_tool on the given executable,
    passing the arg_list, if any, to that executable.
    """
    cmd = [self._python, self._purify_tool,
           '--build_dir', os.path.join(self._build_dir, self._target),
           '--test', test_name]
    return cmd

  def AddPurifyTest(self, test_name, timeout=1200):
    """Adds a step to the factory to run the Purify tests."""
    full_test_name = 'purify test: %s' % test_name
    self.AddTestStep(retcode_command.ReturnCodeCommand,
                     timeout=timeout,
                     test_name=full_test_name,
                     test_command=self.GetPurifyCommand(test_name))

  #######
  # Webkit tests

  def GetPlaybackCommand(self, cache_file):
    """Returns a command list to call the _playback_tool for the given cache
    file.
    """
    cmd = [self._python, self._playback_tool,
           '--target', self._target,
           '--build-dir', self._build_dir,
           '--cache-file', cache_file]
    return cmd

  def AddPlaybackTest(self, cache_name, cache_file, timeout=300):
    """Adds a step to factory to run the playback tests."""
    report_link = self.PERF_BASE_URL + ('/playback/%s/report.html'
                                        % cache_name)
    output_dir = '%s/playback/%s/' % (self.PERF_OUTPUT_DIR, cache_name)

    playback_step_class = self._CreatePerformanceStepClass(
        process_log.PlaybackLogProcessor,
        report_link=report_link,
        output_dir=output_dir)

    # Adds a step to the factory to run the playback tests.
    self.AddTestStep(playback_step_class, timeout=timeout,
                     test_name='playback_test',
                     test_description=' %s' % cache_name,
                     test_command=self.GetPlaybackCommand(cache_file))

  def GetWebkitResultDir(self):
    """Returns the path to the layout-test result dir, relative to the
    build_dir passed to the layout-test script (for example, relative to
    'build/webkit/'if the build_dir is 'webkit').
    """
    # Starting from webkit-release/build/webkit/, save the results in
    # webkit-release/layout-test-results/.  Use forward slashes to avoid
    # issues with escaping; the Python scripts handle them fine.
    return '/'.join(['..', '..', 'layout-test-results'])

  def GetWebkitTestCommand(self, with_pageheap):
    """Returns a command list to call the _layout_test_tool to run the WebKit
    layout tests.  If with_pageheap is True, page-heap checking will be enabled
    for test_shell.exe.

    The presence of 'jsc' anywhere in the identifier is used to determine when
    to tell the script it's testing a JSC build.
    """
    if 'jsc' in self._identifier:
      build_type_id = 'kjs'
    else:
      build_type_id = 'v8'
    cmd = [self._python, self._layout_test_tool,
           '--target', self._target,
           '--build-type', build_type_id,
           '-o', self.GetWebkitResultDir(),
           '--build-dir', self._build_dir,
           '--pixel-tests']
    if with_pageheap:
      cmd.append('--enable-pageheap')
    return cmd

  def GetNodeLeakTestCommand(self, cache_file, url_list):
    """Returns a command list to call the _node_leak_test_tool to run the node
    leak tests.
    """
    cmd = [self._python, self._node_leak_test_tool,
           '--target', self._target,
           '--build-dir', self._build_dir,
           '--cache-file', cache_file,
           '--url-list', url_list]
    return cmd

  def AddNodeLeakTest(self, cache_file, url_list, timeout=480):
    """Adds a step to run the node leak tests.
    """
    self.AddTestStep(retcode_command.ReturnCodeCommand,
                     timeout=timeout,
                     test_name='node_leak_tests',
                     test_command=self.GetNodeLeakTestCommand(cache_file,
                                                              url_list))

  def GetWebkitArchiveCommand(self):
    """Returns a command list to call the _layout_archive_tool to archive
    WebKit layout test results.
    """
    return [self._python, self._layout_archive_tool,
            '--results-dir', self.GetWebkitResultDir(),
            '--build-dir', self._build_dir]

  def AddWebkitTests(self, with_pageheap, test_timeout=600,
                     archive_timeout=600, archive_results=False):
    """Adds a step to the factory to run the WebKit layout tests.

    Args:
      with_pageheap: if True, page-heap checking will be enabled for
          test_shell.exe
      test_timeout: buildbot timeout for the test step
      archive_timeout: buildbot timeout for archiving the test results and
          crashes, if requested
      archive_results: whether to archive the test results
      archive_crashes: whether to archive crash reports resulting from the
          tests
    """
    pageheap_description = ''
    if with_pageheap:
      pageheap_description = ' (--enable-pageheap)'

    self.AddTestStep(webkit_test_command.WebKitCommand,
                     test_name='webkit_tests',
                     timeout=test_timeout,
                     test_description=pageheap_description,
                     test_command=self.GetWebkitTestCommand(with_pageheap))

    if archive_results:
      url = '%s/%s/%s' % (self._archive_url, 'layout_test_results',
                          self._identifier)
      self.AddArchiveStep(
          data_description='webkit_tests results',
          timeout=archive_timeout,
          base_url=url,
          link_text='layout test results',
          command=self.GetWebkitArchiveCommand())

  def AddTestShellTests(self):
    """Adds a step to the factory to run the test_shell_tests."""
    self.AddBasicGTestTestStep('test_shell_tests')

  #######
  # Crash handler command

  def GetCrashHandlerCommand(self):
    """Returns the command to call the crash handler."""
    return [self._python, self._crash_handler_tool,
            '--build-dir', self._build_dir,
            '--target', self._target]

  def AddRunCrashHandler(self, timeout=240):
    """Adds a step to factory f to run the crash handler."""
    self.AddTestStep(shell.ShellCommand,
                     timeout=timeout,
                     test_name='Start Crash Handler',
                     test_command=self.GetCrashHandlerCommand())

  ######
  # Official build steps.

  def AddDifferentialBuildStep(self):
    """Adds a step to build the differential installer."""
    self._factory.addStep(shell.ShellCommand,
                          description='differential_installer_build',
                          timeout=1800,
                          command=[self._python,
                                   self._differential_installer_tool,
                                   '--target', self._target, '--build-dir',
                                   self._build_dir])

  def AddStageBuildStep(self):
    """Adds a step to stage the build"""
    self._factory.addStep(shell.ShellCommand,
                          description='stage_build',
                          timeout=1800,
                          command=[self._python, self._archive_tool,
                                   '--target', self._target,
                                   '--mode', 'official',
                                   '--build-dir', self._build_dir])
