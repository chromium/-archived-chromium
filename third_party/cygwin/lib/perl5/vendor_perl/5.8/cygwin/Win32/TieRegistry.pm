# Win32/TieRegistry.pm -- Perl module to easily use a Registry
# (on Win32 systems so far).
# by Tye McQueen, tye@metronet.com, see http://www.metronet.com/~tye/.

#
# Skip to "=head" line for user documentation.
#


package Win32::TieRegistry;


use strict;
use vars qw( $PACK $VERSION @ISA @EXPORT @EXPORT_OK );

$PACK= "Win32::TieRegistry";	# Used in error messages.
$VERSION= '0.26';


use Carp;

require Tie::Hash;
@ISA= qw(Tie::Hash);

# Required other modules:
use Win32API::Registry 0.12 qw( :KEY_ :HKEY_ :REG_ );

#Optional other modules:
use vars qw( $_NoMoreItems $_FileNotFound $_TooSmall $_MoreData $_SetDualVar );

if(  eval { require Win32::WinError }  ) {
    $_NoMoreItems= Win32::WinError::constant("ERROR_NO_MORE_ITEMS",0);
    $_FileNotFound= Win32::WinError::constant("ERROR_FILE_NOT_FOUND",0);
    $_TooSmall= Win32::WinError::constant("ERROR_INSUFFICIENT_BUFFER",0);
    $_MoreData= Win32::WinError::constant("ERROR_MORE_DATA",0);
} else {
    $_NoMoreItems= "^No more data";
    $_FileNotFound= "cannot find the file";
    $_TooSmall= " data area passed to ";
    $_MoreData= "^more data is avail";
}
if(  $_SetDualVar= eval { require SetDualVar }  ) {
    import SetDualVar;
}


#Implementation details:
#    When opened:
#	HANDLE		long; actual handle value
#	MACHINE		string; name of remote machine ("" if local)
#	PATH		list ref; machine-relative full path for this key:
#			["LMachine","System","Disk"]
#			["HKEY_LOCAL_MACHINE","System","Disk"]
#	DELIM		char; delimiter used to separate subkeys (def="\\")
#	OS_DELIM	char; always "\\" for Win32
#	ACCESS		long; usually KEY_ALL_ACCESS, perhaps KEY_READ, etc.
#	ROOTS		string; var name for "Lmachine"->HKEY_LOCAL_MACHINE map
#	FLAGS		int; bits to control certain options
#    Often:
#	VALUES		ref to list of value names (data/type never cached)
#	SUBKEYS		ref to list of subkey names
#	SUBCLASSES	ref to list of subkey classes
#	SUBTIMES	ref to list of subkey write times
#	MEMBERS		ref to list of subkey_name.DELIM's, DELIM.value_name's
#	MEMBHASH	hash ref to with MEMBERS as keys and 1's as values
#    Once Key "Info" requested:
#	Class CntSubKeys CntValues MaxSubKeyLen MaxSubClassLen
#	MaxValNameLen MaxValDataLen SecurityLen LastWrite
#    If is tied to a hash and iterating over key values:
#	PREVIDX		int; index of last MEMBERS element return
#    If is the key object returned by Load():
#	UNLOADME	list ref; information about Load()ed key
#    If is a subkey of a "loaded" key other than the one returned by Load():
#	DEPENDON	obj ref; object that can't be destroyed before us


#Package-local variables:

# Option flag bits:
use vars qw( $Flag_ArrVal $Flag_TieVal $Flag_DualTyp $Flag_DualBin
	     $Flag_FastDel $Flag_HexDWord $Flag_Split $Flag_FixNulls );
$Flag_ArrVal=	0x0001;
$Flag_TieVal=	0x0002;
$Flag_FastDel=	0x0004;
$Flag_HexDWord=	0x0008;
$Flag_Split=	0x0010;
$Flag_DualTyp=	0x0020;
$Flag_DualBin=	0x0040;
$Flag_FixNulls=	0x0080;


use vars qw( $RegObj %_Roots %RegHash $Registry );

# Short-hand for HKEY_* constants:
%_Roots= (
    "Classes" =>	HKEY_CLASSES_ROOT,
    "CUser" =>		HKEY_CURRENT_USER,
    "LMachine" =>	HKEY_LOCAL_MACHINE,
    "Users" =>		HKEY_USERS,
    "PerfData" =>	HKEY_PERFORMANCE_DATA,	# Too picky to be useful
    "CConfig" =>	HKEY_CURRENT_CONFIG,
    "DynData" =>	HKEY_DYN_DATA,		# Too picky to be useful
);

# Basic master Registry object:
$RegObj= {};
@$RegObj{qw( HANDLE MACHINE PATH DELIM OS_DELIM ACCESS FLAGS ROOTS )}= (
    "NONE", "", [], "\\", "\\",
    KEY_READ|KEY_WRITE, $Flag_HexDWord|$Flag_FixNulls, "${PACK}::_Roots" );
$RegObj->{FLAGS} |= $Flag_DualTyp|$Flag_DualBin   if  $_SetDualVar;
bless $RegObj;

# Fill cache for master Registry object:
@$RegObj{qw( VALUES SUBKEYS SUBCLASSES SUBTIMES )}= (
    [],  [ keys(%_Roots) ],  [],  []  );
grep( s#$#$RegObj->{DELIM}#,
  @{ $RegObj->{MEMBERS}= [ @{$RegObj->{SUBKEYS}} ] } );
@$RegObj{qw( Class MaxSubKeyLen MaxSubClassLen MaxValNameLen
  MaxValDataLen SecurityLen LastWrite CntSubKeys CntValues )}=
    ( "", 0, 0, 0, 0, 0, 0, 0, 0 );

# Create master Registry tied hash:
$RegObj->Tie( \%RegHash );

# Create master Registry combination object and tied hash reference:
$Registry= \%RegHash;
bless $Registry;


# Preloaded methods go here.


# Map option names to name of subroutine that controls that option:
use vars qw( @_opt_subs %_opt_subs );
@_opt_subs= qw( Delimiter ArrayValues TieValues SplitMultis DWordsToHex
	FastDelete FixSzNulls DualTypes DualBinVals AllowLoad AllowSave );
@_opt_subs{@_opt_subs}= @_opt_subs;

sub import
{
    my $pkg= shift(@_);
    my $level= $Exporter::ExportLevel;
    my $expto= caller($level);
    my @export= ();
    my @consts= ();
    my $registry= $Registry->Clone;
    local( $_ );
    while(  @_  ) {
	$_= shift(@_);
	if(  /^\$(\w+::)*\w+$/  ) {
	    push( @export, "ObjVar" )   if  /^\$RegObj$/;
	    push( @export, $_ );
	} elsif(  /^\%(\w+::)*\w+$/  ) {
	    push( @export, $_ );
	} elsif(  /^[$%]/  ) {
	    croak "${PACK}->import:  Invalid variable name ($_)";
	} elsif(  /^:/  ||  /^(H?KEY|REG)_/  ) {
	    push( @consts, $_ );
	} elsif(  ! @_  ) {
	    croak "${PACK}->import:  Missing argument after option ($_)";
	} elsif(  exists $_opt_subs{$_}  ) {
	    $_= $_opt_subs{$_};
	    $registry->$_( shift(@_) );
	} elsif(  /^TiedRef$/  ) {
	    $_= shift(@_);
	    if(  ! ref($_)  &&  /^(\$?)(\w+::)*\w+$/  ) {
		$_= '$'.$_   unless  '$' eq $1;
	    } elsif(  "SCALAR" ne ref($_)  ) {
		croak "${PACK}->import:  Invalid var after TiedRef ($_)";
	    }
	    push( @export, $_ );
	} elsif(  /^TiedHash$/  ) {
	    $_= shift(@_);
	    if(  ! ref($_)  &&  /^(\%?)(\w+::)*\w+$/  ) {
		$_= '%'.$_   unless  '%' eq $1;
	    } elsif(  "HASH" ne ref($_)  ) {
		croak "${PACK}->import:  Invalid var after TiedHash ($_)";
	    }
	    push( @export, $_ );
	} elsif(  /^ObjectRef$/  ) {
	    $_= shift(@_);
	    if(  ! ref($_)  &&  /^(\$?)(\w+::)*\w+$/  ) {
		push( @export, "ObjVar" );
		$_= '$'.$_   unless  '$' eq $1;
	    } elsif(  "SCALAR" eq ref($_)  ) {
		push( @export, "ObjRef" );
	    } else {
		croak "${PACK}->import:  Invalid var after ObjectRef ($_)";
	    }
	    push( @export, $_ );
	} elsif(  /^ExportLevel$/  ) {
	    $level= shift(@_);
	    $expto= caller($level);
	} elsif(  /^ExportTo$/  ) {
	    undef $level;
	    $expto= caller($level);
	} else {
	    croak "${PACK}->import:  Invalid option ($_)";
	}
    }
    Win32API::Registry->export( $expto, @consts )   if  @consts;
    @export= ('$Registry')   unless  @export;
    while(  @export  ) {
	$_= shift( @export );
	if(  /^\$((?:\w+::)*)(\w+)$/  ) {
	    my( $pack, $sym )= ( $1, $2 );
	    $pack= $expto   unless  defined($pack)  &&  "" ne $pack;
	    no strict 'refs';
	    *{"${pack}::$sym"}= \${"${pack}::$sym"};
	    ${"${pack}::$sym"}= $registry;
	} elsif(  /^\%((?:\w+::)*)(\w+)$/  ) {
	    my( $pack, $sym )= ( $1, $2 );
	    $pack= $expto   unless  defined($pack)  &&  "" ne $pack;
	    no strict 'refs';
	    *{"${pack}::$sym"}= \%{"${pack}::$sym"};
	    $registry->Tie( \%{"${pack}::$sym"} );
	} elsif(  "SCALAR" eq ref($_)  ) {
	    $$_= $registry;
	} elsif(  "HASH" eq ref($_)  ) {
	    $registry->Tie( $_ );
	} elsif(  /^ObjVar$/  ) {
	    $_= shift( @_ );
	    /^\$((?:\w+::)*)(\w+)$/;
	    my( $pack, $sym )= ( $1, $2 );
	    $pack= $expto   unless  defined($pack)  &&  "" ne $pack;
	    no strict 'refs';
	    *{"${pack}::$sym"}= \${"${pack}::$sym"};
	    ${"${pack}::$sym"}= $registry->ObjectRef;
	} elsif(  /^ObjRef$/  ) {
	    ${shift(@_)}= $registry->ObjectRef;
	} else {
	    die "Impossible var to export ($_)";
	}
    }
}


use vars qw( @_new_Opts %_new_Opts );
@_new_Opts= qw( ACCESS DELIM MACHINE DEPENDON );
@_new_Opts{@_new_Opts}= (1) x @_new_Opts;

sub _new
{
    my $this= shift( @_ );
    $this= tied(%$this)   if  ref($this)  &&  tied(%$this);
    my $class= ref($this) || $this;
    my $self= {};
    my( $handle, $rpath, $opts )= @_;
    if(  @_ < 2  ||  "ARRAY" ne ref($rpath)  ||  3 < @_
     ||  3 == @_ && "HASH" ne ref($opts)  ) {
	croak "Usage:  ${PACK}->_new( \$handle, \\\@path, {OPT=>VAL,...} );\n",
	      "  options: @_new_Opts\nCalled";
    }
    @$self{qw( HANDLE PATH )}= ( $handle, $rpath );
    @$self{qw( MACHINE ACCESS DELIM OS_DELIM ROOTS FLAGS )}=
      ( $this->Machine, $this->Access, $this->Delimiter,
        $this->OS_Delimiter, $this->_Roots, $this->_Flags );
    if(  ref($opts)  ) {
	my @err= grep( ! $_new_Opts{$_}, keys(%$opts) );
	@err  and  croak "${PACK}->_new:  Invalid options (@err)";
	@$self{ keys(%$opts) }= values(%$opts);
    }
    bless $self, $class;
    return $self;
}


sub _split
{
    my $self= shift( @_ );
    $self= tied(%$self)   if  tied(%$self);
    my $path= shift( @_ );
    my $delim= @_ ? shift(@_) : $self->Delimiter;
    my $list= [ split( /\Q$delim/, $path ) ];
    return $list;
}


sub _rootKey
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $keyPath= shift(@_);
    my $delim= @_ ? shift(@_) : $self->Delimiter;
    my( $root, $subPath );
    if(  "ARRAY" eq ref($keyPath)  ) {
	$subPath= $keyPath;
    } else {
	$subPath= $self->_split( $keyPath, $delim );
    }
    $root= shift( @$subPath );
    if(  $root =~ /^HKEY_/  ) {
	my $handle= Win32API::Registry::constant($root,0);
	$handle  or  croak "Invalid HKEY_ constant ($root): $!";
	return( $self->_new( $handle, [$root], {DELIM=>$delim} ),
	        $subPath );
    } elsif(  $root =~ /^([-+]|0x)?\d/  ) {
	return( $self->_new( $root, [sprintf("0x%lX",$root)],
			     {DELIM=>$delim} ),
		$subPath );
    } else {
	my $roots= $self->Roots;
	if(  $roots->{$root}  ) {
	    return( $self->_new( $roots->{$root}, [$root], {DELIM=>$delim} ),
	            $subPath );
	}
	croak "No such root key ($root)";
    }
}


sub _open
{
    my $this= shift(@_);
    $this= tied(%$this)   if  ref($this)  &&  tied(%$this);
    my $subPath= shift(@_);
    my $sam= @_ ? shift(@_) : $this->Access;
    my $subKey= join( $this->OS_Delimiter, @$subPath );
    my $handle= 0;
    $this->RegOpenKeyEx( $subKey, 0, $sam, $handle )
      or  return ();
    return $this->_new( $handle, [ @{$this->_Path}, @$subPath ],
      { ACCESS=>$sam, ( defined($this->{UNLOADME}) ? ("DEPENDON",$this)
	: defined($this->{DEPENDON}) ? ("DEPENDON",$this->{DEPENDON}) : () )
      } );
}


sub ObjectRef
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    return $self;
}


sub _constant
{
    my( $name, $desc )= @_;
    my $value= Win32API::Registry::constant( $name, 0 );
    my $func= (caller(1))[3];
    if(  0 == $value  ) {
	if(  $! =~ /invalid/i  ) {
	    croak "$func: Invalid $desc ($name)";
	} elsif(  0 != $!  ) {
	    croak "$func: \u$desc ($name) not support on this platform";
	}
    }
    return $value;
}


sub _connect
{
    my $this= shift(@_);
    $this= tied(%$this)   if  ref($this)  &&  tied(%$this);
    my $subPath= pop(@_);
    $subPath= $this->_split( $subPath )   unless  ref($subPath);
    my $machine= @_ ? shift(@_) : shift(@$subPath);
    my $handle= 0;
    my( $temp )= $this->_rootKey( [@$subPath] );
    $temp->RegConnectRegistry( $machine, $temp->Handle, $handle )
      or  return ();
    my $self= $this->_new( $handle, [shift(@$subPath)], {MACHINE=>$machine} );
    return( $self, $subPath );
}


use vars qw( @Connect_Opts %Connect_Opts );
@Connect_Opts= qw(Access Delimiter);
@Connect_Opts{@Connect_Opts}= (1) x @Connect_Opts;

sub Connect
{
    my $this= shift(@_);
    my $tied=  ref($this)  &&  tied(%$this);
    $this= tied(%$this)   if  $tied;
    my( $machine, $key, $opts )= @_;
    my $delim= "";
    my $sam;
    my $subPath;
    if(  @_ < 2  ||  3 < @_
     ||  3 == @_ && "HASH" ne ref($opts)  ) {
	croak "Usage:  \$obj= ${PACK}->Connect(",
	      " \$Machine, \$subKey, { OPT=>VAL,... } );\n",
	      "  options: @Connect_Opts\nCalled";
    }
    if(  ref($opts)  ) {
	my @err= grep( ! $Connect_Opts{$_}, keys(%$opts) );
	@err  and  croak "${PACK}->Connect:  Invalid options (@err)";
    }
    $delim= "$opts->{Delimiter}"   if  defined($opts->{Delimiter});
    $delim= $this->Delimiter   if  "" eq $delim;
    $sam= defined($opts->{Access}) ? $opts->{Access} : $this->Access;
    $sam= _constant($sam,"key access type")   if  $sam =~ /^KEY_/;
    ( $this, $subPath )= $this->_connect( $machine, $key );
    return ()   unless  defined($this);
    my $self= $this->_open( $subPath, $sam );
    return ()   unless  defined($self);
    $self->Delimiter( $delim );
    $self= $self->TiedRef   if  $tied;
    return $self;
}


