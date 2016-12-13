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


"""Test harness script that invokes O3D system-tests.

This script constructs the necessary testing environment required for the
DirectX-based O3D system-tests.  It invokes each test within the PIX
debugging tool to capture the graphics command stream and framebuffer
contents.

See (http://wiki.corp.google.com/twiki/bin/view/Main/ClientThreeDTestingPlan)
for the o3d testing plan.

Usage:
  test_driver.py [-capture_only] [-hardware]

The script will scan all of the directories, and invoke system-tests for all
<name>_test directories present.  Each of the system-tests in system_tests.exe
will be invoked one at a time through PIXWin.exe.  PIX has been configured
to interact with the testing framework to capture graphics command streams
and framebuffer contents.

After the test has executed, the script will parse the persisted log files
and compare the generated results.

The framework assumes that each test-case is named with a camel-caps
translation of the directory name.  So in the case of the directory
tests\import_test, the system invokes the gUnit test ImportTest.

Command Arguments:
 -capture_only:  The script will only capture results.  All verification with
  reference-data will be bypassed. This flag should be used when updating the
  reference-data.

 - hardware:  The the system tests will execute using the graphics hardware, as
  opposed to the software reference rasterizer.  Use this mode to look for
  problems related to a specific hardware platform.
"""


import os
import re
from stat import ST_MODE
from stat import S_ISDIR
import sys

_PIX_EXECUTABLE = '.\PIXWin.exe'

# Template for the command line required to invoke a system-test within PIX.
_PIX_EXPERIMENT_INVOKE = ('%s  %s -start -runfile %s -targetstartfolder %s '
                          '-targetargs %s')

# Template for the command line required to invoke PIX when constructing a
# .csv file from a .PIXRun file.
_PIX_RUNFILE_INVOKE = '%s %s -exporttocsv %s'

# Template command line argument for the gunit system that selectively
# executes individual groups of tests.
_GUNIT_INVOKE = '--gunit_filter=%s.*'
_SYSTEM_TEST = '.\system_test.exe'
_EXPERIMENT_FILE_REFERENCE = 'testing_framework_reference.PIXExp'
_EXPERIMENT_FILE_HARDWARE = 'testing_framework_hardware.PIXExp'

# Command line required to invoke the perceptual diff utility.
_PDIFF = ('perceptualdiff.exe %s %s -verbose -fov 45 -threshold 40000')

# Regular expression used to search for hexadecimal pointer values in 
# generated log files
_HEX_EXPRESSION = re.compile('0x[0-9ABCDEF]+')

# Constant command argument string to enable capture-only mode.
_CAPTURE_FLAG = '-capture_only'

# Command flags to specify usage of the hardware rasterizer for the tests.
_HARDWARE_RASTERIZER = '-hardware'

# Boolen set to True if test results should be captured and not validated.
_GENERATE_ONLY = False

# Boolean set to True if hardware rasterizer is to be used instead of the DX
# reference rasterizer.
_HARDWARE_DEVICE = False


def ConstructTestName(filesystem_name):
  """Returns a camel-caps translation of the input string.

  Args:
    filesystem_name: The name of the test, as found on the file-system.
    Typically this name is_of_this_form.

  Returns:
    A camel-caps version of the input string.  _ delimiters are removed, and
    the first letters of words are capitalized.
  """
  names = filesystem_name.split('_')
  test_name = ''
  for word in names:
    test_name += word.upper()[0] + word[1:]
  return test_name


def AssertCapturedFrames(test_name):
  """Performs the image validation for test-case frame captures.

  Args:
    test_name: The name of the test, as found on the file-system.

  Returns:
    True if all images match within tolerance, false otherwise.
  """
  print 'Status:  Validating captured frames against reference data.'
  
  reference_files = os.listdir('./' + test_name + '/reference_frames')
  generated_files = os.listdir('./' + test_name)
  generated_files = [(file_name) for file_name in generated_files 
                     if '.png' in file_name]

  if set(reference_files) != set(generated_files):
    print 'Error:  Reference file set does not match generated file set.'
    print 'Reference files: ' + ', '.join(reference_files)
    print 'Generated files: ' + ', '.join(generated_files)
    return False

  # Assuming both the result and reference image sets are the same size,
  # verify that corresponding images are similar within tolerance.
  for file in reference_files:
    reference_file = test_name + '/reference_frames/' + file
    generated_file = test_name + '/' + file
    if os.system(_PDIFF % (reference_file, generated_file)):
      error = ('Error:  Reference framebuffer (%s) does not match generated '
               'file (%s).')
      print error % (reference_file, generated_file)
      return False

  return True


