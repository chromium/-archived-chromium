#!/usr/bin/perl

#
# Given a memwatcher logfile, group memory allocations by callstack.
#
# Usage:
#
#   memprof.pl <logfile>
#
#      logfile -- The memwatcher.logXXXX file to summarize.
#
#
#
# Sample output:
#
# 54061617        100.00% AllocationStack::AllocationStack
# 41975368        77.64%  malloc
# 11886592        21.99%  VirtualAlloc
#  7168000        13.26%  v8::internal::OS::Allocate
#  7168000        13.26%  v8::internal::MemoryAllocator::AllocateRawMemory
#  5976184        11.05%  WebCore::V8Bridge::evaluate
#  5767168        10.67%  v8::internal::MemoryAllocator::AllocatePages
#  5451776        10.08%  WebCore::V8Proxy::initContextIfNeeded
#  ....
#
#
#
# ********
# Note: The output is not currently sorted. To make it more legible,
# you will want to sort numerically by the first field:
#   $ ./memprof.pl memwatcher.log3620.txt | sort -n -r
# ********
#

sub process_raw($$) {
  my $file = shift;
  my $filter = shift;

  my %leaks = ();
  my %stackframes = ();

  my $blamed = 0;
  my $bytes = 0;
  my $hits = 0;
  open (LOGFILE, "$file") or die("could not open $file");
  while(<LOGFILE>) {
    my $line = $_;
#print "$line";
    chomp($line);
    if ($line =~ m/([0-9]*) bytes, ([0-9]*) items/) {

      # If we didn't find any frames to account this to, log that.
      if ($blamed == 0) {
        $leaks{"UNACCOUNTED"} += $bytes;
      }

#print "START\n";
      #print("stackframe " . $1 . ", " . $2 . "\n");
      $hits = $2;
      $bytes = $1;
      %stackframes = ();   # we have a new frame, clear the list.
      $blamed = 0;         # we haven't blamed anyone yet
    }
    elsif ($line =~ m/Total Bytes:[ ]*([0-9]*)/) {
      $total_bytes += $1;
    }
    elsif ($line =~ m/=============/) {
      next;
    }
    elsif ($line =~ m/[ ]*([\-a-zA-Z_\\0-9\.]*) \(([0-9]*)\):[ ]*([<>_a-zA-Z_0-9:]*)/) {
#      print("junk: " . $line . "\n");
#    print("file: $1\n"); 
#    print("line: $2\n"); 
#    print("function: $3\n"); 
#   
      
      # blame the function
      my $pig = $3;
#      my $pig = $1;

      # only add the memory if this function is not yet on our callstack
      if (!exists $stackframes{$pig}) {  
        $leaks{$pig} += $bytes;
      }

      $stackframes{$pig}++;
      $blamed++;
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
my $filter = shift;

# Process the file.
process_raw($filename, $filter);
