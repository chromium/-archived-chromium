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


"""O3D interface class generator.

This module generates the O3D interface classes.
"""


import os
import syntax_tree
import cpp_utils
import naming

class CircularDefinition(Exception):
  """Raised when a circular type definition is found."""

  def __init__(self, type_defn):
    Exception.__init__(self)
    self.type = type_defn


class BadForwardDeclaration(Exception):
  """Raised when an impossible forward declaration is required."""


def ForwardDecl(section, type_defn):
  """Emits the forward declaration of a type, if possible.

  Inner types (declared inside a class) cannot be forward-declared.
  Only classes can be forward-declared.

  Args:
    section: the section to emit to.
    type_defn: the Definition for the type to forward-declare.

  Raises:
    BadForwardDeclaration: an inner type or a non-class was passed as an
      argument.
  """
  # inner types cannot be forward-declared
  if type_defn.parent.defn_type != 'Namespace':
    raise BadForwardDeclaration
  stack = type_defn.GetParentScopeStack()
  if type_defn.defn_type == 'Class':
    for scope in stack:
      if scope.name:
        section.PushNamespace(scope.name)
    section.EmitCode('class %s;' % type_defn.name)
    for scope in stack[::-1]:
      if scope.name:
        section.PopNamespace()
  else:
    raise BadForwardDeclaration


