package Win32::ODBC;

$VERSION = '0.032';

# Win32::ODBC.pm
#       +==========================================================+
#       |                                                          |
#       |                     ODBC.PM package                      |
#       |                     ---------------                      |
#       |                                                          |
#       | Copyright (c) 1996, 1997 Dave Roth. All rights reserved. |
#       |   This program is free software; you can redistribute    |
#       | it and/or modify it under the same terms as Perl itself. |
#       |                                                          |
#       +==========================================================+
#
#
#         based on original code by Dan DeMaggio (dmag@umich.edu)
#
#	Use under GNU General Public License or Larry Wall's "Artistic License"
#
#	Check the README.TXT file that comes with this package for details about
#	it's history.
#

require Exporter;
require DynaLoader;

$ODBCPackage = "Win32::ODBC";
$ODBCPackage::Version = 970208;
$::ODBC = $ODBCPackage;
$CacheConnection = 0;

    #   Reserve ODBC in the main namespace for US!
*ODBC::=\%Win32::ODBC::;


@ISA= qw( Exporter DynaLoader );
    # Items to export into callers namespace by default. Note: do not export
    # names by default without a very good reason. Use EXPORT_OK instead.
    # Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
            ODBC_ADD_DSN
            ODBC_REMOVE_DSN
            ODBC_CONFIG_DSN
            ODBC_ADD_SYS_DSN
            ODBC_REMOVE_SYS_DSN
            ODBC_CONFIG_SYS_DSN

            SQL_DONT_CLOSE
            SQL_DROP
            SQL_CLOSE
            SQL_UNBIND
            SQL_RESET_PARAMS

            SQL_FETCH_NEXT
            SQL_FETCH_FIRST
            SQL_FETCH_LAST
            SQL_FETCH_PRIOR
            SQL_FETCH_ABSOLUTE
            SQL_FETCH_RELATIVE
            SQL_FETCH_BOOKMARK

            SQL_COLUMN_COUNT
            SQL_COLUMN_NAME
            SQL_COLUMN_TYPE
            SQL_COLUMN_LENGTH
            SQL_COLUMN_PRECISION
            SQL_COLUMN_SCALE
            SQL_COLUMN_DISPLAY_SIZE
            SQL_COLUMN_NULLABLE
            SQL_COLUMN_UNSIGNED
            SQL_COLUMN_MONEY
            SQL_COLUMN_UPDATABLE
            SQL_COLUMN_AUTO_INCREMENT
            SQL_COLUMN_CASE_SENSITIVE
            SQL_COLUMN_SEARCHABLE
            SQL_COLUMN_TYPE_NAME
            SQL_COLUMN_TABLE_NAME
            SQL_COLUMN_OWNER_NAME
            SQL_COLUMN_QUALIFIER_NAME
            SQL_COLUMN_LABEL
            SQL_COLATT_OPT_MAX
            SQL_COLUMN_DRIVER_START
            SQL_COLATT_OPT_MIN
            SQL_ATTR_READONLY
            SQL_ATTR_WRITE
            SQL_ATTR_READWRITE_UNKNOWN
            SQL_UNSEARCHABLE
            SQL_LIKE_ONLY
            SQL_ALL_EXCEPT_LIKE
            SQL_SEARCHABLE
        );
    #The above are included for backward compatibility


sub new
{
    my ($n, $self);
    my ($type) = shift;
    my ($DSN) = shift;
    my (@Results) = @_;

    if (ref $DSN){
        @Results = ODBCClone($DSN->{'connection'});
    }else{
        @Results = ODBCConnect($DSN, @Results);
    }
    @Results = processError(-1, @Results);
    if (! scalar(@Results)){
        return undef;
    }
    $self = bless {};
    $self->{'connection'} = $Results[0];
    $ErrConn = $Results[0];
    $ErrText = $Results[1];
    $ErrNum = 0;
    $self->{'DSN'} = $DSN;
    $self;
}

####
#   Close this ODBC session (or all sessions)
####
sub Close
{
    my ($self, $Result) = shift;
    $Result = DESTROY($self);
    $self->{'connection'} = -1;
    return $Result;
}

####
#   Auto-Kill an instance of this module
####
sub DESTROY
{
    my ($self) = shift;
    my (@Results) = (0);
    if($self->{'connection'} > -1){
        @Results = ODBCDisconnect($self->{'connection'});
        @Results = processError($self, @Results);
        if ($Results[0]){
            undef $self->{'DSN'};
            undef @{$self->{'fnames'}};
            undef %{$self->{'field'}};
            undef %{$self->{'connection'}};
        }
    }
    return $Results[0];
}


sub sql{
    return (Sql(@_));
}

####
#   Submit an SQL Execute statement for processing
####
sub Sql{
    my ($self, $Sql, @Results) = @_;
    @Results = ODBCExecute($self->{'connection'}, $Sql);
    return updateResults($self, @Results);
}

