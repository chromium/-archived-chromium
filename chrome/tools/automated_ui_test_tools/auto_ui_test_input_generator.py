#!/usr/bin/python
#
# Copyright 2008 Google Inc. All Rights Reserved.

"""This generates the input file for automated_ui_tests.exe.

We take in a list of possible actions separated  by new lines, the number of
commands per file, and the number of actions per command, and generate
a single random file, or one or more files containing all possible commands,
or one file with a partial set of possible commands starting at a certain
command number.

Example usage:
  python auto_ui_test_input_generator.py --commands-per-file 50
      --actions-per-command 5 --action-list-file possible_actions.txt
      --output random_file.txt

  Generates a file called random_file.txt with 50 commands containing 5 actions
  each randomly chosen from the list of new line separated actions in
  possible_actions.txt

  python auto_ui_test_input_generator.py --commands-per-file 200
      --actions-per-command 6 --partial-coverage-from 700
      --action-list-file possible_actions.txt
      --output partial_coverage.txt

  Generates a file called partial_coverage.txt with 200 commands containing 6
  actions each starting at command #700 and ending at command #899 and chosen
  from the list of new line separated actions possible_actions.txt


Options:
  --action-list-file input_file_name
    Name of file containing possible actions separated  by new lines. You can
    find supported actions in the automated_ui_tests.h documentation.

  --output output_file_name
    Name of XML file that will be outputted for use as input for
    automated_ui_tests.exe.

  --commands-per-file commands_per_file
    Number of commands per file.

  --actions-per-command actions_per_command
    Number of actions per command.

  --full-coverage
    If full_coverage flag is set will output as many files as neccesary to cover
    every combination of possible actions. Files will be named
    output_file_name_1.txt, output_file_name_2.txt, etc...
    If --full-coverage is true, --full-coverage-one-file and
    --partial-coverage-from are ignored.

  --full-coverage-one-file
    Just like --full_coverage, but outputs to just one file.
    Ignores commands_per_file. This is very likely to cause memory errors on
    large action sets. If --full coverage-one-file is true,
    --partial-coverage-from is ignored.

  --partial-coverage-from command_to_start_at
    Outputs a part of the full coverage, starting at command number
    |command_to_start_at| and ending at command number |command_to_start_at| +
    |commands_per_file|. Command numbering starts at 0, and the maximum
    command number is number_of_actions_we_choose_from ^ actions_per_command - 1.
    If |command_to_start_at| + |commands_per_file| is greater than the maximum
    command number, then only the commands up to the maximum command number
    are printed.

  --quiet
    Silence progress messages.
"""

import optparse
import os.path
import random
import xml.dom.minidom

