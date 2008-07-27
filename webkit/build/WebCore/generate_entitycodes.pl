#!/usr/bin/perl -w
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

#
# This script is for generating the reverse entity code mapping using 
# HTMLEntityNames.gperf as input.

# Get original file path from agruments.
my $originalFileName = shift;

# Open file for reading.
open $FILE, "<", $originalFileName or die "Can't open $originalFileName: $!";

# status for indicating current parsing status.
# 0 means waiting start, 1 means starting parsing, 2 means stopping parsing.
my $parseStatus = 0;

# Counter for calculating number of entities.
my $dataCount = 0;

# print file head comments.
print "// This file is generated from HTMLEntityNames.gperf. DO NOT EDIT IT!\n\n";

#print  an anonymous namespace to avoid link-time conflicts.
print "namespace {\n\n";

# print the strutcure declaration of EntityCode.
print "struct EntityCode {\n  wchar_t code;\n  const char* name;\n};\n\n";

# print the array declaration of entities.
print "static const EntityCode entity_codes[] = {\n";

while(<$FILE>) {
  chomp;
  if (/^\%\%$/i) {
    ++$parseStatus;
  } elsif (!/^\#/ && $parseStatus == 1) {
    my ($name, $code) = split(/,/);
    if (defined($name) && defined($code)) {
      $name =~ s/\s//g;
      $code =~ s/\s//g;
      if (length($name) > 0 && length($code) > 0) {
        print ",\n" if $dataCount > 0;
        print "  {$code, \"&$name;\"}";
        ++$dataCount;
      }
    }
  }
}

# print declaration of entity_codes_length and close anonymous namespace declaration.
print "\n};\n\nstatic int entity_codes_length = sizeof(entity_codes) / sizeof(EntityCode);\n\n}\n";
