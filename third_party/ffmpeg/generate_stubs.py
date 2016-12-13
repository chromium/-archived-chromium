#!/usr/bin/python
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates windows and posix stub files for a given set of signatures.

For libraries that need to be loaded outside of the standard executable startup
path mechanism, stub files need to be generated for the wanted functions.  In
windows, this is done via "def" files and the delay load mechanism.  On a posix
system, a set of stub functions need to be generated that dispatch to functions
found via dlsym.

This script takes a set of files, where each file is a list of C-style
signatures (one signature per line).  The output is either a windows def file,
or a header + implementation file of stubs suitable for use in a posix system.
"""

__author__ = 'ajwong@chromium.org (Albert J. Wong)'

import optparse
import os
import re
import string
import subprocess


class Error(Exception):
  pass


class BadSignatureError(Error):
  pass


# Regular expression used to parse signatures in the input files. The regex
# is built around identifying the "identifier" for the function name.  We
# consider the identifier to be the string that follows these constraints:
#
#   1) Starts with [_a-ZA-Z] (C++ spec 2.10).
#   2) Continues with [_a-ZA-Z0-9] (C++ spec 2.10).
#   3) Preceeds an opening parenthesis by 0 or more whitespace chars.
#
# From that, all preceeding characters are considered the return value.
# Trailing characters should have a substring matching the form (.*).  That
# is considered the arguments.
SIGNATURE_REGEX = re.compile('(.+?)([_a-zA-Z][_a-zA-Z0-9]+)\s*\((.*?)\)')

# Used for generating C++ identifiers.
INVALID_C_IDENT_CHARS = re.compile('[^_a-zA-Z0-9]')

# Constans defning the supported file types options.
FILE_TYPE_WIN = 'windows_lib'
FILE_TYPE_POSIX_STUB = 'posix_stubs'


def RemoveTrailingSlashes(path):
  """Removes trailing forward slashes from the path.

  We expect unix style path separators (/).  Gyp passes unix style
  separators even in windows so this is okay.

  Args:
    path: The path to strip trailing slashes from.

  Returns:
    The path with trailing slashes removed.
  """
  # Special case a single trailing slash since '/' != '' in a path.
  if path == '/':
    return path

  return path.rstrip('/')


def ExtractModuleName(infile_path):
  """Infers the module name from the input file path.

  The input filename is supposed to be in the form "ModuleName.sigs".
  This function splits the filename from the extention on that basename of
  the path and returns that as the module name.

  Args:
    infile_path: String holding the path to the input file.

  Returns:
    The module name as a string.
  """
  # File name format is ModuleName.sig so split at the period.
  basename = os.path.basename(infile_path)
  return os.path.splitext(basename)[0]


def ParseSignatures(infile):
  """Parses function signatures in the input file.

  This function parses a file of signatures into a list of dictionaries that
  represent the function signatures in the input file.  Each dictionary has
  the following keys:
    return_type: A string with the return type.
    name: A string with the name of the function.
    params: A list of each function parameter declaration (type + name)

  The format of the input file is one C-style function signature per line, no
  trailing semicolon.  Empty lines are allowed.  An empty line is a line that
  consists purely of whitespace.  Lines that begin with a # are considered
  comment lines and are ignored.

  We assume that "int foo(void)" is the same as "int foo()", which is not
  true in C where "int foo()" is equivalent to "int foo(...)".  Our generated
  code is C++, and we do not handle varargs, so this is a case that can be
  ignored for now.

  Args:
    infile: File object holding a text file of function signatures.

  Returns:
    A list of dictionaries, where each dictionary represents one function
    signature.

  Raises:
    BadSignatureError: A line could not be parsed as a signature.
  """
  signatures = []
  for line in infile:
    line = line.strip()
    if line and line[0] != '#':
      m = SIGNATURE_REGEX.match(line)
      if m is None:
        raise BadSignatureError('Unparsable line: %s' % line)
      signatures.append(
          {'return_type': m.group(1).strip(),
           'name': m.group(2).strip(),
           'params': [arg.strip() for arg in m.group(3).split(',')]})
  return signatures


class WindowsLibCreator(object):
  """Creates an import library for the functions provided.

  In windows, we created a def file listing the library entry points, then
  generate a stub .lib based on that def file.  This class enapsulates this
  logic.
  """

  def __init__(self, module_name, signatures, intermediate_dir, outdir_path):
    """Initializes the WindowsLibCreator for creating a library stub.

    Args:
      module_name: The name of the module we are writing a stub for.
      signatures: The list of signatures to create stubs for.
      intermediate_dir: The directory where the generated .def files should go.
      outdir_path: The directory where generated .lib files should go.
    """
    self.module_name = module_name
    self.signatures = signatures
    self.intermediate_dir = intermediate_dir
    self.outdir_path = outdir_path

  def DefFilePath(self):
    """Generates the path of the def file for the given module_name.

    Returns:
      A string with the path to the def file.
    """
    # Output file name is in the form "module_name.def".
    return '%s/%s.def' % (self.intermediate_dir, self.module_name)

  def LibFilePath(self):
    """Generates the path of the lib file for the given module_name.

    Returns:
      A string with the path to the lib file.
    """
    # Output file name is in the form "module_name.lib".
    return '%s/%s.lib' % (self.outdir_path, self.module_name)

  def WriteDefFile(self):
    """Creates a windows library defintion file.

    The def file format is basically a list of function names.  Generation is
    simple.  After outputting the LIBRARY and EXPORTS lines, print out each
    function name, one to a line, preceeded by 2 spaces.

    Calling this function will create a def file in the outdir_path with the
    name "module_name.def".
    """
    # Open the def file for writing.
    try:
      outfile = open(self.DefFilePath(), 'w')
      self._WriteDefFile(outfile)
    finally:
      if outfile is not None:
        outfile.close()

  def _WriteDefFile(self, outfile):
    """Implementation of WriteDefFile for testing.

    This implementation allows injection of the outfile object for testing.

    Args:
      outfile: File to populate with definitions.
    """
    # Write file header.
    outfile.write('LIBRARY %s\n' % self.module_name)
    outfile.write('EXPORTS\n')

    # Output an entry for each signature.
    for sig in self.signatures:
      outfile.write('  ')
      outfile.write(sig['name'])
      outfile.write('\n')

  def CreateLib(self):
    """Creates a windows library file.

    Calling this function will create a lib file in the outdir_path that exports
    the signatures passed into the object.
    """
    self.WriteDefFile()
    subprocess.call(['lib', '/nologo', '/machine:X86',
                     '/def:' + self.DefFilePath(),
                     '/out:' + self.LibFilePath()])


class PosixStubWriter(object):
  """Creates a file of stub functions for a library that is opened via dloepn.

  Windows provides a function in their compiler known as delay loading, which
  effectively generates a set of stub functions for a dynamic library that
  delays loading of the dynamic library/resolution of the symbols until one of
  the needed functions are accessed.

  In posix, RTLD_LAZY does something similar with DSOs.  This is the default
  link mode for DSOs.  However, even though the symbol is not resolved until
  first usage, the DSO must be present at load time of the main binary.

  To simulate the windows delay load procedure, we need to create a set of
  stub functions that allow for correct linkage of the main binary, but
  dispatch to the dynamically resolved symbol when the module is initialized.

  This class takes a list of function signatures, and generates a set of stub
  functions plus initialization code for them.
  """

  def __init__(self, module_name, signatures):
    """Initializes PosixStubWriter for this set of signatures and module_name.

    Args:
      module_name: The name of the module we are writing a stub for.
      signatures: The list of signatures to create stubs for.
    """
    self.signatures = signatures
    self.module_name = module_name

  @classmethod
  def CStyleIdentifier(cls, identifier):
    """Generates a C style identifier.

    The module_name has all invalid identifier characters removed (anything
    that's not [_a-zA-Z0-9]) and is run through string.capwords to try
    and approximate camel case.

    Args:
      identifier: The string with the module name to turn to C-style.

    Returns:
      A string that can be used as part of a C identifier.
    """
    return string.capwords(re.sub(INVALID_C_IDENT_CHARS, '', identifier))

  @classmethod
  def EnumName(cls, module_name):
    """Gets the enum name for the module.

    Takes the module name and creates a suitable enum name.  The module_name
    is munged to be a valid C identifier then prefixed with the string
    "kModule" to generate a Google style enum name.

    Args:
      module_name: The name of the module to generate an enum name for.

    Returns:
      A string with the name of the enum value representing this module.
    """
    return 'kModule%s' % PosixStubWriter.CStyleIdentifier(module_name)

  @classmethod
  def IsInitializedName(cls, module_name):
    """Gets the name of function that checks initialization of this module.

    The name is in the format IsModuleInitialized.  Where "Module" is replaced
    with the module name, munged to be a valid C identifier.

    Args:
      module_name: The name of the module to generate the function name for.

    Returns:
      A string with the name of the initialization check function.
    """
    return 'Is%sInitialized' % PosixStubWriter.CStyleIdentifier(module_name)

  @classmethod
  def InitializeModuleName(cls, module_name):
    """Gets the name of the function that initializes this module.

    The name is in the format InitializeModule.  Where "Module" is replaced
    with the module name, munged to be a valid C identifier.

    Args:
      module_name: The name of the module to generate the function name for.

    Returns:
      A string with the name of the initialization function.
    """
    return 'Initialize%s' % PosixStubWriter.CStyleIdentifier(module_name)

  @classmethod
  def UninitializeModuleName(cls, module_name):
    """Gets the name of the function that uninitializes this module.

    The name is in the format UninitializeModule.  Where "Module" is replaced
    with the module name, munged to be a valid C identifier.

    Args:
      module_name: The name of the module to generate the function name for.

    Returns:
      A string with the name of the uninitialization function.
    """
    return 'Uninitialize%s' % PosixStubWriter.CStyleIdentifier(module_name)

  @classmethod
  def HeaderFilePath(cls, stubfile_name, outdir_path):
    """Generate the path to the header file for the stub code.

    Args:
      stubfile_name: A string with name of the output file (minus extension).
      outdir_path: A string with the path of the directory to create the files
          in.

    Returns:
      A string with the path to the header file.
    """
    return '%s/%s.h' % (outdir_path, stubfile_name)

  @classmethod
  def ImplementationFilePath(cls, stubfile_name, outdir_path):
    """Generate the path to the implementation file for the stub code.

    Args:
      stubfile_name: A string with name of the output file (minus extension).
      outdir_path: A string with the path of the directory to create the files
          in.

    Returns:
      A string with the path to the implementation file.
    """
    return '%s/%s.cc' % (outdir_path, stubfile_name)

  @classmethod
  def StubFunctionPointer(cls, signature):
    """Generates a function pointer declaration for the given signature.

    Args:
      signature: The hash representing the function signature.

    Returns:
      A string with the declaration of the function pointer for the signature.
    """
    return 'static %s (*%s_ptr)(%s) = NULL;' % (signature['return_type'],
                                                signature['name'],
                                                ', '.join(signature['params']))

  @classmethod
  def StubFunction(cls, signature):
    """Generates a stub function definition for the given signature.

    The function definitions are created with __attribute__((weak)) so that
    they may be overridden by a real static link or mock versions to be used
    when testing.

    Args:
      signature: The hash representing the function signature.

    Returns:
      A string with the stub function definition.
    """
    # Generate the return statement prefix if this is not a void function.
    return_prefix = ''
    if signature['return_type'] != 'void':
      return_prefix = 'return '

    # Generate the argument list.
    arguments = [re.split('[\*& ]', arg)[-1].strip() for arg in
                 signature['params']]
    arg_list = ', '.join(arguments)
    if arg_list == 'void':
      arg_list = ''

    return """extern %(return_type)s %(name)s(%(params)s) __attribute__((weak));
%(return_type)s %(name)s(%(params)s) {
  %(return_prefix)s%(name)s_ptr(%(arg_list)s);
}""" % {'return_type': signature['return_type'],
        'name': signature['name'],
        'params': ', '.join(signature['params']),
        'return_prefix': return_prefix,
        'arg_list': arg_list}

  @classmethod
  def WriteImplementationPreamble(cls, header_path, outfile):
    """Write the necessary includes for the implementation file.

    Args:
      header_path: The path to the header file.
      outfile: The file handle to populate.
    """
    # Write file header.
    outfile.write("""// This is generated file. Do not modify directly.

#include "%s"

#include <stdlib.h>  // For NULL.
#include <dlfcn.h>   // For dysym, dlopen.

#include <map>
#include <vector>
""" % header_path)

  @classmethod
  def WriteUmbrellaInitializer(cls, module_names, namespace, outfile):
    """Writes a single function that will open + inialize each module.

    This intializer will take in an stl map of that lists the correct
    dlopen target for each module.  The map type is
    std::map<enum StubModules, vector<std::string>> which matches one module
    to a list of paths to try in dlopen.

    This function is an all-or-nothing function.  If any module fails to load,
    all other modules are dlclosed, and the function returns.  Though it is
    not enforced, this function should only be called once.

    Args:
      module_names: A list with the names of the modules in this stub file.
      namespace: The namespace these functions should be in.
      outfile: The file handle to populate with pointer definitions.
    """
    # Open namespace and add typedef for internal data structure.
    outfile.write("""namespace %s {
typedef std::map<StubModules, void*> StubHandleMap;
""" % namespace)

    # Write a static cleanup function.
    outfile.write("""static void CloseLibraries(StubHandleMap* stub_handles) {
  for (StubHandleMap::const_iterator it = stub_handles->begin();
       it != stub_handles->end();
       ++it) {
    dlclose(it->second);
  }

  stub_handles->clear();
}
""")

    # Create the stub initialize function.
    outfile.write("""bool InitializeStubs(const StubPathMap& path_map) {
  StubHandleMap opened_libraries;
  for (int i = 0; i < kNumStubModules; ++i) {
    StubModules cur_module = static_cast<StubModules>(i);
    // If a module is missing, we fail.
    StubPathMap::const_iterator it = path_map.find(cur_module);
    if (it == path_map.end()) {
      CloseLibraries(&opened_libraries);
      return false;
    }

    // Otherwise, attempt to dlopen the library.
    const std::vector<std::string>& paths = it->second;
    bool module_opened = false;
    for (std::vector<std::string>::const_iterator dso_path = paths.begin();
         !module_opened && dso_path != paths.end();
         ++dso_path) {
      void* handle = dlopen(dso_path->c_str(), RTLD_LAZY);
      if (handle != NULL) {
        module_opened = true;
        opened_libraries[cur_module] = handle;
      }
    }

    if (!module_opened) {
      CloseLibraries(&opened_libraries);
      return false;
    }
  }

  // Initialize each module if we have not already failed.
""")

    # Call each module initializer.
    for module in module_names:
      outfile.write('  %s(opened_libraries[%s]);\n' %
                    (PosixStubWriter.InitializeModuleName(module),
                     PosixStubWriter.EnumName(module)))
    outfile.write('\n')

    # Check the initialization status, clean up on error.
    initializer_checks = ['!%s()' % PosixStubWriter.IsInitializedName(name)
                          for name in module_names]
    uninitializers = ['%s()' % PosixStubWriter.UninitializeModuleName(name)
                      for name in module_names]
    outfile.write("""  // Check that each module is initialized correctly.
  // Close all previously opened libraries on failure.
  if (%(conditional)s) {
    %(uninitializers)s;
    CloseLibraries(&opened_libraries);
    return false;
  }

  return true;
}

}  // namespace %(namespace)s
""" % {'conditional': ' ||\n      '.join(initializer_checks),
       'uninitializers': ';\n    '.join(uninitializers),
       'namespace': namespace})

  @classmethod
  def WriteHeaderContents(cls, module_names, namespace, header_guard, outfile):
    """Writes a header file for the stub file generated for module_names.

    The header file exposes the following:
       1) An enum, StubModules, listing with an entry for each enum.
       2) A typedef for a StubPathMap allowing for specification of paths to
          search for each module.
       3) The IsInitialized/Initialize/Uninitialize functions for each module.
       4) An umbrella initialize function for all modules.

    Args:
      module_names: A list with the names of each module in this stub file.
      namespace: The namespace these functions should be in.
      header_guard: The macro to use as our header guard.
      outfile: The output file to populate.
    """
    outfile.write("""// This is generated file. Do not modify directly.