def GenerateCSVStream(test_name):
  """Invokes PIX to convert a PIXRun file to a .csv file.

  Args:
    test_name:  The name of the test for which the .csv file is
                to be generated.
  """

  runfile_invoke = _PIX_RUNFILE_INVOKE % (
      _PIX_EXECUTABLE,
      test_name + '/' + test_name + '.PIXRun',
      test_name + '/' + test_name + '.csv')

  os.system(runfile_invoke)


def RemovePointerReferences(call_log):
  """Returns the input argument with all pointer values removed.

  Args:
    call_log: String containing an dx call-log entry.

  Returns:
    The input string with all substrings matching '0x[0-9A-F]+', which is the
    pointer syntax used for the log files, removed.
  """

  return _HEX_EXPRESSION.sub('', call_log)


def PartitionCSVLine(line):
  """Returns a list of all column values in a comma-separated-value line.

  Args:
    line: The input line from a comma-separated-value source.

  Returns:
    CSV files may include quoted text segments containing commas.  This
    routine will return an array of all of the columns present in the line,
     respecting quoted regions.
  """

  # This routine is more complicated than expected due to the presence of
  # quoted lists within colums.  The algorithm proceeds by first splitting
  # the line by quoted sub-strings.  Sub-strings not matching a quoted
  # expression are then split on ','.
  quoted_column = re.compile('(\"[^\"]*\")')
  block_partitions = quoted_column.split(line)

  output_columns = []

  for chunk in block_partitions:
    # If this chunk is a quoted string, append it to the output verbatim.
    if quoted_column.match(chunk):
      output_columns += [chunk]
    else:
      output_columns += chunk.split(',')
  
  # Return the output set of columns with all empty entries removed.
  # Empty columns will be present due to the interaction of the split by
  # quoted string, and split by ','.
  # For example:  here, is, "a (tuple, example)", to, try
  # After the first split: ['here, is, ', 'a (tuple, example)', 'to, try']
  # After the second split:
  #  ['here', 'is', '', 'a (tuple, example)', 'to', 'try']
  # Since these empty columns are not present in the original list, we remove
  # them here.
  return [(column) for column in output_columns if column != ""]


def AssertCapturedStreams(test_name):
  """Performes graphics command stream verification for test with name
     test_name.

  Args:
    test_name: The name of the test, as found on the file-system.

  Returns:
    Returns true if the generated testing streams match the reference streams.
  """

  print 'Status: Validating generated command streams.'

  generated_stream = open(test_name + '/' + test_name + '.csv')
  reference_stream = open(test_name + '/reference_stream.csv')

  reference_lines = reference_stream.readlines()
  generated_lines = generated_stream.readlines()

  if len(reference_lines) != len(generated_lines):
    print 'Error:  Reference and generated logs differ in length.'
    return False

  #  Compare each of the log lines from both files.
  for index in range(0, len(reference_lines)):
    generated_line = generated_lines[index]
    reference_line = reference_lines[index]

    # Partition each csv line correctly wrt quoted blocks
    reference_columns = PartitionCSVLine(reference_line)
    generated_columns = PartitionCSVLine(generated_line)

    # Only perform deep-validation on 'Call' commands.
    if reference_columns[0] != 'Call':
      continue

    generated_log = RemovePointerReferences(generated_columns[2])
    reference_log = RemovePointerReferences(reference_columns[2])

    if (generated_log != reference_log or 
        reference_columns[0] != generated_columns[0] or
        reference_columns[1] != generated_columns[1]):
      print 'Error: Log file mis-match.  Line: %i' % index
      print 'Reference = %s' % reference_line
      print 'Generated = %s' % generated_line
      return False

  return True


