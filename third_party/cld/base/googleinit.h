// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GOOGLEINIT_H_
#define BASE_GOOGLEINIT_H_

//------------------------------------------------------------------------

// Initialization sequence in C++ and Google.
//
// This library provides helpers to arrange pieces of initialization code
// for some global objects/state to be executed at well-defined
// moment in time and in a well-defined order.
//
// This library is flexible enough and should be used to do all
// initialization for global/static objects
// except maybe for a few very-low level libraries (mainly inside //base).
// Comments in googleinit.cc discuss the reasons for this.
//
// For the default *MODULE* macros provided below,
// the initialization happens automatically
// during execution of InitGoogle() -- see google.h,
// a call to which should normally be the first statement of main().
//
// A module can register code to be executed by InitGoogle().
// For example, one could place the following in common/hostname.cc:
//
//    static const char* my_hostname = NULL;
//    REGISTER_MODULE_INITIALIZER(hostname, {
//      // Code to initialize "my_hostname"
//    });
//
// (Note that, due to preprocessor weirdness, there may be issues with
// the use of commas in your initialization code.  If you run into them,
// try using parens around the commas, or use a helper function as follows:
//
//    static const char* my_hostname = NULL;
//    static void InitMyHostname() {
//      // Code to initialize "my_hostname"
//    }
//    REGISTER_MODULE_INITIALIZER(my_hostname, InitMyHostname());
//
// This also helps the compiler to accurately attribute compilation errors
// to pieces of your initialization code.
//
// Note that each piece of registered initialized code is tagged
// with an identifier ('my_hostname' in the previous example).  This
// is useful to control the order of initialization.  For example,
// if we want file initialization to occur after hostname
// initialization, we can place the following in file/base/file.cc:
//
//   REGISTER_MODULE_INITIALIZER(file, {
//     // File initialization goes here
//   });
//   REGISTER_MODULE_INITIALIZER_SEQUENCE(my_hostname, file);
//     // requires my_hostname's initializer to run before file's
//
// Alternatively the following *deprecated* method is also supported
// to accomplish the same ordering of initialization:
//
//   REGISTER_MODULE_INITIALIZER(file, {
//     REQUIRE_MODULE_INITIALIZED(my_hostname);
//     // File initialization goes here
//   });
//
// REQUIRE_MODULE_INITIALIZED should really be used only when
// REGISTER_MODULE_INITIALIZER_SEQUENCE can not be e.g. to explicitly make
// a subset of initializers executed from some non-initializer code or
// to define run-time-dependent module dependencies.
//
// For either of the above to compile we should also place the following
// into common/hostname.h and #include that file into file/base/file.cc:
//
//   DECLARE_MODULE_INITIALIZER(my_hostname);
//
// Initialization dependencies defined via REGISTER_MODULE_INITIALIZER_SEQUENCE
// are more flexible: unlike with REQUIRE_MODULE_INITIALIZED, one can also
// require that the initialization code defined in the current .cc file
// be executed before some other initializer, e.g.:
//
//   In foo_factory.h:
//     DECLARE_MODULE_INITIALIZER(foo_factory_init);
//     DECLARE_MODULE_INITIALIZER(foo_factory);
//
//   In foo_factory.cc:
//     static FooFactory* foo_factory = NULL
//     REGISTER_MODULE_INITIALIZER(foo_factory_init, {
//       foo_factory = new FooFactory(...);
//     });
//     REGISTER_MODULE_INITIALIZER(foo_factory, {
//       // e.g. code for some final assimilation of all things registered
//       // with foo_factory can go here
//     });
//     REGISTER_MODULE_INITIALIZER_SEQUENCE(foo_factory_init, foo_factory);
//
//   In my_foo_maker.cc:
//     REGISTER_MODULE_INITIALIZER(my_registerer, {
//       // registration of some my_method with foo_factory goes here
//     });
//     REGISTER_MODULE_INITIALIZER_SEQUENCE_3(
//       foo_factory_init, my_registerer, foo_factory);
//         // my_registerer runs after foo_factory_init, but before foo_factory
//
//   In foo_factory_user.cc:
//     REGISTER_MODULE_INITIALIZER(foo_user, {
//       // use of foo_factory goes here
//     });
//     REGISTER_MODULE_INITIALIZER_SEQUENCE(foo_factory, foo_user);
//
// In the above example the initializer execution order will be
// foo_factory_init, my_registerer, foo_factory, foo_user
// even though both foo_factory.cc and foo_factory_user.cc do not have
// explicit dependencies on my_foo_maker.cc (they do not have to know/care
// if it exists).
//
// It is an error to introduce cycles in the initialization
// dependencies.  The program will die with an error message
// if the initialization code encounters cyclic dependencies.
//
// Normally all the registered initializers are executed after
// command-line flags have been parsed.
// If you need your initializer to run before parsing of the command-line flags,
// e.g. to adjust the (default) value of certain flags, then include google.h
// and add a directive like this to your .cc file:
//
//   REGISTER_MODULE_INITIALIZER_SEQUENCE(
//     my_foo_init, command_line_flags_parsing);
//
// Note that you can't instead call
// REQUIRE_MODULE_INITIALIZED(command_line_flags_parsing);
// in the code of your initializer: actual command-line parsing
// is executed by InitGoogle() not in a registered initializer.
//
// A piece of code can declare a dependency on a module using the
// REQUIRE_MODULE macro. This macro creates a link time dependency
// between the .o which the macro is compiled in and the specified
// module. This can be useful in making link time dependencies
// explicit in the code instead of relying on the correctness of the
// BUILD files. For example, foo.cc can declare (see
// file/base/file.h for the REQUIRE_FILE_MODULE definition):
//
//   REQUIRE_FILE_MODULE(localfile);
//
// Similarly to other uses, DECLARE_FILE_INITIALIZER(localfile)
// should be #include-d for the above to compile.
// The above will guarantee that the localfile module will be linked into
// an application which foo.cc is linked into. Specifically, a link
// error will occur if the localfile module is not linked in. The
// preferred usage of REQUIRE_*_MODULE is for the module writer to
// provide an external .h which contains the REQUIRE_* macro. In the
// above example, the localfile module writer would provide localfile.h:
//
//   #ifndef FILE_LOCALFILE_H_
//   #define FILE_LOCALFILE_H_
//
//   #include "file/base/file.h"
//
//   DECLARE_FILE_INITIALIZER(localfile);
//   REQUIRE_FILE_MODULE(localfile);
//
//   #endif  // FILE_LOCALFILE_H_
//
// Now a user of localfile can declare their dependence on it by
// #including "localfile.h".

