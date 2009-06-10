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

mkdir -p $OUTDIR
cp "$LIBCMT" "$OUTDIR"
cp "$LIBCMTPDB" "$OUTDIR"


# We'll remove the symbols based on paths found in either the VS2005 or VS2008
# libcmt.lib files.
LIBCMTSRCPATHVS2005="build\\intel\\mt_obj\\"
LIBCMTSRCPATHVS2008="f:\\dd\\vctools\\crt_bld\\SELF_X86\\crt\\src\\build\\INTEL\\mt_obj\\"

OBJFILES="malloc.obj free.obj realloc.obj new.obj delete.obj new2.obj delete2.obj align.obj msize.obj heapinit.obj expand.obj heapchk.obj heapwalk.obj heapmin.obj sbheap.obj calloc.obj recalloc.obj calloc_impl.obj new_mode.obj"

for FILE in $OBJFILES
do
  LIB /IGNORE:4006,4014,4221 /REMOVE:${LIBCMTSRCPATHVS2005}${FILE} $OUTCMT
  LIB /IGNORE:4006,4014,4221 /REMOVE:${LIBCMTSRCPATHVS2008}${FILE} $OUTCMT
done
