#
# TEST-ASYNC.PL
# Test Win32::Internet's Asynchronous Operations
# by Aldo Calpini <dada@divinf.it>
#
# WARNING: this code is most likely to fail with almost-random errors
#          I don't know what is wrong here, any hint will be greatly
#          appreciated! 

use Win32::Internet;

$params{'flags'} = INTERNET_FLAG_ASYNC;
$params{'opentype'} = INTERNET_OPEN_TYPE_DIRECT;
$I = new Win32::Internet(\%params);

# print "Error: ", $I->Error(), "\n";
print "I.handle=", $I->{'handle'}, "\n";

$return = $I->SetStatusCallback();
print "SetStatusCallback=$return";
print "ERROR" if $return eq undef;
print "\n";

$buffer = $I->QueryOption(INTERNET_OPTION_READ_BUFFER_SIZE);
print "Buffer=$buffer\n";

$host = "ftp.activeware.com";
$user = "anonymous";
$pass = "dada\@divinf.it";


print "Doing FTP()...\n";

$handle2 = $I->FTP($FTP, $host, $user, $pass, 21, 1);

print "Returned from FTP()...\n";

($n, $t) = $I->Error();

if($n == 997) {
  print "Going asynchronous...\n";
  ($status, $info) = $I->GetStatusCallback(1);
  while($status != 100 and $status != 70) {
    if($oldstatus != $status) {
      if($status == 60) {
        $FTP->{'handle'} = $info;
      } elsif($status == 10) {
        print "resolving name...                   \n";
      } elsif($status == 11) {
        print "name resolved...                    \n";
      } elsif($status == 20) {
        print "connecting...                       \n";
      } elsif($status == 21) {
        print "connected...                        \n";
      } elsif($status == 30) {
        print "sending...                          \n";
      } elsif($status == 31) {
        print "$info bytes sent.                   \n";
      } elsif($status == 40) {    
        print "receiving...                        \n";
      } elsif($status == 41) {      
        print "$info bytes received.               \n";
      } else {
        print "status=$status\n";
      }    
    }
    $oldstatus = $status;
    ($status, $info) = $I->GetStatusCallback(1);
  }
} else {
  print "Error=", $I->Error(), "\n";
}
print "FTP.handle=", $FTP->{'handle'}, "\n";
print "STATUS(after FTP)=", $I->GetStatusCallback(1), "\n";

#                                    "/pub/microsoft/sdk/activex13.exe",

print "Doing Get()...\n";

$file = "/Perl-Win32/perl5.001m/currentBuild/110-i86.zip";

$FTP->Get($file, "110-i86.zip", 1, 0, 2);

print "Returned from Get()...\n";

($n, $t) = $I->Error();
if($n == 997) {
  print "Going asynchronous...\n";
  $bytes = 0;
  $oldstatus = 0;
  ($status, $info) = $I->GetStatusCallback(2);
  while($status != 100 and $status != 70) {
    # print "status=$status info=$info\n";
    # if($oldstatus!=$status) {
      if($status == 10) {
        print "resolving name...                   \n";
      } elsif($status == 11) {
        print "name resolved...                    \n";
      } elsif($status == 20) {
        print "connecting...                       \n";
      } elsif($status == 21) {
        print "connected...                        \n";
      #} elsif($status == 30) {
      #  print "sending...                          \n";
      } elsif($status == 31) {
        print "$info bytes sent.                   \n";
      #} elsif($status == 40) {    
      #  print "receiving...                       \n";
      } elsif($status == 41) {      
        $bytes = $bytes+$info;
        print "$bytes bytes received.              \n";
      #} else {
      #  print "status=$status\n";
      }
    # }
    $oldstatus = $status;
    undef $status, $info;
    ($status, $info) = $I->GetStatusCallback(2);
  } 
} else {
  print "Error=[$n] $t\n";
}
print "\n";
($status, $info) = $I->GetStatusCallback(2);
print "STATUS(after Get)=$status\n";
print "Error=", $I->Error(), "\n";
exit(0);


