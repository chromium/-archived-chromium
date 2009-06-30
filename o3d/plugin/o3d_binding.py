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

"""o3d binding model module.

This module implements the glue functions for the o3d binding model, binding
O3D objects.

In C++, objects using this binding model are passed and returned by pointer.
For example
void SetValue(Class *value);
Class *GetValue();

For JS bindings, the browser object holds an id, representing the C++ object
through that can be accessed through the Client object.
"""


import string

import cpp_utils
import java_utils


class CallingConstructor(Exception):
  """Raised when trying to call a constructor on an O3D object."""
  pass


def JavaMemberString(scope, type_defn):
  """Gets the representation of a member name in Java.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition for the type.

  Returns:
    a string representing the type
  """
  return java_utils.GetScopedName(scope, type_defn)


def CppTypedefString(scope, type_defn):
  """Gets the representation of a type when used in a C++ typedef.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition for the type.

  Returns:
    a (string, boolean) pair, the first element being the representation of
    the type, the second element indicating whether or not the definition of
    the type is needed for the expression to be valid.
  """
  return cpp_utils.GetScopedName(scope, type_defn), False


def CppMemberString(scope, type_defn):
  """Gets the representation of a type when used as a C++ class member.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition for the type.

  Returns:
    a (string, boolean) pair, the first element being the representation of
    the type, the second element indicating whether or not the definition of
    the type is needed for the expression to be valid.
  """
  return '%s*' % cpp_utils.GetScopedName(scope, type_defn), False


def CppReturnValueString(scope, type_defn):
  """Gets the representation of a type when used as a C++ function return value.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition for the type.

  Returns:
    a (string, boolean) pair, the first element being the representation of
    the type, the second element indicating whether or not the definition of
    the type is needed for the expression to be valid.
  """
  return '%s*' % cpp_utils.GetScopedName(scope, type_defn), False


def CppParameterString(scope, type_defn):
  """Gets the representation of a type when used for a function parameter.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition for the type.

  Returns:
    a (string, boolean) pair, the first element being the representation of
    the type, the second element indicating whether or not the definition of
    the type is needed for the expression to be valid.
  """
  return '%s*' % cpp_utils.GetScopedName(scope, type_defn), False


def CppMutableParameterString(scope, type_defn):
  """Gets the representation of a type for a mutable function parameter.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition for the type.

  Returns:
    a (string, boolean) pair, the first element being the string representing
    the type, the second element indicating whether or not the definition of
    the type is needed for the expression to be valid.
  """
  return '%s*' % cpp_utils.GetScopedName(scope, type_defn), False


def CppMutableToNonMutable(scope, type_defn, expr):
  """Gets the string converting a mutable expression to a non-mutable one.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition for the type.
    expr: a string for the mutable expression.

  Returns:
    a string, which is the non-mutable expression.
  """
  (scope, type_defn) = (scope, type_defn)  # silence gpylint.
  return expr


def CppBaseClassString(scope, type_defn):
  """Gets the representation of a type for a base class.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition for the type.

  Returns:
    a (string, boolean) pair, the first element being the string representing
    the type, the second element indicating whether or not the definition of
    the type is needed for the expression to be valid.
  """
  return cpp_utils.GetScopedName(scope, type_defn)


def CppCallMethod(scope, type_defn, object_expr, mutable, method, param_exprs):
  """Gets the representation of a member function call.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition, representing the type of the object being called.
    object_expr: a string, which is the expression for the object being called.
    mutable: a boolean, whether or not the 'object_expr' expression is mutable
      or not
    method: a Function, representing the function to call.
    param_exprs: a list of strings, each being the expression for the value of
      each parameter.

  Returns:
    a string, which is the expression for the function call.
  """
  (scope, type_defn, mutable) = (scope, type_defn, mutable)  # silence gpylint.
  return '%s->%s(%s)' % (object_expr, method.name, ', '.join(param_exprs))


def CppCallStaticMethod(scope, type_defn, method, param_exprs):
  """Gets the representation of a static function call.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition, representing the type of the object being called.
    method: a Function, representing the function to call.
    param_exprs: a list of strings, each being the expression for the value of
      each parameter.

  Returns:
    a string, which is the expression for the function call.
  """
  return '%s::%s(%s)' % (cpp_utils.GetScopedName(scope, type_defn),
                         method.name, ', '.join(param_exprs))


def CppCallConstructor(scope, type_defn, method, param_exprs):
  """Gets the representation of a constructor call.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition, representing the type of the object being called.
    method: a Function, representing the constructor to call.
    param_exprs: a list of strings, each being the expression for the value of
      each parameter.

  Returns:
    a string, which is the expression for the constructor call.

  Raises:
    CallingConstructor: always. O3D objects can't be constructed directly.
  """
  raise CallingConstructor