//------------------------------------------------------------------------

// The following code is mostly ugly details about how the
// initialization is implemented, and can be safely ignored
// by users of the initialization facility.

#include <string>

#include "base/basictypes.h"

// A static instance of 'GoogleInitializer' is declared for every
// piece of initialization code.  The constructor registers the
// code in the initialization table.  This class is thread-safe.
class GoogleInitializer {
 public:
  typedef void (*Initializer)();

  // Register the specified initialization "function" as the
  // initialization code for "name". The "type" parameter controls
  // which initializer set this initializer will added to. Note that
  // an initializer might end up being run from a different set if it
  // is required using the REQUIRE_GOOGLE_INITIALIZED macro. Normally
  // the type parameter is "module". Its existence allows the
  // specification of other "initializer sets." See file/base/file.h
  // and File::Init() for an example of such a set. It's unlikely this
  // additional functionality will be used very often.
  GoogleInitializer(const char* type, const char* name, Initializer function);

  // Invoke all registered initializers that have not yet been
  // executed. The "type" parameter specifies which set of
  // initializers to run. The initializers are invoked in
  // lexicographically increasing order by name, except as necessary
  // to satisfy dependencies. This routine is invoked by InitGoogle(),
  // so application code should not call it except in special
  // circumstances.
  static void RunInitializers(const char* type);