class ComprehensiveListGenerator:
  """Generates a comprehensive list of all the ways to choose x combinations
  from an input list, with replacement.

  Init takes |list_length|, which is the length of the of the combination and
  |source_list|, which is the list we want to choose from.
  GetNextPortionOfSourceList() returns a list of length |list_length| with a
  portion of the complete list of all combinations or None once all possible
  combinations have been returned.

  Example:
  >>> list_gen = ComprehensiveListGenerator(2, ['a','b','c'])
  >>> print list_gen.GetNextPortionOfSourceList()
  ['a','a']
  >>> print list_gen.GetNextPortionOfSourceList()
  ['a','b']
  >>> print list_gen.GetNextPortionOfSourceList()
  ['a','c']
  >>> ...print list_gen.GetNextPortionOfSourceList() 6 more times...
  >>> print list_gen.GetNextPortionOfSourceList()
  None
  >>> list_gen.SetIntegerListToCombinationNumber(2)
  >>> print list_gen.GetCurrentPortionOfSourceList()
  ['a','c']
  >>> print list_gen.GetNextPortionOfSourceList()
  ['b','a']
  >>> list_gen.SetIntegerListToCombinationNumber(8)
  >>> print list_gen.GetCurrentPortionOfSourceList()
  ['c','c']
  >>> list_gen.SetIntegerListToCombinationNumber(9)
  >>> print list_gen.GetCurrentPortionOfSourceList()
  None

  Attributes:
    __list_length:    Length of the resulting combinations.
    __source_list:    The list we are pulling combinations from.
    __integer_list:   A list of integers representing which indices of
                    |source_list| to return.
  """

  def __init__(self, list_length, source_list):
    self.__list_length = list_length
    self.__integer_list = None
    self.__source_list = source_list

  def SetIntegerListToCombinationNumber(self, combo_number):
    """ Sets the integer list to represent the |combo_number|th number in the
    sequence, counting from 0.

    Args:
      combo_number: Number to set the integer list to represent.

    Returns: Sets integer_list to None and returns False if the combo_number is
    out of range (bigger than the maximum number of combinations possible or
    less than 0)
    """
    if (combo_number < 0 or
       combo_number >= len(self.__source_list) ** self.__list_length):
     self.__integer_list = None
     return False
    if self.__integer_list == None:
     self.__integer_list = []
     for i in range(self.__list_length):
       self.__integer_list.append(0)
    reversed_range = range(self.__list_length)
    reversed_range.reverse()
    index_max_value = len(self.__source_list)
    quotient = 0
    for index in reversed_range:
      quotient, remainder = divmod(combo_number, index_max_value)
      combo_number = quotient
      self.__integer_list[index] = remainder

    return True

  def __IncrementIntegerListIndex(self, index):
    """ Increments the given index of integer_list, rolling over to 0 and
    incrementing the a lower index if the index is incremented above the last
    index of source_list

    Args:
      index: The index integer_list to attempt to increment.

    Returns: False if it is impossible to incremement any index in the list
    which is less than or equal to the given index.
    """
    self.__integer_list[index] += 1
    if self.__integer_list[index] >= len(self.__source_list):
      # We've incremented beyond the length of source_list, so reset to zero...
      self.__integer_list[index] = 0
      # And if our index is high enough, increment the next index. Otherwise
      # we can't increment any further and should return false.
      if index > 0:
        return self.__IncrementIntegerListIndex(index-1)
      else:
        # Restart the integer list at the beginning
        self.__integer_list = None
        return False
    # Successfuly incremented the index, return true.
    return True

  def __IncrementIntegerList(self):
    """ Gets the next integer list in the series by attempting to increment the
    final index of integer_list.

    Returns: False if we can't increment any index in the integer_list.
    """

    # If the list is empty we just started, so populate it with zeroes.
    if self.__integer_list == None:
      self.__integer_list = []
      for i in range(self.__list_length):
        self.__integer_list.append(0)
      return True
    else:
      return self.__IncrementIntegerListIndex(self.__list_length-1)

  def GetCurrentPortionOfSourceList(self):
    """ Returns the current portion of source_list corresponding to the
    integer_list

    For example, if our current state is:
      integer_list = [0,1,0,2]
      source_list = ['a','b','c','d']
    Then calling GetCurrentPortionOfSourceList() returns:
      ['a','b','a','c']

    Returns: None if the integer_list is empty, otherwise a list of length
    list_length with a combination of elements from source_list
    """
    portion_list = []
    if self.__integer_list == None:
      return None

    for index in range(self.__list_length):
      portion_list.append(self.__source_list[self.__integer_list[index]])

    return portion_list

  def GetNextPortionOfSourceList(self):
    """ Increments the integer_list and then returns the current portion of
    source_list corresponding to the integer_list.

    This is the only function outside users should be calling. It will advance
    to the next combination of elements from source_list, and return it. See
    the class documentation for proper use.

    Returns: None if all possible combinations of source_list have previously
    been returned. Otherwise a new list of length list_length with a combination
    of elements from source_list.
    """
    if self.__IncrementIntegerList():
      return self.GetCurrentPortionOfSourceList()
    else:
      return None