my @_newVirtual_keys= qw( MEMBERS VALUES SUBKEYS SUBTIMES SUBCLASSES
    Class SecurityLen LastWrite CntValues CntSubKeys
    MaxValNameLen MaxValDataLen MaxSubKeyLen MaxSubClassLen );

sub _newVirtual
{
    my $self= shift(@_);
    my( $rPath, $root, $opts )= @_;
    my $new= $self->_new( "NONE", $rPath, $opts )
      or  return ();
    @{$new}{@_newVirtual_keys}= @{$root->ObjectRef}{@_newVirtual_keys};
    return $new;
}


#$key= new Win32::TieRegistry "LMachine/System/Disk";
#$key= new Win32::TieRegistry "//Server1/LMachine/System/Disk";
#Win32::TieRegistry->new( HKEY_LOCAL_MACHINE, {DELIM=>"/",ACCESS=>KEY_READ} );
#Win32::TieRegistry->new( [ HKEY_LOCAL_MACHINE, ".../..." ], {DELIM=>$DELIM} );
#$key->new( ... );

use vars qw( @new_Opts %new_Opts );
@new_Opts= qw(Access Delimiter);
@new_Opts{@new_Opts}= (1) x @new_Opts;

sub new
{
    my $this= shift( @_ );
    $this= tied(%$this)   if  ref($this)  &&  tied(%$this);
    if(  ! ref($this)  ) {
	no strict "refs";
	my $self= ${"${this}::Registry"};
	croak "${this}->new failed since ${PACK}::new sees that ",
	  "\$${this}::Registry is not an object."
	  if  ! ref($self);
	$this= $self->Clone;
    }
    my( $subKey, $opts )= @_;
    my $delim= "";
    my $dlen;
    my $sam;
    my $subPath;
    if(  @_ < 1  ||  2 < @_
     ||  2 == @_ && "HASH" ne ref($opts)  ) {
	croak "Usage:  \$obj= ${PACK}->new( \$subKey, { OPT=>VAL,... } );\n",
	      "  options: @new_Opts\nCalled";
    }
    if(  defined($opts)  ) {
	my @err= grep( ! $new_Opts{$_}, keys(%$opts) );
	@err  and  die "${PACK}->new:  Invalid options (@err)";
    }
    $delim= "$opts->{Delimiter}"   if  defined($opts->{Delimiter});
    $delim= $this->Delimiter   if  "" eq $delim;
    $dlen= length($delim);
    $sam= defined($opts->{Access}) ? $opts->{Access} : $this->Access;
    $sam= _constant($sam,"key access type")   if  $sam =~ /^KEY_/;
    if(  "ARRAY" eq ref($subKey)  ) {
	$subPath= $subKey;
	if(  "NONE" eq $this->Handle  &&  @$subPath  ) {
	    ( $this, $subPath )= $this->_rootKey( $subPath );
	}
    } elsif(  $delim x 2 eq substr($subKey,0,2*$dlen)  ) {
	my $path= $this->_split( substr($subKey,2*$dlen), $delim );
	my $mach= shift(@$path);
	if(  ! @$path  ) {
	    return $this->_newVirtual( $path, $Registry,
			    {MACHINE=>$mach,DELIM=>$delim,ACCESS=>$sam} );
	}
	( $this, $subPath )= $this->_connect( $mach, $path );
	return ()   if  ! defined($this);
	if(  0 == @$subPath  ) {
	    $this->Delimiter( $delim );
	    return $this;
	}
    } elsif(  $delim eq substr($subKey,0,$dlen)  ) {
	( $this, $subPath )= $this->_rootKey( substr($subKey,$dlen), $delim );
    } elsif(  "NONE" eq $this->Handle  &&  "" ne $subKey  ) {
	my( $mach )= $this->Machine;
	if(  $mach  ) {
	    ( $this, $subPath )= $this->_connect( $mach, $subKey );
	} else {
	    ( $this, $subPath )= $this->_rootKey( $subKey, $delim );
	}
    } else {
	$subPath= $this->_split( $subKey, $delim );
    }
    return ()   unless  defined($this);
    if(  0 == @$subPath  &&  "NONE" eq $this->Handle  ) {
	return $this->_newVirtual( $this->_Path, $this,
				   { DELIM=>$delim, ACCESS=>$sam } );
    }
    my $self= $this->_open( $subPath, $sam );
    return ()   unless  defined($self);
    $self->Delimiter( $delim );
    return $self;
}


sub Open
{
    my $self= shift(@_);
    my $tied=  ref($self)  &&  tied(%$self);
    $self= tied(%$self)   if  $tied;
    $self= $self->new( @_ );
    $self= $self->TiedRef   if  defined($self)  &&  $tied;
    return $self;
}


sub Clone
{
    my $self= shift( @_ );
    my $new= $self->Open("");
    return $new;
}


{ my @flush;
    sub Flush
    {
	my $self= shift(@_);
	$self= tied(%$self)   if  tied(%$self);
	my( $flush )= shift(@_);
	@_  and  croak "Usage:  \$key->Flush( \$bFlush );";
	return 0   if  "NONE" eq $self->Handle;
	@flush= qw( VALUES SUBKEYS SUBCLASSES SUBTIMES MEMBERS Class
		    CntSubKeys CntValues MaxSubKeyLen MaxSubClassLen
		    MaxValNameLen MaxValDataLen SecurityLen LastWrite PREVIDX )
	  unless  @flush;
	delete( @$self{@flush} );
	if(  defined($flush)  &&  $flush  ) {
	    return $self->RegFlushKey();
	} else {
	    return 1;
	}
    }
}


sub _DualVal
{
    my( $hRef, $num )= @_;
    if(  $_SetDualVar  &&  $$hRef{$num}  ) {
	&SetDualVar( $num, "$$hRef{$num}", 0+$num );
    }
    return $num;
}


use vars qw( @_RegDataTypes %_RegDataTypes );
@_RegDataTypes= qw( REG_SZ REG_EXPAND_SZ REG_BINARY REG_LINK REG_MULTI_SZ
		    REG_DWORD_LITTLE_ENDIAN REG_DWORD_BIG_ENDIAN REG_DWORD
		    REG_RESOURCE_LIST REG_FULL_RESOURCE_DESCRIPTOR
		    REG_RESOURCE_REQUIREMENTS_LIST REG_NONE );
# Make sure that REG_DWORD appears _after_ other REG_DWORD_*
# items above and that REG_NONE appears _last_.
foreach(  @_RegDataTypes  ) {
    $_RegDataTypes{Win32API::Registry::constant($_,0)}= $_;
}

sub GetValue
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    1 == @_  or  croak "Usage:  (\$data,\$type)= \$key->GetValue('ValName');";
    my( $valName )= @_;
    my( $valType, $valData, $dLen )= (0,"",0);
    return ()   if  "NONE" eq $self->Handle;
    $self->RegQueryValueEx( $valName, [], $valType, $valData,
      $dLen= ( defined($self->{MaxValDataLen}) ? $self->{MaxValDataLen} : 0 )
    )  or  return ();
    if(  REG_DWORD == $valType  ) {
	my $val= unpack("L",$valData);
	$valData= sprintf "0x%08.8lX", $val   if  $self->DWordsToHex;
	&SetDualVar( $valData, $valData, $val )   if  $self->DualBinVals
    } elsif(  REG_BINARY == $valType  &&  length($valData) <= 4  ) {
	&SetDualVar( $valData, $valData, hex reverse unpack("h*",$valData) )
	  if  $self->DualBinVals;
    } elsif(  ( REG_SZ == $valType || REG_EXPAND_SZ == $valType )
          &&  $self->FixSzNulls  ) {
	substr($valData,-1)= ""   if  "\0" eq substr($valData,-1);
    } elsif(  REG_MULTI_SZ == $valType  &&  $self->SplitMultis  ) {
	## $valData =~ s/\0\0$//;	# Why does this often fail??
	substr($valData,-2)= ""   if  "\0\0" eq substr($valData,-2);
	$valData= [ split( /\0/, $valData, -1 ) ]
    }
    if(  ! wantarray  ) {
	return $valData;
    } elsif(  ! $self->DualTypes  ) {
	return( $valData, $valType );
    } else {
	return(  $valData,  _DualVal( \%_RegDataTypes, $valType )  );
    }
}


sub _ErrNum
{
    # return $^E;
    return Win32::GetLastError();
}


sub _ErrMsg
{
    # return $^E;
    return Win32::FormatMessage( Win32::GetLastError() );
}

sub _Err
{
    my $err;
    # return $^E;
    return _ErrMsg   if  ! $_SetDualVar;
    return &SetDualVar( $err, _ErrMsg, _ErrNum );
}

sub _NoMoreItems
{
    return
      $_NoMoreItems =~ /^\d/
        ?  _ErrNum == $_NoMoreItems
        :  _ErrMsg =~ /$_NoMoreItems/io;
}


sub _FileNotFound
{
    return
      $_FileNotFound =~ /^\d/
        ?  _ErrNum == $_FileNotFound
        :  _ErrMsg =~ /$_FileNotFound/io;
}


sub _TooSmall
{
    return
      $_TooSmall =~ /^\d/
        ?  _ErrNum == $_TooSmall
        :  _ErrMsg =~ /$_TooSmall/io;
}


sub _MoreData
{
    return
      $_MoreData =~ /^\d/
        ?  _ErrNum == $_MoreData
        :  _ErrMsg =~ /$_MoreData/io;
}


sub _enumValues
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my( @names )= ();
    my $pos= 0;
    my $name= "";
    my $nlen= 1+$self->Information("MaxValNameLen");
    while(  $self->RegEnumValue($pos++,$name,$nlen,[],[],[],[])  ) {
	push( @names, $name );
    }
    if(  ! _NoMoreItems()  ) {
	return ();
    }
    $self->{VALUES}= \@names;
    return 1;
}


sub ValueNames
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \@names= \$key->ValueNames;";
    $self->_enumValues   unless  $self->{VALUES};
    return @{$self->{VALUES}};
}


sub _enumSubKeys
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my( @subkeys, @classes, @times )= ();
    my $pos= 0;
    my( $subkey, $class, $time )= ("","","");
    my( $namSiz, $clsSiz )= $self->Information(
			      qw( MaxSubKeyLen MaxSubClassLen ));
    $namSiz++;  $clsSiz++;
    while(  $self->RegEnumKeyEx(
	      $pos++, $subkey, $namSiz, [], $class, $clsSiz, $time )  ) {
	push( @subkeys, $subkey );
	push( @classes, $class );
	push( @times, $time );
    }
    if(  ! _NoMoreItems()  ) {
	return ();
    }
    $self->{SUBKEYS}= \@subkeys;
    $self->{SUBCLASSES}= \@classes;
    $self->{SUBTIMES}= \@times;
    return 1;
}


sub SubKeyNames
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \@names= \$key->SubKeyNames;";
    $self->_enumSubKeys   unless  $self->{SUBKEYS};
    return @{$self->{SUBKEYS}};
}


sub SubKeyClasses
{
    my $self= shift(@_);
    @_  and  croak "Usage:  \@classes= \$key->SubKeyClasses;";
    $self->_enumSubKeys   unless  $self->{SUBCLASSES};
    return @{$self->{SUBCLASSES}};
}


sub SubKeyTimes
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \@times= \$key->SubKeyTimes;";
    $self->_enumSubKeys   unless  $self->{SUBTIMES};
    return @{$self->{SUBTIMES}};
}


sub _MemberNames
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \$arrayRef= \$key->_MemberNames;";
    if(  ! $self->{MEMBERS}  ) {
	$self->_enumValues   unless  $self->{VALUES};
	$self->_enumSubKeys   unless  $self->{SUBKEYS};
	my( @members )= (  map( $_.$self->{DELIM}, @{$self->{SUBKEYS}} ),
			   map( $self->{DELIM}.$_, @{$self->{VALUES}} )  );
	$self->{MEMBERS}= \@members;
    }
    return $self->{MEMBERS};
}


sub _MembersHash
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \$hashRef= \$key->_MembersHash;";
    if(  ! $self->{MEMBHASH}  ) {
	my $aRef= $self->_MemberNames;
	$self->{MEMBHASH}= {};
	@{$self->{MEMBHASH}}{@$aRef}= (1) x @$aRef;
    }
    return $self->{MEMBHASH};
}


sub MemberNames
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \@members= \$key->MemberNames;";
    return @{$self->_MemberNames};
}


sub Information
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my( $time, $nkeys, $nvals, $xsec, $xkey, $xcls, $xname, $xdata )=
	("",0,0,0,0,0,0,0);
    my $clen= 8;
    if(  ! $self->RegQueryInfoKey( [], [], $nkeys, $xkey, $xcls,
				   $nvals, $xname, $xdata, $xsec, $time )  ) {
	return ();
    }
    if(  defined($self->{Class})  ) {
	$clen= length($self->{Class});
    } else {
	$self->{Class}= "";
    }
    while(  ! $self->RegQueryInfoKey( $self->{Class}, $clen,
				      [],[],[],[],[],[],[],[],[])
        &&  _MoreData  ) {
	$clen *= 2;
    }
    my( %info );
    @info{ qw( LastWrite CntSubKeys CntValues SecurityLen
	       MaxValDataLen MaxSubKeyLen MaxSubClassLen MaxValNameLen )
    }=       ( $time,    $nkeys,    $nvals,   $xsec,
               $xdata,       $xkey,       $xcls,         $xname );
    if(  @_  ) {
	my( %check );
	@check{keys(%info)}= keys(%info);
	my( @err )= grep( ! $check{$_}, @_ );
	if(  @err  ) {
	    croak "${PACK}::Information- Invalid info requested (@err)";
	}
	return @info{@_};
    } else {
	return %info;
    }
}


sub Delimiter
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    $self= $RegObj   unless  ref($self);
    my( $oldDelim )= $self->{DELIM};
    if(  1 == @_  &&  "" ne "$_[0]"  ) {
	delete $self->{MEMBERS};
	delete $self->{MEMBHASH};
	$self->{DELIM}= "$_[0]";
    } elsif(  0 != @_  ) {
	croak "Usage:  \$oldDelim= \$key->Delimiter(\$newDelim);";
    }
    return $oldDelim;
}


sub Handle
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \$handle= \$key->Handle;";
    $self= $RegObj   unless  ref($self);
    return $self->{HANDLE};
}


sub Path
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \$path= \$key->Path;";
    my $delim= $self->{DELIM};
    $self= $RegObj   unless  ref($self);
    if(  "" eq $self->{MACHINE}  ) {
	return(  $delim . join( $delim, @{$self->{PATH}} ) . $delim  );
    } else {
	return(  $delim x 2
	  . join( $delim, $self->{MACHINE}, @{$self->{PATH}} )
	  . $delim  );
    }
}


sub _Path
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \$arrRef= \$key->_Path;";
    $self= $RegObj   unless  ref($self);
    return $self->{PATH};
}


sub Machine
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \$machine= \$key->Machine;";
    $self= $RegObj   unless  ref($self);
    return $self->{MACHINE};
}


sub Access
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \$access= \$key->Access;";
    $self= $RegObj   unless  ref($self);
    return $self->{ACCESS};
}


sub OS_Delimiter
{
    my $self= shift(@_);
    @_  and  croak "Usage:  \$backslash= \$key->OS_Delimiter;";
    return $self->{OS_DELIM};
}


sub _Roots
{
    my $self= shift(@_);
    $self= tied(%$self)   if  ref($self)  &&  tied(%$self);
    @_  and  croak "Usage:  \$varName= \$key->_Roots;";
    $self= $RegObj   unless  ref($self);
    return $self->{ROOTS};
}


sub Roots
{
    my $self= shift(@_);
    $self= tied(%$self)   if  ref($self)  &&  tied(%$self);
    @_  and  croak "Usage:  \$hashRef= \$key->Roots;";
    $self= $RegObj   unless  ref($self);
    return eval "\\%$self->{ROOTS}";
}


sub TIEHASH
{
    my( $this )= shift(@_);
    $this= tied(%$this)   if  ref($this)  &&  tied(%$this);
    my( $key )= @_;
    if(  1 == @_  &&  ref($key)  &&  "$key" =~ /=/  ) {
	return $key;	# $key is already an object (blessed reference).
    }
    return $this->new( @_ );
}