  // If this initialization has not yet been executed, runs it
  // right after running all the initializers registered to come before it,
  // these initializers are invoked in lexicographically increasing order
  // by name, except as necessary to satisfy dependencies.
  // It is an error to call this method if the corresponding
  // initializer method is currently active (i.e., we do not
  // allow cycles in the requirement graph).
  void Require();

  // Helper data-holder struct that is passed into
  // DependencyRegisterer's c-tor below.
  struct Dependency {
    Dependency(const char* n, GoogleInitializer* i) : name(n), initializer(i) {}
    const char* const name;
    GoogleInitializer* const initializer;
  };

  // A static instance of 'DependencyRegisterer' is declared for every
  // piece of initializer ordering definition.  The constructor registers the
  // ordering relation in the initialization table.  This class is thread-safe.
  struct DependencyRegisterer {
    // Ask to run initializer specified by 'dependency'
    // before the 'initializer' with 'name'.
    // Both initializers are supposed to be of type 'type'.
    DependencyRegisterer(const char* type,
                         const char* name,
                         GoogleInitializer* initializer,
                         const Dependency& dependency);
   private:
    void SharedConstructorCode(const char* type,
                               const char* name,
                               GoogleInitializer* initializer,
                               const Dependency& dependency);

    DISALLOW_EVIL_CONSTRUCTORS(DependencyRegisterer);
  };

  // Side note: If we happen to decide that connecting all initializers into an
  // explicit DAG with one/few sink node(s) that depend on everything else
  // is important (to explicitly specify in code all the
  // required initializers of a binary) we can provide something like
  //   static bool DoneAllInitializers(const char *type);
  // to check that all registered initializers have been executed.
  // Going this route does not seem worth it though:
  // it's equivalent to mandating creation of a third complete
  // module dependency DAG, the first two being via #include-s and BUILD
  // dependencies.

  // Helper structs in .cc; public to declare file-level globals.
  struct InitializerData;
  struct TypeData;

 private:
  void SharedConstructorCode(const char* type,
                             const char* name,
                             Initializer function);

  const string  type_;           // Initializer type
  const string  name_;           // Initializer name
  Initializer   function_;       // The actual initializer
  bool          done_;           // Finished initializing?
  bool          is_active_;      // Is currently running

  // Implementation helper for Require() and RunInitializers:
  // Runs initializer *this and all its dependencies
  // if that has not happened yet.
  // Assumes table_lock is reader-held and TypeData::lock for type_ is held.
  void RunIfNecessary_Locked();

  // Helper to initialize/create and return data for a given initializer type.
  static TypeData* InitializerTypeData(const char* type);

  DISALLOW_EVIL_CONSTRUCTORS(GoogleInitializer);
};

//------------------------------------------------------------------------

// Implementation Internals (most users should ignore)
//
// The *_GOOGLE_* macros are used to make separate initializer
// sets. They should not be used directly by application code, but are
// useful to library writers who want to create a new registration
// mechanism. (See google2file.h and the *_FILE_* macros for an
// example).

// TODO(maxim): When DECLARE_GOOGLE_INITIALIZER is not used in
// REQUIRE_GOOGLE_INITIALIZED and REQUIRE_GOOGLE_MODULE
// put google_initializer_##type##_##name (and google_init_##type##_##name)
// into a gI##type namespace to force our users to use
// DECLARE_GOOGLE_INITIALIZER not reimplement it
// to manually declare an initializer.

#define DECLARE_GOOGLE_INITIALIZER(type, name) \
  extern GoogleInitializer google_initializer_##type##_##name

#define REGISTER_GOOGLE_INITIALIZER(type, name, body) \
  static void google_init_##type##_##name() { body; } \
  GoogleInitializer google_initializer_##type##_##name( \
    #type, #name, google_init_##type##_##name)