class AutomatedTestInputGenerator:
  """Creates a random file with with the name |file_name| with
  the number of commands and actions specified in the command line.

  Attributes:
    __commands_per_file: Number of commands per file.
    __actions_per_command: Number of actions per command.
    __actions_list: A list of strings representing the possible actions.
    __is_verbose: If true, print progress messages
  """
  def __init__(self):
    (options,args) = ParseCommandLine()
    input_file = open(options.input_file_name)
    actions_list = input_file.readlines()
    input_file.close()

    self.__commands_per_file = options.commands_per_file
    self.__actions_per_command = options.actions_per_command
    self.__actions_list = [action.strip() for action in actions_list]
    self.__is_verbose = options.is_verbose

  def __CreateDocument(self):
    """ Create the starter XML document.

    Returns: A tuple containing the XML document and its root element named
    "Command List".
    """
    doc = xml.dom.minidom.Document()
    root_element = doc.createElement("CommandList")
    doc.appendChild(root_element)
    return doc, root_element

  def __WriteToOutputFile(self, file_name, output):
    """Writes |output| to file with name |filename|. Overwriting any pre-existing
       file.

    Args:
      file_name: Name of the file to create.
      output: The string to write to file.
    """
    output_file = open(file_name, 'w')
    output_file.write(output)
    output_file.close()

  def CreateRandomFile(self, file_name):
    """Creates a random file with with the name |file_name| with
       the number of commands and actions specified in the command line.

    Args:
      file_name - Name of the file to create.

    Return:
      Nothing.
    """
    output_doc, root_element = self.__CreateDocument()
    for command_num in range(0, self.__commands_per_file):
      command_element = output_doc.createElement("command")
      for action_num in range(0,self.__actions_per_command):
        action_element = output_doc.createElement(
            random.choice(self.__actions_list))
        command_element.appendChild(action_element)
      root_element.appendChild(command_element)
    self.__WriteToOutputFile(file_name, output_doc.toprettyxml())

  def __AddCommandToDoc(self, output_doc, root_element, command, command_num):
    """Adds a given command to the output XML document

    Args:
      output_doc - The output XML document. Used to create elements.
      root_element - The root element of the XML document. (What we add the
                     command to)
      command - The name of the command element we create and add to the doc.
      command_num - The number of the command.

    Return:
      Nothing.
    """
    command_element = output_doc.createElement("command")
    command_element.setAttribute("number", str(command_num))
    for action in command:
      action_element = output_doc.createElement(action)
      command_element.appendChild(action_element)
    root_element.appendChild(command_element)

  def CreateComprehensiveFile(self, file_name, start_command, write_to_end):
    """Creates one file containing all or part of the comprehensive list of
    commands starting at a set command.

    Args:
      file_name - Name of the file to create.
      start_command - Command number to start at.
      write_to_end - If true, writes all remaining commands, starting at
                     start_command. (If start_command is 0, this would write all
                     possible commands to one file.) If false, write only
                     commands_per_file commands starting at start_command

    Return:
      Nothing.
    """
    list_generator = ComprehensiveListGenerator(self.__actions_per_command,
                                                self.__actions_list)
    output_doc, root_element = self.__CreateDocument()
    command_counter = start_command
    end_command = start_command + self.__commands_per_file
    is_complete = False
    # Set the underlying integer representation of the command to the
    # the starting command number.
    list_generator.SetIntegerListToCombinationNumber(start_command)
    command = list_generator.GetCurrentPortionOfSourceList()
    while (command != None and
          (write_to_end == True or command_counter < end_command)):
      self.__AddCommandToDoc(output_doc, root_element, command, command_counter)
      command_counter += 1;
      command = list_generator.GetNextPortionOfSourceList()

    self.__WriteToOutputFile(file_name, output_doc.toprettyxml())

  def CreateComprehensiveFiles(self, file_name):
    """Creates a comprehensive coverage of all possible combinations from
    action_list of length commands_per_file. Names them |file_name|_1,
    |file_name|_2, and so on.

    Args:
      file_name - Name of the file to create.

    Return:
      Nothing.
    """
    list_generator = ComprehensiveListGenerator(self.__actions_per_command,
                                                self.__actions_list)

    is_complete = False
    file_counter = 0
    # Split the file name so we can include the file number before the extension.
    base_file_name, extension = os.path.splitext(file_name)
    command_num = 0

    while is_complete == False:
      output_doc, root_element = self.__CreateDocument()
      file_counter += 1
      if self.__is_verbose and file_counter % 200 == 0:
        print "Created " + str(file_counter) + " files... "

      for i in range(self.__commands_per_file):
        # Get the next sequence of actions as a list.
        command = list_generator.GetNextPortionOfSourceList()
        if command == None:
          is_complete = True
          break

        # Parse through the list and add them to the output document as children
        # of a command element
        self.__AddCommandToDoc(output_doc, root_element, command, command_num)
        command_num += 1

      # Finished the commands for this file, so write it and start on next file.
      self.__WriteToOutputFile(base_file_name + "_" + str(file_counter) +
                             extension, output_doc.toprettyxml())

