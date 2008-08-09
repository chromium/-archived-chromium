#!/usr/bin/perl
#
# Read a memtrace logfile from stdin and group memory allocations by logical
# code component. The code component is guessed from the callstack, and
# is something like {v8, sqlite, disk cache, skia, etc..}
#
# Usage:
#
#   summary.pl
#
#      [STDIN] -- The memwatcher.logXXXX file to summarize.
#

sub process_stdin() {
  my %leaks = ();
  my $total_bytes = 0;

  while(<STDIN>) {
    my $line = $_;
    chomp($line);
    my $bytes, $loc;
    ($bytes, $loc) = ($line =~ m/[ \t]*([0-9]*)[ \t]*[0-9\.%]*[ \t]*(.*)/);
    my $location_blame = "";

#    print "Found: $bytes, $loc\n";

    $total_bytes += $bytes;

    if ($loc =~ m/v8/) {
      $location_blame = "v8";
    } elsif ($loc =~ m/sqlite/) {
      $location_blame = "sqlite";
    } elsif ($loc =~ m/SkBitmap/) {
      $location_blame = "skia";
    } elsif ($loc =~ m/disk_cache/) {
      $location_blame = "disk cache";
    } elsif ($loc =~ m/skia/) {
      $location_blame = "skia";
    } elsif ($loc =~ m/:WSA/) {
      $location_blame = "net";
    } elsif ($loc =~ m/dns/) {
      $location_blame = "net";
    } elsif ($loc =~ m/trunk\\net/) {
      $location_blame = "net";
    } elsif ($loc =~ m/WinHttp/) {
      $location_blame = "WinHttp";
    } elsif ($loc =~ m/:I_Crypt/) {
      $location_blame = "WinHttpSSL";
    } elsif ($loc =~ m/CryptGetTls/) {
      $location_blame = "WinHttpSSL";
    } elsif ($loc =~ m/WinVerifyTrust/) {
      $location_blame = "WinHttpSSL";
    } elsif ($loc =~ m/Cert/) {
      $location_blame = "WinHttpSSL";
    } elsif ($loc =~ m/plugin/) {
      $location_blame = "plugin";
    } elsif ($loc =~ m/NP_/) {
      $location_blame = "plugin";
    } elsif ($loc =~ m/hunspell/) {
      $location_blame = "hunspell";
    } elsif ($loc =~ m/decoder/) {
      $location_blame = "img decoder";
    } elsif ($loc =~ m/TextCodec/) {
      $location_blame = "fonts";
    } elsif ($loc =~ m/glyph/) {
      $location_blame = "fonts";
    } elsif ($loc =~ m/cssparser/) {
      $location_blame = "webkit css";
    } elsif ($loc =~ m/::CSS/) {
      $location_blame = "webkit css";
    } elsif ($loc =~ m/Arena/) {
      $location_blame = "webkit arenas";
    } elsif ($loc =~ m/IPC/) {
      $location_blame = "ipc";
    } elsif ($loc =~ m/trunk\\chrome\\browser/) {
      $location_blame = "browser";
    } elsif ($loc =~ m/trunk\\chrome\\renderer/) {
      $location_blame = "renderer";
    } elsif ($loc =~ m/webcore\\html/) {
      $location_blame = "webkit webcore html";
    } elsif ($loc =~ m/webkit.*string/) {
      $location_blame = "webkit strings";
    } elsif ($loc =~ m/htmltokenizer/) {
      $location_blame = "webkit HTMLTokenizer";
    } elsif ($loc =~ m/javascriptcore/) {
      $location_blame = "webkit javascriptcore";
    } elsif ($loc =~ m/webkit/) {
      $location_blame = "webkit other";
#      print "$location_blame: ($bytes) $loc\n";
    } else {
      $location_blame = "unknown";
#      print "$location_blame: ($bytes) $loc\n";
    }

    # surface large outliers
    if ($bytes > 1000000 && $location_blame eq "unknown") {
      $location_blame = $loc;
    }

    $leaks{$location_blame} += $bytes;
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

process_stdin();