def InvokeTest(test_name):
  """Invoke the system-test with name test_name.

  Args:
    test_name: The name of the test, as found on the file-system.

  Returns:
    True if the test succeeds, false otherwise.
  """

  global _GENERATE_ONLY

  if _HARDWARE_DEVICE:
    pix_experiment_file = _EXPERIMENT_FILE_HARDWARE
  else:
    pix_experiment_file = _EXPERIMENT_FILE_REFERENCE

  print 'Status: Executing test : %s\n' % test_name
  gunit_invoke = _GUNIT_INVOKE % ConstructTestName(test_name)
  pix_invoke = _PIX_EXPERIMENT_INVOKE % (_PIX_EXECUTABLE,
                                         pix_experiment_file,
                                         test_name + '.PIXRun',
                                         test_name,
                                         gunit_invoke)
  os.system(pix_invoke)

  # Invoke PIX to translate the just created .PIXRun file to a .csv suitable
  # for parsing.
  GenerateCSVStream(test_name)

  # If invoked for capture-only, then exit here before validation.
  if _GENERATE_ONLY:
    return True

  if not(AssertCapturedFrames(test_name)):
    return False

  if not(AssertCapturedStreams(test_name)):
    return False

  return True


def BuildTestList():
  """Returns a list of all available system tests.

  Returns:
    A list of test_names, constructed from the set of directory names
    in the current directory.  All directories containing the substring
    'test' are included in the returned set.
  """

  testing_directory = os.listdir('.')
  test_list = []

  for test in testing_directory:
    mode = os.stat(test)[ST_MODE]
    if ('test' in test and test != 'unittest_data' and
        test != 'bitmap_test' and
        test != 'conditioner_test_data' and S_ISDIR(mode)):
      test_list += [test]
  return test_list


def ValidateArgs(argv):
  """Validates the script arguments, and displays a help message, 
     if necessary.

  Args:
    argv: Array of script arguments, in argv format.

  Returns:
    True if the arguments are valid.  See the usage description for
    valid arguments.
  """

  global _GENERATE_ONLY
  global _HARDWARE_DEVICE

  # TODO : Make use of the gflags library to ease parsing of command line
  # arguments.
  argument_set = [_CAPTURE_FLAG, _HARDWARE_RASTERIZER]

  for arg in argv[1:]:
    if arg not in argument_set:
      print 'O3D System-Test Harness - Usage:'
      print ('test_driver.py [' + _CAPTURE_FLAG + '] [' +
             _HARDWARE_RASTERIZER + ']')
      print 'Arguments:'
      print _CAPTURE_FLAG + ' : Force generation of reference data.'
      print (_HARDWARE_RASTERIZER +
             ' : Force usage of hardware for test data generation')
      return False

  if _CAPTURE_FLAG in argv[1:]:
    _GENERATE_ONLY = True

  if _HARDWARE_RASTERIZER in argv[1:]:
    print 'Rendering with local graphics hardware.'
    _HARDWARE_DEVICE = True
  else:
    print 'Rendering with DX9 reference rasterizer.'
    
  return True


def main(argv):
  """Main entry point of the script.

  Args:
    argv:  The c-like arguments to the script.

  Returns:
    True if all tests pass, false othwerwise.
  """

  print 'Running O3D system tests.'

  if not ValidateArgs(argv):
    return False

  os.chdir(os.path.dirname(argv[0]))

  test_set = BuildTestList()

  # Invoke each test, tracking failures.
  test_failures = []
  for test in test_set:
    if not InvokeTest(test):
      test_failures += [test]

  if len(test_failures) == 0:
    print 'Success:  All tests passed.'
    return True
  else:
    print 'Error Summary:  The following tests failed:'
    print test_failures
    return False


if __name__ == '__main__':
  if main(sys.argv):
    # Return with a 0 for success (per unix convention).
    sys.exit(0)
  else:
    # Return with a 1 for failure (per unix convention).
    sys.exit(1)
