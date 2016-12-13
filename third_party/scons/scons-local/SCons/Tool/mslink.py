"""SCons.Tool.mslink

Tool-specific initialization for the Microsoft linker.

There normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.

"""

#
# Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009 The SCons Foundation
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

__revision__ = "src/engine/SCons/Tool/mslink.py 3897 2009/01/13 06:45:54 scons"

import os.path

import SCons.Action
import SCons.Defaults
import SCons.Errors
import SCons.Platform.win32
import SCons.Tool
import SCons.Tool.msvc
import SCons.Tool.msvs
import SCons.Util

def pdbGenerator(env, target, source, for_signature):
    try:
        return ['/PDB:%s' % target[0].attributes.pdb, '/DEBUG']
    except (AttributeError, IndexError):
        return None

def _dllTargets(target, source, env, for_signature, paramtp):
    listCmd = []
    dll = env.FindIxes(target, '%sPREFIX' % paramtp, '%sSUFFIX' % paramtp)
    if dll: listCmd.append("/out:%s"%dll.get_string(for_signature))

    implib = env.FindIxes(target, 'LIBPREFIX', 'LIBSUFFIX')
    if implib: listCmd.append("/implib:%s"%implib.get_string(for_signature))

    return listCmd

def _dllSources(target, source, env, for_signature, paramtp):
    listCmd = []

    deffile = env.FindIxes(source, "WINDOWSDEFPREFIX", "WINDOWSDEFSUFFIX")
    for src in source:
        # Check explicitly for a non-None deffile so that the __cmp__
        # method of the base SCons.Util.Proxy class used for some Node
        # proxies doesn't try to use a non-existent __dict__ attribute.
        if deffile and src == deffile:
            # Treat this source as a .def file.
            listCmd.append("/def:%s" % src.get_string(for_signature))
        else:
            # Just treat it as a generic source file.
            listCmd.append(src)
    return listCmd

def windowsShlinkTargets(target, source, env, for_signature):
    return _dllTargets(target, source, env, for_signature, 'SHLIB')

def windowsShlinkSources(target, source, env, for_signature):
    return _dllSources(target, source, env, for_signature, 'SHLIB')

def _windowsLdmodTargets(target, source, env, for_signature):
    """Get targets for loadable modules."""
    return _dllTargets(target, source, env, for_signature, 'LDMODULE')

def _windowsLdmodSources(target, source, env, for_signature):
    """Get sources for loadable modules."""
    return _dllSources(target, source, env, for_signature, 'LDMODULE')

def _dllEmitter(target, source, env, paramtp):
    """Common implementation of dll emitter."""
    SCons.Tool.msvc.validate_vars(env)

    extratargets = []
    extrasources = []

    dll = env.FindIxes(target, '%sPREFIX' % paramtp, '%sSUFFIX' % paramtp)
    no_import_lib = env.get('no_import_lib', 0)

    if not dll:
        raise SCons.Errors.UserError, 'A shared library should have exactly one target with the suffix: %s' % env.subst('$%sSUFFIX' % paramtp)

    insert_def = env.subst("$WINDOWS_INSERT_DEF")
    if not insert_def in ['', '0', 0] and \
       not env.FindIxes(source, "WINDOWSDEFPREFIX", "WINDOWSDEFSUFFIX"):

        # append a def file to the list of sources
        extrasources.append(
            env.ReplaceIxes(dll,
                            '%sPREFIX' % paramtp, '%sSUFFIX' % paramtp,
                            "WINDOWSDEFPREFIX", "WINDOWSDEFSUFFIX"))

    version_num, suite = SCons.Tool.msvs.msvs_parse_version(env.get('MSVS_VERSION', '6.0'))
    if version_num >= 8.0 and env.get('WINDOWS_INSERT_MANIFEST', 0):
        # MSVC 8 automatically generates .manifest files that must be installed
        extratargets.append(
            env.ReplaceIxes(dll,
                            '%sPREFIX' % paramtp, '%sSUFFIX' % paramtp,
                            "WINDOWSSHLIBMANIFESTPREFIX", "WINDOWSSHLIBMANIFESTSUFFIX"))

    if env.has_key('PDB') and env['PDB']:
        pdb = env.arg2nodes('$PDB', target=target, source=source)[0]
        extratargets.append(pdb)
        target[0].attributes.pdb = pdb

    if not no_import_lib and \
       not env.FindIxes(target, "LIBPREFIX", "LIBSUFFIX"):
        # Append an import library to the list of targets.
        extratargets.append(
            env.ReplaceIxes(dll,
                            '%sPREFIX' % paramtp, '%sSUFFIX' % paramtp,
                            "LIBPREFIX", "LIBSUFFIX"))
        # and .exp file is created if there are exports from a DLL
        extratargets.append(
            env.ReplaceIxes(dll,
                            '%sPREFIX' % paramtp, '%sSUFFIX' % paramtp,
                            "WINDOWSEXPPREFIX", "WINDOWSEXPSUFFIX"))

    return (target+extratargets, source+extrasources)