def ParseCommandLine():
  """Parses the command line.

  Return: List of options and their values and a list of arguments which were
  unparsed.
  """
  parser = optparse.OptionParser()
  parser.set_defaults(full_coverage=False)
  parser.add_option("-i", "--action-list-file", dest="input_file_name",
                    type="string", action="store", default="possible_actions.txt",
                    help="input file with a test of newline separated actions"
                    "which are possible. Default is 'possible_actions.txt'")
  parser.add_option("-o", "--output", dest="output_file_name", type="string",
                    action="store", default="automated_ui_tests.txt",
                    help="the file to output the command list to")
  parser.add_option("-c", "--commands-per-file", dest="commands_per_file",
                    type="int", action="store", default=500,
                    help="number of commands per file")
  parser.add_option("-a", "--actions-per-command", dest="actions_per_command",
                    type="int", action="store", default=5,
                    help="number of actions per command")
  parser.add_option("-f", "--full-coverage", dest="full_coverage",
                    action="store_true", help="Output files for every possible"
                    "combination. Default is to output just one random file.")
  parser.add_option("-q", "--quiet",
                    action="store_false", dest="is_verbose", default=True,
                    help="don't print progress message while creating files")
  parser.add_option("-1", "--full-coverage-one-file",
                    action="store_true", dest="full_coverage_one_file",
                    default=False,
                    help="complete coverage all outputted to one file")
  parser.add_option("-p", "--partial-coverage-from", dest="start_at_command",
                    type="int", action="store", default=-1,
                    help="partial list from the complete coverage, starting at"
                    "command #start_at_command")

  return parser.parse_args()

def main():
  (options,args) = ParseCommandLine()
  test_generator = AutomatedTestInputGenerator()
  if options.full_coverage == True:
    if options.full_coverage_one_file and options.is_verbose == True:
      print ("Error: Both --full-coverage and --full-coverage-one-file present,"
            " ignoring --full-coverage-one-file")
    if options.start_at_command >= 0 and options.is_verbose == True:
      print ("Error: Both --full-coverage and --partial-coverage-from present,"
            " ignoring --partial-coverage-from")
    if options.is_verbose == True:
      print "Starting to write comprehensive files:"
    test_generator.CreateComprehensiveFiles(options.output_file_name)
    if options.is_verbose == True:
      print "Finished writing comprehensive files."
  elif options.full_coverage_one_file:
    if options.start_at_command >= 0 and options.is_verbose == True:
      print ("Error: Both --full-coverage-one-file present and"
            "--partial-coverage-from present, ignoring --partial-coverage-from")
    if options.is_verbose == True:
      print "Starting to write comprehensive file:"
    test_generator.CreateComprehensiveFile(options.output_file_name, 0, True)
    if options.is_verbose == True:
      print "Finished writing comprehensive file."
  elif options.start_at_command >= 0:
    if options.is_verbose == True:
      print "Starting to write partial file:"
    test_generator.CreateComprehensiveFile(options.output_file_name,
                                           options.start_at_command , False)
    if options.is_verbose == True:
      print "Finished writing partial file."
  else:
    test_generator.CreateRandomFile(options.output_file_name)
    if options.is_verbose == True:
      print "Output written to file: " + options.output_file_name

if __name__ == '__main__':
  main()