#ifndef %(guard_name)s
#define %(guard_name)s

#include <map>
#include <string>
#include <vector>

namespace %(namespace)s {
""" % {'guard_name': header_guard, 'namespace': namespace})

    # Generate the Initializer protoypes for each module.
    outfile.write('// Individual module initializer functions.\n')
    for name in module_names:
      outfile.write("""bool %s();
void %s(void* module);
void %s();

""" % (PosixStubWriter.IsInitializedName(name),
       PosixStubWriter.InitializeModuleName(name),
       PosixStubWriter.UninitializeModuleName(name)))

    # Generate the enum for umbrella initializer.
    outfile.write('// Enum and typedef for umbrella initializer.\n')
    outfile.write('enum StubModules {\n')
    outfile.write('  %s = 0,\n' % PosixStubWriter.EnumName(module_names[0]))
    for name in module_names[1:]:
      outfile.write('  %s,\n' % PosixStubWriter.EnumName(name))
    outfile.write('  kNumStubModules\n')
    outfile.write('};\n')
    outfile.write('\n')

    # Generate typedef and prototype for umbrella initializer and close the
    # header file.
    outfile.write(
"""typedef std::map<StubModules, std::vector<std::string> > StubPathMap;

// Umbrella initializer for all the modules in this stub file.
bool InitializeStubs(const StubPathMap& path_map);

}  // namespace %s