class O3DInterfaceGenerator(object):
  """Header generator class.

  This class takes care of the details of generating a C++ header file
  containing all the definitions from a syntax tree.

  It contains a list of functions named after each of the Definition classes in
  syntax_tree, with a common signature. The appropriate function will be called
  for each definition, to generate the code for that definition.
  """

  class GenerationContext(object):
    def __init__(self, header_scope, cpp_scope, header_section, cpp_section):
      self.header_scope = header_scope
      self.header_section = header_section
      self.cpp_scope = cpp_scope
      self.cpp_section = cpp_section
      self.needed_decl = set()
      self.needed_defn = set()
      self.emitted_defn = set()

    def Fork(self, header_scope, cpp_scope, header_section, cpp_section):
      new_context = type(self)(header_scope, cpp_scope, header_section,
                               cpp_section)
      new_context.needed_decl = self.needed_decl
      new_context.needed_defn = self.needed_defn
      new_context.emitted_defn = self.emitted_defn
      return new_context

    def CheckType(self, need_defn, type_defn):
      """Checks for the definition or declaration of a type.

      This function helps keeping track of which types are needed to be defined
      or declared in the C++ file before other definitions can happen. If the
      definition is needed (and is not in this C++ header file), the proper
      include will be generated. If the type only needs to be forward-declared,
      the forward declaration will be output (if the type is not otherwise
      defined).

      Args:
        need_defn: a boolean, True if the C++ definition of the type is needed,
          False if only the declaration is needed.
        type_defn: the Definition of the type to check.
      """
      while type_defn.defn_type == 'Array':
        # arrays are implicitly defined with their data type
        type_defn = type_defn.data_type
      if need_defn:
        if type_defn not in self.emitted_defn:
          self.needed_defn.add(type_defn)
      else:
        if type_defn in self.emitted_defn:
          return
        if type_defn.parent and type_defn.parent.defn_type != 'Namespace':
          # inner type: need the definition of the parent.
          self.CheckType(True, type_defn.parent)
        else:
          # only forward-declare classes.
          # typedefs could be forward-declared by emitting the definition again,
          # but this necessitates the source type to be forward-declared before.
          # TODO: see if it is possible to find a proper ordering that let
          # us forward-declare typedefs instead of needing to include the
          # definition.
          if type_defn.defn_type == 'Class':
            self.needed_decl.add(type_defn)
          else:
            self.needed_defn.add(type_defn)

  def __init__(self, output_dir, namespace):
    self._output_dir = output_dir
    self._void_type = namespace.LookupTypeRecursive('void')

  def GetHeaderFile(self, idl_file):
    return idl_file.source.split('.')[0] + '.h'

  def GetCppFile(self, idl_file):
    return idl_file.source.split('.')[0] + '.cc'

  def GetInterfaceInclude(self, type_defn):
    if self.NeedsGlue(type_defn):
      return self.GetHeaderFile(type_defn.source.file)
    else:
      return type_defn.GetDefinitionInclude()

  def GetImplementationInclude(self, type_defn):
    return type_defn.GetDefinitionInclude()

  def IsVoid(self, type_defn):
    return type_defn.GetFinalType() == self._void_type

  def NeedsGlue(self, obj):
    return obj.LookupBindingModel() == 'o3d' or 'glue_iface' in obj.attributes

  def GetSectionFromAttributes(self, parent_section, defn):
    """Gets the code section appropriate for a given definition.

    Classes have 3 definition sections: private, protected and public. This
    function will pick one of the sections, based on the attributes of the
    definition, if its parent is a class. For other scopes (namespaces) it will
    return the parent scope main section.

    Args:
      parent_section: the main section for the parent scope.
      defn: the definition.

    Returns:
      the appropriate section.
    """
    if defn.parent and defn.parent.defn_type == 'Class':
      if 'private' in defn.attributes:
        return parent_section.GetSection('private:') or parent_section
      elif 'protected' in defn.attributes:
        return parent_section.GetSection('protected:') or parent_section
      else:
        return parent_section.GetSection('public:') or parent_section
    else:
      return parent_section

  def Verbatim(self, context, obj):
    """Generates the code for a Verbatim definition.

    Verbatim definitions being written for a particular type of output file,
    this function will check the 'verbatim' attribute, and only emit the
    verbatim code if it is 'cpp_header'.

    Args:
      parent_section: the main section of the parent scope.
      obj: the Verbatim definition.
    """
    try:
      verbatim_attr = obj.attributes['verbatim']
    except KeyError:
      source = obj.source
      print ('%s:%d ignoring verbatim with no verbatim attribute' %
             (source.file.source, source.line))
      return
    if verbatim_attr == 'o3d_iface_header':
      section = self.GetSectionFromAttributes(context.header_section, obj)
      section.EmitCode(obj.text)
    elif verbatim_attr == 'o3d_iface_cpp':
      context.cpp_section.EmitCode(obj.text)

  def Typedef(self, context, obj):
    """Generates the code for a Typedef definition.

    Args:
      parent_section: the main section of the parent scope.
      obj: the Typedef definition.

    Returns:
      a list of (boolean, Definition) pairs, of all the types that need
      to be declared (boolean is False) or defined (boolean is True) before
      this definition.
    """
    section = self.GetSectionFromAttributes(context.header_section, obj)
    bm = obj.type.binding_model
    type_string, unused_need_defn = bm.CppTypedefString(context.header_scope,
                                                        obj.type)
    context.CheckType(True, obj.type)
    section.EmitCode('typedef %s %s;' % (type_string, obj.name))

  def Variable(self, context, obj):
    """Generates the code for a Variable definition.

    This function will generate the member/global variable declaration, as well
    as the setter/getter functions if specified in the attributes.

    Args:
      parent_section: the main section of the parent scope.
      obj: the Variable definition.
    """
    bm = obj.type.binding_model
    type_string, need_defn = bm.CppMemberString(context.header_scope, obj.type)
    context.CheckType(need_defn, obj.type)
    need_glue = self.NeedsGlue(obj) or (obj.parent.is_type and
                                        self.NeedsGlue(obj.parent));

    getter_attributes = {}
    if 'static' in obj.attributes:
      getter_attributes['static'] = obj.attributes['static']
      static = 'static '
    else:
      static = ''
    for attr in ['public', 'protected', 'private']:
      if attr in obj.attributes:
        getter_attributes[attr] = obj.attributes[attr]

    if not need_glue:
      if obj.parent.defn_type == 'Class':
        if 'field_access' in obj.attributes:
          member_section = context.header_section.GetSection(
              obj.attributes['field_access'] + ':')
        else:
          member_section = context.header_section.GetSection('private:')
      else:
        member_section = context.header_section
      field_name = naming.Normalize(obj.name, naming.LowerTrailing)
      member_section.EmitCode('%s%s %s;' % (static, type_string, field_name))

    if 'getter' in obj.attributes:
      func = obj.MakeGetter(getter_attributes, cpp_utils.GetGetterName(obj))
      if need_glue:
        self.FunctionGlue(context, func)
        impl = None
      else:
        impl = ' { return %s; }' % field_name
      self.FunctionDecl(context, func, impl)
    if 'setter' in obj.attributes:
      func = obj.MakeSetter(getter_attributes, cpp_utils.GetSetterName(obj))
      if need_glue:
        self.FunctionGlue(context, func)
        impl = None
      else:
        impl = ' { %s = %s; }' % (field_name, obj.name)
      self.FunctionDecl(context, func, impl)

  def GetParamsDecls(self, scope, obj, context=None):
    param_strings = []
    for p in obj.params:
      bm = p.type.binding_model
      if p.mutable:
        text, need_defn = bm.CppMutableParameterString(scope, p.type)
      else:
        text, need_defn = bm.CppParameterString(scope, p.type)
      if context:
        context.CheckType(need_defn, p.type)
      param_strings += ['%s %s' % (text, p.name)]
    return ', '.join(param_strings)

  def FunctionDecl(self, context, obj, impl_string=None):
    section = self.GetSectionFromAttributes(context.header_section, obj)
    if not impl_string:
      impl_string = ';'
    params_string = self.GetParamsDecls(context.header_scope, obj, context)
    prefix_strings = []
    suffix_strings = []
    for attrib in ['static', 'virtual', 'inline']:
      if attrib in obj.attributes:
        prefix_strings.append(attrib)
    if prefix_strings:
      prefix_strings.append('')
    if 'const' in obj.attributes:
      suffix_strings.append('const')
    if 'pure' in obj.attributes:
      suffix_strings.append('= 0')
    if suffix_strings:
      suffix_strings.insert(0, '')
    prefix = ' '.join(prefix_strings)
    suffix = ' '.join(suffix_strings)
    if obj.type:
      bm = obj.type.binding_model
      return_type, need_defn = bm.CppReturnValueString(context.header_scope,
                                                       obj.type)
      context.CheckType(need_defn, obj.type)
      section.EmitCode('%s%s %s(%s)%s%s' % (prefix, return_type, obj.name,
                                            params_string, suffix, impl_string))
    else:
      section.EmitCode('%s%s(%s)%s%s' % (prefix, obj.name, params_string,
                                         suffix, impl_string))

  def FunctionGlue(self, context, obj):
    if not obj.type:
      # TODO autogen a factory
      return
    if 'pure' in obj.attributes:
      return

    if obj.parent.is_type:
      func_name = '%s::%s' % (obj.parent.name, obj.name)
      if 'static' in obj.attributes:
        call_prefix = 'impl::' + func_name
      else:
        # this_call
        if self.NeedsGlue(obj.parent):
          call_prefix = 'GetImpl()->'
        else:
          call_prefix = ''
    else:
      call_prefix = ''
      func_name = obj.name

    params_string = self.GetParamsDecls(context.cpp_scope, obj)
    param_exprs = []
    for p in obj.params:
      if self.NeedsGlue(p.type):
        param_exprs.append('%s->GetImpl()' % p.name)
      else:
        param_exprs.append(p.name)

    if not self.IsVoid(obj.type):
      return_prefix = 'return '
      if self.NeedsGlue(obj.type):
        return_suffix = '->GetIface()'
      else:
        return_suffix = ''
    else:
      return_prefix = ''
      return_suffix = ''
    
    bm = obj.type.binding_model
    return_type, unused = bm.CppReturnValueString(context.header_scope,
                                                  obj.type)
    if 'const' in obj.attributes:
      func_suffix = ' const'
    else:
      func_suffix = ''

    section = context.cpp_section
    section.EmitCode('%s %s(%s)%s {' % (return_type, func_name, params_string,
                                         func_suffix))
    section.EmitCode('%s%s%s(%s)%s;' % (return_prefix, call_prefix, obj.name,
                                        ', '.join(param_exprs), return_suffix))
    section.EmitCode('}')

  def Function(self, context, obj):
    """Generates the code for a Function definition.

    Args:
      parent_section: the main section of the parent scope.
      obj: the Function definition.
    """
    self.FunctionDecl(context, obj)
    if self.NeedsGlue(obj) or (obj.parent.is_type and
                               self.NeedsGlue(obj.parent)):
      self.FunctionGlue(context, obj)

  def Class(self, context, obj):
    """Generates the code for a Class definition.

    This function will recursively generate the code for all the definitions
    inside the class. These definitions will be output into one of 3 sections
    (private, protected, public), depending on their attributes. These
    individual sections will only be output if they are not empty.

    Args:
      parent_section: the main section of the parent scope.
      obj: the Class definition.
    """
    h_section = self.GetSectionFromAttributes(context.header_section,
                                              obj).CreateSection(obj.name)
    c_section = context.cpp_section

    need_glue = self.NeedsGlue(obj)
    if need_glue:
      h_section.PushNamespace('impl')
      h_section.EmitCode('class %s;' % obj.name)
      h_section.PopNamespace()
      h_section.EmitCode('')
    if obj.base_type:
      bm = obj.base_type.binding_model
      h_section.EmitCode('class %s : public %s {' %
                       (obj.name, bm.CppBaseClassString(context.header_scope,
                                                        obj.base_type)))
      context.CheckType(True, obj.base_type)
    else:
      h_section.EmitCode('class %s {' % obj.name)
    public_section = h_section.CreateSection('public:')
    protected_section = h_section.CreateSection('protected:')
    private_section = h_section.CreateSection('private:')

    new_context = context.Fork(obj, context.cpp_scope, h_section, c_section)
    self.DefinitionList(new_context, obj.defn_list)

    if need_glue:
      public_section.EmitCode('impl::%s *GetImpl();' % obj.name)
      c_section.EmitCode('impl::%s *%s::GetImpl() {' % (obj.name, obj.name))
      c_section.EmitCode('return static_cast<impl::%s *>(impl_);' % obj.name)
      c_section.EmitCode('}')

    if not public_section.IsEmpty():
      public_section.AddPrefix('public:')
    if not protected_section.IsEmpty():
      protected_section.AddPrefix('protected:')
    if not private_section.IsEmpty():
      private_section.AddPrefix('private:')
    h_section.EmitCode('};')

  def Namespace(self, context, obj):
    """Generates the code for a Namespace definition.

    This function will recursively generate the code for all the definitions
    inside the namespace.

    Args:
      parent_section: the main section of the parent scope.
      obj: the Namespace definition.
    """
    context.header_section.PushNamespace(obj.name)
    context.cpp_section.PushNamespace(obj.name)
    new_context = context.Fork(obj, obj, context.header_section,
                               context.cpp_section)
    self.DefinitionList(new_context, obj.defn_list)
    context.header_section.PopNamespace()
    context.cpp_section.PopNamespace()

  def Typename(self, context, obj):
    """Generates the code for a Typename definition.

    Since typenames (undefined types) cannot be expressed in C++, this function
    will not output code. The definition may be output with a verbatim section.

    Args:
      parent_section: the main section of the parent scope.
      scope: the parent scope.
      obj: the Typename definition.
    """

  def Enum(self, context, obj):
    """Generates the code for an Enum definition.

    Args:
      parent_section: the main section of the parent scope.
      scope: the parent scope.
      obj: the Enum definition.
    """
    section = self.GetSectionFromAttributes(context.header_section, obj)
    section.EmitCode('enum %s {' % obj.name)
    for value in obj.values:
      if value.value is None:
        section.EmitCode('%s,' % value.name)
      else:
        section.EmitCode('%s = %s,' % (value.name, value.value))
    section.EmitCode('};')

  def DefinitionList(self, context, defn_list):
    """Generates the code for all the definitions in a list.

    Args:
      parent_section: the main section of the parent scope.
      scope: the parent scope.
      defn_list: the list of definitions.
    """
    for obj in defn_list:
      context.emitted_defn.add(obj)
      # array types are implicitly defined
      for k in obj.array_defns:
        context.emitted_defn.add(obj.array_defns[k])
      getattr(self, obj.defn_type)(context, obj)

  def Generate(self, idl_file, namespace, defn_list):
    """Generates the header file.

    Args:
      idl_file: the source IDL file containing the definitions, as a
        idl_parser.File instance.
      namespace: a Definition for the global namespace.
      defn_list: the list of top-level definitions.

    Returns:
      a cpp_utils.CppFileWriter that contains the C++ header file code.

    Raises:
      CircularDefinition: circular definitions were found in the file.
    """
    all_defn = syntax_tree.GetObjectsRecursive(defn_list)
    need_glue = False
    for defn in all_defn:
      if self.NeedsGlue(defn):
        need_glue = True
        break
    if not need_glue:
      return []

    header_writer = cpp_utils.CppFileWriter(
        '%s/%s' % (self._output_dir, self.GetHeaderFile(idl_file)), True)

    cpp_writer = cpp_utils.CppFileWriter(
        '%s/%s' % (self._output_dir, self.GetCppFile(idl_file)), True)

    h_decl_section = header_writer.CreateSection('decls')
    h_code_section = header_writer.CreateSection('defns')
    c_code_section = cpp_writer.CreateSection('glue')

    context = self.GenerationContext(namespace, namespace, h_code_section,
                                     c_code_section)

    self.DefinitionList(context, defn_list)

    context.needed_decl -= context.needed_defn
    if context.needed_decl:
      for type_defn in context.needed_decl:
        # TODO: sort by namespace so that we don't open and close them
        # more than necessary.
        ForwardDecl(h_decl_section, type_defn)
      h_decl_section.EmitCode('')

    for type_defn in context.needed_defn:
      if type_defn.source.file == idl_file:
        raise CircularDefinition(type_defn)

    h_includes = set(self.GetInterfaceInclude(type_defn)
                     for type_defn in context.needed_defn)
    c_includes = set(self.GetImplementationInclude(type_defn)
                     for type_defn in context.emitted_defn
                     if self.NeedsGlue(type_defn))
    c_includes.add(self.GetHeaderFile(idl_file))

    for include_file in h_includes:
      if include_file is not None:
        header_writer.AddInclude(include_file)
    for include_file in c_includes:
      if include_file is not None:
        cpp_writer.AddInclude(include_file)
    return [header_writer, cpp_writer]


def ProcessFiles(output_dir, pairs, namespace):
  """Generates the headers for all input files.

  Args:
    output_dir: the output directory.
    pairs: a list of (idl_parser.File, syntax_tree.Definition list) describing
      the list of top-level definitions in each source file.
    namespace: a syntax_tree.Namespace for the global namespace.

  Returns:
    a list of cpp_utils.CppFileWriter, one for each output file.
  """
  output_dir = output_dir + '/iface'
  if not os.access(output_dir + '/', os.F_OK):
    os.makedirs(output_dir)

  generator = O3DInterfaceGenerator(output_dir, namespace)
  writer_list = []
  for (f, defn) in pairs:
    writer_list += generator.Generate(f, namespace, defn)
  return writer_list


def main():
  pass

if __name__ == '__main__':
  main()
