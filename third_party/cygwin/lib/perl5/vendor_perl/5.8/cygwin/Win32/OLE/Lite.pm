package Win32::OLE;

sub _croak { require Carp; Carp::croak(@_) }

unless (defined &Dispatch) {
    DynaLoader::boot_DynaLoader('DynaLoader')
	    unless defined(&DynaLoader::dl_load_file);
    my $file;
    foreach my $dir (@INC) {
	my $try = "$dir/auto/Win32/OLE/OLE.dll";
	last if $file = (-f $try && $try);
    }
    _croak("Can't locate loadable object for module Win32::OLE".
	   " in \@INC (\@INC contains: @INC)")
	unless $file;	# wording similar to error from 'require'

    my $libref = DynaLoader::dl_load_file($file, 0) or
	_croak("Can't load '$file' for module Win32::OLE: ".
	       DynaLoader::dl_error()."\n");

    my $boot_symbol_ref = DynaLoader::dl_find_symbol($libref, "boot_Win32__OLE")
	or _croak("Can't find 'boot_Win32__OLE' symbol in $file\n");

    my $xs = DynaLoader::dl_install_xsub("Win32::OLE::bootstrap",
					 $boot_symbol_ref, $file);
    &$xs('Win32::OLE');
}

if (defined &DB::sub && !defined $_Unique) {
    warn "Win32::OLE operating in debugging mode: _Unique => 1\n";
    $_Unique = 1;
}

$Warn = 1;

sub CP_ACP   {0;}     # ANSI codepage
sub CP_OEMCP {1;}     # OEM codepage
sub CP_MACCP {2;}
sub CP_UTF7  {65000;}
sub CP_UTF8  {65001;}

sub DISPATCH_METHOD          {1;}
sub DISPATCH_PROPERTYGET     {2;}
sub DISPATCH_PROPERTYPUT     {4;}
sub DISPATCH_PROPERTYPUTREF  {8;}

sub COINIT_MULTITHREADED     {0;}  # Default
sub COINIT_APARTMENTTHREADED {2;}  # Use single threaded apartment model

# Bogus COINIT_* values to indicate special cases:
sub COINIT_OLEINITIALIZE     {-1;} # Use OleInitialize instead of CoInitializeEx
sub COINIT_NO_INITIALIZE     {-2;} # We are already initialized, just believe me

sub HRESULT {
    my $hr = shift;
    $hr -= 2**32 if $hr & 0x80000000;
    return $hr;
}

# CreateObject is defined here only because it is documented in the
# "Learning Perl on Win32 Systems" Gecko book. Please use Win32::OLE->new().
sub CreateObject {
    if (ref($_[0]) && UNIVERSAL::isa($_[0],'Win32::OLE')) {
	$AUTOLOAD = ref($_[0]) . '::CreateObject';
	goto &AUTOLOAD;
    }

    # Hack to allow C<$obj = CreateObject Win32::OLE 'My.App';>. Although this
    # is contrary to the Gecko, we just make it work since it doesn't hurt.
    return Win32::OLE->new($_[1]) if $_[0] eq 'Win32::OLE';

    # Gecko form: C<$success = Win32::OLE::CreateObject('My.App',$obj);>
    $_[1] = Win32::OLE->new($_[0]);
    return defined $_[1];
}

sub LastError {
    unless (defined $_[0]) {
	# Win32::OLE::LastError() will always return $Win32::OLE::LastError
	return $LastError;
    }

    if (ref($_[0]) && UNIVERSAL::isa($_[0],'Win32::OLE')) {
	$AUTOLOAD = ref($_[0]) . '::LastError';
	goto &AUTOLOAD;
    }

    #no strict 'refs';
    my $LastError = "$_[0]::LastError";
    $$LastError = $_[1] if defined $_[1];
    return $$LastError;
}

my $Options = "^(?:CP|LCID|Warn|Variant|_NewEnum|_Unique)\$";

