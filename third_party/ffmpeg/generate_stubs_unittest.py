#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittest for the generate_stubs.py.

Since generate_stubs.py is a code generator, it is hard to do a very good
test.  Instead of creating a golden-file test, which might be flakey, this
test elects instead to verify that various components "exist" within the
generated file as a sanity check.  In particular, there is a simple hit
test to make sure that umbrella functions, etc., do try and include every
function they are responsible for invoking.  Missing an invocation is quite
easily missed.

There is no attempt to verify ordering of different components, or whether
or not those components are going to parse incorrectly because of prior
errors or positioning.  Most of that should be caught really fast anyways
during any attempt to use a badly behaving script.
"""

import re
import StringIO
import unittest
import generate_stubs as gs


def _MakeSignature(return_type, name, params):
  return {'return_type': return_type,
          'name': name,
          'params': params}


SIMPLE_SIGNATURES = [
    ('int foo(int a)', _MakeSignature('int', 'foo', ['int a'])),
    ('int bar(int a, double b)', _MakeSignature('int', 'bar',
                                                ['int a', 'double b'])),
    ('int baz(void)', _MakeSignature('int', 'baz', ['void'])),
    ('void quux(void)', _MakeSignature('void', 'quux', ['void'])),
    ('void waldo(void);', _MakeSignature('void', 'waldo', ['void'])),
    ('int corge(void);', _MakeSignature('int', 'corge', ['void'])),
    ]

TRICKY_SIGNATURES = [
    ('const struct name *foo(int a, struct Test* b);  ',
     _MakeSignature('const struct name *',
                    'foo',
                    ['int a', 'struct Test* b'])),
    ('const struct name &foo(int a, struct Test* b);',
     _MakeSignature('const struct name &',
                    'foo',
                    ['int a', 'struct Test* b'])),
    ('const struct name &_foo(int a, struct Test* b);',
     _MakeSignature('const struct name &',
                    '_foo',
                    ['int a', 'struct Test* b'])),
    ('struct name const * const _foo(int a, struct Test* b) '
     '__attribute__((inline));',
     _MakeSignature('struct name const * const',
                    '_foo',
                    ['int a', 'struct Test* b']))
    ]

INVALID_SIGNATURES = ['I am bad', 'Seriously bad(', ';;;']


class GenerateStubModuleFunctionsUnittest(unittest.TestCase):
  def testRemoveTrailingSlashes(self):
    # Check simple cases.
    self.assertEqual('/a/path', gs.RemoveTrailingSlashes('/a/path/'))
    self.assertEqual('/a/path', gs.RemoveTrailingSlashes('/a/path///'))
    self.assertEqual('/a/path', gs.RemoveTrailingSlashes('/a/path'))

    # Should not remove the last slash.
    self.assertEqual('/', gs.RemoveTrailingSlashes('/'))

  def testExtractModuleName(self):
    self.assertEqual('somefile-2', gs.ExtractModuleName('somefile-2.ext'))

  def testParseSignatures_EmptyFile(self):
    # Empty file just generates empty signatures.
    infile = StringIO.StringIO()
    signatures = gs.ParseSignatures(infile)
    self.assertEqual(0, len(signatures))

  def testParseSignatures_SimpleSignatures(self):
    file_contents = '\n'.join([x[0] for x in SIMPLE_SIGNATURES])
    infile = StringIO.StringIO(file_contents)
    signatures = gs.ParseSignatures(infile)
    self.assertEqual(len(SIMPLE_SIGNATURES), len(signatures))

    # We assume signatures are in order.
    for i in xrange(len(SIMPLE_SIGNATURES)):
      self.assertEqual(SIMPLE_SIGNATURES[i][1], signatures[i],
                       msg='Expected %s\nActual %s\nFor %s' %
                       (SIMPLE_SIGNATURES[i][1],
                        signatures[i],
                        SIMPLE_SIGNATURES[i][0]))

  def testParseSignatures_TrickySignatures(self):
    file_contents = '\n'.join([x[0] for x in TRICKY_SIGNATURES])
    infile = StringIO.StringIO(file_contents)
    signatures = gs.ParseSignatures(infile)
    self.assertEqual(len(TRICKY_SIGNATURES), len(signatures))

    # We assume signatures are in order.
    for i in xrange(len(TRICKY_SIGNATURES)):
      self.assertEqual(TRICKY_SIGNATURES[i][1], signatures[i],
                       msg='Expected %s\nActual %s\nFor %s' %
                       (TRICKY_SIGNATURES[i][1],
                        signatures[i],
                        TRICKY_SIGNATURES[i][0]))

  def testParseSignatures_InvalidSignatures(self):
    for i in INVALID_SIGNATURES:
      infile = StringIO.StringIO(i)
      self.assertRaises(gs.BadSignatureError, gs.ParseSignatures, infile)

  def testParseSignatures_CommentsIgnored(self):
    my_sigs = []
    my_sigs.append('# a comment')
    my_sigs.append(SIMPLE_SIGNATURES[0][0])
    my_sigs.append('# another comment')
    my_sigs.append(SIMPLE_SIGNATURES[0][0])
    my_sigs.append('# a third comment')
    my_sigs.append(SIMPLE_SIGNATURES[0][0])

    file_contents = '\n'.join(my_sigs)
    infile = StringIO.StringIO(file_contents)
    signatures = gs.ParseSignatures(infile)
    self.assertEqual(3, len(signatures))


class WindowsLibCreatorUnittest(unittest.TestCase):
  def setUp(self):
    self.module_name = 'my_module-1'
    self.signatures = [sig[1] for sig in SIMPLE_SIGNATURES]
    self.out_dir = 'out_dir'
    self.intermediate_dir = 'intermediate_dir'
    self.creator = gs.WindowsLibCreator(self.module_name,
                                        self.signatures,
                                        self.intermediate_dir,
                                        self.out_dir)

  def testDefFilePath(self):
    self.assertEqual('intermediate_dir/my_module-1.def',
                     self.creator.DefFilePath())

  def testLibFilePath(self):
    self.assertEqual('out_dir/my_module-1.lib', self.creator.LibFilePath())

  def testWriteDefFile(self):
    outfile = StringIO.StringIO()
    self.creator._WriteDefFile(outfile)
    contents = outfile.getvalue()

    # Check that the file header is correct.
    self.assertTrue(contents.startswith("""LIBRARY %s