sub Tie
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my( $hRef )= @_;
    if(  1 != @_  ||  ! ref($hRef)  ||  "$hRef" !~ /(^|=)HASH\(/  ) {
	croak "Usage: \$key->Tie(\\\%hash);";
    }
    return  tie %$hRef, ref($self), $self;
}


sub TiedRef
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $hRef= @_ ? shift(@_) : {};
    return ()   if  ! defined($self);
    $self->Tie($hRef);
    bless $hRef, ref($self);
    return $hRef;
}


sub _Flags
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $oldFlags= $self->{FLAGS};
    if(  1 == @_  ) {
	$self->{FLAGS}= shift(@_);
    } elsif(  0 != @_  ) {
	croak "Usage:  \$oldBits= \$key->_Flags(\$newBits);";
    }
    return $oldFlags;
}


sub ArrayValues
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $oldFlag= $Flag_ArrVal == ( $Flag_ArrVal & $self->{FLAGS} );
    if(  1 == @_  ) {
	my $bool= shift(@_);
	if(  $bool  ) {
	    $self->{FLAGS} |= $Flag_ArrVal;
	} else {
	    $self->{FLAGS} &= ~( $Flag_ArrVal | $Flag_TieVal );
	}
    } elsif(  0 != @_  ) {
	croak "Usage:  \$oldBool= \$key->ArrayValues(\$newBool);";
    }
    return $oldFlag;
}


sub TieValues
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $oldFlag= $Flag_TieVal == ( $Flag_TieVal & $self->{FLAGS} );
    if(  1 == @_  ) {
	my $bool= shift(@_);
	if(  $bool  ) {
	    croak "${PACK}->TieValues cannot be enabled with this version";
	    $self->{FLAGS} |= $Flag_TieVal;
	} else {
	    $self->{FLAGS} &= ~$Flag_TieVal;
	}
    } elsif(  0 != @_  ) {
	croak "Usage:  \$oldBool= \$key->TieValues(\$newBool);";
    }
    return $oldFlag;
}


sub FastDelete
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $oldFlag= $Flag_FastDel == ( $Flag_FastDel & $self->{FLAGS} );
    if(  1 == @_  ) {
	my $bool= shift(@_);
	if(  $bool  ) {
	    $self->{FLAGS} |= $Flag_FastDel;
	} else {
	    $self->{FLAGS} &= ~$Flag_FastDel;
	}
    } elsif(  0 != @_  ) {
	croak "Usage:  \$oldBool= \$key->FastDelete(\$newBool);";
    }
    return $oldFlag;
}


sub SplitMultis
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $oldFlag= $Flag_Split == ( $Flag_Split & $self->{FLAGS} );
    if(  1 == @_  ) {
	my $bool= shift(@_);
	if(  $bool  ) {
	    $self->{FLAGS} |= $Flag_Split;
	} else {
	    $self->{FLAGS} &= ~$Flag_Split;
	}
    } elsif(  0 != @_  ) {
	croak "Usage:  \$oldBool= \$key->SplitMultis(\$newBool);";
    }
    return $oldFlag;
}


sub DWordsToHex
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $oldFlag= $Flag_HexDWord == ( $Flag_HexDWord & $self->{FLAGS} );
    if(  1 == @_  ) {
	my $bool= shift(@_);
	if(  $bool  ) {
	    $self->{FLAGS} |= $Flag_HexDWord;
	} else {
	    $self->{FLAGS} &= ~$Flag_HexDWord;
	}
    } elsif(  0 != @_  ) {
	croak "Usage:  \$oldBool= \$key->DWordsToHex(\$newBool);";
    }
    return $oldFlag;
}


sub FixSzNulls
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $oldFlag= $Flag_FixNulls == ( $Flag_FixNulls & $self->{FLAGS} );
    if(  1 == @_  ) {
	my $bool= shift(@_);
	if(  $bool  ) {
	    $self->{FLAGS} |= $Flag_FixNulls;
	} else {
	    $self->{FLAGS} &= ~$Flag_FixNulls;
	}
    } elsif(  0 != @_  ) {
	croak "Usage:  \$oldBool= \$key->FixSzNulls(\$newBool);";
    }
    return $oldFlag;
}


sub DualTypes
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $oldFlag= $Flag_DualTyp == ( $Flag_DualTyp & $self->{FLAGS} );
    if(  1 == @_  ) {
	my $bool= shift(@_);
	if(  $bool  ) {
	    croak "${PACK}->DualTypes cannot be enabled since ",
		  "SetDualVar module not installed"
	      unless  $_SetDualVar;
	    $self->{FLAGS} |= $Flag_DualTyp;
	} else {
	    $self->{FLAGS} &= ~$Flag_DualTyp;
	}
    } elsif(  0 != @_  ) {
	croak "Usage:  \$oldBool= \$key->DualTypes(\$newBool);";
    }
    return $oldFlag;
}


sub DualBinVals
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $oldFlag= $Flag_DualBin == ( $Flag_DualBin & $self->{FLAGS} );
    if(  1 == @_  ) {
	my $bool= shift(@_);
	if(  $bool  ) {
	    croak "${PACK}->DualBinVals cannot be enabled since ",
		  "SetDualVar module not installed"
	      unless  $_SetDualVar;
	    $self->{FLAGS} |= $Flag_DualBin;
	} else {
	    $self->{FLAGS} &= ~$Flag_DualBin;
	}
    } elsif(  0 != @_  ) {
	croak "Usage:  \$oldBool= \$key->DualBinVals(\$newBool);";
    }
    return $oldFlag;
}


sub GetOptions
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my( $opt, $meth );
    if(  ! @_  ||  1 == @_  &&  "HASH" eq ref($_[0])  ) {
	my $href= @_ ? $_[0] : {};
	foreach $opt (  grep !/^Allow/, @_opt_subs  ) {
	    $meth= $_opt_subs{$opt};
	    $href->{$opt}=  $self->$meth();
	}
	return @_ ? $self : $href;
    }
    my @old;
    foreach $opt (  @_  ) {
	$meth= $_opt_subs{$opt};
	if(  defined $meth  ) {
	    if(  $opt eq "AllowLoad"  ||  $opt eq "AllowSave"  ) {
		croak "${PACK}->GetOptions:  Getting current setting of $opt ",
		      "not supported in this release";
	    }
	    push(  @old,  $self->$meth()  );
	} else {
	    croak "${PACK}->GetOptions:  Invalid option ($opt) ",
		  "not one of ( ", join(" ",grep !/^Allow/, @_opt_subs), " )";
	}
    }
    return wantarray ? @old : $old[-1];
}


sub SetOptions
{
    my $self= shift(@_);
    # Don't get object if hash ref so "ref" returns original ref.
    my( $opt, $meth, @old );
    while(  @_  ) {
	$opt= shift(@_);
	$meth= $_opt_subs{$opt};
	if(  ! @_  ) {
	    croak "${PACK}->SetOptions:  Option value missing ",
		  "after option name ($opt)";
	} elsif(  defined $meth  ) {
	    push(  @old,  $self->$meth( shift(@_) )  );
	} elsif(  $opt eq substr("reference",0,length($opt))  ) {
	    shift(@_)   if  @_;
	    push(  @old,  $self  );
	} else {
	    croak "${PACK}->SetOptions:  Invalid option ($opt) ",
		  "not one of ( @_opt_subs )";
	}
    }
    return wantarray ? @old : $old[-1];
}


sub _parseTiedEnt
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $ent= shift(@_);
    my $delim= shift(@_);
    my $dlen= length( $delim );
    my $parent= @_ ? shift(@_) : 0;
    my $off;
    if(  $delim x 2 eq substr($ent,0,2*$dlen)  &&  "NONE" eq $self->Handle  ) {
	if(  0 <= ( $off= index( $ent, $delim x 2, 2*$dlen ) )  ) {
	    return(  substr( $ent, 0, $off ),  substr( $ent, 2*$dlen+$off )  );
	} elsif(  $delim eq substr($ent,-$dlen)  ) {
	    return( substr($ent,0,-$dlen) );
	} elsif(  2*$dlen <= ( $off= rindex( $ent, $delim ) )  ) {
	    return(  substr( $ent, 0, $off ),
	      undef,  substr( $ent, $dlen+$off )  );
	} elsif(  $parent  ) {
	    return();
	} else {
	    return( $ent );
	}
    } elsif(  $delim eq substr($ent,0,$dlen)  &&  "NONE" ne $self->Handle  ) {
	return( undef, substr($ent,$dlen) );
    } elsif(  $self->{MEMBERS}  &&  $self->_MembersHash->{$ent}  ) {
	return( substr($ent,0,-$dlen) );
    } elsif(  0 <= ( $off= index( $ent, $delim x 2 ) )  ) {
	return(  substr( $ent, 0, $off ),  substr( $ent, 2*$dlen+$off ) );
    } elsif(  $delim eq substr($ent,-$dlen)  ) {
	if(  $parent
	 &&  0 <= ( $off= rindex( $ent, $delim, length($ent)-2*$dlen ) )  ) {
	    return(  substr($ent,0,$off),
	      undef,  undef,  substr($ent,$dlen+$off,-$dlen)  );
	} else {
	    return( substr($ent,0,-$dlen) );
	}
    } elsif(  0 <= ( $off= rindex( $ent, $delim ) )  ) {
	return(
	  substr( $ent, 0, $off ),  undef,  substr( $ent, $dlen+$off )  );
    } else {
	return( undef, undef, $ent );
    }
}


sub _FetchValue
{
    my $self= shift( @_ );
    my( $val, $createKey )= @_;
    my( $data, $type );
    if(  ( $data, $type )= $self->GetValue( $val )  ) {
	return $self->ArrayValues ? [ $data, $type ]
	       : wantarray        ? ( $data, $type )
				  : $data;
    } elsif(  $createKey  and  $data= $self->new($val)  ) {
	return $data->TiedRef;
    } else {
	return ();
    }
}


sub FETCH
{
    my $self= shift(@_);
    my $ent= shift(@_);
    my $delim= $self->Delimiter;
    my( $key, $val, $ambig )= $self->_parseTiedEnt( $ent, $delim, 0 );
    my $sub;
    if(  defined($key)  ) {
	if(  defined($self->{MEMBHASH})
	 &&  $self->{MEMBHASH}->{$key.$delim}
	 &&  0 <= index($key,$delim)  ) {
	    return ()
	      unless  $sub= $self->new( $key,
			      {"Delimiter"=>$self->OS_Delimiter} );
	    $sub->Delimiter($delim);
	} else {
	    return ()
	      unless  $sub= $self->new( $key );
	}
    } else {
	$sub= $self;
    }
    if(  defined($val)  ) {
	return $sub->_FetchValue( $val );
    } elsif(  ! defined($ambig)  ) {
	return $sub->TiedRef;
    } elsif(  defined($key)  ) {
	return $sub->FETCH(  $ambig  );
    } else {
	return $sub->_FetchValue( $ambig, "" ne $ambig );
    }
}


sub _FetchOld
{
    my( $self, $key )= @_;
    my $old= $self->FETCH($key);
    if(  $old  ) {
	my $copy= {};
	%$copy= %$old;
	return $copy;
    }
    # return $^E;
    return _Err;
}


sub DELETE
{
    my $self= shift(@_);
    my $ent= shift(@_);
    my $delim= $self->Delimiter;
    my( $key, $val, $ambig, $subkey )= $self->_parseTiedEnt( $ent, $delim, 1 );
    my $sub;
    my $fast= defined(wantarray) ? $self->FastDelete : 2;
    my $old= 1;	# Value returned if FastDelete is set.
    if(  defined($key)
     &&  ( defined($val) || defined($ambig) || defined($subkey) )  ) {
	return ()
	  unless  $sub= $self->new( $key );
    } else {
	$sub= $self;
    }
    if(  defined($val)  ) {
	$old= $sub->GetValue($val) || _Err   unless  2 <= $fast;
	$sub->RegDeleteValue( $val );
    } elsif(  defined($subkey)  ) {
	$old= $sub->_FetchOld( $subkey.$delim )   unless  $fast;
	$sub->RegDeleteKey( $subkey );
    } elsif(  defined($ambig)  ) {
	if(  defined($key)  ) {
	    $old= $sub->DELETE($ambig);
	} else {
	    $old= $sub->GetValue($ambig) || _Err   unless  2 <= $fast;
	    if(  defined( $old )  ) {
		$sub->RegDeleteValue( $ambig );
	    } else {
		$old= $sub->_FetchOld( $ambig.$delim )   unless  $fast;
		$sub->RegDeleteKey( $ambig );
	    }
	}
    } elsif(  defined($key)  ) {
	$old= $sub->_FetchOld( $key.$delim )   unless  $fast;
	$sub->RegDeleteKey( $key );
    } else {
	croak "${PACK}->DELETE:  Key ($ent) can never be deleted";
    }
    return $old;
}


sub SetValue
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    my $name= shift(@_);
    my $data= shift(@_);
    my( $type )= @_;
    my $size;
    if(  ! defined($type)  ) {
	if(  "ARRAY" eq ref($data)  ) {
	    croak "${PACK}->SetValue:  Value is array reference but ",
		  "no data type given"
	      unless  2 == @$data;
	    ( $data, $type )= @$data;
	} else {
	    $type= REG_SZ;
	}
    }
    $type= _constant($type,"registry value data type")   if  $type =~ /^REG_/;
    if(  REG_MULTI_SZ == $type  &&  "ARRAY" eq ref($data)  ) {
	$data= join( "\0", @$data ) . "\0\0";
	## $data= pack(  "a*" x (1+@$data),  map( $_."\0", @$data, "" )  );
    } elsif(  ( REG_SZ == $type || REG_EXPAND_SZ == $type )
          &&  $self->FixSzNulls  ) {
	$data .= "\0"    unless  "\0" eq substr($data,0,-1);
    } elsif(  REG_DWORD == $type  &&  $data =~ /^0x[0-9a-fA-F]{3,}$/  ) {
	$data= pack( "L", hex($data) );
	# We could to $data=pack("L",$data) for REG_DWORD but I see
	# no nice way to always destinguish when to do this or not.
    }
    return $self->RegSetValueEx( $name, 0, $type, $data, length($data) );
}


sub StoreKey
{
    my $this= shift(@_);
    $this= tied(%$this)   if  ref($this)  &&  tied(%$this);
    my $subKey= shift(@_);
    my $data= shift(@_);
    my $ent;
    my $self;
    if(  ! ref($data)  ||  "$data" !~ /(^|=)HASH/  ) {
	croak "${PACK}->StoreKey:  For ", $this->Path.$subKey, ",\n",
	      "  subkey data must be a HASH reference";
    }
    if(  defined( $$data{""} )  &&  "HASH" eq ref($$data{""})  ) {
	$self= $this->CreateKey( $subKey, delete $$data{""} );
    } else {
	$self= $this->CreateKey( $subKey );
    }
    return ()   if  ! defined($self);
    foreach $ent (  keys(%$data)  ) {
	return ()
	  unless  $self->STORE( $ent, $$data{$ent} );
    }
    return $self;
}


# = { "" => {OPT=>VAL}, "val"=>[], "key"=>{} } creates a new key
# = "string" creates a new REG_SZ value
# = [ data, type ] creates a new value
sub STORE
{
    my $self= shift(@_);
    my $ent= shift(@_);
    my $data= shift(@_);
    my $delim= $self->Delimiter;
    my( $key, $val, $ambig, $subkey )= $self->_parseTiedEnt( $ent, $delim, 1 );
    my $sub;
    if(  defined($key)
     &&  ( defined($val) || defined($ambig) || defined($subkey) )  ) {
	return ()
	  unless  $sub= $self->new( $key );
    } else {
	$sub= $self;
    }
    if(  defined($val)  ) {
	croak "${PACK}->STORE:  For ", $sub->Path.$delim.$val, ",\n",
	      "  value data cannot be a HASH reference"
	  if  ref($data)  &&  "$data" =~ /(^|=)HASH/;
	$sub->SetValue( $val, $data );
    } elsif(  defined($subkey)  ) {
	croak "${PACK}->STORE:  For ", $sub->Path.$subkey.$delim, ",\n",
	      "  subkey data must be a HASH reference"
	  unless  ref($data)  &&  "$data" =~ /(^|=)HASH/;
	$sub->StoreKey( $subkey, $data );
    } elsif(  defined($ambig)  ) {
	if(  ref($data)  &&  "$data" =~ /(^|=)HASH/  ) {
	    $sub->StoreKey( $ambig, $data );
	} else {
	    $sub->SetValue( $ambig, $data );
	}
    } elsif(  defined($key)  ) {
	croak "${PACK}->STORE:  For ", $sub->Path.$key.$delim, ",\n",
	      "  subkey data must be a HASH reference"
	  unless  ref($data)  &&  "$data" =~ /(^|=)HASH/;
	$sub->StoreKey( $key, $data );
    } else {
	croak "${PACK}->STORE:  Key ($ent) can never be created nor set";
    }
}


