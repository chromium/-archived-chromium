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


"""Docbuilder for O3D and o3djs."""


import os
import os.path
import sys
import imp
import types
import glob
import subprocess

_java_exe = ''
_script_path = os.path.dirname(os.path.realpath(__file__))

GlobalsDict = { }


def MakePath(file_path):
  """Makes a path absolute given a path relativel to this script."""
  return os.path.join(_script_path, file_path)


def UpdateGlobals(dict):
  """Copies pairs from dict into GlobalDict."""
  for i, v in dict.items():
    GlobalsDict.__setitem__(i, v)


def GetCallingNamespaces():
  """Return the locals and globals for the function that called
  into this module in the current call stack."""
  try: 1/0
  except ZeroDivisionError:
    # Don't start iterating with the current stack-frame to
    # prevent creating reference cycles (f_back is safe).
    frame = sys.exc_info()[2].tb_frame.f_back

  # Find the first frame that *isn't* from this file
  while frame.f_globals.get("__name__") == __name__:
    frame = frame.f_back

  return frame.f_locals, frame.f_globals


def ComputeExports(exports):
  """Compute a dictionary of exports given one of the parameters
  to the Export() function or the exports argument to SConscript()."""

  loc, glob = GetCallingNamespaces()

  retval = {}
  try:
    for export in exports:
      if isinstance(export, types.DictType):
        retval.update(export)
      else:
        try:
          retval[export] = loc[export]
        except KeyError:
          retval[export] = glob[export]
  except KeyError, x:
    raise Error, "Export of non-existent variable '%s'"%x

  return retval


def Export(*vars):
  """Copies the named variables to GlobalDict."""
  for var in vars:
    UpdateGlobals(ComputeExports(vars))


def Import(filename):
  """Imports a python file in a scope with 'Export' defined."""
  scope = {'__builtins__': globals()['__builtins__'],
           'Export': Export}
  file = open(filename, 'r')
  exec file in scope
  file.close()


def Execute(args):
  """Executes an external program."""
  # Comment the next line in for debugging.
  # print "Execute: ", ' '.join(args)
  if subprocess.call(args) > 0:
    raise RuntimeError('FAILED: ' + ' '.join(args))


def AppendBasePath(folder, filenames):
  """Appends a base path to a ist of files"""
  return [os.path.join(folder, filename) for filename in filenames]


def RunNixysa(idl_files, generate, output_dir, nixysa_options):
  """Executes Nixysa."""
  python_exe = 'python'
  Execute([
    python_exe,
    MakePath('../third_party/nixysa/files/codegen.py'),
    '--binding-module=o3d:%s' % MakePath('../plugin/o3d_binding.py'),
    '--generate=' + generate,
    '--force',
    '--output-dir=' + output_dir] +
    nixysa_options +
    idl_files)


def RunJSDocToolkit(js_files, output_dir, prefix):
  """Executes the JSDocToolkit."""
  list_filename = MakePath('../scons-out/docs/obj/doclist.conf')
  f = open(list_filename, 'w')
  f.write('{\n_: [\n')
  for filename in js_files:
    f.write('"%s",\n' % filename.replace('\\', '/'))
  f.write(']\n}\n')
  f.close()

  Execute([
    _java_exe,
    '-Djsdoc.dir=%s' % MakePath('../third_party/jsdoctoolkit/files'),
    '-jar',
    MakePath('../third_party/jsdoctoolkit/files/jsrun.jar'),
    MakePath('../third_party/jsdoctoolkit/files/app/run.js'),
    '-D="prefix:%s"' % prefix,
    '-v',
    '-t=%s' % MakePath('./jsdoc-toolkit-templates//'),
    '-d=' + output_dir,
    '-c=' + list_filename])


def BuildJavaScriptForDocsFromIDLs(idl_files, output_dir):
  RunNixysa(idl_files, 'jsheader', output_dir, [])


def BuildJavaScriptForExternsFromIDLs(idl_files, output_dir):
  if (os.path.exists(output_dir)):
    for filename in glob.glob(os.path.join(output_dir, '*.js')):
      os.unlink(filename)
  RunNixysa(idl_files, 'jsheader', output_dir, ['--no-return-docs'])


def BuildO3DDocsFromJavaScript(js_files, output_dir):
  RunJSDocToolkit(js_files, output_dir, 'classo3d_1_1_')


def BuildO3DExternsFile(js_files_dir, extra_externs_file, externs_file):
  outfile = open(externs_file, 'w')
  filenames = (glob.glob(os.path.join(js_files_dir, '*.js')) +
               [extra_externs_file])
  for filename in filenames:
    infile = open(filename, 'r')
    outfile.write(infile.read())
    infile.close()
  outfile.close()


def BuildCompiledO3DJS(o3djs_files,
                       externs_path,
                       o3d_externs_js_path,
                       compiled_o3djs_outpath):
  Execute([
    _java_exe,
    '-jar',
    MakePath('JSCompiler_deploy.jar'),
    '--externs=%s' % externs_path,
    ('--externs=%s' % o3d_externs_js_path),
    ('--js_output_file=%s' % compiled_o3djs_outpath)] +
    ['-js=%s' % (x, ) for x in o3djs_files]);


def main():
  """Builds the O3D API docs and externs and the o3djs docs."""
  global _java_exe
  _java_exe = sys.argv[1]

  js_list_filename = MakePath('../samples/o3djs/js_list.scons')
  idl_list_filename = MakePath('../plugin/idl_list.scons')
  js_list_basepath = os.path.dirname(js_list_filename)
  idl_list_basepath = os.path.dirname(idl_list_filename)

  docs_js_outpath = MakePath('../scons-out/docs/obj/documentation/apijs')
  externs_js_outpath = MakePath('../scons-out/docs/obj/externs')
  o3d_docs_html_outpath = MakePath('../scons-out/docs/obj/documentation/html')
  o3d_externs_path = MakePath('../scons-out/docs/obj/o3d-externs.js')
  compiled_o3djs_outpath = MakePath(
      '../scons-out/docs/obj/documentation/base.js')
  externs_path = MakePath('externs/externs.js')
  o3d_extra_externs_path = MakePath('externs/o3d-extra-externs.js')

  Import(js_list_filename)
  Import(idl_list_filename)

  idl_files = AppendBasePath(idl_list_basepath, GlobalsDict['O3D_IDL_SOURCES'])
  o3djs_files = AppendBasePath(js_list_basepath, GlobalsDict['O3D_JS_SOURCES'])

  # we need to put base.js first?
  o3djs_files = (
      filter(lambda x: x.endswith('base.js'), o3djs_files) +
      filter(lambda x: not x.endswith('base.js'), o3djs_files))

  docs_js_files = [os.path.join(
                       docs_js_outpath,
                       os.path.splitext(os.path.basename(f))[0] + '.js')
                   for f in GlobalsDict['O3D_IDL_SOURCES']]

  BuildJavaScriptForDocsFromIDLs(idl_files, docs_js_outpath)
  BuildO3DDocsFromJavaScript([o3d_extra_externs_path] + docs_js_files,
                             o3d_docs_html_outpath)
  BuildJavaScriptForExternsFromIDLs(idl_files, externs_js_outpath)
  BuildO3DExternsFile(externs_js_outpath,
                      o3d_extra_externs_path,
                      o3d_externs_path)
  BuildCompiledO3DJS(o3djs_files,
                     externs_path,
                     o3d_externs_path,
                     compiled_o3djs_outpath)


if __name__ == '__main__':
  main()