####
#   Retrieve data from a particular field
####
sub Data{

        #   Change by JOC 06-APR-96
        #   Altered by Dave Roth <dave@roth.net> 96.05.07
    my($self) = shift;
    my(@Fields) = @_;
    my(@Results, $Results, $Field);

    if ($self->{'Dirty'}){
        GetData($self);
        $self->{'Dirty'} = 0;
    }
    @Fields = @{$self->{'fnames'}} if (! scalar(@Fields));
    foreach $Field (@Fields) {
        if (wantarray) {
            push(@Results, data($self, $Field));
        } else {
            $Results .= data($self, $Field);
        }
    }
    return wantarray ? @Results : $Results;
}

sub DataHash{
    my($self, @Results) = @_;
    my(%Results, $Element);

    if ($self->{'Dirty'}){
        GetData($self);
        $self->{'Dirty'} = 0;
    }
    @Results = @{$self->{'fnames'}} if (! scalar(@Results));
    foreach $Element (@Results) {
        $Results{$Element} = data($self, $Element);
    }

    return %Results;
}

####
#   Retrieve data from the data buffer
####
sub data
{  $_[0]->{'data'}->{$_[1]}; }


sub fetchrow{
    return (FetchRow(@_));
}
####
#   Put a row from an ODBC data set into data buffer
####
sub FetchRow{
    my ($self, @Results) = @_;
    my ($item, $num, $sqlcode);
        # Added by JOC 06-APR-96
        #   $num = 0;
    $num = 0;
    undef $self->{'data'};


    @Results = ODBCFetch($self->{'connection'}, @Results);
    if (! (@Results = processError($self, @Results))){
        ####
        #   There should be an innocuous error "No records remain"
        #   This indicates no more records in the dataset
        ####
        return undef;
    }
        #   Set the Dirty bit so we will go and extract data via the
        #   ODBCGetData function. Otherwise use the cache.
    $self->{'Dirty'} = 1;

        #   Return the array of field Results.
    return @Results;
}

sub GetData{
    my($self) = @_;
    my @Results;
    my $num = 0;

    @Results = ODBCGetData($self->{'connection'});
    if (!(@Results = processError($self, @Results))){
        return undef;
    }
        ####
        #   This is a special case. Do not call processResults
        ####
    ClearError();
    foreach (@Results){
        s/ +$// if defined $_; # HACK
        $self->{'data'}->{ ${$self->{'fnames'}}[$num] } = $_;
        $num++;
    }
        #   return is a hack to interface with a assoc array.
    return wantarray? (1, 1): 1;
}

####
#   See if any more ODBC Results Sets
#		Added by Brian Dunfordshore <Brian_Dunfordshore@bridge.com> 
#		96.07.10
####
sub MoreResults{
    my ($self) = @_;

    my(@Results) = ODBCMoreResults($self->{'connection'});
    return (processError($self, @Results))[0];
}

####
#   Retrieve the catalog from the current DSN
#	NOTE: All Field names are uppercase!!!
####
sub Catalog{
    my ($self) = shift;
    my ($Qualifier, $Owner, $Name, $Type) = @_;
    my (@Results) = ODBCTableList($self->{'connection'}, $Qualifier, $Owner, $Name, $Type);

        #   If there was an error return 0 else 1
    return (updateResults($self, @Results) != 1);
}

####
#   Return an array of names from the catalog for the current DSN
#       TableList($Qualifier, $Owner, $Name, $Type)
#           Return: (array of names of tables)
#	NOTE: All Field names are uppercase!!!
####
sub TableList{
    my ($self) = shift;
    my (@Results) = @_;
    if (! scalar(@Results)){
        @Results = ("", "", "%", "TABLE");
    }

    if (! Catalog($self, @Results)){
        return undef;
    }
    undef @Results;
    while (FetchRow($self)){
        push(@Results, Data($self, "TABLE_NAME"));
    }
    return sort(@Results);
}


sub fieldnames{
    return (FieldNames(@_));
}
####
#   Return an array of fieldnames extracted from the current dataset
####
sub FieldNames { $self = shift; return @{$self->{'fnames'}}; }


####
#   Closes this connection. This is used mostly for testing. You should
#   probably use Close().
####
sub ShutDown{
    my($self) = @_;
    print "\nClosing connection $self->{'connection'}...";
    $self->Close();
    print "\nDone\n";
}

####
#   Return this connection number
####
sub Connection{
    my($self) = @_;
    return $self->{'connection'};
}

####
#   Returns the current connections that are in use.
####
sub GetConnections{
    return ODBCGetConnections();
}

####
#   Set the Max Buffer Size for this connection. This determines just how much
#   ram can be allocated when a fetch() is performed that requires a HUGE amount
#   of memory. The default max is 10k and the absolute max is 100k.
#   This will probably never be used but I put it in because I noticed a fetch()
#   of a MEMO field in an Access table was something like 4Gig. Maybe I did
#   something wrong, but after checking several times I decided to impliment
#   this limit thingie.
####
sub SetMaxBufSize{
    my($self, $Size) = @_;
    my(@Results) = ODBCSetMaxBufSize($self->{'connection'}, $Size);
    return (processError($self, @Results))[0];
}

####
#   Returns the Max Buffer Size for this connection. See SetMaxBufSize().
####
sub GetMaxBufSize{
    my($self) = @_;
    my(@Results) = ODBCGetMaxBufSize($self->{'connection'});
    return (processError($self, @Results))[0];
}


