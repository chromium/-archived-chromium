## Process this file with automake to produce Makefile.in

# Make sure that when we re-make ./configure, we get the macros we need
# ACLOCAL_AMFLAGS = -I m4

# This is so we can #include <google/foo>
AM_CPPFLAGS = -I$(top_srcdir)/src

# For a non-optimized (debug) build, change "-DNDEBUG" to "-DDEBUG".
AM_CXXFLAGS = -DNDEBUG

# Enable verbose gcc warnings.  We use the older option name "-W" to be
# compatible with gcc v3.3 and earlier.  That option should be changed to
# "-Wextra" when we can be sure that early gcc versions will not be used.
if GCC
AM_CXXFLAGS += -Wall -Wwrite-strings -Woverloaded-virtual -W
endif

AM_LDFLAGS = -no-undefined

googleincludedir = $(includedir)/google
## The .h files you want to install (that is, .h files that people
## who install this package can include in their own applications.)
googleinclude_HEADERS = src/google/vcdecoder.h src/google/vcencoder.h \
			src/google/output_string.h

docdir = $(prefix)/share/doc/$(PACKAGE)-$(VERSION)
## This is for HTML and other documentation you want to install.
## Add your documentation files (in doc/) in addition to these
## top-level boilerplate files.  Also add a TODO file if you have one.
dist_doc_DATA = AUTHORS COPYING ChangeLog INSTALL NEWS README THANKS

## The libraries (.so's) you want to install
lib_LTLIBRARIES =

## Internal libraries that will not be installed
noinst_LTLIBRARIES =

## Binaries to be built; these are added in the RULES section
bin_PROGRAMS =

## unittests you want to run when people type 'make check'.
## Individual tests are added to TESTS in the RULES section.
## TESTS is for binary unittests, check_SCRIPTS for script-based unittests.
## TESTS_ENVIRONMENT sets environment variables for when you run unittest,
## but it only seems to take effect for *binary* unittests (argh!)
TESTS =
TESTS_ENVIRONMENT = SRCDIR="$(top_srcdir)"
check_SCRIPTS =
# Every time you add a unittest to check_SCRIPTS, add it here too
noinst_SCRIPTS =

## vvvv RULES TO MAKE THE LIBRARIES, BINARIES, AND UNITTESTS

# google-gflags: Used for command-line client
#     Please refer to http://code.google.com/p/google-gflags/ for details
noinst_LTLIBRARIES += libgflags.la
libgflags_la_SOURCES = src/google/gflags.h \
		       src/gflags.cc \
		       src/gflags_reporting.cc

# gtest (Google Test): Used for unit tests only
#     Please refer to http://code.google.com/p/googletest/ for details
noinst_LTLIBRARIES += libgtest.la
libgtest_la_SOURCES = src/gtest/gtest.h \
		      src/gtest/gtest-death-test.h \
		      src/gtest/gtest-message.h \
		      src/gtest/gtest_pred_impl.h \
		      src/gtest/gtest-spi.h \
		      src/gtest/gtest_prod.h \
		      src/gtest/internal/gtest-death-test-internal.h \
		      src/gtest/internal/gtest-filepath.h \
		      src/gtest/internal/gtest-internal.h \
		      src/gtest/internal/gtest-port.h \
		      src/gtest/internal/gtest-string.h \
		      src/gtest/src/gtest-internal-inl.h \
		      src/gtest/gtest.cc \
		      src/gtest/gtest-death-test.cc \
		      src/gtest/gtest-filepath.cc \
		      src/gtest/gtest-port.cc \
		      src/testing.h

noinst_LTLIBRARIES += libgtest_main.la
libgtest_main_la_SOURCES = src/gtest/gtest_main.cc
libgtest_main_la_LIBADD = libgtest.la

# libvcdcom: The open-vcdiff *common* library
lib_LTLIBRARIES += libvcdcom.la
libvcdcom_la_SOURCES = src/google/output_string.h \
		       src/addrcache.h \
		       src/checksum.h \
		       src/codetable.h \
		       src/logging.h \
		       src/varint_bigendian.h \
		       src/vcdiff_defs.h \
		       src/zlib.h \
		       src/zconf.h \
		       src/adler32.c \
		       src/addrcache.cc \
		       src/codetable.cc \
		       src/logging.cc \
		       src/varint_bigendian.cc