def CppSetField(scope, type_defn, object_expr, field, param_expr):
  """Gets the representation of an expression setting a field in an object.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition, representing the type of the object containing the
      field being set.
    object_expr: a string, which is the expression for the object containing
      the field being set.
    field: a string, the name of the field to be set.
    param_expr: a strings, being the expression for the value to be set.

  Returns:
    a string, which is the expression for setting the field.
  """
  (scope, type_defn) = (scope, type_defn)  # silence gpylint.
  return '%s->%s(%s)' % (object_expr, cpp_utils.GetSetterName(field),
                         param_expr)


def CppGetField(scope, type_defn, object_expr, field):
  """Gets the representation of an expression getting a field in an object.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition, representing the type of the object containing the
      field being retrieved.
    object_expr: a string, which is the expression for the object containing
      the field being retrieved.
    field: a string, the name of the field to be retrieved.

  Returns:
    a string, which is the expression for getting the field.
  """
  (scope, type_defn) = (scope, type_defn)  # silence gpylint.
  return '%s->%s()' % (object_expr, cpp_utils.GetGetterName(field))


def CppSetStatic(scope, type_defn, field, param_expr):
  """Gets the representation of an expression setting a static field.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition, representing the type of the object containing the
      field being set.
    field: a string, the name of the field to be set.
    param_expr: a strings, being the expression for the value to be set.

  Returns:
    a string, which is the expression for setting the field.
  """
  return '%s::%s(%s)' % (cpp_utils.GetScopedName(scope, type_defn),
                         cpp_utils.GetSetterName(field), param_expr)


def CppGetStatic(scope, type_defn, field):
  """Gets the representation of an expression getting a static field.

  Args:
    scope: a Definition for the scope in which the expression will be written.
    type_defn: a Definition, representing the type of the object containing the
      field being retrieved.
    field: a string, the name of the field to be retrieved.

  Returns:
    a string, which is the expression for getting the field.
  """
  return '%s::%s()' % (cpp_utils.GetScopedName(scope, type_defn),
                       cpp_utils.GetGetterName(field))


def JSDocTypeString(type_defn):
  """Gets the representation of a type in JSDoc notation.

  Args:
    type_defn: a Definition for the type.

  Returns:
    a string that is the JSDoc notation of type_defn.
  """
  type_defn = type_defn.GetFinalType()
  type_stack = type_defn.GetParentScopeStack()
  name = type_defn.name
  return '!' + '.'.join([s.name for s in type_stack[1:]] + [name])


_binding_glue_header_template = string.Template('')


def NpapiBindingGlueHeader(scope, type_defn):
  """Gets the NPAPI glue header for a given type.

  Args:
    scope: a Definition for the scope in which the glue will be written.
    type_defn: a Definition, representing the type.

  Returns:
    a string, the glue header.
  """
  class_name = cpp_utils.GetScopedName(scope, type_defn)
  return _binding_glue_header_template.substitute(Class=class_name)


_binding_glue_cpp_template = string.Template("""
void InitializeGlue(NPP npp) {
  InitializeIds(npp);
  glue::_o3d::RegisterType(npp, ${Class}::GetApparentClass(), &npclass);
}

glue::_o3d::NPAPIObject *GetNPObject(NPP npp, ${Class}* object) {
  return glue::_o3d::GetNPObject(npp, object);
}

static NPObject *Allocate(NPP npp, NPClass *theClass) {
  return glue::_o3d::Allocate(npp, theClass);
}

static void Deallocate(NPObject *header) {
  return glue::_o3d::Deallocate(header);
}
""")


def NpapiBindingGlueCpp(scope, type_defn):
  """Gets the NPAPI glue implementation for a given type.

  Args:
    scope: a Definition for the scope in which the glue will be written.
    type_defn: a Definition, representing the type.

  Returns:
    a string, the glue implementation.
  """
  class_name = cpp_utils.GetScopedName(scope, type_defn)
  return _binding_glue_cpp_template.substitute(Class=class_name)


dispatch_function_header_template = string.Template("""
glue::_o3d::NPAPIObject *${variable_npobject} =
    static_cast<glue::_o3d::NPAPIObject *>(header);
NPP ${npp} = ${variable_npobject}->npp();
${Class} *${variable} = glue::_o3d::GetClient(
    ${npp})->GetById<${Class}>(${variable_npobject}->id());
${result} = (${variable} != NULL);
if (!${result}) {
  O3D_ERROR(glue::_o3d::GetServiceLocator(${npp})) <<
    "Invalid object; perhaps it's been destroyed already?";
}
""")