####
#   Returns the DSN for this connection as an associative array.
####
sub GetDSN{
    my($self, $DSN) = @_;
    if(! ref($self)){
        $DSN = $self;
        $self = 0;
    }
    if (! $DSN){
        $self = $self->{'connection'};
	}
    my(@Results) = ODBCGetDSN($self, $DSN);
    return (processError($self, @Results));
}

####
#   Returns an associative array of $XXX{'DSN'}=Description
####
sub DataSources{
    my($self, $DSN) = @_;
    if(! ref $self){
        $DSN = $self;
        $self = 0;
    }
    my(@Results) = ODBCDataSources($DSN);
    return (processError($self, @Results));
}

####
#   Returns an associative array of $XXX{'Driver Name'}=Driver Attributes
####
sub Drivers{
    my($self) = @_;
    if(! ref $self){
        $self = 0;
    }
    my(@Results) = ODBCDrivers();
    return (processError($self, @Results));
}

####
#   Returns the number of Rows that were affected by the previous SQL command.
####
sub RowCount{
    my($self, $Connection) = @_;
    if (! ref($self)){
        $Connection = $self;
        $self = 0;
    }
    if (! $Connection){$Connection = $self->{'connection'};}
    my(@Results) = ODBCRowCount($Connection);
    return (processError($self, @Results))[0];
}

####
#   Returns the Statement Close Type -- how does ODBC Close a statment.
#       Types:
#           SQL_DROP
#           SQL_CLOSE
#           SQL_UNBIND
#           SQL_RESET_PARAMS
####
sub GetStmtCloseType{
    my($self, $Connection) = @_;
    if (! ref($self)){
        $Connection = $self;
        $self = 0;
    }
    if (! $Connection){$Connection = $self->{'connection'};}
    my(@Results) = ODBCGetStmtCloseType($Connection);
    return (processError($self, @Results));
}

####
#   Sets the Statement Close Type -- how does ODBC Close a statment.
#       Types:
#           SQL_DROP
#           SQL_CLOSE
#           SQL_UNBIND
#           SQL_RESET_PARAMS
#   Returns the newly set value.
####
sub SetStmtCloseType{
    my($self, $Type, $Connection) = @_;
    if (! ref($self)){
        $Connection = $Type;
        $Type = $self;
        $self = 0;
    }
    if (! $Connection){$Connection = $self->{'connection'};}
    my(@Results) = ODBCSetStmtCloseType($Connection, $Type);
    return (processError($self, @Results))[0];
}

sub ColAttributes{
    my($self, $Type, @Field) = @_;
    my(%Results, @Results, $Results, $Attrib, $Connection, $Temp);
    if (! ref($self)){
        $Type = $Field;
        $Field = $self;
        $self = 0;
    }
    $Connection = $self->{'connection'};
    if (! scalar(@Field)){ @Field = $self->fieldnames;}
    foreach $Temp (@Field){
        @Results = ODBCColAttributes($Connection, $Temp, $Type);
        ($Attrib) = processError($self, @Results);
        if (wantarray){
            $Results{$Temp} = $Attrib;
        }else{
            $Results .= "$Temp";
        }
    }
    return wantarray? %Results:$Results;
}

sub GetInfo{
    my($self, $Type) = @_;
    my($Connection, @Results);
    if(! ref $self){
        $Type = $self;
        $self = 0;
        $Connection = 0;
    }else{
        $Connection = $self->{'connection'};
    }
    @Results = ODBCGetInfo($Connection, $Type);
    return (processError($self, @Results))[0];
}

sub GetConnectOption{
    my($self, $Type) = @_;
    my(@Results);
    if(! ref $self){
        $Type = $self;
        $self = 0;
    }
    @Results = ODBCGetConnectOption($self->{'connection'}, $Type);
    return (processError($self, @Results))[0];
}

sub SetConnectOption{
    my($self, $Type, $Value) = @_;
    if(! ref $self){
        $Value = $Type;
        $Type = $self;
        $self = 0;
    }
    my(@Results) = ODBCSetConnectOption($self->{'connection'}, $Type, $Value);
    return (processError($self, @Results))[0];
}


sub Transact{
    my($self, $Type) = @_;
    my(@Results);
    if(! ref $self){
        $Type = $self;
        $self = 0;
    }
    @Results = ODBCTransact($self->{'connection'}, $Type);
    return (processError($self, @Results))[0];
}


sub SetPos{
    my($self, @Results) = @_;
    @Results = ODBCSetPos($self->{'connection'}, @Results);
    $self->{'Dirty'} = 1;
    return (processError($self, @Results))[0];
}

sub ConfigDSN{
    my($self) = shift @_;
    my($Type, $Connection);
    if(! ref $self){
        $Type = $self;
        $Connection = 0;
        $self = 0;
    }else{
        $Type = shift @_;
        $Connection = $self->{'connection'};
    }
    my($Driver, @Attributes) = @_;
    @Results = ODBCConfigDSN($Connection, $Type, $Driver, @Attributes);
    return (processError($self, @Results))[0];
}