sub EXISTS
{
    my $self= shift(@_);
    my $ent= shift(@_);
    return defined( $self->FETCH($ent) );
}


sub FIRSTKEY
{
    my $self= shift(@_);
    my $members= $self->_MemberNames;
    $self->{PREVIDX}= 0;
    return @{$members} ? $members->[0] : undef;
}


sub NEXTKEY
{
    my $self= shift(@_);
    my $prev= shift(@_);
    my $idx= $self->{PREVIDX};
    my $members= $self->_MemberNames;
    if(  ! defined($idx)  ||  $prev ne $members->[$idx]  ) {
	$idx= 0;
	while(  $idx < @$members  &&  $prev ne $members->[$idx]  ) {
	    $idx++;
	}
    }
    $self->{PREVIDX}= ++$idx;
    return $members->[$idx];
}


sub DESTROY
{
    my $self= shift(@_);
    return   if  tied(%$self);
    my $unload;
    local $@;
    eval { $unload= $self->{UNLOADME}; 1 }
	or  return;
    my $debug= $ENV{DEBUG_TIE_REGISTRY};
    if(  defined($debug)  ) {
	if(  1 < $debug  ) {
	    my $hand= $self->Handle;
	    my $dep= $self->{DEPENDON};
	    carp "${PACK} destroying ", $self->Path, " (",
		 "NONE" eq $hand ? $hand : sprintf("0x%lX",$hand), ")",
		 defined($dep) ? (" [depends on ",$dep->Path,"]") : ();
	} else {
	    warn "${PACK} destroying ", $self->Path, ".\n";
	}
    }
    $self->RegCloseKey
      unless  "NONE" eq $self->Handle;
    if(  defined($unload)  ) {
	if(  defined($debug)  &&  1 < $debug  ) {
	    my( $obj, $subKey, $file )= @$unload;
	    warn "Unloading ", $self->Path,
	      " (from ", $obj->Path, ", $subKey)...\n";
	}
	$self->UnLoad
	  ||  warn "Couldn't unload ", $self->Path, ": ", _ErrMsg, "\n";
	## carp "Never unloaded ${PACK}::Load($$unload[2])";
    }
    #delete $self->{DEPENDON};
}


use vars qw( @CreateKey_Opts %CreateKey_Opts %_KeyDispNames );
@CreateKey_Opts= qw( Access Class Options Delimiter
		     Disposition Security Volatile Backup );
@CreateKey_Opts{@CreateKey_Opts}= (1) x @CreateKey_Opts;
%_KeyDispNames= ( REG_CREATED_NEW_KEY() => "REG_CREATED_NEW_KEY",
		  REG_OPENED_EXISTING_KEY() => "REG_OPENED_EXISTING_KEY" );

sub CreateKey
{
    my $self= shift(@_);
    my $tied= tied(%$self);
    $self= tied(%$self)   if  $tied;
    my( $subKey, $opts )= @_;
    my( $sam )= $self->Access;
    my( $delim )= $self->Delimiter;
    my( $class )= "";
    my( $flags )= 0;
    my( $secure )= [];
    my( $garb )= [];
    my( $result )= \$garb;
    my( $handle )= 0;
    if(  @_ < 1  ||  2 < @_
     ||  2 == @_ && "HASH" ne ref($opts)  ) {
	croak "Usage:  \$new= \$old->CreateKey( \$subKey, {OPT=>VAL,...} );\n",
	      "  options: @CreateKey_Opts\nCalled";
    }
    if(  defined($opts)  ) {
	$sam= $opts->{"Access"}   if  defined($opts->{"Access"});
	$class= $opts->{Class}   if  defined($opts->{Class});
	$flags= $opts->{Options}   if  defined($opts->{Options});
	$delim= $opts->{"Delimiter"}   if  defined($opts->{"Delimiter"});
	$secure= $opts->{Security}   if  defined($opts->{Security});
	if(  defined($opts->{Disposition})  ) {
	    "SCALAR" eq ref($opts->{Disposition})
	      or  croak "${PACK}->CreateKey option `Disposition'",
			" must provide a scalar reference";
	    $result= $opts->{Disposition};
	}
	if(  0 == $flags  ) {
	    $flags |= REG_OPTION_VOLATILE
	      if  defined($opts->{Volatile})  &&  $opts->{Volatile};
	    $flags |= REG_OPTION_BACKUP_RESTORE
	      if  defined($opts->{Backup})  &&  $opts->{Backup};
	}
    }
    my $subPath= ref($subKey) ? $subKey : $self->_split($subKey,$delim);
    $subKey= join( $self->OS_Delimiter, @$subPath );
    $self->RegCreateKeyEx( $subKey, 0, $class, $flags, $sam,
			   $secure, $handle, $$result )
      or  return ();
    if(  ! ref($$result)  &&  $self->DualTypes  ) {
	$$result= _DualVal( \%_KeyDispNames, $$result );
    }
    my $new= $self->_new( $handle, [ @{$self->_Path}, @{$subPath} ] );
    $new->{ACCESS}= $sam;
    $new->{DELIM}= $delim;
    $new= $new->TiedRef   if  $tied;
    return $new;
}


use vars qw( $Load_Cnt @Load_Opts %Load_Opts );
$Load_Cnt= 0;
@Load_Opts= qw(NewSubKey);
@Load_Opts{@Load_Opts}= (1) x @Load_Opts;

sub Load
{
    my $this= shift(@_);
    my $tied=  ref($this)  &&  tied(%$this);
    $this= tied(%$this)   if  $tied;
    my( $file, $subKey, $opts )= @_;
    if(  2 == @_  &&  "HASH" eq ref($subKey)  ) {
	$opts= $subKey;
	undef $subKey;
    }
    @_ < 1  ||  3 < @_  ||  defined($opts) && "HASH" ne ref($opts)
      and  croak "Usage:  \$key= ",
	     "${PACK}->Load( \$fileName, [\$newSubKey,] {OPT=>VAL...} );\n",
	     "  options: @Load_Opts @new_Opts\nCalled";
    if(  defined($opts)  &&  exists($opts->{NewSubKey})  ) {
	$subKey= delete $opts->{NewSubKey};
    }
    if(  ! defined( $subKey )  ) {
	if(  "" ne $this->Machine  ) {
	    ( $this )= $this->_connect( [$this->Machine,"LMachine"] );
	} else {
	    ( $this )= $this->_rootKey( "LMachine" );	# Could also be "Users"
	}
	$subKey= "PerlTie:$$." . ++$Load_Cnt;
    }
    $this->RegLoadKey( $subKey, $file )
      or  return ();
    my $self= $this->new( $subKey, defined($opts) ? $opts : () );
    if(  ! defined( $self )  ) {
	{ my $err= Win32::GetLastError();
	#{ local( $^E ); #}
	    $this->RegUnLoadKey( $subKey )  or  carp
	      "Can't unload $subKey from ", $this->Path, ": ", _ErrMsg, "\n";
	    Win32::SetLastError($err);
	}
	return ();
    }
    $self->{UNLOADME}= [ $this, $subKey, $file ];
    $self= $self->TiedRef   if  $tied;
    return $self;
}


sub UnLoad
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    @_  and  croak "Usage:  \$key->UnLoad;";
    my $unload= $self->{UNLOADME};
    "ARRAY" eq ref($unload)
      or  croak "${PACK}->UnLoad called on a key which was not Load()ed";
    my( $obj, $subKey, $file )= @$unload;
    $self->RegCloseKey;
    return Win32API::Registry::RegUnLoadKey( $obj->Handle, $subKey );
}


sub AllowSave
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    return $self->AllowPriv( "SeBackupPrivilege", @_ );
}


sub AllowLoad
{
    my $self= shift(@_);
    $self= tied(%$self)   if  tied(%$self);
    return $self->AllowPriv( "SeRestorePrivilege", @_ );
}


# RegNotifyChangeKeyValue( hKey, bWatchSubtree, iNotifyFilter, hEvent, bAsync )


sub RegCloseKey { my $self= shift(@_);
    Win32API::Registry::RegCloseKey $self->Handle, @_; }
sub RegConnectRegistry { my $self= shift(@_);
    Win32API::Registry::RegConnectRegistry @_; }
sub RegCreateKey { my $self= shift(@_);
    Win32API::Registry::RegCreateKey $self->Handle, @_; }
sub RegCreateKeyEx { my $self= shift(@_);
    Win32API::Registry::RegCreateKeyEx $self->Handle, @_; }
sub RegDeleteKey { my $self= shift(@_);
    Win32API::Registry::RegDeleteKey $self->Handle, @_; }
sub RegDeleteValue { my $self= shift(@_);
    Win32API::Registry::RegDeleteValue $self->Handle, @_; }
sub RegEnumKey { my $self= shift(@_);
    Win32API::Registry::RegEnumKey $self->Handle, @_; }
sub RegEnumKeyEx { my $self= shift(@_);
    Win32API::Registry::RegEnumKeyEx $self->Handle, @_; }
sub RegEnumValue { my $self= shift(@_);
    Win32API::Registry::RegEnumValue $self->Handle, @_; }
sub RegFlushKey { my $self= shift(@_);
    Win32API::Registry::RegFlushKey $self->Handle, @_; }
sub RegGetKeySecurity { my $self= shift(@_);
    Win32API::Registry::RegGetKeySecurity $self->Handle, @_; }
sub RegLoadKey { my $self= shift(@_);
    Win32API::Registry::RegLoadKey $self->Handle, @_; }
sub RegNotifyChangeKeyValue { my $self= shift(@_);
    Win32API::Registry::RegNotifyChangeKeyValue $self->Handle, @_; }
sub RegOpenKey { my $self= shift(@_);
    Win32API::Registry::RegOpenKey $self->Handle, @_; }
sub RegOpenKeyEx { my $self= shift(@_);
    Win32API::Registry::RegOpenKeyEx $self->Handle, @_; }
sub RegQueryInfoKey { my $self= shift(@_);
    Win32API::Registry::RegQueryInfoKey $self->Handle, @_; }
sub RegQueryMultipleValues { my $self= shift(@_);
    Win32API::Registry::RegQueryMultipleValues $self->Handle, @_; }
sub RegQueryValue { my $self= shift(@_);
    Win32API::Registry::RegQueryValue $self->Handle, @_; }
sub RegQueryValueEx { my $self= shift(@_);
    Win32API::Registry::RegQueryValueEx $self->Handle, @_; }
sub RegReplaceKey { my $self= shift(@_);
    Win32API::Registry::RegReplaceKey $self->Handle, @_; }
sub RegRestoreKey { my $self= shift(@_);
    Win32API::Registry::RegRestoreKey $self->Handle, @_; }
sub RegSaveKey { my $self= shift(@_);
    Win32API::Registry::RegSaveKey $self->Handle, @_; }
sub RegSetKeySecurity { my $self= shift(@_);
    Win32API::Registry::RegSetKeySecurity $self->Handle, @_; }
sub RegSetValue { my $self= shift(@_);
    Win32API::Registry::RegSetValue $self->Handle, @_; }
sub RegSetValueEx { my $self= shift(@_);
    Win32API::Registry::RegSetValueEx $self->Handle, @_; }
sub RegUnLoadKey { my $self= shift(@_);
    Win32API::Registry::RegUnLoadKey $self->Handle, @_; }
sub AllowPriv { my $self= shift(@_);
    Win32API::Registry::AllowPriv @_; }


# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

Win32::TieRegistry - Powerful and easy ways to manipulate a registry
[on Win32 for now].

=head1 SYNOPSIS

  use Win32::TieRegistry 0.20 ( UseOptionName=>UseOptionValue[,...] );

  $Registry->SomeMethodCall(arg1,...);

  $subKey= $Registry->{"Key\\SubKey\\"};
  $valueData= $Registry->{"Key\\SubKey\\\\ValueName"};
  $Registry->{"Key\\SubKey\\"}= { "NewSubKey" => {...} };
  $Registry->{"Key\\SubKey\\\\ValueName"}= "NewValueData";
  $Registry->{"\\ValueName"}= [ pack("fmt",$data), REG_DATATYPE ];

=head1 EXAMPLES

  use Win32::TieRegistry( Delimiter=>"#", ArrayValues=>0 );
  $pound= $Registry->Delimiter("/");
  $diskKey= $Registry->{"LMachine/System/Disk/"}
    or  die "Can't read LMachine/System/Disk key: $^E\n";
  $data= $key->{"/Information"}
    or  die "Can't read LMachine/System/Disk//Information value: $^E\n";
  $remoteKey= $Registry->{"//ServerA/LMachine/System/"}
    or  die "Can't read //ServerA/LMachine/System/ key: $^E\n";
  $remoteData= $remoteKey->{"Disk//Information"}
    or  die "Can't read ServerA's System/Disk//Information value: $^E\n";
  foreach $entry (  keys(%$diskKey)  ) {
      ...
  }
  foreach $subKey (  $diskKey->SubKeyNames  ) {
      ...
  }
  $diskKey->AllowSave( 1 );
  $diskKey->RegSaveKey( "C:/TEMP/DiskReg", [] );

=head1 DESCRIPTION

The I<Win32::TieRegistry> module lets you manipulate the Registry
via objects [as in "object oriented"] or via tied hashes.  But
you will probably mostly use a combination reference, that is, a
reference to a tied hash that has also been made an object so that
you can mix both access methods [as shown above].

If you did not get this module as part of L<libwin32>, you might
want to get a recent version of L<libwin32> from CPAN which should
include this module and the I<Win32API::Registry> module that it
uses.

Skip to the L<SUMMARY> section if you just want to dive in and start
using the Registry from Perl.

Accessing and manipulating the registry is extremely simple using
I<Win32::TieRegistry>.  A single, simple expression can return
you almost any bit of information stored in the Registry.
I<Win32::TieRegistry> also gives you full access to the "raw"
underlying API calls so that you can do anything with the Registry
in Perl that you could do in C.  But the "simple" interface has
been carefully designed to handle almost all operations itself
without imposing arbitrary limits while providing sensible
defaults so you can list only the parameters you care about.

But first, an overview of the Registry itself.

=head2 The Registry

The Registry is a forest:  a collection of several tree structures.
The root of each tree is a key.  These root keys are identified by
predefined constants whose names start with "HKEY_".  Although all
keys have a few attributes associated with each [a class, a time
stamp, and security information], the most important aspect of keys
is that each can contain subkeys and can contain values.

Each subkey has a name:  a string which cannot be blank and cannot
contain the delimiter character [backslash: C<'\\'>] nor nul
[C<'\0'>].  Each subkey is also a key and so can contain subkeys
and values [and has a class, time stamp, and security information].

Each value has a name:  a string which E<can> be blank and E<can>
contain the delimiter character [backslash: C<'\\'>] and any
character except for null, C<'\0'>.  Each value also has data
associated with it.  Each value's data is a contiguous chunk of
bytes, which is exactly what a Perl string value is so Perl
strings will usually be used to represent value data.

Each value also has a data type which says how to interpret the
value data.  The primary data types are:

=over

=item REG_SZ

A null-terminated string.

=item REG_EXPAND_SZ

A null-terminated string which contains substrings consisting of a
percent sign [C<'%'>], an environment variable name, then a percent
sign, that should be replaced with the value associate with that
environment variable.  The system does I<not> automatically do this
substitution.

=item REG_BINARY

Some arbitrary binary value.  You can think of these as being
"packed" into a string.

If your system has the L<SetDualVar> module installed,
the C<DualBinVals()> option wasn't turned off, and you
fetch a C<REG_BINARY> value of 4 bytes or fewer, then
you can use the returned value in a numeric context to
get at the "unpacked" numeric value.  See C<GetValue()>
for more information.

=item REG_MULTI_SZ

Several null-terminated strings concatenated together with an
extra trailing C<'\0'> at the end of the list.  Note that the list
can include empty strings so use the value's length to determine
the end of the list, not the first occurrence of C<'\0\0'>.
It is best to set the C<SplitMultis()> option so I<Win32::TieRegistry>
will split these values into an array of strings for you.

=item REG_DWORD

A long [4-byte] integer value.  These values are expected either
packed into a 4-character string or as a hex string of E<more than>
4 characters [but I<not> as a numeric value, unfortunately, as there is
no sure way to tell a numeric value from a packed 4-byte string that
just happens to be a string containing a valid numeric value].

