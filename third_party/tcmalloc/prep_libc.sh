#!/bin/sh -x
# This script takes libcmt.lib for VS2005 and removes the allocation related 
# functions from it.
#
# Usage: prep_libcmt.bat <VCInstallDir> <OutputFile>
#
# VCInstallDir is the path where VC is installed, typically:
#    C:\Program Files\Microsoft Visual Studio 8\VC\
#
# OutputFile is the directory where the modified libcmt file should be stored.
#

LIBCMT="${1}\\libcmt.lib"
LIBCMTPDB="${1}\\libcmt.pdb"
OUTDIR=$2
OUTCMT="${2}\\libcmt.lib"

mkdir $OUTDIR
cp "$LIBCMT" "$OUTDIR"
cp "$LIBCMTPDB" "$OUTDIR"
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\malloc.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\free.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\realloc.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\calloc.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\new.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\delete.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\new2.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\delete2.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\align.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\msize.obj $OUTCMT

LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\heapinit.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\expand.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\heapchk.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\heapwalk.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\heapmin.obj $OUTCMT
LIB /IGNORE:4006,4221 /REMOVE:build\\intel\\mt_obj\\sbheap.obj $OUTCMT