def NpapiDispatchFunctionHeader(scope, type_defn, variable, npp, success):
  """Gets a header for NPAPI glue dispatch functions.

  This function creates a string containing a C++ code snippet that should be
  included at the beginning of NPAPI glue dispatch functions like Invoke or
  GetProperty. This code snippet will declare and initialize certain variables
  that will be used in the dispatch functions, like the NPObject representing
  the object, or a pointer to the NPP instance.

  Args:
    scope: a Definition for the scope in which the glue will be written.
    type_defn: a Definition, representing the type.
    variable: a string, representing a name of a variable that can be used to
      store a reference to the object.
    npp: a string, representing the name of the variable that holds the pointer
      to the NPP instance. Will be declared by the code snippet.
    success: the name of a bool variable containing the current success status.
      (is not declared by the code snippet).

  Returns:
    a (string, string) pair, the first string being the code snippet, and the
    second string being an expression to access the object.
  """
  class_name = cpp_utils.GetScopedName(scope, type_defn)
  variable_npobject = '%s_npobject' % variable
  text = dispatch_function_header_template.substitute(
      variable_npobject=variable_npobject, npp=npp, variable=variable,
      Class=class_name, result=success)
  return text, variable


from_npvariant_template = string.Template("""
${Class} *${variable} = NULL;
if (NPVARIANT_IS_OBJECT(${input})) {
  NPObject *npobject = NPVARIANT_TO_OBJECT(${input});
  if (glue::_o3d::CheckObject(npp, npobject,
                                   ${Class}::GetApparentClass())) {
    glue::_o3d::NPAPIObject *client_object =
         static_cast<glue::_o3d::NPAPIObject *>(npobject);
    ${variable} =
        glue::_o3d::GetClient(${npp})->GetById<${Class}>(
            client_object->id());
    if (${variable} == NULL) {
      ${result} = false;
      *error_handle = "Error in " ${context}
              ": input wasn't a valid object from this plugin instance.";
    } else {
      ${result} = true;
    }
  } else {
    *error_handle = "Error in " ${context}
        ": invalid type.";
    ${result} = false;
  }
} else {
  *error_handle = "Error in " ${context}
      ": was expecting an object.";
  ${result} = false;
}
""")


def NpapiFromNPVariant(scope, type_defn, input_expr, variable, success,
    exception_context, npp):
  """Gets the string to get a value from a NPVariant.

  This function creates a string containing a C++ code snippet that is used to
  retrieve a value from a NPVariant. If an error occurs, like if the NPVariant
  is not of the correct type, the snippet will set the success status variable
  to false and set *error_handle with an appropriate error message.

  Args:
    scope: a Definition for the scope in which the glue will be written.
    type_defn: a Definition, representing the type of the value.
    input_expr: an expression representing the NPVariant to get the value from.
    variable: a string, representing a name of a variable that can be used to
      store a reference to the value.
    success: the name of a bool variable containing the current success status.
    exception_context: the name of a string containing context information, for
      use in exception reporting.
    npp: a string, representing the name of the variable that holds the pointer
      to the NPP instance.

  Returns:
    a (string, string) pair, the first string being the code snippet and the
    second one being the expression to access that value.
  """
  class_name = cpp_utils.GetScopedName(scope, type_defn)
  text = from_npvariant_template.substitute(Class=class_name,
                                            variable=variable,
                                            input=input_expr,
                                            npp=npp,
                                            result=success,
                                            context=exception_context)
  return (text, variable)


expr_to_npobject_template = string.Template("""
glue::_o3d::NPAPIObject *${variable} =
    glue::_o3d::GetNPObject(${npp}, ${expr});
if (!${variable}) {
  *error_handle = "Error : type cannot be null.";
  ${result} = false;
}
""")

npobject_to_npvariant_template = string.Template("""
OBJECT_TO_NPVARIANT(${variable}, *${output});
""")


def NpapiExprToNPVariant(scope, type_defn, variable, expression, output,
                         success, npp):
  """Gets the string to store a value into a NPVariant.

  This function creates a string containing a C++ code snippet that is used to
  store a value into a NPVariant. That operation takes two phases, one that
  allocates necessary NPAPI resources, and that can fail, and one that actually
  sets the NPVariant (that can't fail). If an error occurs, the snippet will
  set the success status variable to false.

  Args:
    scope: a Definition for the scope in which the glue will be written.
    type_defn: a Definition, representing the type of the value.
    variable: a string, representing a name of a variable that can be used to
      store a reference to the value.
    expression: a string representing the expression that yields the value to
      be stored.
    output: an expression representing a pointer to the NPVariant to store the
      value into.
    success: the name of a bool variable containing the current success status.
    npp: a string, representing the name of the variable that holds the pointer
      to the NPP instance.

  Returns:
    a (string, string) pair, the first string being the code snippet for the
    first phase, and the second one being the code snippet for the second phase.
  """
  (scope, type_defn) = (scope, type_defn)  # silence gpylint.
  phase_1_text = expr_to_npobject_template.substitute(variable=variable,
                                                      npp=npp,
                                                      expr=expression,
                                                      result=success)
  phase_2_text = npobject_to_npvariant_template.substitute(
      variable=variable,
      output=output,
      result=success)
  return phase_1_text, phase_2_text


def main():
  pass

if __name__ == '__main__':
  main()