def windowsLibEmitter(target, source, env):
    return _dllEmitter(target, source, env, 'SHLIB')

def ldmodEmitter(target, source, env):
    """Emitter for loadable modules.
    
    Loadable modules are identical to shared libraries on Windows, but building
    them is subject to different parameters (LDMODULE*).
    """
    return _dllEmitter(target, source, env, 'LDMODULE')

def prog_emitter(target, source, env):
    SCons.Tool.msvc.validate_vars(env)

    extratargets = []

    exe = env.FindIxes(target, "PROGPREFIX", "PROGSUFFIX")
    if not exe:
        raise SCons.Errors.UserError, "An executable should have exactly one target with the suffix: %s" % env.subst("$PROGSUFFIX")

    version_num, suite = SCons.Tool.msvs.msvs_parse_version(env.get('MSVS_VERSION', '6.0'))
    if version_num >= 8.0 and env.get('WINDOWS_INSERT_MANIFEST', 0):
        # MSVC 8 automatically generates .manifest files that have to be installed
        extratargets.append(
            env.ReplaceIxes(exe,
                            "PROGPREFIX", "PROGSUFFIX",
                            "WINDOWSPROGMANIFESTPREFIX", "WINDOWSPROGMANIFESTSUFFIX"))

    if env.has_key('PDB') and env['PDB']:
        pdb = env.arg2nodes('$PDB', target=target, source=source)[0]
        extratargets.append(pdb)
        target[0].attributes.pdb = pdb

    return (target+extratargets,source)

def RegServerFunc(target, source, env):
    if env.has_key('register') and env['register']:
        ret = regServerAction([target[0]], [source[0]], env)
        if ret:
            raise SCons.Errors.UserError, "Unable to register %s" % target[0]
        else:
            print "Registered %s sucessfully" % target[0]
        return ret
    return 0

regServerAction = SCons.Action.Action("$REGSVRCOM", "$REGSVRCOMSTR")
regServerCheck = SCons.Action.Action(RegServerFunc, None)
shlibLinkAction = SCons.Action.Action('${TEMPFILE("$SHLINK $SHLINKFLAGS $_SHLINK_TARGETS $( $_LIBDIRFLAGS $) $_LIBFLAGS $_PDB $_SHLINK_SOURCES")}')
compositeShLinkAction = shlibLinkAction + regServerCheck
ldmodLinkAction = SCons.Action.Action('${TEMPFILE("$LDMODULE $LDMODULEFLAGS $_LDMODULE_TARGETS $( $_LIBDIRFLAGS $) $_LIBFLAGS $_PDB $_LDMODULE_SOURCES")}')
compositeLdmodAction = ldmodLinkAction + regServerCheck