How such values are returned depends on the C<DualBinVals()> and
C<DWordsToHex()> options.  See C<GetValue()> for details.

=back

In the underlying Registry calls, most places which take a
subkey name also allow you to pass in a subkey "path" -- a
string of several subkey names separated by the delimiter
character, backslash [C<'\\'>].  For example, doing
C<RegOpenKeyEx(HKEY_LOCAL_MACHINE,"SYSTEM\\DISK",...)> is much
like opening the C<"SYSTEM"> subkey of C<HKEY_LOCAL_MACHINE>,
then opening its C<"DISK"> subkey, then closing the C<"SYSTEM">
subkey.

All of the I<Win32::TieRegistry> features allow you to use your
own delimiter in place of the system's delimiter, [C<'\\'>].  In
most of our examples we will use a forward slash [C<'/'>] as our
delimiter as it is easier to read and less error prone to use when
writing Perl code since you have to type two backslashes for each
backslash you want in a string.  Note that this is true even when
using single quotes -- C<'\\HostName\LMachine\'> is an invalid
string and must be written as C<'\\\\HostName\\LMachine\\'>.

You can also connect to the registry of other computers on your
network.  This will be discussed more later.

Although the Registry does not have a single root key, the
I<Win32::TieRegistry> module creates a virtual root key for you
which has all of the I<HKEY_*> keys as subkeys.

=head2 Tied Hashes Documentation

Before you can use a tied hash, you must create one.  One way to
do that is via:

    use Win32::TieRegistry ( TiedHash => '%RegHash' );

which exports a C<%RegHash> variable into your package and ties it
to the virtual root key of the Registry.  An alternate method is:

    my %RegHash;
    use Win32::TieRegistry ( TiedHash => \%RegHash );

There are also several ways you can tie a hash variable to any
other key of the Registry, which are discussed later.

Note that you will most likely use C<$Registry> instead of using
a tied hash.  C<$Registry> is a reference to a hash that has
been tied to the virtual root of your computer's Registry [as if,
C<$Registry= \%RegHash>].  So you would use C<$Registry-E<gt>{Key}>
rather than C<$RegHash{Key}> and use C<keys %{$Registry}> rather
than C<keys %RegHash>, for example.

For each hash which has been tied to a Registry key, the Perl
C<keys> function will return a list containing the name of each
of the key's subkeys with a delimiter character appended to it and
containing the name of each of the key's values with a delimiter
prepended to it.  For example:

    keys( %{ $Registry->{"HKEY_CLASSES_ROOT\\batfile\\"} } )

might yield the following list value:

    ( "DefaultIcon\\",  # The subkey named "DefaultIcon"
      "shell\\",        # The subkey named "shell"
      "shellex\\",      # The subkey named "shellex"
      "\\",             # The default value [named ""]
      "\\EditFlags" )   # The value named "EditFlags"

For the virtual root key, short-hand subkey names are used as
shown below.  You can use the short-hand name, the regular
I<HKEY_*> name, or any numeric value to access these keys, but
the short-hand names are all that will be returned by the C<keys>
function.

=over

=item "Classes" for HKEY_CLASSES_ROOT