sub Version{
	my($self, @Packages) = @_;
    my($Temp, @Results);
	if (! ref($self)){
		push(@Packages, $self);
	}
	my($ExtName, $ExtVersion) = Info();
	if (! scalar(@Packages)){
		@Packages = ("ODBC.PM", "ODBC.PLL");
	}
	foreach $Temp (@Packages){
		if ($Temp =~ /pll/i){
            push(@Results, "ODBC.PM:$Win32::ODBC::Version");
		}elsif ($Temp =~ /pm/i){
            push(@Results, "ODBC.PLL:$ExtVersion");
		}
	}
    return @Results;
}


sub SetStmtOption{
    my($self, $Option, $Value) = @_;
    if(! ref $self){
        $Value = $Option;
        $Option = $self;
        $self = 0;
    }
    my(@Results) = ODBCSetStmtOption($self->{'connection'}, $Option, $Value);
    return (processError($self, @Results))[0];
}

sub GetStmtOption{
    my($self, $Type) = @_;
    if(! ref $self){
        $Type = $self;
        $self = 0;
    }
    my(@Results) = ODBCGetStmtOption($self->{'connection'}, $Type);
    return (processError($self, @Results))[0];
}

sub GetFunctions{
    my($self, @Results)=@_;
    @Results = ODBCGetFunctions($self->{'connection'}, @Results);
    return (processError($self, @Results));
}

sub DropCursor{
    my($self) = @_;
    my(@Results) = ODBCDropCursor($self->{'connection'});
    return (processError($self, @Results))[0];
}

sub SetCursorName{
    my($self, $Name) = @_;
    my(@Results) = ODBCSetCursorName($self->{'connection'}, $Name);
    return (processError($self, @Results))[0];
}

sub GetCursorName{
    my($self) = @_;
    my(@Results) = ODBCGetCursorName($self->{'connection'});
    return (processError($self, @Results))[0];
}

sub GetSQLState{
    my($self) = @_;
    my(@Results) = ODBCGetSQLState($self->{'connection'});
    return (processError($self, @Results))[0];
}


# ----------- R e s u l t   P r o c e s s i n g   F u n c t i o n s ----------
####
#   Generic processing of data into associative arrays
####
sub updateResults{
    my ($self, $Error, @Results) = @_;

    undef %{$self->{'field'}};

    ClearError($self);
    if ($Error){
        SetError($self, $Results[0], $Results[1]);
        return ($Error);
    }

    @{$self->{'fnames'}} = @Results;

    foreach (0..$#{$self->{'fnames'}}){
        s/ +$//;
        $self->{'field'}->{${$self->{'fnames'}}[$_]} = $_;
    }
    return undef;
}

# ----------------------------------------------------------------------------
# ----------------- D e b u g g i n g   F u n c t i o n s --------------------

sub Debug{
    my($self, $iDebug, $File) = @_;
    my(@Results);
    if (! ref($self)){
        if (defined $self){
            $File = $iDebug;
            $iDebug = $self;
        }
        $Connection = 0;
        $self = 0;
    }else{
        $Connection = $self->{'connection'};
    }
    push(@Results, ($Connection, $iDebug));
    push(@Results, $File) if ($File ne "");
    @Results = ODBCDebug(@Results);
    return (processError($self, @Results))[0];
}

####
#   Prints out the current dataset (used mostly for testing)
####
sub DumpData {
    my($self) = @_; my($f, $goo);

        #   Changed by JOC 06-Apr-96
        #   print "\nDumping Data for connection: $conn->{'connection'}\n";
    print "\nDumping Data for connection: $self->{'connection'}\n";
    print "Error: \"";
    print $self->Error();
    print "\"\n";
    if (! $self->Error()){
       foreach $f ($self->FieldNames){
            print $f . " ";
            $goo .= "-" x length($f);
            $goo .= " ";
        }
        print "\n$goo\n";
        while ($self->FetchRow()){
            foreach $f ($self->FieldNames){
                print $self->Data($f) . " ";
            }
            print "\n";
        }
    }
}

sub DumpError{
    my($self) = @_;
    my($ErrNum, $ErrText, $ErrConn);
    my($Temp);

    print "\n---------- Error Report: ----------\n";
    if (ref $self){
        ($ErrNum, $ErrText, $ErrConn) = $self->Error();
        ($Temp = $self->GetDSN()) =~ s/.*DSN=(.*?);.*/$1/i;
        print "Errors for \"$Temp\" on connection " . $self->{'connection'} . ":\n";
    }else{
        ($ErrNum, $ErrText, $ErrConn) = Error();
        print "Errors for the package:\n";
    }

    print "Connection Number: $ErrConn\nError number: $ErrNum\nError message: \"$ErrText\"\n";
    print "-----------------------------------\n";

}

####
#   Submit an SQL statement and print data about it (used mostly for testing)
####
sub Run{
    my($self, $Sql) = @_;

    print "\nExcecuting connection $self->{'connection'}\nsql statement: \"$Sql\"\n";
    $self->Sql($Sql);
    print "Error: \"";
    print $self->error;
    print "\"\n";
    print "--------------------\n\n";
}

# ----------------------------------------------------------------------------
# ----------- E r r o r   P r o c e s s i n g   F u n c t i o n s ------------