#endif  // %s
""" % (namespace, header_guard))

  def WriteImplementationContents(self, namespace, outfile):
    """Given a file handle, write out the stub definitions for this module.

    Args:
      namespace: The namespace these functions should be in.
      outfile: The file handle to populate.
    """
    outfile.write('extern "C" {\n')
    outfile.write('\n')
    self.WriteFunctionPointers(outfile)
    self.WriteStubFunctions(outfile)
    outfile.write('\n')
    outfile.write('}  // extern "C"\n')
    outfile.write('\n')

    outfile.write('namespace %s {\n' % namespace)
    outfile.write('\n')
    self.WriteModuleInitializeFunctions(outfile)
    outfile.write('}  // namespace %s\n\n' % namespace)

  def WriteFunctionPointers(self, outfile):
    """Write the function pointer declarations needed by the stubs.

    We need function pointers to hold the actual location of the function
    implementation returned by dlsym.  This function outputs a pointer
    definition for each signature in the module.

    Pointers will be named with the following pattern "FuntionName_ptr".

    Args:
      outfile: The file handle to populate with pointer definitions.
    """
    outfile.write(
"""// Static pointers that will hold the location of the real function
// implementations after the module has been loaded.
""")
    for sig in self.signatures:
      outfile.write("%s\n" % PosixStubWriter.StubFunctionPointer(sig))
    outfile.write("\n");

  def WriteStubFunctions(self, outfile):
    """Write the function stubs to handle dispatching to real implementations.

    Functions that have a return type other than void will look as follows:

      ReturnType FunctionName(A a) {
        return FunctionName_ptr(a);
      }

    Functions with a return type of void will look as follows:

      void FunctionName(A a) {
        FunctionName_ptr(a);
      }

    Args:
      outfile: The file handle to populate.
    """
    outfile.write('// Stubs that dispatch to the real implementations.\n')
    for sig in self.signatures:
      outfile.write("%s\n" % PosixStubWriter.StubFunction(sig));

  def WriteModuleInitializeFunctions(self, outfile):
    """Write functions to initialize/query initlialization of the module.

    This creates 2 functions IsModuleInitialized and InitializeModule where
    "Module" is replaced with the module name, first letter capitalized.

    The InitializeModule function takes a handle that is retrieved from dlopen
    and attempts to assign each function pointer above via dlsym.

    The IsModuleInitialized returns true if none of the required functions
    pointers are NULL.

    Args:
      outfile: The file handle to populate.
    """
    ptr_names = ['%s_ptr' % sig['name'] for sig in self.signatures]

    # Construct the function that tests that all the function pointers above
    # have been properly populated.
    outfile.write(
"""// Returns true if all stubs have been properly initialized.
bool %s() {
  if (%s) {
    return true;
  } else {
    return false;
  }
}

