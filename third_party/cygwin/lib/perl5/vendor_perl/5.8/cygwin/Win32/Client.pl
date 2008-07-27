use strict;
use Win32::Pipe;

####
#   You may notice that named pipe names are case INsensitive!
####

my $PipeName = "\\\\.\\pipe\\TEST this LoNG Named Pipe!";

print "I am falling asleep for few seconds, so that we give time\nFor the server to get up and running.\n";
sleep(4);
print "\nOpening a pipe ...\n";

if (my $Pipe = Win32::Pipe->new($PipeName)) {
    print "\n\nPipe has been opened, writing data to it...\n";
    print "-------------------------------------------\n";
    $Pipe->Write("\n" . Win32::Pipe::Credit() . "\n\n");
    while () {
	print "\nCommands:\n";
	print "  FILE:xxxxx  Dumps the file xxxxx.\n";
	print "  Credit      Dumps the credit screen.\n";
	print "  Quit        Quits this client (server remains running).\n";
	print "  Exit        Exits both client and server.\n";
	print "  -----------------------------------------\n";

	my $In = <STDIN>;
	chop($In);

	if ((my $File = $In) =~ s/^file:(.*)/$1/i){
	    if (-s $File) {
		if (open(FILE, "< $File")) {
		    while ($File = <FILE>) {
			$In .= $File;
		    };
		    close(FILE);
		}
	    }
	}

	if ($In =~ /^credit$/i){
	    $In = "\n" . Win32::Pipe::Credit() . "\n\n";
	}

	unless ($Pipe->Write($In)) {
	    print "Writing to pipe failed.\n";
	    last;
	}

	if ($In =~ /^(exit|quit)$/i) {
	    print "\nATTENTION: Closing due to user request.\n";
	    last;
	}
    }
    print "Closing...\n";
    $Pipe->Close();
}
else {
    my($Error, $ErrorText) = Win32::Pipe::Error();
    print "Error:$Error \"$ErrorText\"\n";
    sleep(4);
}

print "Done...\n";
