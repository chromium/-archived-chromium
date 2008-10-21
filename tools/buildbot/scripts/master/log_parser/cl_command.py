#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A buildbot command for running the visual c++ compiler and interpreting the
output.

"""

import re
from twisted.web import util
from buildbot.steps import shell
from buildbot.status import builder
from buildbot.process import buildstep

# One of these two strings, used to identify the build system, must appear in
# the build output before any compiler output from individual projects.
VS_NAME = 'Visual Studio Version 8'
IB_NAME = 'IncrediBuild Console 3'


class IbOutputParser:
  """ Class that parses IB Log and outputs HTML

  If VS log is fed, immidatedly converts it to IB log
  It creates HTML document that has links that can navigate directly to
  errors
  """
  HEADER = """
    <html>
    <head>
    <style type="text/css">

      body {
        font-family: "Courier New", courier, monotype;
      }

      span.stdout {
        font-family: "Courier New", courier, monotype;
      }

      .error {
        font-family: "Courier New", courier, monotype;
        color: red;
      }

      #header {
        font-family: "Courier New", courier, monotype;
        color: red;
      }

    </style>
    </head>
    <body>
  """

  NAME_MATCHER = \
      re.compile(r'^--------------------Configuration: ([A-Za-z0-9_-]+) - ')

  ERROR_LINE_MATCHER = re.compile(r'(.*) error [C|L]') #Compiler or linker

  WARNING_LINE_MATCHER = re.compile(r'(.*) warning [C|L]')

  ERROR_MATCHER = re.compile(r'(.*) - (\d+) error\(s\)')

  VS_STEPS_MATCHER = re.compile(r'^(\d+)\>(.*)')

  LOG_TYPE_MATCHER = re.compile(
    '^(?P<IB>IncrediBuild Console)|(?P<DEVENV>Microsoft \(R\) Visual Studio)')

  def __init__(self, log_content):
    self.log_content = log_content
    self.__failed_projects = None
    self.__lines_with_warnings = None
    if not self._IsIbLog():
      self.log_content = DevenvLogToIbLog(self.log_content).Convert()

  def FoundWarnings(self):
    """Method that indicates whether log has any traces of warnings."""
    return len(self.WarningLines()) > 0

  def WarningLines(self):
    """Returns unique list of compiler and linker warning lines."""
    if self.__lines_with_warnings is None:
      warnings = set()
      for line in self.log_content.splitlines():
        match = IbOutputParser.WARNING_LINE_MATCHER.match(line)
        if match:
          warnings.add(line)
      self.__lines_with_warnings = list(warnings)

    return self.__lines_with_warnings

  def FoundErrors(self):
    """ Method that indicates whether log has any traces of errors."""
    return len(self.FailedProjects()) > 0

  def ErrorLogHtml(self):
    """ Converts text IB compilation log and creates HTML log with
    nice error links."""
    content = []
    self._ResetState()
    for line in self.log_content.splitlines():
      content.append('%s\n' % self._ProcessLine(line))
    return self._HtmlHeader() + ''.join(content) + '</body></html>'

  def FailedProjects(self):
    """ Returns a hash with project names that appeared as failed in the log
    with number of errors for that project.
    """
    if self.__failed_projects is None:
      self.__failed_projects = {}
      for line in self.log_content.splitlines():
        match = IbOutputParser.ERROR_MATCHER.match(line)
        if match and int(match.group(2)) > 0:
          self.__failed_projects[match.group(1)] = int(match.group(2))

    return self.__failed_projects

  def _IsIbLog(self):
    for line in self.log_content.splitlines():
      match = IbOutputParser.LOG_TYPE_MATCHER.match(line)
      if match:
        return (match.groupdict()['IB'] != None)
    # assuming by default an IB log instead of failing
    return True

  def _ResetState(self):
    self.project_errors = {} # {projectname : [error lines]}
    self.project_name_being_processed = None

  def _ProcessLine(self, line):
    self._UpdateProjectName(line)
    content = self._MakeHtmlFriendly(line)
    content = self._ApplyPossibleErrorHtmlAttributes(content)
    return content

  def _ApplyPossibleErrorHtmlAttributes(self, content):
    if IbOutputParser.ERROR_LINE_MATCHER.match(content):
      self._AddToProjectErrors(content)
      return ("<div class='error'><a name='%s_%s'>%s</a></div>" %
                (self.project_name_being_processed,
                 len(self.project_errors[self.project_name_being_processed]),
                 content))
    return content

  def _AddToProjectErrors(self, content):
    if not self.project_errors.has_key(self.project_name_being_processed):
      self.project_errors[self.project_name_being_processed] = []
    self.project_errors[self.project_name_being_processed].append(content)

  def _MakeHtmlFriendly(self, line):
    return '%s<br />' %util.htmlIndent(line)

  def _UpdateProjectName(self, line):
    match = IbOutputParser.NAME_MATCHER.match(line)
    if match:
      self.project_name_being_processed =  match.group(1)

  def _HtmlHeader(self):
    html = [IbOutputParser.HEADER, '<div id="header">']
    for projectname, errors in self.project_errors.iteritems():
      html.extend([projectname,' errors:<br />'])
      index = 0
      for error in errors:
        index += 1
        error_listing = ('%s: <a href="#%s_%s">%s</a></br /></br /></br />' %
                         (index, projectname, index, error))
        html.append(error_listing)
    html.append('</div>')
    return ''.join(html)


class DevenvLogToIbLog:
  """ Converts Visual Studio logs to look like IB logs

  The logs of VS is quite complicated and instead of creating custom
  parser we convert it to IB log and feed to IB log if necessary.
  """
  PROJECT_HEADER_REGEXP = \
        re.compile(r'------ .+: Project: (.+), Configuration: (.+) ------')

  VS_STEPS_MATCHER = re.compile(r'^(\d+)\>(.*)')

  def __init__(self, content):
    self._content = content
    self.__project_outputs = {}

  def Convert(self):
    """ Does the actual conversion.

    Visual Studio output lines with multiple projects look like:
    HEADER INFO
    PROJECT_ID> log line
    FOOTER INFO
    Individual project outputs are sequential however different project
    outputs are not as they can be processed in parallel. The method goes
    through the log and collects output lines per project and once it is
    done reading, it groups output per project and formats them in IB format.
   """
    header = []
    footer = []
    for line in self._content.splitlines():
      match = DevenvLogToIbLog.VS_STEPS_MATCHER.match(line)
      if match:
        self._ProcessProjectOutputLine(match)
      else:
        if not self.__DoneWithHeader():
          header.extend([line, '\n'])
        else:
          footer.extend([line, '\n'])

    all_projects_output = self._JoinProjectOutputs()
    return ''.join(header + all_projects_output + footer)

  def _ProcessProjectOutputLine(self, match):
    """ Processes project output lines"""

    project_id = int(match.group(1))
    if not project_id in self.__project_outputs:
      self.__project_outputs[project_id] = []
    self.__project_outputs[project_id].append(match.group(2))
    self.__project_outputs[project_id].append('\n')

  def _JoinProjectOutputs(self):
    """Groups log lines per project and makes them look like IB log lines"""

    all_projects_output = []
    for project_id, output in self.__project_outputs.iteritems():
      if len(output) > 0:
        match = DevenvLogToIbLog.PROJECT_HEADER_REGEXP.match(output[0])
        output[0] = (
            '--------------------Configuration: %s - %s-----------------------'
              %(match.group(1), match.group(2))
            )
      all_projects_output.extend(output)
    return all_projects_output

  def __DoneWithHeader(self):
    return len(self.__project_outputs) !=  0


class CLCommand(shell.Compile):
  """Buildbot command that knows how to display the CL compiler output."""

  def __init__(self, enable_warnings, **kwargs):
    shell.ShellCommand.__init__(self, **kwargs)

  def createSummary(self, log):
    self.__AddLogs()

  def __AddLogs(self):
    try:
      ib_output_parser = IbOutputParser(self.getLog('stdio').getText())
      self.__AddErrorHtmlLog(ib_output_parser)
      self.__AddFailedProjectNames(ib_output_parser)
      self.__AddWarningLog(ib_output_parser)
    except:
      # Regex groups can possibly fail
      pass

  def  __AddFailedProjectNames(self, ib_output_parser):
    failed_projects = ib_output_parser.FailedProjects()
    for projectname in failed_projects:
      self.addCompleteLog(
          '%s - %d error(s)' % (projectname, failed_projects[projectname]), '')

  def __AddErrorHtmlLog(self, ib_output_parser):
    if ib_output_parser.FoundErrors():
      self.addHTMLLog("stdio_html", ib_output_parser.ErrorLogHtml())

  def __AddWarningLog(self, ib_output_parser):
    if ib_output_parser.FoundWarnings():
      self.addCompleteLog(
          'warnings', '\n'.join(ib_output_parser.WarningLines()))