####
#   Process Errors returned from a call to ODBCxxxx().
#   It is assumed that the Win32::ODBC function returned the following structure:
#      ($ErrorNumber, $ResultsText, ...)
#           $ErrorNumber....0 = No Error
#                           >0 = Error Number
#           $ResultsText.....if no error then this is the first Results element.
#                           if error then this is the error text.
####
sub processError{
    my($self, $Error, @Results) = @_;
    if ($Error){
        SetError($self, $Results[0], $Results[1]);
        undef @Results;
    }
    return @Results;
}

####
#   Return the last recorded error message
####
sub error{
    return (Error(@_));
}

sub Error{
    my($self) = @_;
    if(ref($self)){
        if($self->{'ErrNum'}){
            my($State) = ODBCGetSQLState($self->{'connection'});
            return (wantarray)? ($self->{'ErrNum'}, $self->{'ErrText'}, $self->{'connection'}, $State) :"[$self->{'ErrNum'}] [$self->{'connection'}] [$State] \"$self->{'ErrText'}\"";
        }
    }elsif ($ErrNum){
        return (wantarray)? ($ErrNum, $ErrText, $ErrConn):"[$ErrNum] [$ErrConn] \"$ErrText\"";
    }
    return undef
}

####
#   SetError:
#       Assume that if $self is not a reference then it is just a placeholder
#       and should be ignored.
####
sub SetError{
    my($self, $Num, $Text, $Conn) = @_;
    if (ref $self){
        $self->{'ErrNum'} = $Num;
        $self->{'ErrText'} = $Text;
        $Conn = $self->{'connection'} if ! $Conn;
    }
    $ErrNum = $Num;
    $ErrText = $Text;

        ####
        #   Test Section Begin
        ####
#    $! = ($Num, $Text);
        ####
        #   Test Section End
        ####

    $ErrConn = $Conn;
}

sub ClearError{
    my($self, $Num, $Text) = @_;
    if (ref $self){
        undef $self->{'ErrNum'};
        undef $self->{'ErrText'};
    }else{
        undef $ErrConn;
        undef $ErrNum;
        undef $ErrText;
    }
    ODBCCleanError();
    return 1;
}


sub GetError{
    my($self, $Connection) = @_;
    my(@Results);
    if (! ref($self)){
        $Connection = $self;
        $self = 0;
    }else{
        if (! defined($Connection)){
            $Connection = $self->{'connection'};
        }
    }

    @Results = ODBCGetError($Connection);
    return @Results;
}




# ----------------------------------------------------------------------------
# ------------------ A U T O L O A D   F U N C T I O N -----------------------

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    #reset $! to zero to reset any current errors.
    local $! = 0;
    $val = constant($constname);

    if ($! != 0) {
    if ($! =~ /Invalid/) {
        $AutoLoader::AUTOLOAD = $AUTOLOAD;
        goto &AutoLoader::AUTOLOAD;
    }
    else {

            # Added by JOC 06-APR-96
            # $pack = 0;
        $pack = 0;
        ($pack,$file,$line) = caller;
            print "Your vendor has not defined Win32::ODBC macro $constname, used in $file at line $line.";
    }
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}


    #   --------------------------------------------------------------
    #
    #
    #   Make sure that we shutdown ODBC and free memory even if we are
    #   using perlis.dll on Win32 platform!
END{
#    ODBCShutDown() unless $CacheConnection;
}


bootstrap Win32::ODBC;

# Preloaded methods go here.

# Autoload methods go after __END__, and are processed by the autosplit program.

1;
__END__

=head1 NAME

Win32::ODBC - ODBC Extension for Win32

=head1 SYNOPSIS

To use this module, include the following statement at the top of your
script:

    use Win32::ODBC;

Next, create a data connection to your DSN:

    $Data = new Win32::ODBC("MyDSN");

B<NOTE>: I<MyDSN> can be either the I<DSN> as defined in the ODBC
Administrator, I<or> it can be an honest-to-God I<DSN Connect String>.

    Example: "DSN=My Database;UID=Brown Cow;PWD=Moo;"

You should check to see if C<$Data> is indeed defined, otherwise there
has been an error.

You can now send SQL queries and retrieve info to your heart's
content! See the description of the methods provided by this module
below and also the file F<TEST.PL> as referred to in L<INSTALLATION
NOTES> to see how it all works.

Finally, B<MAKE SURE> that you close your connection when you are
finished:

    $Data->Close();

=head1 DESCRIPTION

=head2 Background

This is a hack of Dan DeMaggio's <dmag@umich.edu> F<NTXS.C> ODBC
implementation. I have recoded and restructured most of it including
most of the F<ODBC.PM> package, but its very core is still based on
Dan's code (thanks Dan!).

The history of this extension is found in the file F<HISTORY.TXT> that
comes with the original archive (see L<INSTALLATION NOTES> below).

=head2 Benefits

And what are the benefits of this module?

=over

=item *

The number of ODBC connections is limited by memory and ODBC itself
(have as many as you want!).

=item *

The working limit for the size of a field is 10,240 bytes, but you can
increase that limit (if needed) to a max of 2,147,483,647 bytes. (You
can always recompile to increase the max limit.)

