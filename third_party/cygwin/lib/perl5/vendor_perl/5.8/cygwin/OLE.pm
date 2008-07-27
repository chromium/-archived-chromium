# Compatibility layer for applications using the old toplevel OLE.pm.
# New code should use Win32::OLE

# This file is based on ../lib/OLE.pm from ActiveState build 315.

# Compatibility notes:
# - "GetObject" -> "GetActiveObject"
# - "keys %$collection" -> "Win32::OLE::Enum->All($collection)"
#                       or "in $Collection"
# - "unnamed" default method retries

########################################################################
package Win32;
########################################################################

sub OLELastError {return OLE->LastError()}


########################################################################
package OLE::Variant;
########################################################################

use Win32::OLE qw(CP_ACP);
use Win32::OLE::Variant;

use strict;
use vars qw($AUTOLOAD @ISA $LCID $CP $Warn $LastError $_NewEnum $_Unique);
@ISA = qw(Win32::OLE::Variant);

$Warn = 0;
$LCID = 2 << 10; # LOCALE_SYSTEM_DEFAULT
$CP = CP_ACP;
$_NewEnum = 0;
$_Unique = 0;

sub new {
    my $self = shift;
    my $variant = $self->SUPER::new(@_);
    $OLE::LastError = $Win32::OLE->LastError unless defined $variant;
    return $variant;
}


########################################################################
package OLE::Tie;
########################################################################
use strict;
use vars qw(@ISA);
@ISA = qw(Win32::OLE::Tie);

# !!! It is VERY important that Win32::OLE::Tie::DESTROY gets called. !!!
# If you subclass DESTROY, don't forget to call $self->SUPER::DESTROY.
# Otherwise the OLE interfaces will not be released until process termination!

# Retry default method if property doesn't exist
sub FETCH {
    my ($self,$key) = @_;
    return $self->SUPER::Fetch($key, 1);
}

sub STORE {
    my ($self,$key,$value) = @_;
    $self->SUPER::Store($key, $value, 1);
}

# Enumerate collection members, not object properties
*FIRSTKEY = *Win32::OLE::Tie::FIRSTENUM;
*NEXTKEY = *Win32::OLE::Tie::NEXTENUM;


########################################################################
package OLE;
########################################################################
use Win32::OLE qw(CP_ACP);

# Use OleInitialize() instead of CoInitializeEx:
Win32::OLE->Initialize(Win32::OLE::COINIT_OLEINITIALIZE);

use strict;

# Disable overload; unfortunately "no overload" doesn't do it :-(
# Overloading is no longer enabled by default in Win32::OLE
#use overload '""'     => sub {overload::StrVal($_[0])},
#             '0+'     => sub {overload::StrVal($_[0])};

use vars qw($AUTOLOAD @ISA $LCID $CP $Warn $LastError $Tie);
@ISA = qw(Win32::OLE);

$Warn = 0;
$LCID = 2 << 10; # LOCALE_SYSTEM_DEFAULT
$CP = CP_ACP;
$Tie = 'OLE::Tie';

sub new {
    my $class = shift;
    $class = shift if $class eq 'OLE';
    return OLE->SUPER::new($class);
}

sub copy {
    my $class = shift;
    $class = shift if $class eq 'OLE';
    return OLE->SUPER::GetActiveObject($class);
}

sub AUTOLOAD {
    my $self = shift;
    my $retval;
    $AUTOLOAD =~ s/.*:://o;

    Carp::croak("Cannot autoload class method \"$AUTOLOAD\"") 
      unless ref($self) && UNIVERSAL::isa($self,'OLE');

    local $^H = 0; # !hack alert!
    unless (defined $self->Dispatch($AUTOLOAD, $retval, @_)) {
	# Retry default method
	$self->Dispatch(undef, $retval, $AUTOLOAD, @_);
    }
    return $retval;
}

*CreateObject = \&new;
*GetObject = \&copy;

# Automation data types.

sub VT_EMPTY {0;}
sub VT_NULL {1;}
sub VT_I2 {2;}
sub VT_I4 {3;}
sub VT_R4 {4;}
sub VT_R8 {5;}
sub VT_CY {6;}
sub VT_DATE {7;}
sub VT_BSTR {8;}
sub VT_DISPATCH {9;}
sub VT_ERROR {10;}
sub VT_BOOL {11;}
sub VT_VARIANT {12;}
sub VT_UNKNOWN {13;}
sub VT_I1 {16;}
sub VT_UI1 {17;}
sub VT_UI2 {18;}
sub VT_UI4 {19;}
sub VT_I8 {20;}
sub VT_UI8 {21;}
sub VT_INT {22;}
sub VT_UINT {23;}
sub VT_VOID {24;}
sub VT_HRESULT {25;}
sub VT_PTR {26;}
sub VT_SAFEARRAY {27;}
sub VT_CARRAY {28;}
sub VT_USERDEFINED {29;}
sub VT_LPSTR {30;}
sub VT_LPWSTR {31;}
sub VT_FILETIME {64;}
sub VT_BLOB {65;}
sub VT_STREAM {66;}
sub VT_STORAGE {67;}
sub VT_STREAMED_OBJECT {68;}
sub VT_STORED_OBJECT {69;}
sub VT_BLOB_OBJECT {70;}
sub VT_CF {71;}
sub VT_CLSID {72;}

sub TKIND_ENUM {0;}
sub TKIND_RECORD {1;}
sub TKIND_MODULE {2;}
sub TKIND_INTERFACE {3;}
sub TKIND_DISPATCH {4;}
sub TKIND_COCLASS {5;}
sub TKIND_ALIAS {6;}
sub TKIND_UNION {7;}
sub TKIND_MAX {8;}

1;