Contains mappings between file name extensions and the uses
for such files along with configuration information for COM
[MicroSoft's Common Object Model] objects.  Usually a link to
the C<"SOFTWARE\\Classes"> subkey of the C<HKEY_LOCAL_MACHINE>
key.

=item "CUser" for HKEY_CURRENT_USER

Contains information specific to the currently logged-in user.
Mostly software configuration information.  Usually a link to
a subkey of the C<HKEY_USERS> key.

=item "LMachine" for HKEY_LOCAL_MACHINE

Contains all manner of information about the computer.

=item "Users" for HKEY_USERS

Contains one subkey, C<".DEFAULT">, which gets copied to a new
subkey whenever a new user is added.  Also contains a subkey for
each user of the system, though only those for active users
[usually only one] are loaded at any given time.

=item "PerfData" for HKEY_PERFORMANCE_DATA

Used to access data about system performance.  Access via this key
is "special" and all but the most carefully constructed calls will
fail, usually with C<ERROR_INSUFFICIENT_BUFFER>.  For example, you
can't enumerate key names without also enumerating values which
require huge buffers but the exact buffer size required cannot be
determined beforehand because C<RegQueryInfoKey()> E<always> fails
with C<ERROR_INSUFFICIENT_BUFFER> for C<HKEY_PERFORMANCE_DATA> no
matter how it is called.  So it is currently not very useful to
tie a hash to this key.  You can use it to create an object to use
for making carefully constructed calls to the underlying Reg*()
routines.

=item "CConfig" for HKEY_CURRENT_CONFIG

Contains minimal information about the computer's current
configuration that is required very early in the boot process.
For example, setting for the display adapter such as screen
resolution and refresh rate are found in here.

=item "DynData" for HKEY_DYN_DATA

Dynamic data.  We have found no documentation for this key.

=back

A tied hash is much like a regular hash variable in Perl -- you give
it a key string inside braces, [C<{> and C<}>], and it gives you
back a value [or lets you set a value].  For I<Win32::TieRegistry>
hashes, there are two types of values that will be returned.

=over

=item SubKeys

If you give it a string which represents a subkey, then it will
give you back a reference to a hash which has been tied to that
subkey.  It can't return the hash itself, so it returns a
reference to it.  It also blesses that reference so that it is
also an object so you can use it to call method functions.

=item Values

If you give it a string which is a value name, then it will give
you back a string which is the data for that value.  Alternately,
you can request that it give you both the data value string and
the data value type [we discuss how to request this later].  In
this case, it would return a reference to an array where the value
data string is element C<[0]> and the value data type is element
C<[1]>.

=back

The key string which you use in the tied hash must be interpreted
to determine whether it is a value name or a key name or a path
that combines several of these or even other things.  There are
two simple rules that make this interpretation easy and
unambiguous:

    Put a delimiter after each key name.
    Put a delimiter in front of each value name.

Exactly how the key string will be intepreted is governed by the
following cases, in the order listed.  These cases are designed
to "do what you mean".  Most of the time you won't have to think
about them, especially if you follow the two simple rules above.
After the list of cases we give several examples which should be
clear enough so feel free to skip to them unless you are worried
about the details.

=over

=item Remote machines

If the hash is tied to the virtual root of the registry [or the
virtual root of a remote machine's registry], then we treat hash
key strings which start with the delimiter character specially.

If the hash key string starts with two delimiters in a row, then
those should be immediately followed by the name of a remote
machine whose registry we wish to connect to.  That can be
followed by a delimiter and more subkey names, etc.  If the
machine name is not following by anything, then a virtual root
for the remote machine's registry is created, a hash is tied to
it, and a reference to that hash it is returned.

=item Hash key string starts with the delimiter

If the hash is tied to a virtual root key, then the leading
delimiter is ignored.  It should be followed by a valid Registry
root key name [either a short-hand name like C<"LMachine">, an
I<HKEY_*> value, or a numeric value].   This alternate notation is
allowed in order to be more consistant with the C<Open()> method
function.

For all other Registry keys, the leading delimiter indicates
that the rest of the string is a value name.  The leading
delimiter is stripped and the rest of the string [which can
be empty and can contain more delimiters] is used as a value
name with no further parsing.

=item Exact match with direct subkey name followed by delimiter

If you have already called the Perl C<keys> function on the tied
hash [or have already called C<MemberNames> on the object] and the
hash key string exactly matches one of the strings returned, then
no further parsing is done.  In other words, if the key string
exactly matches the name of a direct subkey with a delimiter
appended, then a reference to a hash tied to that subkey is
returned [but only if C<keys> or C<MemberNames> has already
been called for that tied hash].

This is only important if you have selected a delimiter other than
the system default delimiter and one of the subkey names contains
the delimiter you have chosen.  This rule allows you to deal with
subkeys which contain your chosen delimiter in their name as long
as you only traverse subkeys one level at a time and always
enumerate the list of members before doing so.

The main advantage of this is that Perl code which recursively
traverses a hash will work on hashes tied to Registry keys even if
a non-default delimiter has been selected.

=item Hash key string contains two delimiters in a row

If the hash key string contains two [or more] delimiters in a row,
then the string is split between the first pair of delimiters.
The first part is interpreted as a subkey name or a path of subkey
names separated by delimiters and with a trailing delimiter.  The
second part is interpreted as a value name with one leading
delimiter [any extra delimiters are considered part of the value
name].

=item Hash key string ends with a delimiter

If the key string ends with a delimiter, then it is treated
as a subkey name or path of subkey names separated by delimiters.

=item Hash key string contains a delimiter

If the key string contains a delimiter, then it is split after
the last delimiter.  The first part is treated as a subkey name or
path of subkey names separated by delimiters.  The second part
is ambiguous and is treated as outlined in the next item.

=item Hash key string contains no delimiters

If the hash key string contains no delimiters, then it is ambiguous.

If you are reading from the hash [fetching], then we first use the
key string as a value name.  If there is a value with a matching
name in the Registry key which the hash is tied to, then the value
data string [and possibly the value data type] is returned.
Otherwise, we retry by using the hash key string as a subkey name.
If there is a subkey with a matching name, then we return a
reference to a hash tied to that subkey.  Otherwise we return
C<undef>.

If you are writing to the hash [storing], then we use the key
string as a subkey name only if the value you are storing is a
reference to a hash value.  Otherwise we use the key string as
a value name.

=back

=head3 Examples

Here are some examples showing different ways of accessing Registry
information using references to tied hashes:

=over

=item Canonical value fetch

    $tip18= $Registry->{"HKEY_LOCAL_MACHINE\\Software\\Microsoft\\"
               . 'Windows\\CurrentVersion\\Explorer\\Tips\\\\18'};

Should return the text of important tip number 18.  Note that two
backslashes, C<"\\">, are required to get a single backslash into
a Perl double-quoted or single-qouted string.  Note that C<"\\">
is appended to each key name [C<"HKEY_LOCAL_MACHINE"> through
C<"Tips">] and C<"\\"> is prepended to the value name, C<"18">.

=item Changing your delimiter

    $Registry->Delimiter("/");
    $tip18= $Registry->{"HKEY_LOCAL_MACHINE/Software/Microsoft/"
               . 'Windows/CurrentVersion/Explorer/Tips//18'};

This usually makes things easier to read when working in Perl.
All remaining examples will assume the delimiter has been changed
as above.

=item Using intermediate keys

    $ms= $Registry->{"LMachine/Software/Microsoft/"};
    $tips= $ms->{"Windows/CurrentVersion/Explorer/Tips/"};
    $tip18= $winlogon->{"/18"};

Same as above but opens more keys into the Registry which lets you
efficiently re-access those intermediate keys.  This is slightly
less efficient if you never reuse those intermediate keys.

=item Chaining in a single statement

    $tip18= $Registry->{"LMachine/Software/Microsoft/"}->
              {"Windows/CurrentVersion/Explorer/Tips/"}->{"/18"};

Like above, this creates intermediate key objects then uses
them to access other data.  Once this statement finishes, the
intermediate key objects are destroyed.  Several handles into
the Registry are opened and closed by this statement so it is
less efficient but there are times when this will be useful.

=item Even less efficient example of chaining

    $tip18= $Registry->{"LMachine/Software/Microsoft"}->
              {"Windows/CurrentVersion/Explorer/Tips"}->{"/18"};

Because we left off the trailing delimiters, I<Win32::TieRegistry>
doesn't know whether final names, C<"Microsoft"> and C<"Tips">,
are subkey names or value names.  So this statement ends up
executing the same code as the next one.

=item What the above really does

    $tip18= $Registry->{"LMachine/Software/"}->{"Microsoft"}->
              {"Windows/CurrentVersion/Explorer/"}->{"Tips"}->{"/18"};

With more chains to go through, more temporary objects are created
and later destroyed than in our first chaining example.  Also,
when C<"Microsoft"> is looked up, I<Win32::TieRegistry> first
tries to open it as a value and fails then tries it as a subkey.
The same is true for when it looks up C<"Tips">.

=item Getting all of the tips

    $tips= $Registry->{"LMachine/Software/Microsoft/"}->
              {"Windows/CurrentVersion/Explorer/Tips/"}
      or  die "Can't find the Windows tips: $^E\n";
    foreach(  keys %$tips  ) {
        print "$_: ", $tips->{$_}, "\n";
    }

First notice that we actually check for failure for the first
time.  We are assuming that the C<"Tips"> key contains no subkeys. 
Otherwise the C<print> statement would show something like
C<"Win32::TieRegistry=HASH(0xc03ebc)"> for each subkey.

The output from the above code will start something like:

    /0: If you don't know how to do something,[...]

=back

=head3 Deleting items

You can use the Perl C<delete> function to delete a value from a
Registry key or to delete a subkey as long that subkey contains
no subkeys of its own.  See L<More Examples>, below, for more
information.

=head3 Storing items

You can use the Perl assignment operator [C<=>] to create new
keys, create new values, or replace values.  The values you store
should be in the same format as the values you would fetch from a
tied hash.  For example, you can use a single assignment statement
to copy an entire Registry tree.  The following statement:

    $Registry->{"LMachine/Software/Classes/Tie_Registry/"}=
      $Registry->{"LMachine/Software/Classes/batfile/"};

creates a C<"Tie_Registry"> subkey under the C<"Software\\Classes">
subkey of the C<HKEY_LOCAL_MACHINE> key.  Then it populates it
with copies of all of the subkeys and values in the C<"batfile">
subkey and all of its subkeys.  Note that you need to have
called C<$Registry-E<gt>ArrayValues(1)> for the proper value data
type information to be copied.  Note also that this release of
I<Win32::TieRegistry> does not copy key attributes such as class
name and security information [this is planned for a future release].

The following statement creates a whole subtree in the Registry:

    $Registry->{"LMachine/Software/FooCorp/"}= {
        "FooWriter/" => {
            "/Version" => "4.032",
            "Startup/" => {
                "/Title" => "Foo Writer Deluxe ][",
                "/WindowSize" => [ pack("LL",$wid,$ht), "REG_BINARY" ],
                "/TaskBarIcon" => [ "0x0001", "REG_DWORD" ],
            },
            "Compatibility/" => {
                "/AutoConvert" => "Always",
                "/Default Palette" => "Windows Colors",
            },
        },
        "/License", => "0123-9C8EF1-09-FC",
    };

Note that all but the last Registry key used on the left-hand
side of the assignment [that is, "LMachine/Software/" but not
"FooCorp/"] must already exist for this statement to succeed.

By using the leading a trailing delimiters on each subkey name and
value name, I<Win32::TieRegistry> will tell you if you try to assign
subkey information to a value or visa-versa.

=head3 More examples

=over

=item Adding a new tip

    $tips= $Registry->{"LMachine/Software/Microsoft/"}->
              {"Windows/CurrentVersion/Explorer/Tips/"}
      or  die "Can't find the Windows tips: $^E\n";
    $tips{'/186'}= "Be very careful when making changes to the Registry!";

=item Deleting our new tip

    $tips= $Registry->{"LMachine/Software/Microsoft/"}->
              {"Windows/CurrentVersion/Explorer/Tips/"}
      or  die "Can't find the Windows tips: $^E\n";
    $tip186= delete $tips{'/186'};

Note that Perl's C<delete> function returns the value that was deleted.

=item Adding a new tip differently

    $Registry->{"LMachine/Software/Microsoft/" .
                "Windows/CurrentVersion/Explorer/Tips//186"}=
      "Be very careful when making changes to the Registry!";

=item Deleting differently

    $tip186= delete $Registry->{"LMachine/Software/Microsoft/Windows/" .
                                "CurrentVersion/Explorer/Tips//186"};

Note that this only deletes the tail of what we looked up, the
C<"186"> value, not any of the keys listed.

=item Deleting a key

WARNING:  The following code will delete all information about the
current user's tip preferences.  Actually executing this command
would probably cause the user to see the Welcome screen the next
time they log in and may cause more serious problems.  This
statement is shown as an example only and should not be used when
experimenting.

    $tips= delete $Registry->{"CUser/Software/Microsoft/Windows/" .
                              "CurrentVersion/Explorer/Tips/"};

This deletes the C<"Tips"> key and the values it contains.  The
C<delete> function will return a reference to a hash [not a tied
hash] containing the value names and value data that were deleted.

The information to be returned is copied from the Registry into a
regular Perl hash before the key is deleted.  If the key has many
subkeys, this copying could take a significant amount of memory
and/or processor time.  So you can disable this process by calling
the C<FastDelete> member function:

    $prevSetting= $regKey->FastDelete(1);

which will cause all subsequent delete operations via C<$regKey>
to simply return a true value if they succeed.  This optimization
is automatically done if you use C<delete> in a void context.

=item Technical notes on deleting

If you use C<delete> to delete a Registry key or value and use
the return value, then I<Win32::TieRegistry> usually looks up the
current contents of that key or value so they can be returned if
the deletion is successful.  If the deletion succeeds but the
attempt to lookup the old contents failed, then the return value
of C<delete> will be C<$^E> from the failed part of the operation.

=item Undeleting a key

    $Registry->{"LMachine/Software/Microsoft/Windows/" .
                "CurrentVersion/Explorer/Tips/"}= $tips;

This adds back what we just deleted.  Note that this version of
I<Win32::TieRegistry> will use defaults for the key attributes
[such as class name and security] and will not restore the
previous attributes.

=item Not deleting a key

WARNING:  Actually executing the following code could cause
serious problems.  This statement is shown as an example only and
should not be used when experimenting.

    $res= delete $Registry->{"CUser/Software/Microsoft/Windows/"}
    defined($res)  ||  die "Can't delete URL key: $^E\n";

Since the "Windows" key should contain subkeys, that C<delete>
statement should make no changes to the Registry, return C<undef>,
and set C<$^E> to "Access is denied".

=item Not deleting again

    $tips= $Registry->{"CUser/Software/Microsoft/Windows/" .
                       "CurrentVersion/Explorer/Tips/"};
    delete $tips;

The Perl C<delete> function requires that its argument be an
expression that ends in a hash element lookup [or hash slice],
which is not the case here.  The C<delete> function doesn't
know which hash $tips came from and so can't delete it.

=back

=head2 Objects Documentation

The following member functions are defined for use on
I<Win32::TieRegistry> objects:

=over

=item new

The C<new> method creates a new I<Win32::TieRegistry> object.
C<new> is mostly a synonym for C<Open()> so see C<Open()> below for
information on what arguments to pass in.  Examples:

    $machKey= new Win32::TieRegistry "LMachine"
      or  die "Can't access HKEY_LOCAL_MACHINE key: $^E\n";
    $userKey= Win32::TieRegistry->new("CUser")
      or  die "Can't access HKEY_CURRENT_USER key: $^E\n";

Note that calling C<new> via a reference to a tied hash returns
a simple object, not a reference to a tied hash.

=item Open

=item $subKey= $key->Open( $sSubKey, $rhOptions )

The C<Open> method opens a Registry key and returns a new
I<Win32::TieRegistry> object associated with that Registry key.
If C<Open> is called via a reference to a tied hash, then C<Open>
returns another reference to a tied hash.  Otherwise C<Open>
returns a simple object and you should then use C<TiedRef> to get
a reference to a tied hash.

C<$sSubKey> is a string specifying a subkey to be opened.
Alternately C<$sSubKey> can be a reference to an array value
containing the list of increasingly deep subkeys specifying the
path to the subkey to be opened.

C<$rhOptions> is an optional reference to a hash containing extra
options.  The C<Open> method supports two options, C<"Delimiter">
and C<"Access">, and C<$rhOptions> should have only have zero or
more of these strings as keys.  See the "Examples" section below
for more information.

The C<"Delimiter"> option specifies what string [usually a single
character] will be used as the delimiter to be appended to subkey
names and prepended to value names.  If this option is not specified,
the new key [C<$subKey>] inherits the delimiter of the old key
[C<$key>].

The C<"Access"> option specifies what level of access to the
Registry key you wish to have once it has been opened.  If this
option is not specified, the new key [C<$subKey>] is opened with
the same access level used when the old key [C<$key>] was opened.
The virtual root of the Registry pretends it was opened with
access C<KEY_READ()|KEY_WRITE()> so this is the default access when
opening keys directory via C<$Registry>.  If you don't plan on
modifying a key, you should open it with C<KEY_READ> access as
you may not have C<KEY_WRITE> access to it or some of its subkeys.

If the C<"Access"> option value is a string that starts with
C<"KEY_">, then it should match E<one> of the predefined access
levels [probably C<"KEY_READ">, C<"KEY_WRITE">, or
C<"KEY_ALL_ACCESS">] exported by the I<Win32API::Registry> module.
Otherwise, a numeric value is expected.  For maximum flexibility,
include C<use Win32::TieRegistry qw(:KEY_);>, for example, near
the top of your script so you can specify more complicated access
levels such as C<KEY_READ()|KEY_WRITE()>.

If C<$sSubKey> does not begin with the delimiter [or C<$sSubKey>
is an array reference], then the path to the subkey to be opened
will be relative to the path of the original key [C<$key>].  If
C<$sSubKey> begins with a single delimiter, then the path to the
subkey to be opened will be relative to the virtual root of the
Registry on whichever machine the original key resides.  If
C<$sSubKey> begins with two consectutive delimiters, then those
must be followed by a machine name which causes the C<Connect()>
method function to be called.

Examples:

    $machKey= $Registry->Open( "LMachine", {Access=>KEY_READ(),Delimiter=>"/"} )
      or  die "Can't open HKEY_LOCAL_MACHINE key: $^E\n";
    $swKey= $machKey->Open( "Software" );
    $logonKey= $swKey->Open( "Microsoft/Windows NT/CurrentVersion/Winlogon/" );
    $NTversKey= $swKey->Open( ["Microsoft","Windows NT","CurrentVersion"] );
    $versKey= $swKey->Open( qw(Microsoft Windows CurrentVersion) );

    $remoteKey= $Registry->Open( "//HostA/LMachine/System/", {Delimiter=>"/"} )
      or  die "Can't connect to HostA or can't open subkey: $^E\n";

=item Clone

=item $copy= $key->Clone

Creates a new object that is associated with the same Registry key
as the invoking object.

=item Connect

=item $remoteKey= $Registry->Connect( $sMachineName, $sKeyPath, $rhOptions )

The C<Connect> method connects to the Registry of a remote machine,
and opens a key within it, then returns a new I<Win32::TieRegistry>
object associated with that remote Registry key.  If C<Connect>
was called using a reference to a tied hash, then the return value
will also be a reference to a tied hash [or C<undef>].  Otherwise,
if you wish to use the returned object as a tied hash [not just as
an object], then use the C<TiedRef> method function after C<Connect>.

C<$sMachineName> is the name of the remote machine.  You don't have
to preceed the machine name with two delimiter characters.

C<$sKeyPath> is a string specifying the remote key to be opened.
Alternately C<$sKeyPath> can be a reference to an array value
containing the list of increasingly deep keys specifying the path
to the key to be opened.

C<$rhOptions> is an optional reference to a hash containing extra
options.  The C<Connect> method supports two options, C<"Delimiter">
and C<"Access">.  See the C<Open> method documentation for more
information on these options.

C<$sKeyPath> is already relative to the virtual root of the Registry
of the remote machine.  A single leading delimiter on C<sKeyPath>
will be ignored and is not required.

C<$sKeyPath> can be empty in which case C<Connect> will return an
object representing the virtual root key of the remote Registry.
Each subsequent use of C<Open> on this virtual root key will call
the system C<RegConnectRegistry> function.

The C<Connect> method can be called via any I<Win32::TieRegistry>
object, not just C<$Registry>.  Attributes such as the desired
level of access and the delimiter will be inherited from the
object used but the C<$sKeyPath> will always be relative to the
virtual root of the remote machine's registry.

Examples:

    $remMachKey= $Registry->Connect( "HostA", "LMachine", {Delimiter->"/"} )
      or  die "Can't connect to HostA's HKEY_LOCAL_MACHINE key: $^E\n";

    $remVersKey= $remMachKey->Connect( "www.microsoft.com",
                   "LMachine/Software/Microsoft/Inetsrv/CurrentVersion/",
                   { Access=>KEY_READ, Delimiter=>"/" } )
      or  die "Can't check what version of IIS Microsoft is running: $^E\n";

    $remVersKey= $remMachKey->Connect( "www",
                   qw(LMachine Software Microsoft Inetsrv CurrentVersion) )
      or  die "Can't check what version of IIS we are running: $^E\n";

=item ObjectRef

=item $object_ref= $obj_or_hash_ref->ObjectRef

For a simple object, just returns itself [C<$obj == $obj->ObjectRef>].

For a reference to a tied hash [if it is also an object], C<ObjectRef>
returns the simple object that the hash is tied to.

This is primarilly useful when debugging since typing C<x $Registry>
will try to display your I<entire> registry contents to your screen.
But the debugger command C<x $Registry->ObjectRef> will just dump
the implementation details of the underlying object to your screen.

=item Flush( $bFlush )

Flushes all cached information about the Registry key so that future
uses will get fresh data from the Registry.

If the optional C<$bFlush> is specified and a true value, then
C<RegFlushKey()> will be called, which is almost never necessary.

=item GetValue

=item $ValueData= $key->GetValue( $sValueName )

=item ($ValueData,$ValueType)= $key->GetValue( $sValueName )

Gets a Registry value's data and data type.

C<$ValueData> is usually just a Perl string that contains the
value data [packed into it].  For certain types of data, however,
C<$ValueData> may be processed as described below.

C<$ValueType> is the C<REG_*> constant describing the type of value
data stored in C<$ValueData>.  If the C<DualTypes()> option is on,
then C<$ValueType> will be a dual value.  That is, when used in a
numeric context, C<$ValueType> will give the numeric value of a
C<REG_*> constant.  However, when used in a non-numeric context,
C<$ValueType> will return the name of the C<REG_*> constant, for
example C<"REG_SZ"> [note the quotes].  So both of the following
can be true at the same time:

    $ValueType == REG_SZ()
    $ValueType eq "REG_SZ"

=over

=item REG_SZ and REG_EXPAND_SZ

If the C<FixSzNulls()> option is on, then the trailing C<'\0'> will be
stripped [unless there isn't one] before values of type C<REG_SZ>
and C<REG_EXPAND_SZ> are returned.  Note that C<SetValue()> will add
a trailing C<'\0'> under similar circumstances.

=item REG_MULTI_SZ

If the C<SplitMultis()> option is on, then values of this type are
returned as a reference to an array containing the strings.  For
example, a value that, with C<SplitMultis()> off, would be returned as:

    "Value1\000Value2\000\000"

would be returned, with C<SplitMultis()> on, as:

    [ "Value1", "Value2" ]

=item REG_DWORD

If the C<DualBinVals()> option is on, then the value is returned
as a scalar containing both a string and a number [much like
the C<$!> variable -- see the L<SetDualVar> module for more
information] where the number part is the "unpacked" value.
Use the returned value in a numeric context to access this part
of the value.  For example:

    $num= 0 + $Registry->{"CUser/Console//ColorTable01"};

If the C<DWordsToHex()> option is off, the string part of the
returned value is a packed, 4-byte string [use C<unpack("L",$value)>
to get the numeric value.

If C<DWordsToHex()> is on, the string part of the returned value is
a 10-character hex strings [with leading "0x"].  You can use
C<hex($value)> to get the numeric value.

Note that C<SetValue()> will properly understand each of these
returned value formats no matter how C<DualBinVals()> is set.

=back

=item ValueNames

=item @names= $key->ValueNames

Returns the list of value names stored directly in a Registry key.
Note that the names returned do I<not> have a delimiter prepended
to them like with C<MemberNames()> and tied hashes.

Once you request this information, it is cached in the object and
future requests will always return the same list unless C<Flush()>
has been called.

=item SubKeyNames

=item @key_names= $key->SubKeyNames

Returns the list of subkey names stored directly in a Registry key.
Note that the names returned do I<not> have a delimiter appended
to them like with C<MemberNames()> and tied hashes.

Once you request this information, it is cached in the object and
future requests will always return the same list unless C<Flush()>
has been called.

=item SubKeyClasses

=item @classes= $key->SubKeyClasses

Returns the list of classes for subkeys stored directly in a
Registry key.  The classes are returned in the same order as
the subkey names returned by C<SubKeyNames()>.

=item SubKeyTimes

=item @times= $key->SubKeyTimes

Returns the list of last-modified times for subkeys stored
directly in a Registry key.  The times are returned in the same
order as the subkey names returned by C<SubKeyNames()>.  Each
time is a C<FILETIME> structure packed into a Perl string.

Once you request this information, it is cached in the object and
future requests will always return the same list unless C<Flush()>
has been called.

=item MemberNames

=item @members= $key->MemberNames

Returns the list of subkey names and value names stored directly
in a Registry key.  Subkey names have a delimiter appended to the
end and value names have a delimiter prepended to the front.

Note that a value name could end in a delimiter [or could be C<"">
so that the member name returned is just a delimiter] so the
presence or absence of the leading delimiter is what should be
used to determine whether a particular name is for a subkey or a
value, not the presence or absence of a trailing delimiter.

Once you request this information, it is cached in the object and
future requests will always return the same list unless C<Flush()>
has been called.

=item Information

=item %info= $key->Information

=item @items= $key->Information( @itemNames );

Returns the following information about a Registry key:

=over

=item LastWrite

A C<FILETIME> structure indicating when the key was last modified
and packed into a Perl string.

=item CntSubKeys

The number of subkeys stored directly in this key.

=item CntValues

The number of values stored directly in this key.

=item SecurityLen

The length [in bytes] of the largest[?] C<SECURITY_DESCRIPTOR>
associated with the Registry key.

=item MaxValDataLen

The length [in bytes] of the longest value data associated with
a value stored in this key.

=item MaxSubKeyLen

The length [in chars] of the longest subkey name associated with
a subkey stored in this key.

=item MaxSubClassLen

The length [in chars] of the longest class name associated with
a subkey stored directly in this key.

=item MaxValNameLen

The length [in chars] of the longest value name associated with
a value stored in this key.

=back

With no arguments, returns a hash [not a reference to a hash] where
the keys are the names for the items given above and the values
are the information describe above.  For example:

    %info= ( "CntValues" => 25,         # Key contains 25 values.
             "MaxValNameLen" => 20,     # One of which has a 20-char name.
             "MaxValDataLen" => 42,     # One of which has a 42-byte value.
             "CntSubKeys" => 1,         # Key has 1 immediate subkey.
             "MaxSubKeyLen" => 13,      # One of which has a 12-char name.
             "MaxSubClassLen" => 0,     # All of which have class names of "".
             "SecurityLen" => 232,      # One SECURITY_DESCRIPTOR is 232 bytes.
             "LastWrite" => "\x90mZ\cX{\xA3\xBD\cA\c@\cA"
                           # Key was last modifed 1998/06/01 16:29:32 GMT
           );

With arguments, each one must be the name of a item given above.
The return value is the information associated with the listed
names.  In other words:

    return $key->Information( @names );

returns the same list as:

    %info= $key->Information;
    return @info{@names};

=item Delimiter

=item $oldDelim= $key->Delimiter

=item $oldDelim= $key->Delimiter( $newDelim )

Gets and possibly changes the delimiter used for this object.  The
delimiter is appended to subkey names and prepended to value names
in many return values.  It is also used when parsing keys passed
to tied hashes.

The delimiter defaults to backslash (C<'\\'>) but is inherited from
the object used to create a new object and can be specified by an
option when a new object is created.

=item Handle

=item $handle= $key->Handle

Returns the raw C<HKEY> handle for the associated Registry key as
an integer value.  This value can then be used to Reg*() calls
from I<Win32API::Registry>.  However, it is usually easier to just
call the I<Win32API::Registry> calls directly via:

    $key->RegNotifyChangeKeyValue( ... );

For the virtual root of the local or a remote Registry,
C<Handle()> return C<"NONE">.

=item Path

=item $path= $key->Path

Returns a string describing the path of key names to this
Registry key.  The string is built so that if it were passed
to C<$Registry->Open()>, it would reopen the same Registry key
[except in the rare case where one of the key names contains
C<$key->Delimiter>].

=item Machine

=item $computerName= $key->Machine

Returns the name of the computer [or "machine"] on which this Registry
key resides.  Returns C<""> for local Registry keys.

=item Access

Returns the numeric value of the bit mask used to specify the
types of access requested when this Registry key was opened.  Can
be compared to C<KEY_*> values.

=item OS_Delimiter

Returns the delimiter used by the operating system's RegOpenKeyEx()
call.  For Win32, this is always backslash (C<"\\">).

=item Roots

Returns the mapping from root key names like C<"LMachine"> to their
associated C<HKEY_*> constants.  Primarily for internal use and
subject to change.

=item Tie

=item $key->Tie( \%hash );

Ties the referenced hash to that Registry key.  Pretty much the
same as

    tie %hash, ref($key), $key;

Since C<ref($key)> is the class [package] to tie the hash to and
C<TIEHASH()> just returns its argument, C<$key>, [without calling
C<new()>] when it sees that it is already a blessed object.

=item TiedRef

=item $TiedHashRef= $hash_or_obj_ref->TiedRef

For a simple object, returns a reference to a hash tied to the
object.  Used to promote a simple object into a combined object
and hash ref.

If already a reference to a tied hash [that is also an object],
it just returns itself [C<$ref == $ref->TiedRef>].

Mostly used internally.

=item ArrayValues

=item $oldBool= $key->ArrayValues

=item $oldBool= $key->ArrayValues( $newBool )

Gets the current setting of the C<ArrayValues> option and possibly
turns it on or off.

When off, Registry values fetched via a tied hash are returned as
just a value scalar [the same as C<GetValue()> in a scalar context].
When on, they are returned as a reference to an array containing
the value data as the C<[0]> element and the data type as the C<[1]>
element.

=item TieValues

=item $oldBool= TieValues

=item $oldBool= TieValues( $newBool )

Gets the current setting of the C<TieValues> option and possibly
turns it on or off.

Turning this option on is not yet supported in this release of
I<Win32::TieRegistry>.  In a future release, turning this option
on will cause Registry values returned from a tied hash to be
a tied array that you can use to modify the value in the Registry.

=item FastDelete

=item $oldBool= $key->FastDelete

=item $oldBool= $key->FastDelete( $newBool )

Gets the current setting of the C<FastDelete> option and possibly
turns it on or off.

When on, successfully deleting a Registry key [via a tied hash]
simply returns C<1>.

When off, successfully deleting a Registry key [via a tied hash
and not in a void context] returns a reference to a hash that
contains the values present in the key when it was deleted.  This
hash is just like that returned when referencing the key before it
was deleted except that it is an ordinary hash, not one tied to
the I<Win32::TieRegistry> package.

Note that deleting either a Registry key or value via a tied hash
I<in a void context> prevents any overhead in trying to build an
appropriate return value.

Note that deleting a Registry I<value> via a tied hash [not in
a void context] returns the value data even if <FastDelete> is on.

=item SplitMultis

=item $oldBool= $key->SplitMultis

=item $oldBool= $key->SplitMultis( $newBool )

Gets the current setting of the C<SplitMultis> option and possibly
turns it on or off.

If on, Registry values of type C<REG_MULTI_SZ> are returned as
a reference to an array of strings.  See C<GetValue()> for more
information.

=item DWordsToHex

=item $oldBool= $key->DWordsToHex

=item $oldBool= $key->DWordsToHex( $newBool )

Gets the current setting of the C<DWordsToHex> option and possibly
turns it on or off.

If on, Registry values of type C<REG_DWORD> are returned as a hex
string with leading C<"0x"> and longer than 4 characters.  See
C<GetValue()> for more information.

=item FixSzNulls

=item $oldBool= $key->FixSzNulls

=item $oldBool= $key->FixSzNulls( $newBool )

Gets the current setting of the C<FixSzNulls> option and possibly
turns it on or off.

If on, Registry values of type C<REG_SZ> and C<REG_EXPAND_SZ> have
trailing C<'\0'>s added before they are set and stripped before
they are returned.  See C<GetValue()> and C<SetValue()> for more
information.

=item DualTypes

=item $oldBool= $key->DualTypes

=item $oldBool= $key->DualTypes( $newBool )

Gets the current setting of the C<DualTypes> option and possibly
turns it on or off.

If on, data types are returned as a combined numeric/string value
holding both the numeric value of a C<REG_*> constant and the
string value of the constant's name.  See C<GetValue()> for
more information.

=item DualBinVals

=item $oldBool= $key->DualBinVals

=item $oldBool= $key->DualBinVals( $newBool )

Gets the current setting of the C<DualBinVals> option and possibly
turns it on or off.

If on, Registry value data of type C<REG_BINARY> and no more than
4 bytes long and Registry values of type C<REG_DWORD> are returned
as a combined numeric/string value where the numeric value is the
"unpacked" binary value as returned by:

        hex reverse unpack( "h*", $valData )

on a "little-endian" computer.  [Would be C<hex unpack("H*",$valData)>
on a "big-endian" computer if this module is ever ported to one.]

See C<GetValue()> for more information.

=item GetOptions

=item @oldOptValues= $key->GetOptions( @optionNames )

=item $refHashOfOldOpts= $key->GetOptions()

=item $key->GetOptions( \%hashForOldOpts )

Returns the current setting of any of the following options:

    Delimiter     FixSzNulls    DWordsToHex
    ArrayValues   SplitMultis   DualBinVals
    TieValues     FastDelete    DualTypes

Pass in one or more of the above names (as strings) to get back
an array of the corresponding current settings in the same order:

  my( $fastDel, $delim )= $key->GetOptions("FastDelete","Delimiter");

Pass in no arguments to get back a reference to a hash where
the above option names are the keys and the values are
the corresponding current settings for each option:

  my $href= $key->GetOptions();
  my $delim= $href->{Delimiter};

Pass in a single reference to a hash to have the above key/value
pairs I<added> to the referenced hash.  For this case, the
return value is the original object so further methods can be
chained after the call to GetOptions:

  my %oldOpts;
  $key->GetOptions( \%oldOpts )->SetOptions( Delimiter => "/" );

=item SetOptions

=item @oldOpts= $key->SetOptions( optNames=>$optValue,... )

Changes the current setting of any of the following options,
returning the previous setting(s):

    Delimiter     FixSzNulls    DWordsToHex   AllowLoad
    ArrayValues   SplitMultis   DualBinVals   AllowSave
    TieValues     FastDelete    DualTypes

For C<AllowLoad> and C<AllowSave>, instead of the previous
setting, C<SetOptions> returns whether or not the change was
successful.

In a scalar context, returns only the last item.  The last
option can also be specified as C<"ref"> or C<"r"> [which doesn't
need to be followed by a value] to allow chaining:

    $key->SetOptions(AllowSave=>1,"ref")->RegSaveKey(...)

=item SetValue

=item $okay= $key->SetValue( $ValueName, $ValueData );

=item $okay= $key->SetValue( $ValueName, $ValueData, $ValueType );

Adds or replaces a Registry value.  Returns a true value if
successfully, false otherwise.

C<$ValueName> is the name of the value to add or replace and
should I<not> have a delimiter prepended to it.  Case is ignored.

C<$ValueType> is assumed to be C<REG_SZ> if it is omitted.  Otherwise,
it should be one the C<REG_*> constants.

C<$ValueData> is the data to be stored in the value, probably packed
into a Perl string.  Other supported formats for value data are
listed below for each posible C<$ValueType>.

=over

=item REG_SZ or REG_EXPAND_SZ

The only special processing for these values is the addition of
the required trailing C<'\0'> if it is missing.  This can be
turned off by disabling the C<FixSzNulls> option.

=item REG_MULTI_SZ

These values can also be specified as a reference to a list of
strings.  For example, the following two lines are equivalent:

    $key->SetValue( "Val1\000Value2\000LastVal\000\000", "REG_MULTI_SZ" );
    $key->SetValue( ["Val1","Value2","LastVal"], "REG_MULTI_SZ" );

Note that if the required two trailing nulls (C<"\000\000">) are
missing, then this release of C<SetValue()> will I<not> add them.

=item REG_DWORD

These values can also be specified as a hex value with the leading
C<"0x"> included and totaling I<more than> 4 bytes.  These will be
packed into a 4-byte string via:

    $data= pack( "L", hex($data) );

=item REG_BINARY

This value type is listed just to emphasize that no alternate
format is supported for it.  In particular, you should I<not> pass
in a numeric value for this type of data.  C<SetValue()> cannot
distinguish such from a packed string that just happens to match
a numeric value and so will treat it as a packed string.

=back

An alternate calling format:

    $okay= $key->SetValue( $ValueName, [ $ValueData, $ValueType ] );

[two arguments, the second of which is a reference to an array
containing the value data and value type] is supported to ease
using tied hashes with C<SetValue()>.

=item CreateKey

=item $newKey= $key->CreateKey( $subKey );

=item $newKey= $key->CreateKey( $subKey, { Option=>OptVal,... } );

Creates a Registry key or just updates attributes of one.  Calls
C<RegCreateKeyEx()> then, if it succeeded, creates an object
associated with the [possibly new] subkey.

C<$subKey> is the name of a subkey [or a path to one] to be
created or updated.  It can also be a reference to an array
containing a list of subkey names.

The second argument, if it exists, should be a reference to a
hash specifying options either to be passed to C<RegCreateKeyEx()>
or to be used when creating the associated object.  The following
items are the supported keys for this options hash:

=over

=item Delimiter

Specifies the delimiter to be used to parse C<$subKey> and to be
used in the new object.  Defaults to C<$key->Delimiter>.

=item Access

Specifies the types of access requested when the subkey is opened.
Should be a numeric bit mask that combines one or more C<KEY_*>
constant values.

=item Class

The name to assign as the class of the new or updated subkey.
Defaults to C<""> as we have never seen a use for this information.

=item Disposition

Lets you specify a reference to a scalar where, upon success, will be
stored either C<REG_CREATED_NEW_KEY()> or C<REG_OPENED_EXISTING_KEY()>
depending on whether a new key was created or an existing key was
opened.

If you, for example, did C<use Win32::TieRegistry qw(REG_CREATED_NEW_KEY)>
then you can use C<REG_CREATED_NEW_KEY()> to compare against the numeric
value stored in the referenced scalar.

If the C<DualTypes> option is enabled, then in addition to the
numeric value described above, the referenced scalar will also
have a string value equal to either C<"REG_CREATED_NEW_KEY"> or
C<"REG_OPENED_EXISTING_KEY">, as appropriate.

=item Security

Lets you specify a C<SECURITY_ATTRIBUTES> structure packed into a
Perl string.  See C<Win32API::Registry::RegCreateKeyEx()> for more
information.

=item Volatile

If true, specifies that the new key should be volatile, that is,
stored only in memory and not backed by a hive file [and not saved
if the computer is rebooted].  This option is ignored under
Windows 95.  Specifying C<Volatile=E<GT>1>  is the same as
specifying C<Options=E<GT>REG_OPTION_VOLATILE>.

=item Backup

If true, specifies that the new key should be opened for
backup/restore access.  The C<Access> option is ignored.  If the
calling process has enabled C<"SeBackupPrivilege">, then the
subkey is opened with C<KEY_READ> access as the C<"LocalSystem">
user which should have access to all subkeys.  If the calling
process has enabled C<"SeRestorePrivilege">, then the subkey is
opened with C<KEY_WRITE> access as the C<"LocalSystem"> user which
should have access to all subkeys.

This option is ignored under Windows 95.  Specifying C<Backup=E<GT>1>
is the same as specifying C<Options=E<GT>REG_OPTION_BACKUP_RESTORE>.

=item Options

Lets you specify options to the C<RegOpenKeyEx()> call.  The value
for this option should be a numeric value combining zero or more
of the C<REG_OPTION_*> bit masks.  You may with to used the
C<Volatile> and/or C<Backup> options instead of this one.

=back

=item StoreKey

=item $newKey= $key->StoreKey( $subKey, \%Contents );

Primarily for internal use.

Used to create or update a Registry key and any number of subkeys
or values under it or its subkeys.

C<$subKey> is the name of a subkey to be created [or a path of
subkey names separated by delimiters].  If that subkey already
exists, then it is updated.

C<\%Contents> is a reference to a hash containing pairs of
value names with value data and/or subkey names with hash
references similar to C<\%Contents>.  Each of these cause
a value or subkey of C<$subKey> to be created or updated.

If C<$Contents{""}> exists and is a reference to a hash, then
it used as the options argument when C<CreateKey()> is called
for C<$subKey>.  This allows you to specify ...

    if(  defined( $$data{""} )  &&  "HASH" eq ref($$data{""})  ) {
        $self= $this->CreateKey( $subKey, delete $$data{""} );

=item Load

=item $newKey= $key->Load( $file )

=item $newKey= $key->Load( $file, $newSubKey )

=item $newKey= $key->Load( $file, $newSubKey, { Option=>OptVal... } )

=item $newKey= $key->Load( $file, { Option=>OptVal... } )

Loads a hive file into a Registry.  That is, creates a new subkey
and associates a hive file with it.

C<$file> is a hive file, that is a file created by calling
C<RegSaveKey()>.  The C<$file> path is interpreted relative to
C<%SystemRoot%/System32/config> on the machine where C<$key>
resides.

C<$newSubKey> is the name to be given to the new subkey.  If
C<$newSubKey> is specified, then C<$key> must be
C<HKEY_LOCAL_MACHINE> or C<HKEY_USERS> of the local computer
or a remote computer and C<$newSubKey> should not contain any
occurrences of either the delimiter or the OS delimiter.

If C<$newSubKey> is not specified, then it is as if C<$key>
was C<$Registry-E<GT>{LMachine}> and C<$newSubKey> is
C<"PerlTie:999"> where C<"999"> is actually a sequence number
incremented each time this process calls C<Load()>.

You can specify as the last argument a reference to a hash
containing options.  You can specify the same options that you
can specify to C<Open()>.  See C<Open()> for more information on
those.  In addition, you can specify the option C<"NewSubKey">.
The value of this option is interpretted exactly as if it was
specified as the C<$newSubKey> parameter and overrides the
C<$newSubKey> if one was specified.

The hive is automatically unloaded when the returned object
[C<$newKey>] is destroyed.  Registry key objects opened within
the hive will keep a reference to the C<$newKey> object so that
it will not be destroyed before these keys are closed.

=item UnLoad

=item $okay= $key->UnLoad

Unloads a hive that was loaded via C<Load()>.  Cannot unload other
hives.  C<$key> must be the return from a previous call to C<Load()>.
C<$key> is closed and then the hive is unloaded.

=item AllowSave

=item $okay= AllowSave( $bool )

Enables or disables the C<"ReBackupPrivilege"> privilege for the
current process.  You will probably have to enable this privilege
before you can use C<RegSaveKey()>.

The return value indicates whether the operation succeeded, not
whether the privilege was previously enabled.

=item AllowLoad

=item $okay= AllowLoad( $bool )

Enables or disables the C<"ReRestorePrivilege"> privilege for the
current process.  You will probably have to enable this privilege
before you can use C<RegLoadKey()>, C<RegUnLoadKey()>,
C<RegReplaceKey()>, or C<RegRestoreKey> and thus C<Load()> and
C<UnLoad()>.

The return value indicates whether the operation succeeded, not
whether the privilege was previously enabled.

=back

=head2 Exports [C<use> and C<import()>]

To have nothing imported into your package, use something like:

    use Win32::TieRegistry 0.20 ();

which would verify that you have at least version 0.20 but wouldn't
call C<import()>.  The F<Changes> file can be useful in figuring out
which, if any, prior versions of I<Win32::TieRegistry> you want to
support in your script.

The code

    use Win32::TieRegistry;

imports the variable C<$Registry> into your package and sets it
to be a reference to a hash tied to a copy of the master Registry
virtual root object with the default options.  One disadvantage
to this "default" usage is that Perl does not support checking
the module version when you use it.

Alternately, you can specify a list of arguments on the C<use>
line that will be passed to the C<Win32::TieRegistry->import()>
method to control what items to import into your package.  These
arguments fall into the following broad categories:

=over

=item Import a reference to a hash tied to a Registry virtual root

You can request that a scalar variable be imported (possibly)
and set to be a reference to a hash tied to a Registry virtual root
using any of the following types of arguments or argument pairs:

=over

=item "TiedRef", '$scalar'

=item "TiedRef", '$pack::scalar'

=item "TiedRef", 'scalar'

=item "TiedRef", 'pack::scalar'

All of the above import a scalar named C<$scalar> into your package
(or the package named "pack") and then sets it.

=item '$scalar'

=item '$pack::scalar'

These are equivalent to the previous items to support a more
traditional appearance to the list of exports.  Note that the
scalar name cannot be "RegObj" here.

=item "TiedRef", \$scalar

=item \$scalar

These versions don't import anything but set the referenced C<$scalar>.

=back

=item Import a hash tied to the Registry virtual root

You can request that a hash variable be imported (possibly)
and tied to a Registry virtual root using any of the following
types of arguments or argument pairs:

=over

=item "TiedHash", '%hash'

=item "TiedHash", '%pack::hash'

=item "TiedHash", 'hash'

=item "TiedHash", 'pack::hash'

All of the above import a hash named C<%hash> into your package
(or the package named "pack") and then sets it.

=item '%hash'

=item '%pack::hash'

These are equivalent to the previous items to support a more
traditional appearance to the list of exports.

=item "TiedHash", \%hash

=item \%hash

These versions don't import anything but set the referenced C<%hash>.

=back

=item Import a Registry virtual root object

You can request that a scalar variable be imported (possibly)
and set to be a Registry virtual root object using any of the
following types of arguments or argument pairs:

=over

=item "ObjectRef", '$scalar'

=item "ObjectRef", '$pack::scalar'

=item "ObjectRef", 'scalar'

=item "ObjectRef", 'pack::scalar'

All of the above import a scalar named C<$scalar> into your package
(or the package named "pack") and then sets it.

=item '$RegObj'

This is equivalent to the previous items for backward compatibility.

=item "ObjectRef", \$scalar

This version doesn't import anything but sets the referenced C<$scalar>.

=back

=item Import constant(s) exported by I<Win32API::Registry>

You can list any constants that are exported by I<Win32API::Registry>
to have them imported into your package.  These constants have names
starting with "KEY_" or "REG_" (or even "HKEY_").

You can also specify C<":KEY_">, C<":REG_">, and even C<":HKEY_"> to
import a whole set of constants.

See I<Win32API::Registry> documentation for more information.

=item Options

You can list any option names that can be listed in the C<SetOptions()>
method call, each folowed by the value to use for that option.
A Registry virtual root object is created, all of these options are
set for it, then each variable to be imported/set is associated with
this object.

In addition, the following special options are supported:

=over

=item ExportLevel

Whether to import variables into your package or some
package that uses your package.  Defaults to the value of
C<$Exporter::ExportLevel> and has the same meaning.  See
the L<Exporter> module for more information.

=item ExportTo

The name of the package to import variables and constants into.
Overrides I<ExportLevel>.

=back

=back

=head3 Specifying constants in your Perl code

This module was written with a strong emphasis on the convenience of
the module user.  Therefore, most places where you can specify a
constant like C<REG_SZ()> also allow you to specify a string
containing the name of the constant, C<"REG_SZ">.  This is convenient
because you may not have imported that symbolic constant.

Perl also emphasizes programmer convenience so the code C<REG_SZ>
can be used to mean C<REG_SZ()> or C<"REG_SZ"> or be illegal.
Note that using C<&REG_SZ> (as we've seen in much Win32 Perl code)
is not a good idea since it passes the current C<@_> to the
C<constant()> routine of the module which, at the least, can give
you a warning under B<-w>.

Although greatly a matter of style, the "safest" practice is probably
to specifically list all constants in the C<use Win32::TieRegistry>
statement, specify C<use strict> [or at least C<use strict qw(subs)>],
and use bare constant names when you want the numeric value.  This will
detect mispelled constant names at compile time.

    use strict;
    my $Registry;
    use Win32::TieRegistry 0.20 (
        TiedRef => \$Registry,  Delimiter => "/",  ArrayValues => 1,
	SplitMultis => 1,  AllowLoad => 1,
        qw( REG_SZ REG_EXPAND_SZ REG_DWORD REG_BINARY REG_MULTI_SZ
	    KEY_READ KEY_WRITE KEY_ALL_ACCESS ),
    );
    $Registry->{"LMachine/Software/FooCorp/"}= {
        "FooWriter/" => {
            "/Fonts" => [ ["Times","Courier","Lucinda"], REG_MULTI_SZ ],
	    "/WindowSize" => [ pack("LL",24,80), REG_BINARY ],
	    "/TaskBarIcon" => [ "0x0001", REG_DWORD ],
	},
    }  or  die "Can't create Software/FooCorp/: $^E\n";

If you don't want to C<use strict qw(subs)>, the second safest practice
is similar to the above but use the C<REG_SZ()> form for constants
when possible and quoted constant names when required.  Note that
C<qw()> is a form of quoting.

    use Win32::TieRegistry 0.20 qw(
        TiedRef $Registry
        Delimiter /  ArrayValues 1  SplitMultis 1  AllowLoad 1
        REG_SZ REG_EXPAND_SZ REG_DWORD REG_BINARY REG_MULTI_SZ
        KEY_READ KEY_WRITE KEY_ALL_ACCESS
    );
    $Registry->{"LMachine/Software/FooCorp/"}= {
        "FooWriter/" => {
            "/Fonts" => [ ["Times","Courier","Lucinda"], REG_MULTI_SZ() ],
	    "/WindowSize" => [ pack("LL",24,80), REG_BINARY() ],
	    "/TaskBarIcon" => [ "0x0001", REG_DWORD() ],
	},
    }  or  die "Can't create Software/FooCorp/: $^E\n";

The examples in this document mostly use quoted constant names
(C<"REG_SZ">) since that works regardless of which constants
you imported and whether or not you have C<use strict> in your
script.  It is not the best choice for you to use for real
scripts (vs. examples) because it is less efficient and is not
supported by most other similar modules.

=head1 SUMMARY

Most things can be done most easily via tied hashes.  Skip down to the
the L<Tied Hashes Summary> to get started quickly.

=head2 Objects Summary

Here are quick examples that document the most common functionality
of all of the method functions [except for a few almost useless ones].

    # Just another way of saying Open():
    $key= new Win32::TieRegistry "LMachine\\Software\\",
      { Access=>KEY_READ()|KEY_WRITE(), Delimiter=>"\\" };

    # Open a Registry key:
    $subKey= $key->Open( "SubKey/SubSubKey/",
      { Access=>KEY_ALL_ACCESS, Delimiter=>"/" } );

    # Connect to a remote Registry key:
    $remKey= $Registry->Connect( "MachineName", "LMachine/",
      { Access=>KEY_READ, Delimiter=>"/" } );

    # Get value data:
    $valueString= $key->GetValue("ValueName");
    ( $valueString, $valueType )= $key->GetValue("ValueName");

    # Get list of value names:
    @valueNames= $key->ValueNames;

    # Get list of subkey names:
    @subKeyNames= $key->SubKeyNames;

    # Get combined list of value names (with leading delimiters)
    # and subkey names (with trailing delimiters):
    @memberNames= $key->MemberNames;

    # Get all information about a key:
    %keyInfo= $key->Information;
    # keys(%keyInfo)= qw( Class LastWrite SecurityLen
    #   CntSubKeys MaxSubKeyLen MaxSubClassLen
    #   CntValues MaxValNameLen MaxValDataLen );

    # Get selected information about a key:
    ( $class, $cntSubKeys )= $key->Information( "Class", "CntSubKeys" );

    # Get and/or set delimiter:
    $delim= $key->Delimiter;
    $oldDelim= $key->Delimiter( $newDelim );

    # Get "path" for an open key:
    $path= $key->Path;
    # For example, "/CUser/Control Panel/Mouse/"
    # or "//HostName/LMachine/System/DISK/".

    # Get name of machine where key is from:
    $mach= $key->Machine;
    # Will usually be "" indicating key is on local machine.

    # Control different options (see main documentation for descriptions):
    $oldBool= $key->ArrayValues( $newBool );
    $oldBool= $key->FastDelete( $newBool );
    $oldBool= $key->FixSzNulls( $newBool );
    $oldBool= $key->SplitMultis( $newBool );
    $oldBool= $key->DWordsToHex( $newBool );
    $oldBool= $key->DualBinVals( $newBool );
    $oldBool= $key->DualTypes( $newBool );
    @oldBools= $key->SetOptions( ArrayValues=>1, FastDelete=>1, FixSzNulls=>0,
      Delimiter=>"/", AllowLoad=>1, AllowSave=>1 );
    @oldBools= $key->GetOptions( ArrayValues, FastDelete, FixSzNulls );

    # Add or set a value:
    $key->SetValue( "ValueName", $valueDataString );
    $key->SetValue( "ValueName", pack($format,$valueData), "REG_BINARY" );

    # Add or set a key:
    $key->CreateKey( "SubKeyName" );
    $key->CreateKey( "SubKeyName",
      { Access=>"KEY_ALL_ACCESS", Class=>"ClassName",
        Delimiter=>"/", Volatile=>1, Backup=>1 } );

    # Load an off-line Registry hive file into the on-line Registry:
    $newKey= $Registry->Load( "C:/Path/To/Hive/FileName" );
    $newKey= $key->Load( "C:/Path/To/Hive/FileName", "NewSubKeyName",
                     { Access=>"KEY_READ" } );
    # Unload a Registry hive file loaded via the Load() method:
    $newKey->UnLoad;

    # (Dis)Allow yourself to load Registry hive files:
    $success= $Registry->AllowLoad( $bool );

    # (Dis)Allow yourself to save a Registry key to a hive file:
    $success= $Registry->AllowSave( $bool );

    # Save a Registry key to a new hive file:
    $key->RegSaveKey( "C:/Path/To/Hive/FileName", [] );

=head3 Other Useful Methods

See I<Win32API::Registry> for more information on these methods.
These methods are provided for coding convenience and are
identical to the I<Win32API::Registry> functions except that these
don't take a handle to a Registry key, instead getting the handle
from the invoking object [C<$key>].

    $key->RegGetKeySecurity( $iSecInfo, $sSecDesc, $lenSecDesc );
    $key->RegLoadKey( $sSubKeyName, $sPathToFile );
    $key->RegNotifyChangeKeyValue(
      $bWatchSubtree, $iNotifyFilter, $hEvent, $bAsync );
    $key->RegQueryMultipleValues(
      $structValueEnts, $cntValueEnts, $Buffer, $lenBuffer );
    $key->RegReplaceKey( $sSubKeyName, $sPathToNewFile, $sPathToBackupFile );
    $key->RegRestoreKey( $sPathToFile, $iFlags );
    $key->RegSetKeySecurity( $iSecInfo, $sSecDesc );
    $key->RegUnLoadKey( $sSubKeyName );

=head2 Tied Hashes Summary

For fast learners, this may be the only section you need to read.
Always append one delimiter to the end of each Registry key name
and prepend one delimiter to the front of each Registry value name.

=head3 Opening keys

    use Win32::TieRegistry ( Delimiter=>"/", ArrayValues=>1 );
    $Registry->Delimiter("/");                  # Set delimiter to "/".
    $swKey= $Registry->{"LMachine/Software/"};
    $winKey= $swKey->{"Microsoft/Windows/CurrentVersion/"};
    $userKey= $Registry->
      {"CUser/Software/Microsoft/Windows/CurrentVersion/"};
    $remoteKey= $Registry->{"//HostName/LMachine/"};

=head3 Reading values

    $progDir= $winKey->{"/ProgramFilesDir"};    # "C:\\Program Files"
    $tip21= $winKey->{"Explorer/Tips//21"};     # Text of tip #21.

    $winKey->ArrayValues(1);
    ( $devPath, $type )= $winKey->{"/DevicePath"};
    # $devPath eq "%SystemRoot%\\inf"
    # $type eq "REG_EXPAND_SZ"  [if you have SetDualVar.pm installed]
    # $type == REG_EXPAND_SZ()  [if did C<use Win32::TieRegistry qw(:REG_)>]

=head3 Setting values

    $winKey->{"Setup//SourcePath"}= "\\\\SwServer\\SwShare\\Windows";
    # Simple.  Assumes data type of REG_SZ.

    $winKey->{"Setup//Installation Sources"}=
      [ "D:\x00\\\\SwServer\\SwShare\\Windows\0\0", "REG_MULTI_SZ" ];
    # "\x00" and "\0" used to mark ends of each string and end of list.

    $winKey->{"Setup//Installation Sources"}=
      [ ["D:","\\\\SwServer\\SwShare\\Windows"], "REG_MULTI_SZ" ];
    # Alternate method that is easier to read.

    $userKey->{"Explorer/Tips//DisplayInitialTipWindow"}=
      [ pack("L",0), "REG_DWORD" ];
    $userKey->{"Explorer/Tips//Next"}= [ pack("S",3), "REG_BINARY" ];
    $userKey->{"Explorer/Tips//Show"}= [ pack("L",0), "REG_BINARY" ];

=head3 Adding keys

    $swKey->{"FooCorp/"}= {
        "FooWriter/" => {
            "/Version" => "4.032",
            "Startup/" => {
                "/Title" => "Foo Writer Deluxe ][",
                "/WindowSize" => [ pack("LL",$wid,$ht), "REG_BINARY" ],
                "/TaskBarIcon" => [ "0x0001", "REG_DWORD" ],
            },
            "Compatibility/" => {
                "/AutoConvert" => "Always",
                "/Default Palette" => "Windows Colors",
            },
        },
        "/License", => "0123-9C8EF1-09-FC",
    };

=head3 Listing all subkeys and values

    @members= keys( %{$swKey} );
    @subKeys= grep(  m#^/#,  keys( %{$swKey->{"Classes/batfile/"}} )  );
    # @subKeys= ( "/", "/EditFlags" );
    @valueNames= grep(  ! m#^/#,  keys( %{$swKey->{"Classes/batfile/"}} )  );
    # @valueNames= ( "DefaultIcon/", "shell/", "shellex/" );

=head3 Deleting values or keys with no subkeys

    $oldValue= delete $userKey->{"Explorer/Tips//Next"};

    $oldValues= delete $userKey->{"Explorer/Tips/"};
    # $oldValues will be reference to hash containing deleted keys values.

=head3 Closing keys

    undef $swKey;               # Explicit way to close a key.
    $winKey= "Anything else";   # Implicitly closes a key.
    exit 0;                     # Implicitly closes all keys.

=head2 Tie::Registry

This module was originally called I<Tie::Registry>.  Changing code
that used I<Tie::Registry> over to I<Win32::TieRegistry> is trivial
as the module name should only be mentioned once, in the C<use>
line.  However, finding all of the places that used I<Tie::Registry>
may not be completely trivial so we have included F<Tie/Registry.pm>
which you can install to provide backward compatibility.

=head1 AUTHOR

Tye McQueen.  See http://www.metronet.com/~tye/ or e-mail
tye@metronet.com with bug reports.

=head1 SEE ALSO

I<Win32API::Registry> - Provides access to C<Reg*()>, C<HKEY_*>,
C<KEY_*>, C<REG_*> [required].

I<Win32::WinError> - Defines C<ERROR_*> values [optional].

L<SetDualVar> - For returning C<REG_*> values as combined
string/integer values [optional].

=head1 BUGS

Perl5.004_02 has bugs that make I<Win32::TieRegistry> fail in
strange and subtle ways.

Using I<Win32::TieRegistry> with versions of Perl prior to 5.005
can be tricky or impossible.  Most notes about this have been
removed from the documentation (they get rather complicated
and confusing).  This includes references to C<$^E> perhaps not
being meaningful.

Because Perl hashes are case sensitive, certain lookups are also
case sensistive.  In particular, the root keys ("Classes", "CUser",
"LMachine", "Users", "PerfData", "CConfig", "DynData", and HKEY_*)
must always be entered without changing between upper and lower
case letters.  Also, the special rule for matching subkey names
that contain the user-selected delimiter only works if case is
matched.  All other key name and value name lookups should be case
insensitive because the underlying Reg*() calls ignore case.

Information about each key is cached when using a tied hash.
This cache is not flushed nor updated when changes are made,
I<even when the same tied hash is used> to make the changes.

Current implementations of Perl's "global destruction" phase can
cause objects returned by C<Load()> to be destroyed while keys
within the hive are still open, if the objects still exist when
the script starts to exit.  When this happens, the automatic
C<UnLoad()> will report a failure and the hive will remain loaded
in the Registry.

Trying to C<Load()> a hive file that is located on a remote network
share may silently delete all data from the hive.  This is a bug
in the Win32 APIs, not any Perl code or modules.  This module does
not try to protect you from this bug.

There is no test suite.

=head1 FUTURE DIRECTIONS

The following items are desired by the author and may appear in a
future release of this module.

=over

=item TieValues option

Currently described in main documentation but no yet implemented.

=item AutoRefresh option

Trigger use of C<RegNotifyChangeKeyValue()> to keep tied hash
caches up-to-date even when other programs make changes.

=item Error options

Allow the user to have unchecked calls (calls in a "void context")
to automatically report errors via C<warn> or C<die>.

For complex operations, such a copying an entire subtree, provide
access to detailed information about errors (and perhaps some
warnings) that were encountered.  Let the user control whether
the complex operation continues in spite of errors.

=back

=cut

# Autoload not currently supported by Perl under Windows.