=item *

You can open a connection by either specifing a DSN or a connection
string!

=item *

You can open and close the connections in any order!

=item *

Other things that I can not think of right now... :)

=back

=head1 CONSTANTS

This package defines a number of constants. You may refer to each of
these constants using the notation C<ODBC::xxxxx>, where C<xxxxx> is
the constant.

Example:

   print ODBC::SQL_SQL_COLUMN_NAME, "\n";

=head1 SPECIAL NOTATION

For the method documentation that follows, an B<*> following the
method parameters indicates that that method is new or has been
modified for this version.

=head1 CONSTRUCTOR

=over

=item new ( ODBC_OBJECT | DSN [, (OPTION1, VALUE1), (OPTION2, VALUE2) ...] )
*

Creates a new ODBC connection based on C<DSN>, or, if you specify an
already existing ODBC object, then a new ODBC object will be created
but using the ODBC Connection specified by C<ODBC_OBJECT>. (The new
object will be a new I<hstmt> using the I<hdbc> connection in
C<ODBC_OBJECT>.)

C<DSN> is I<Data Source Name> or a proper C<ODBCDriverConnect> string.

You can specify SQL Connect Options that are implemented before the
actual connection to the DSN takes place. These option/values are the
same as specified in C<GetConnectOption>/C<SetConnectOption> (see
below) and are defined in the ODBC API specs.

Returns a handle to the database on success, or I<undef> on failure.

=back

=head1 METHODS

=over

=item Catalog ( QUALIFIER, OWNER, NAME, TYPE )

Tells ODBC to create a data set that contains table information about
the DSN. Use C<Fetch> and C<Data> or C<DataHash> to retrieve the data.
The returned format is:

    [Qualifier] [Owner] [Name] [Type]

Returns I<true> on error.

=item ColAttributes ( ATTRIBUTE [, FIELD_NAMES ] )

Returns the attribute C<ATTRIBUTE> on each of the fields in the list
C<FIELD_NAMES> in the current record set. If C<FIELD_NAMES> is empty,
then all fields are assumed. The attributes are returned as an
associative array.

=item ConfigDSN ( OPTION, DRIVER, ATTRIBUTE1 [, ATTRIBUTE2, ATTRIBUTE3, ...
] )

Configures a DSN. C<OPTION> takes on one of the following values:

    ODBC_ADD_DSN.......Adds a new DSN.
    ODBC_MODIFY_DSN....Modifies an existing DSN.
    ODBC_REMOVE_DSN....Removes an existing DSN.

    ODBC_ADD_SYS_DSN.......Adds a new System DSN.
    ODBC_MODIFY_SYS_DSN....Modifies an existing System DSN.
    ODBC_REMOVE_SYS_DSN....Removes an existing System DSN.

You must specify the driver C<DRIVER> (which can be retrieved by using
C<DataSources> or C<Drivers>).

C<ATTRIBUTE1> B<should> be I<"DSN=xxx"> where I<xxx> is the name of
the DSN. Other attributes can be any DSN attribute such as:

    "UID=Cow"
    "PWD=Moo"
    "Description=My little bitty Data Source Name"

Returns I<true> on success, I<false> on failure.

B<NOTE 1>: If you use C<ODBC_ADD_DSN>, then you must include at least
I<"DSN=xxx"> and the location of the database.

Example: For MS Access databases, you must specify the
I<DatabaseQualifier>:

    "DBQ=c:\\...\\MyDatabase.mdb"

B<NOTE 2>: If you use C<ODBC_MODIFY_DSN>, then you need only specify
the I<"DNS=xxx"> attribute. Any other attribute you include will be
changed to what you specify.

B<NOTE 3>: If you use C<ODBC_REMOVE_DSN>, then you need only specify
the I<"DSN=xxx"> attribute.

=item Connection ()

Returns the connection number associated with the ODBC connection.

=item Close ()

Closes the ODBC connection. No return value.

=item Data ( [ FIELD_NAME ] )

Returns the contents of column name C<FIELD_NAME> or the current row
(if nothing is specified).

=item DataHash ( [ FIELD1, FIELD2, ... ] )

Returns the contents for C<FIELD1, FIELD2, ...> or the entire row (if
nothing is specified) as an associative array consisting of:

    {Field Name} => Field Data

=item DataSources ()

Returns an associative array of Data Sources and ODBC remarks about them.
They are returned in the form of:

    $ArrayName{'DSN'}=Driver

where I<DSN> is the Data Source Name and ODBC Driver used.

=item Debug ( [ 1 | 0 ] )

Sets the debug option to on or off. If nothing is specified, then
nothing is changed.

Returns the debugging value (I<1> or I<0>).

=item Drivers ()

Returns an associative array of ODBC Drivers and their attributes.
They are returned in the form of:

    $ArrayName{'DRIVER'}=Attrib1;Attrib2;Attrib3;...

where I<DRIVER> is the ODBC Driver Name and I<AttribX> are the
driver-defined attributes.

=item DropCursor ( [ CLOSE_TYPE ] )

