use strict;
use Win32::Pipe;

my $PipeName = "TEST this long named pipe!";
my $NewSize = 2048;
my $iMessage;

while () {
    print "Creating pipe \"$PipeName\".\n";
    if (my $Pipe = new Win32::Pipe($PipeName)) {
	my $PipeSize = $Pipe->BufferSize();
	print "This pipe's current size is $PipeSize byte" . (($PipeSize == 1)? "":"s") . ".\nWe shall change it to $NewSize ...";
	print +(($Pipe->ResizeBuffer($NewSize) == $NewSize)? "Successful":"Unsucessful") . "!\n\n";

	print "\n\nR e a d y   f o r   r e a d i n g :\n";
	print "-----------------------------------\n";

	print "Openning the pipe...\n";
	while ($Pipe->Connect()) {
	    while () {
		++$iMessage;
		print "Reading Message #$iMessage: ";
		my $In = $Pipe->Read();
		unless ($In) {
		    print "Recieved no data, closing connection....\n";
		    last;
		}
		if ($In =~ /^quit/i){
		    print "\n\nQuitting this connection....\n";
		    last;
		}
		elsif ($In =~ /^exit/i){
		    print "\n\nExitting.....\n";
		    exit;
		}
		else{
		    print "\"$In\"\n";
		}
	    }
	    print "Disconnecting...\n";
	    $Pipe->Disconnect();
	}
	print "Closing...\n";
	$Pipe->Close();
    }
}

print "You can't get here...\n";
