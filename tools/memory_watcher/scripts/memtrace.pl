#!/usr/bin/perl

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
      if ($filename =~ m/platformsdk_vista_6_0/) {
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