def generate(env):
    """Add Builders and construction variables for ar to an Environment."""
    SCons.Tool.createSharedLibBuilder(env)
    SCons.Tool.createProgBuilder(env)

    env['SHLINK']      = '$LINK'
    env['SHLINKFLAGS'] = SCons.Util.CLVar('$LINKFLAGS /dll')
    env['_SHLINK_TARGETS'] = windowsShlinkTargets
    env['_SHLINK_SOURCES'] = windowsShlinkSources
    env['SHLINKCOM']   =  compositeShLinkAction
    env.Append(SHLIBEMITTER = [windowsLibEmitter])
    env['LINK']        = 'link'
    env['LINKFLAGS']   = SCons.Util.CLVar('/nologo')
    env['_PDB'] = pdbGenerator
    env['LINKCOM'] = '${TEMPFILE("$LINK $LINKFLAGS /OUT:$TARGET.windows $( $_LIBDIRFLAGS $) $_LIBFLAGS $_PDB $SOURCES.windows")}'
    env.Append(PROGEMITTER = [prog_emitter])
    env['LIBDIRPREFIX']='/LIBPATH:'
    env['LIBDIRSUFFIX']=''
    env['LIBLINKPREFIX']=''
    env['LIBLINKSUFFIX']='$LIBSUFFIX'

    env['WIN32DEFPREFIX']        = ''
    env['WIN32DEFSUFFIX']        = '.def'
    env['WIN32_INSERT_DEF']      = 0
    env['WINDOWSDEFPREFIX']      = '${WIN32DEFPREFIX}'
    env['WINDOWSDEFSUFFIX']      = '${WIN32DEFSUFFIX}'
    env['WINDOWS_INSERT_DEF']    = '${WIN32_INSERT_DEF}'

    env['WIN32EXPPREFIX']        = ''
    env['WIN32EXPSUFFIX']        = '.exp'
    env['WINDOWSEXPPREFIX']      = '${WIN32EXPPREFIX}'
    env['WINDOWSEXPSUFFIX']      = '${WIN32EXPSUFFIX}'

    env['WINDOWSSHLIBMANIFESTPREFIX'] = ''
    env['WINDOWSSHLIBMANIFESTSUFFIX'] = '${SHLIBSUFFIX}.manifest'
    env['WINDOWSPROGMANIFESTPREFIX']  = ''
    env['WINDOWSPROGMANIFESTSUFFIX']  = '${PROGSUFFIX}.manifest'

    env['REGSVRACTION'] = regServerCheck
    env['REGSVR'] = os.path.join(SCons.Platform.win32.get_system_root(),'System32','regsvr32')
    env['REGSVRFLAGS'] = '/s '
    env['REGSVRCOM'] = '$REGSVR $REGSVRFLAGS ${TARGET.windows}'

    try:
        version = SCons.Tool.msvs.get_default_visualstudio_version(env)

        if env.has_key('MSVS_IGNORE_IDE_PATHS') and env['MSVS_IGNORE_IDE_PATHS']:
            include_path, lib_path, exe_path = SCons.Tool.msvc.get_msvc_default_paths(env,version)
        else:
            include_path, lib_path, exe_path = SCons.Tool.msvc.get_msvc_paths(env,version)

        # since other tools can set these, we just make sure that the
        # relevant stuff from MSVS is in there somewhere.
        env.PrependENVPath('INCLUDE', include_path)
        env.PrependENVPath('LIB', lib_path)
        env.PrependENVPath('PATH', exe_path)
    except (SCons.Util.RegError, SCons.Errors.InternalError):
        pass

    # Loadable modules are on Windows the same as shared libraries, but they
    # are subject to different build parameters (LDMODULE* variables).
    # Therefore LDMODULE* variables correspond as much as possible to
    # SHLINK*/SHLIB* ones.
    SCons.Tool.createLoadableModuleBuilder(env)
    env['LDMODULE'] = '$SHLINK'
    env['LDMODULEPREFIX'] = '$SHLIBPREFIX'
    env['LDMODULESUFFIX'] = '$SHLIBSUFFIX'
    env['LDMODULEFLAGS'] = '$SHLINKFLAGS'
    env['_LDMODULE_TARGETS'] = _windowsLdmodTargets
    env['_LDMODULE_SOURCES'] = _windowsLdmodSources
    env['LDMODULEEMITTER'] = [ldmodEmitter]
    env['LDMODULECOM'] = compositeLdmodAction

def exists(env):
    platform = env.get('PLATFORM', '')
    if SCons.Tool.msvs.is_msvs_installed():
        # there's at least one version of MSVS installed.
        return 1
    elif platform in ('win32', 'cygwin'):
        # Only explicitly search for a 'link' executable on Windows
        # systems.  Some other systems (e.g. Ubuntu Linux) have an
        # executable named 'link' and we don't want that to make SCons
        # think Visual Studio is installed.
        return env.Detect('link')
    return None