# libvcddec: The open-vcdiff *decoder* library
lib_LTLIBRARIES += libvcddec.la
libvcddec_la_SOURCES = src/google/vcdecoder.h \
		       src/decodetable.h \
		       src/headerparser.h \
		       src/decodetable.cc \
		       src/headerparser.cc \
		       src/vcdecoder.cc
libvcddec_la_LIBADD = libvcdcom.la

# libvcdenc: The open-vcdiff *encoder* library
lib_LTLIBRARIES += libvcdenc.la
libvcdenc_la_SOURCES = src/google/vcencoder.h \
		       src/blockhash.h \
		       src/compile_assert.h \
		       src/encodetable.h \
		       src/rolling_hash.h \
		       src/vcdiffengine.h \
		       src/blockhash.cc \
		       src/encodetable.cc \
		       src/vcdiffengine.cc \
                       src/vcencoder.cc
libvcdenc_la_LIBADD = libvcdcom.la

bin_PROGRAMS += vcdiff
vcdiff_SOURCES = src/vcdiff_main.cc
vcdiff_LDADD = libvcddec.la libvcdenc.la libgflags.la

TESTS += addrcache_test
addrcache_test_SOURCES = src/addrcache_test.cc
addrcache_test_LDADD = libvcdcom.la libgtest_main.la

TESTS += blockhash_test
blockhash_test_SOURCES = src/blockhash_test.cc
blockhash_test_LDADD = libvcdenc.la libgtest_main.la

TESTS += codetable_test
codetable_test_SOURCES = src/codetable_test.cc
codetable_test_LDADD = libvcdcom.la libgtest_main.la

TESTS += decodetable_test
decodetable_test_SOURCES = src/decodetable_test.cc
decodetable_test_LDADD = libvcddec.la libgtest_main.la

TESTS += encodetable_test
encodetable_test_SOURCES = src/encodetable_test.cc
encodetable_test_LDADD = libvcdenc.la libgtest_main.la

TESTS += headerparser_test
headerparser_test_SOURCES = src/headerparser_test.cc
headerparser_test_LDADD = libvcddec.la libgtest_main.la

TESTS += output_string_test
output_string_test_SOURCES = src/output_string_test.cc
output_string_test_LDADD = libgtest_main.la

TESTS += rolling_hash_test
rolling_hash_test_SOURCES = src/rolling_hash_test.cc
rolling_hash_test_LDADD = libvcdcom.la libgtest_main.la

TESTS += varint_bigendian_test
varint_bigendian_test_SOURCES = src/varint_bigendian_test.cc
varint_bigendian_test_LDADD = libvcdcom.la libgtest_main.la

TESTS += vcdecoder_test
vcdecoder_test_SOURCES = src/vcdecoder_test.cc
vcdecoder_test_LDADD = libvcddec.la libgtest_main.la

TESTS += vcdiffengine_test
vcdiffengine_test_SOURCES = src/vcdiffengine_test.cc
vcdiffengine_test_LDADD = libvcdenc.la libgtest_main.la

TESTS += vcencoder_test
vcencoder_test_SOURCES = src/vcencoder_test.cc
vcencoder_test_LDADD = libvcddec.la libvcdenc.la libgtest_main.la

## ^^^^ END OF RULES TO MAKE THE LIBRARIES, BINARIES, AND UNITTESTS

## This should always include $(TESTS), but may also include other
## binaries that you compile but don't want automatically installed.
noinst_PROGRAMS = $(TESTS)

rpm: dist-gzip packages/rpm.sh packages/rpm/rpm.spec
	@cd packages && ./rpm.sh ${PACKAGE} ${VERSION}

deb: dist-gzip packages/deb.sh packages/deb/*
	@cd packages && ./deb.sh ${PACKAGE} ${VERSION}

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

EXTRA_DIST = packages/rpm.sh packages/rpm/rpm.spec packages/deb.sh \
  packages/deb autogen.sh $(SCRIPTS) libtool