Drops the cursor associated with the ODBC object. This forces the
cursor to be deallocated. This overrides C<SetStmtCloseType>, but the
ODBC object does not lose the C<StmtCloseType> setting. C<CLOSE_TYPE>
can be any valid C<SmtpCloseType> and will perform a close on the stmt
using the specified close type.

Returns I<true> on success, I<false> on failure.

=item DumpData ()

Dumps to the screen the fieldnames and all records of the current data
set. Used primarily for debugging. No return value.

=item Error ()

Returns the last encountered error. The returned value is context
dependent:

If called in a I<scalar> context, then a I<3-element array> is
returned:

    ( ERROR_NUMBER, ERROR_TEXT, CONNECTION_NUMBER )

If called in a I<string> context, then a I<string> is returned:

    "[ERROR_NUMBER] [CONNECTION_NUMBER] [ERROR_TEXT]"

If debugging is on then two more variables are returned:

    ( ..., FUNCTION, LEVEL )

where C<FUNCTION> is the name of the function in which the error
occurred, and C<LEVEL> represents extra information about the error
(usually the location of the error).

=item FetchRow ( [ ROW [, TYPE ] ] )

Retrieves the next record from the keyset. When C<ROW> and/or C<TYPE>
are specified, the call is made using C<SQLExtendedFetch> instead of
C<SQLFetch>.

B<NOTE 1>: If you are unaware of C<SQLExtendedFetch> and its
implications, stay with just regular C<FetchRow> with no parameters.

B<NOTE 2>: The ODBC API explicitly warns against mixing calls to
C<SQLFetch> and C<SQLExtendedFetch>; use one or the other but not
both.

If I<ROW> is specified, it moves the keyset B<RELATIVE> C<ROW> number
of rows.

If I<ROW> is specified and C<TYPE> is B<not>, then the type used is
B<RELATIVE>.

Returns I<true> when another record is available to read, and I<false>
when there are no more records.

=item FieldNames ()

Returns an array of fieldnames found in the current data set. There is
no guarantee on order.

=item GetConnections ()

Returns an array of connection numbers showing what connections are
currently open.

=item GetConnectOption ( OPTION )

Returns the value of the specified connect option C<OPTION>. Refer to
ODBC documentation for more information on the options and values.

Returns a string or scalar depending upon the option specified.

=item GetCursorName ()

Returns the name of the current cursor as a string or I<undef>.

=item GetData ()

Retrieves the current row from the dataset. This is not generally
used by users; it is used internally.

Returns an array of field data where the first element is either
I<false> (if successful) and I<true> (if B<not> successful).

=item getDSN ( [ DSN ] )

Returns an associative array indicating the configuration for the
specified DSN.

If no DSN is specified then the current connection is used.

The returned associative array consists of:

    keys=DSN keyword; values=Keyword value. $Data{$Keyword}=Value

=item GetFunctions ( [ FUNCTION1, FUNCTION2, ... ] )

Returns an associative array indicating the ability of the ODBC Driver
to support the specified functions. If no functions are specified,
then a 100 element associative array is returned containing all
possible functions and their values.

C<FUNCTION> must be in the form of an ODBC API constant like
C<SQL_API_SQLTRANSACT>.

The returned array will contain the results like:

    $Results{SQL_API_SQLTRANSACT}=Value

Example:

    $Results = $O->GetFunctions(
                                $O->SQL_API_SQLTRANSACT,
                                SQL_API_SQLSETCONNECTOPTION
                               );
    $ConnectOption = $Results{SQL_API_SQLSETCONNECTOPTION};
    $Transact = $Results{SQL_API_SQLTRANSACT};

=item GetInfo ( OPTION )

Returns a string indicating the value of the particular
option specified.

=item GetMaxBufSize ()

Returns the current allocated limit for I<MaxBufSize>. For more info,
see C<SetMaxBufSize>.

=item GetSQLState () *

Returns a string indicating the SQL state as reported by ODBC. The SQL
state is a code that the ODBC Manager or ODBC Driver returns after the
execution of a SQL function. This is helpful for debugging purposes.

=item GetStmtCloseType ( [ CONNECTION ] )

Returns a string indicating the type of closure that will be used
everytime the I<hstmt> is freed. See C<SetStmtCloseType> for details.

By default, the connection of the current object will be used. If
C<CONNECTION> is a valid connection number, then it will be used.

=item GetStmtOption ( OPTION )

Returns the value of the specified statement option C<OPTION>. Refer
to ODBC documentation for more information on the options and values.

Returns a string or scalar depending upon the option specified.

=item MoreResults ()

This will report whether there is data yet to be retrieved from the
query. This can happen if the query was a multiple select.

Example:

    "SELECT * FROM [foo] SELECT * FROM [bar]"

B<NOTE>: Not all drivers support this.

Returns I<1> if there is more data, I<undef> otherwise.

=item RowCount ( CONNECTION )

For I<UPDATE>, I<INSERT> and I<DELETE> statements, the returned value
is the number of rows affected by the request or I<-1> if the number
of affected rows is not available.

B<NOTE 1>: This function is not supported by all ODBC drivers! Some
drivers do support this but not for all statements (e.g., it is
supported for I<UPDATE>, I<INSERT> and I<DELETE> commands but not for
the I<SELECT> command).

