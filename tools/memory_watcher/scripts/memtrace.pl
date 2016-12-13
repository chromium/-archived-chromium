#!/usr/bin/perl

#
# Blame callstacks for each memory allocation.
# Similar to memprof.pl, will also try to filter out unuseful stacks.
# TODO: better describe how these tools differ.
#
# Usage:
#
#   memtrace.pl <logfile>
#
#      logfile -- The memwatcher.logXXXX file to summarize.
#
#
#
# Sample output:
#
# 41975368    77.64%      f:\sp\vctools\crt_bld\self_x86\crt\src\malloc.c (163): malloc
#  2097152    3.88%       c:\src\chrome1\src\webkit\pending\frameloader.cpp (3300): WebCore::FrameLoader::committedLoad
#  1572864    2.91%       c:\src\chrome1\src\webkit\port\bridge\v8bridge.cpp (214): WebCore::V8Bridge::evaluate
#  1572864    2.91%       c:\src\chrome1\src\webkit\glue\webframeloaderclient_impl.cc (1071): WebFrameLoaderClient::committedLoad
#  1572864    2.91%       c:\src\chrome1\src\v8\src\ast.h (1181): v8::internal::Visitor::Visit
#
#
#
# ********
# Note: The output is not currently sorted. To make it more legible,
# you will want to sort numerically by the first field:
#   $ ./memtrace.pl memwatcher.log3620.txt | sort -n -r
# ********
#

sub process_raw($) {
  my $file = shift;

  my %leaks = ();

  my $location_bytes = 0;
  my $location_hits = 0;
  my $location_blame = "";
  my $location_last = "";
  my $contains_load_lib = 0;
  my $total_bytes = 0;
  open (LOGFILE, "$file") or die("could not open $file");
  while(<LOGFILE>) {
    my $line = $_;
#print "$line";
    chomp($line);
    if ($line =~ m/([0-9]*) bytes, ([0-9]*) items/) {

#print "START\n";
      # Dump "prior" frame here
      if ($location_bytes > 0) {
#print("GOTLEAK: $location_bytes ($location_hits) $location_blame\n");
        if ($location_blame eq "") {
          $location_blame = $location_last;
        }
        if (!$contains_load_lib) {
          $leaks{$location_blame} += $location_bytes;
        }
        $location_bytes = 0;
        $location_blame = "";
        $contains_load_lib = 0;
      }

      #print("stackframe " . $1 . ", " . $2 . "\n");
      $location_hits = $2;
      $location_bytes = $1;
    }
    elsif ($line =~ m/Total Bytes:[ ]*([0-9]*)/) {
      $total_bytes += $1;
    }
    elsif ($line =~ m/LoadLibrary/) {
      # skip these, they contain false positives.
      $contains_load_lib = 1;
      next;
    }
    elsif ($line =~ m/=============/) {
      next;
    }
    elsif ($line =~ m/Untracking untracked/) {
      next;
    }
    elsif ($line =~ m/[ ]*(c:\\trunk\\[a-zA-Z_\\0-9\.]*) /) {
      my $filename = $1;
      if ($filename =~ m/memory_watcher/) {
        next;
      }
      if ($filename =~ m/skmemory_stdlib.cpp/) {
        next;
      }
      if ($filename =~ m/stringimpl.cpp/) {
        next;
      }
      if ($filename =~ m/stringbuffer.h/) {
        next;
      }
      if ($filename =~ m/fastmalloc.h/) {
        next;
      }
      if ($filename =~ m/microsoft visual studio 8/) {
        next;
      }
      if ($filename =~ m/platformsdk_win2008_6_1/) {
        next;
      }
      if ($location_blame eq "") {
        # use this to blame the line
        $location_blame = $line;
  
        # use this to blame the file.
      #  $location_blame = $filename;

#print("blaming $location_blame\n");
      }
    } else {
#      print("junk: " . $line . "\n");
      if (! ($line =~ m/GetModuleFileNameA/) ) {
        $location_last = $line;
      }
    }
  }

  # now dump our hash table
  my $sum = 0;
  my @keys = keys(%leaks);
  for ($i=0; $i<@keys; $i++) {
    my $key = @keys[$i];
    printf "%8d\t%3.2f%%\t%s\n", $leaks{$key}, (100* $leaks{$key} / $total_bytes), $key;
    $sum += $leaks{$key};
  }
  print("TOTAL: $sum\n");
}


# ----- Main ------------------------------------------------

# Get the command line argument
my $filename = shift;

# Process the file.
process_raw($filename);