// Require initializer name1 of 'type' to run before initializer
// initializer name2 of same 'type' (i.e. in the order they are written out).
// "Sequence" only means ordering, not direct executions sequence
// without any other initializer executed in between.
// Initializers for both modules must be declared
// with DECLARE_GOOGLE_INITIALIZER at this point.
#define REGISTER_GOOGLE_INITIALIZER_SEQUENCE(type, name1, name2) \
  namespace { \
    static GoogleInitializer::DependencyRegisterer \
      google_initializer_dependency_##type##_##name1##_##name2( \
        #type, #name2, &google_initializer_##type##_##name2, \
        GoogleInitializer::Dependency( \
          #name1, &google_initializer_##type##_##name1)); \
  }
// Require initializers name1, name2, name3 of 'type' to run in the above order.
// Added to support this frequent use case more conveniently.
#define REGISTER_GOOGLE_INITIALIZER_SEQUENCE_3(type, name1, name2, name3) \
  REGISTER_GOOGLE_INITIALIZER_SEQUENCE(type, name1, name2); \
  REGISTER_GOOGLE_INITIALIZER_SEQUENCE(type, name2, name3)

// Calling REQUIRE_GOOGLE_INITIALIZED(type, foo) means to make sure intializer
// for foo and everything it depends on have executed, and as such
// it can be used to e.g. pre-execute subsets of initializers
// e.g. before everything is executed via RUN_GOGLE_INITIALIZERS(type).
// The initializer must be declared with DECLARE_GOOGLE_INITIALIZER(type, name).
// TODO : remove DECLARE_GOOGLE_INITIALIZER here
//              when all old code makes use of DECLARE_GOOGLE_INITIALIZER.
#define REQUIRE_GOOGLE_INITIALIZED(type, name) \
  do { \
    DECLARE_GOOGLE_INITIALIZER(type, name); \
    google_initializer_##type##_##name.Require(); \
  } while (0)

#define RUN_GOOGLE_INITIALIZERS(type) \
  do { \
    GoogleInitializer::RunInitializers(#type); \
  } while (0)

// We force the dependant module to be loaded by taking the
// address of an object inside the dependency
// (created by REGISTER_GOOGLE_INITIALIZER).  The rest
// is required to avoid warnings about unused variables and
// make sure gcc doesn't optimize it out of existence.
// The initializer must be declared with DECLARE_GOOGLE_INITIALIZER(type, name).
// TODO : remove DECLARE_GOOGLE_INITIALIZER here
//              when all old code makes use of DECLARE_GOOGLE_INITIALIZER.
#define REQUIRE_GOOGLE_MODULE(type, name) \
  DECLARE_GOOGLE_INITIALIZER(type, name); \
  static struct GoogleModuleRef_##name { \
    GoogleModuleRef_##name(GoogleInitializer* r) : ref(r) {} \
    GoogleInitializer* ref; \
  } google_module_ref_##name(&google_initializer_##type##_##name)


// External Interface (most users should use these macros)

#define DECLARE_MODULE_INITIALIZER(name) \
  DECLARE_GOOGLE_INITIALIZER(module, name)

#define REGISTER_MODULE_INITIALIZER(name, body) \
  REGISTER_GOOGLE_INITIALIZER(module, name, body)

#define REGISTER_MODULE_INITIALIZER_SEQUENCE(name1, name2) \
  REGISTER_GOOGLE_INITIALIZER_SEQUENCE(module, name1, name2)

#define REGISTER_MODULE_INITIALIZER_SEQUENCE_3(name1, name2, name3) \
  REGISTER_GOOGLE_INITIALIZER_SEQUENCE_3(module, name1, name2, name3)

#define REQUIRE_MODULE_INITIALIZED(name) \
  REQUIRE_GOOGLE_INITIALIZED(module, name)

#define RUN_MODULE_INITIALIZERS() \
  RUN_GOOGLE_INITIALIZERS(module)

// TODO : maybe rename this as REQUIRE_MODULE_LINKED
#define REQUIRE_MODULE(name) \
  REQUIRE_GOOGLE_MODULE(module, name)

//------------------------------------------------------------------------

#endif  // BASE_GOOGLEINIT_H_