B<NOTE 2>: Many data sources cannot return the number of rows in a
result set before fetching them; for maximum interoperability,
applications should not rely on this behavior.

Returns the number of affected rows, or I<-1> if not supported by the
driver in the current context.

=item Run ( SQL )

Executes the SQL command B<SQL> and dumps to the screen info about
it. Used primarily for debugging.

No return value.

=item SetConnectOption ( OPTION ) *

Sets the value of the specified connect option B<OPTION>. Refer to
ODBC documentation for more information on the options and values.

Returns I<true> on success, I<false> otherwise.

=item SetCursorName ( NAME ) *

Sets the name of the current cursor.

Returns I<true> on success, I<false> otherwise.

=item SetPos ( ROW [, OPTION, LOCK ] ) *

Moves the cursor to the row C<ROW> within the current keyset (B<not>
the current data/result set).

Returns I<true> on success, I<false> otherwise.

=item SetMaxBufSize ( SIZE )

This sets the I<MaxBufSize> for a particular connection. This will
most likely never be needed but...

The amount of memory that is allocated to retrieve the field data of a
record is dynamic and changes when it need to be larger. I found that
a memo field in an MS Access database ended up requesting 4 Gig of
space. This was a bit much so there is an imposed limit (2,147,483,647
bytes) that can be allocated for data retrieval.

Since it is possible that someone has a database with field data
greater than 10,240, you can use this function to increase the limit
up to a ceiling of 2,147,483,647 (recompile if you need more).

Returns the max number of bytes.

=item SetStmtCloseType ( TYPE [, CONNECTION ] )

Sets a particular I<hstmt> close type for the connection. This is the
same as C<ODBCFreeStmt(hstmt, TYPE)>. By default, the connection of
the current object will be used. If C<CONNECTION> is a valid
connection number, then it will be used.

C<TYPE> may be one of:

    SQL_CLOSE
    SQL_DROP
    SQL_UNBIND
    SQL_RESET_PARAMS

Returns a string indicating the newly set type.

=item SetStmtOption ( OPTION ) *

Sets the value of the specified statement option C<OPTION>. Refer to
ODBC documentation for more information on the options and values.

Returns I<true> on success, I<false> otherwise.

=item ShutDown ()

Closes the ODBC connection and dumps to the screen info about
it. Used primarily for debugging.

No return value.

=item Sql ( SQL_STRING )

Executes the SQL command C<SQL_STRING> on the current connection.

Returns I<?> on success, or an error number on failure.

=item TableList ( QUALIFIER, OWNER, NAME, TYPE )

Returns the catalog of tables that are available in the DSN. For an
unknown parameter, just specify the empty string I<"">.

Returns an array of table names.

=item Transact ( TYPE ) *

Forces the ODBC connection to perform a I<rollback> or I<commit>
transaction.

C<TYPE> may be one of:

    SQL_COMMIT
    SQL_ROLLBACK

B<NOTE>: This only works with ODBC drivers that support transactions.
Your driver supports it if I<true> is returned from:

    $O->GetFunctions($O->SQL_API_SQLTRANSACT)[1]

(See C<GetFunctions> for more details.)

Returns I<true> on success, I<false> otherwise.

=item Version ( PACKAGES )

Returns an array of version numbers for the requested packages
(F<ODBC.pm> or F<ODBC.PLL>). If the list C<PACKAGES> is empty, then
all version numbers are returned.

=back

=head1 LIMITATIONS

What known problems does this thing have?

=over

=item *

If the account under which the process runs does not have write
permission on the default directory (for the process, not the ODBC
DSN), you will probably get a runtime error during a
C<SQLConnection>. I don't think that this is a problem with the code,
but more like a problem with ODBC. This happens because some ODBC
drivers need to write a temporary file. I noticed this using the MS
Jet Engine (Access Driver).

=item *

This module has been neither optimized for speed nor optimized for
memory consumption.

=back

=head1 INSTALLATION NOTES

If you wish to use this module with a build of Perl other than
ActivePerl, you may wish to fetch the original source distribution for
this module at:

  ftp://ftp.roth.net:/pub/ntperl/ODBC/970208/Bin/Win32_ODBC_Build_CORE.zip

or one of the other archives at that same location. See the included
README for hints on installing this module manually, what to do if you
get a I<parse exception>, and a pointer to a test script for this
module.

=head1 OTHER DOCUMENTATION

Find a FAQ for Win32::ODBC at:

  http://www.roth.net/odbc/odbcfaq.htm

=head1 AUTHOR

Dave Roth <rothd@roth.net>

=head1 CREDITS

Based on original code by Dan DeMaggio <dmag@umich.edu>

=head1 DISCLAIMER

I do not guarantee B<ANYTHING> with this package. If you use it you
are doing so B<AT YOUR OWN RISK>! I may or may not support this
depending on my time schedule.

=head1 HISTORY

Last Modified 1999.09.25.

=head1 COPYRIGHT

Copyright (c) 1996-1998 Dave Roth. All rights reserved.

Courtesy of Roth Consulting:  http://www.roth.net/consult/

Use under GNU General Public License. Details can be found at:
http://www.gnu.org/copyleft/gpl.html

=cut