EXPORTS
""" % self.module_name))

    # Check that the signatures were exported.
    for sig in self.signatures:
      pattern = '\n  %s\n' % sig['name']
      self.assertTrue(re.search(pattern, contents),
                      msg='Expected match of "%s" in %s' % (pattern, contents))


class PosixStubWriterUnittest(unittest.TestCase):
  def setUp(self):
    self.module_name = 'my_module-1'
    self.signatures = [sig[1] for sig in SIMPLE_SIGNATURES]
    self.out_dir = 'out_dir'
    self.writer = gs.PosixStubWriter(self.module_name, self.signatures)

  def testEnumName(self):
    self.assertEqual('kModuleMy_module1',
                     gs.PosixStubWriter.EnumName(self.module_name))

  def testIsInitializedName(self):
    self.assertEqual('IsMy_module1Initialized',
                     gs.PosixStubWriter.IsInitializedName(self.module_name))

  def testInitializeModuleName(self):
    self.assertEqual(
        'InitializeMy_module1',
        gs.PosixStubWriter.InitializeModuleName(self.module_name))

  def testUninitializeModuleName(self):
    self.assertEqual(
        'UninitializeMy_module1',
        gs.PosixStubWriter.UninitializeModuleName(self.module_name))

  def testHeaderFilePath(self):
    self.assertEqual('outdir/mystubs-1.h',
                     gs.PosixStubWriter.HeaderFilePath('mystubs-1', 'outdir'))

  def testImplementationFilePath(self):
    self.assertEqual(
        'outdir/mystubs-1.cc',
        gs.PosixStubWriter.ImplementationFilePath('mystubs-1', 'outdir'))

  def testStubFunctionPointer(self):
    self.assertEqual(
        'static int (*foo_ptr)(int a) = NULL;',
        gs.PosixStubWriter.StubFunctionPointer(SIMPLE_SIGNATURES[0][1]))

  def testStubFunction(self):
    # Test for a signature with a return value and a parameter.
    self.assertEqual("""int foo(int a) {
  return foo_ptr(a);
}""", gs.PosixStubWriter.StubFunction(SIMPLE_SIGNATURES[0][1]))

    # Test for a signature with a void return value and no parameters.
    self.assertEqual("""void waldo(void) {
  waldo_ptr();
}""", gs.PosixStubWriter.StubFunction(SIMPLE_SIGNATURES[4][1]))

  def testWriteImplemenationContents(self):
    outfile = StringIO.StringIO()
    self.writer.WriteImplementationContents('my_namespace', outfile)
    contents = outfile.getvalue()

    # Verify namespace exists somewhere.
    self.assertTrue(contents.find('namespace my_namespace {') != -1)

    # Verify that each signature has an _ptr and a function call in the file.
    # Check that the signatures were exported.
    for sig in self.signatures:
      decl = gs.PosixStubWriter.StubFunctionPointer(sig)
      self.assertTrue(contents.find(decl) != -1,
                      msg='Expected "%s" in %s' % (decl, contents))

    # Verify that each signature has an stub function generated for it.
    for sig in self.signatures:
      decl = gs.PosixStubWriter.StubFunction(sig)
      self.assertTrue(contents.find(decl) != -1,
                      msg='Expected "%s" in %s' % (decl, contents))

    # Find module initializer functions.  Make sure all 3 exist.
    decl = gs.PosixStubWriter.InitializeModuleName(self.module_name)
    self.assertTrue(contents.find(decl) != -1,
                    msg='Expected "%s" in %s' % (decl, contents))
    decl = gs.PosixStubWriter.UninitializeModuleName(self.module_name)
    self.assertTrue(contents.find(decl) != -1,
                    msg='Expected "%s" in %s' % (decl, contents))
    decl = gs.PosixStubWriter.IsInitializedName(self.module_name)
    self.assertTrue(contents.find(decl) != -1,
                    msg='Expected "%s" in %s' % (decl, contents))

  def testWriteHeaderContents(self):
    # Data for header generation.
    module_names = ['oneModule', 'twoModule']

    # Make the header.
    outfile = StringIO.StringIO()
    self.writer.WriteHeaderContents(module_names, 'my_namespace', 'GUARD_',
                                    outfile)
    contents = outfile.getvalue()

    # Check for namespace and header guard.
    self.assertTrue(contents.find('namespace my_namespace {') != -1)
    self.assertTrue(contents.find('#ifndef GUARD_') != -1)

    # Check for umbrella initializer.
    self.assertTrue(contents.find('InitializeStubs(') != -1)

    # Check per-module declarations.
    for name in module_names:
      # Check for enums.
      decl = gs.PosixStubWriter.EnumName(name)
      self.assertTrue(contents.find(decl) != -1,
                      msg='Expected "%s" in %s' % (decl, contents))

      # Check for module initializer functions.
      decl = gs.PosixStubWriter.IsInitializedName(name)
      self.assertTrue(contents.find(decl) != -1,
                      msg='Expected "%s" in %s' % (decl, contents))
      decl = gs.PosixStubWriter.InitializeModuleName(name)
      self.assertTrue(contents.find(decl) != -1,
                      msg='Expected "%s" in %s' % (decl, contents))
      decl = gs.PosixStubWriter.UninitializeModuleName(name)
      self.assertTrue(contents.find(decl) != -1,
                      msg='Expected "%s" in %s' % (decl, contents))

  def testWriteUmbrellaInitializer(self):
    # Data for header generation.
    module_names = ['oneModule', 'twoModule']

    # Make the header.
    outfile = StringIO.StringIO()
    self.writer.WriteUmbrellaInitializer(module_names, 'my_namespace', outfile)
    contents = outfile.getvalue()

    # Check for umbrella initializer declaration.
    self.assertTrue(contents.find('bool InitializeStubs(') != -1)

    # If the umbrella initializer is correctly written, each module will have
    # its initializer called, checked, and uninitialized on failure.  Sanity
    # check that here.
    for name in module_names:
      # Check for module initializer functions.
      decl = gs.PosixStubWriter.IsInitializedName(name)
      self.assertTrue(contents.find(decl) != -1,
                      msg='Expected "%s" in %s' % (decl, contents))
      decl = gs.PosixStubWriter.InitializeModuleName(name)
      self.assertTrue(contents.find(decl) != -1,
                      msg='Expected "%s" in %s' % (decl, contents))
      decl = gs.PosixStubWriter.UninitializeModuleName(name)
      self.assertTrue(contents.find(decl) != -1,
                      msg='Expected "%s" in %s' % (decl, contents))

if __name__ == '__main__':
  unittest.main()