sub Option {
    if (ref($_[0]) && UNIVERSAL::isa($_[0],'Win32::OLE')) {
	$AUTOLOAD = ref($_[0]) . '::Option';
	goto &AUTOLOAD;
    }

    my $class = shift;

    if (@_ == 1) {
	my $option = shift;
	return ${"${class}::$option"} if $option =~ /$Options/o;
	_croak("Invalid $class option: $option");
    }

    while (@_) {
	my ($option,$value) = splice @_, 0, 2;
	_croak("Invalid $class option: $option") if $option !~ /$Options/o;
	${"${class}::$option"} = $value;
	$class->_Unique() if $option eq "_Unique";
    }
}

sub Invoke {
    my ($self,$method,@args) = @_;
    $self->Dispatch($method, my $retval, @args);
    return $retval;
}

sub LetProperty {
    my ($self,$method,@args) = @_;
    $self->Dispatch([DISPATCH_PROPERTYPUT, $method], my $retval, @args);
    return $retval;
}

sub SetProperty {
    my ($self,$method,@args) = @_;
    my $wFlags = DISPATCH_PROPERTYPUT;
    if (@args) {
	# If the value is an object then it will be set by reference!
	my $value = $args[-1];
	if (UNIVERSAL::isa($value, 'Win32::OLE')) {
	    $wFlags = DISPATCH_PROPERTYPUTREF;
	}
	elsif (UNIVERSAL::isa($value,'Win32::OLE::Variant')) {
	    my $type = $value->Type & ~0xfff; # VT_TYPEMASK
	    # VT_DISPATCH and VT_UNKNOWN represent COM objects
	    $wFlags = DISPATCH_PROPERTYPUTREF if $type == 9 || $type == 13;
	}
    }
    $self->Dispatch([$wFlags, $method], my $retval, @args);
    return $retval;
}

sub AUTOLOAD {
    my $self = shift;
    my $autoload = substr $AUTOLOAD, rindex($AUTOLOAD, ':')+1;
    _croak("Cannot autoload class method \"$autoload\"")
	unless ref($self) && UNIVERSAL::isa($self, 'Win32::OLE');
    my $success = $self->Dispatch($autoload, my $retval, @_);
    unless (defined $success || ($^H & 0x200) != 0) {
	# Retry default method if C<no strict 'subs';>
	$self->Dispatch(undef, $retval, $autoload, @_);
    }
    return $retval;
}

sub in {
    my @res;
    while (@_) {
	my $this = shift;
	if (UNIVERSAL::isa($this, 'Win32::OLE')) {
	    push @res, Win32::OLE::Enum->All($this);
	}
	elsif (ref($this) eq 'ARRAY') {
	    push @res, @$this;
	}
	else {
	    push @res, $this;
	}
    }
    return @res;
}

sub valof {
    my $arg = shift;
    if (UNIVERSAL::isa($arg, 'Win32::OLE')) {
	require Win32::OLE::Variant;
	my ($class) = overload::StrVal($arg) =~ /^([^=]+)=/;
	#no strict 'refs';
	local $Win32::OLE::CP = ${"${class}::CP"};
	local $Win32::OLE::LCID = ${"${class}::LCID"};
	#use strict 'refs';
	# VT_EMPTY variant for return code
	my $variant = Win32::OLE::Variant->new;
	$arg->Dispatch(undef, $variant);
	return $variant->Value;
    }
    $arg = $arg->Value if UNIVERSAL::can($arg, 'Value');
    return $arg;
}

sub with {
    my $object = shift;
    while (@_) {
	my $property = shift;
	$object->{$property} = shift;
    }
}

########################################################################

package Win32::OLE::Tie;

# Only retry default method under C<no strict 'subs';>
sub FETCH {
    my ($self,$key) = @_;
    if ($key eq "_NewEnum") {
	(my $class = ref $self) =~ s/::Tie$//;
	return [Win32::OLE::Enum->All($self)] if ${"${class}::_NewEnum"};
    }
    $self->Fetch($key, !$Win32::OLE::Strict);
}

sub STORE {
    my ($self,$key,$value) = @_;
    $self->Store($key, $value, !$Win32::OLE::Strict);
}

1;