""" % (PosixStubWriter.IsInitializedName(self.module_name),
       ' &&\n      '.join(ptr_names)))

    # Create function that initializes the module.
    outfile.write('// Initializes the module stubs.\n')
    outfile.write('void %s(void* module) {\n' %
                  PosixStubWriter.InitializeModuleName(self.module_name))
    for sig in self.signatures:
      outfile.write("""  %(name)s_ptr =
    reinterpret_cast<%(return_type)s (*)(%(parameters)s)>(
      dlsym(module, "%(name)s"));
""" % {'name': sig['name'],
       'return_type': sig['return_type'],
       'parameters': ', '.join(sig['params'])})
    outfile.write('}\n')
    outfile.write('\n')

    # Create function that uninitializes the module (sets all pointers to
    # NULL).
    outfile.write('// Uninitialize the module stubs.  Reset to NULL.\n')
    outfile.write('void %s() {\n' %
                  PosixStubWriter.UninitializeModuleName(self.module_name))
    for sig in self.signatures:
      outfile.write('  %s_ptr = NULL;\n' % sig['name'])
    outfile.write('}\n')
    outfile.write('\n')


def main():
  parser = optparse.OptionParser(usage='usage: %prog [options] input')
  parser.add_option('-o',
                    '--output',
                    dest='out_dir',
                    default=None,
                    help='Output location.')
  parser.add_option('-i',
                    '--intermediate_dir',
                    dest='intermediate_dir',
                    default=None,
                    help='Locaiton of intermediate files.')
  parser.add_option('-t',
                    '--type',
                    dest='type',
                    default=None,
                    help=('Type of file. Either "%s" or "%s"' %
                          (FILE_TYPE_POSIX_STUB, FILE_TYPE_WIN)))
  parser.add_option('-s',
                    '--stubfile_name',
                    dest='stubfile_name',
                    default=None,
                    help=('Name of posix_stubs output file. Ignored for '
                          '%s type.' % FILE_TYPE_WIN))
  parser.add_option('-p',
                    '--path_from_source',
                    dest='path_from_source',
                    default=None,
                    help=('The relative path from the project root that the '
                          'generated file should consider itself part of (eg. '
                          'third_party/ffmpeg).  This is used to generate the '
                          'header guard and namespace for our initializer '
                          'functions and does NOT affect the physical output '
                          'location of the file like -o does.  Ignored for '
                          ' %s type.' % FILE_TYPE_WIN))
  parser.add_option('-e',
                    '--extra_stub_header',
                    dest='extra_stub_header',
                    default=None,
                    help=('File to insert after the system includes in the '
                          'generated stub implemenation file. Ignored for '
                          '%s type.' % FILE_TYPE_WIN))
  (options, args) = parser.parse_args()

  if options.out_dir is None:
    parser.error('Output location not specified')
  if len(args) == 0:
    parser.error('No inputs specified')

  if options.type not in [FILE_TYPE_WIN, FILE_TYPE_POSIX_STUB]:
    parser.error('Invalid output file type')

  if options.type == FILE_TYPE_POSIX_STUB:
    if options.stubfile_name is None:
      parser.error('Output file name need for %s' % FILE_TYPE_POSIX_STUB)
    if options.path_from_source is None:
      parser.error('Path from source needed for %s' % FILE_TYPE_POSIX_STUB)

  # Get the names for the output directory and intermdiate directory.
  out_dir = RemoveTrailingSlashes(options.out_dir)
  intermediate_dir = RemoveTrailingSlashes(options.intermediate_dir)
  if intermediate_dir is None:
    intermediate_dir = out_dir

  # Make sure the directories exists.
  if not os.path.exists(out_dir):
    os.makedirs(out_dir)
  if not os.path.exists(intermediate_dir):
    os.makedirs(intermediate_dir)

  if options.type == FILE_TYPE_WIN:
    for input_path in args:
      infile = None
      try:
        infile = open(input_path, 'r')
        signatures = ParseSignatures(infile)
        module_name = ExtractModuleName(os.path.basename(input_path))
        WindowsLibCreator(module_name, signatures, intermediate_dir, out_dir).CreateLib()
      finally:
        if infile is not None:
          infile.close()
  elif options.type == FILE_TYPE_POSIX_STUB:
    # Find the paths to the output files.
    header_path = PosixStubWriter.HeaderFilePath(options.stubfile_name,
                                                 out_dir)
    impl_path = PosixStubWriter.ImplementationFilePath(options.stubfile_name,
                                                       intermediate_dir)

    # Generate some convenience variables for bits of data needed below.
    module_names = [ExtractModuleName(path) for path in args]
    namespace = options.path_from_source.replace('/', '_').lower()
    header_guard = '%s__' % namespace.upper()
    header_include_path = '%s/%s' % (options.path_from_source,
                                     os.path.basename(header_path))

    # First create the implementation file.
    impl_file = None
    try:
      # Open the file, and create the preamble which consists of a file
      # header plus any necessary includes.
      impl_file = open(impl_path, 'w')
      PosixStubWriter.WriteImplementationPreamble(header_include_path,
                                                  impl_file)
      if options.extra_stub_header is not None:
        extra_header_file = None
        try:
          impl_file.write('\n')
          extra_header_file = open(options.extra_stub_header, 'r')
          for line in extra_header_file:
            impl_file.write(line)
          impl_file.write('\n')
        finally:
          if extra_header_file is not None:
            extra_header_file.close()

      # For each signature file, generate the stub population functions
      # for that file.  Each file represents one module.
      for input_path in args:
        name = ExtractModuleName(input_path)
        infile = None
        try:
          infile = open(input_path, 'r')
          signatures = ParseSignatures(infile)
        finally:
          if infile is not None:
            infile.close()
        writer = PosixStubWriter(name, signatures)
        writer.WriteImplementationContents(namespace, impl_file)

      # Lastly, output the umbrella function for the file.
      PosixStubWriter.WriteUmbrellaInitializer(module_names, namespace,
                                               impl_file)
    finally:
      if impl_file is not None:
        impl_file.close()

    # Then create the associated header file.
    header_file = None
    try:
      header_file = open(header_path, 'w')
      PosixStubWriter.WriteHeaderContents(module_names, namespace,
                                          header_guard, header_file)
    finally:
      if header_file is not None:
        header_file.close()

  else:
    raise Error('Should not reach here')


if __name__ == '__main__':
  main()
