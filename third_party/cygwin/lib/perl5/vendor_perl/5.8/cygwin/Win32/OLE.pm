# The documentation is at the __END__

package Win32::OLE;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK @EXPORT_FAIL $AUTOLOAD
	    $CP $LCID $Warn $LastError $_NewEnum $_Unique);

$VERSION = '0.1703';

use Carp;
use Exporter;
use DynaLoader;
@ISA = qw(Exporter DynaLoader);

@EXPORT = qw();
@EXPORT_OK = qw(in valof with HRESULT EVENTS OVERLOAD
                CP_ACP CP_OEMCP CP_MACCP CP_UTF7 CP_UTF8
		DISPATCH_METHOD DISPATCH_PROPERTYGET
		DISPATCH_PROPERTYPUT DISPATCH_PROPERTYPUTREF);
@EXPORT_FAIL = qw(EVENTS OVERLOAD);

sub export_fail {
    shift;
    my @unknown;
    while (@_) {
	my $symbol = shift;
	if ($symbol eq 'OVERLOAD') {
	    eval <<'OVERLOAD';
	        use overload '""'     => \&valof,
	                     '0+'     => \&valof,
	                     fallback => 1;
OVERLOAD
	}
	elsif ($symbol eq 'EVENTS') {
	    Win32::OLE->Initialize(Win32::OLE::COINIT_OLEINITIALIZE());
	}
	else {
	    push @unknown, $symbol;
	}
    }
    return @unknown;
}

unless (defined &Dispatch) {
    # Use regular DynaLoader if XS part is not yet initialized
    bootstrap Win32::OLE;
    require Win32::OLE::Lite;
}

1;

########################################################################

__END__

=head1 NAME

Win32::OLE - OLE Automation extensions

=head1 SYNOPSIS

    $ex = Win32::OLE->new('Excel.Application') or die "oops\n";
    $ex->Amethod("arg")->Bmethod->{'Property'} = "foo";
    $ex->Cmethod(undef,undef,$Arg3);
    $ex->Dmethod($RequiredArg1, {NamedArg1 => $Value1, NamedArg2 => $Value2});

    $wd = Win32::OLE->GetObject("D:\\Data\\Message.doc");
    $xl = Win32::OLE->GetActiveObject("Excel.Application");

=head1 DESCRIPTION

This module provides an interface to OLE Automation from Perl.
OLE Automation brings VisualBasic like scripting capabilities and
offers powerful extensibility and the ability to control many Win32
applications from Perl scripts.

The Win32::OLE module uses the IDispatch interface exclusively.  It is
not possible to access a custom OLE interface.  OLE events and OCX's are
currently not supported.

Actually, that's no longer strictly true.  This module now contains
B<ALPHA> level support for OLE events.  This is largely untested and the
specific interface might still change in the future.

=head2 Methods

=over 8

=item Win32::OLE->new(PROGID[, DESTRUCTOR])

The new() class method starts a new instance of an OLE Automation object.
It returns a reference to this object or C<undef> if the creation failed.

The PROGID argument must be either the OLE I<program id> or the I<class id>
of the required application.  The optional DESTRUCTOR specifies a DESTROY-like
method.  This can be either a CODE reference or a string containing an OLE
method name.  It can be used to cleanly terminate OLE applications in case the
Perl program dies.

To create an object via DCOM on a remote server you can use an array
reference in place of PROGID.  The referenced array must contain the
machine name and the I<program id> or I<class id>.  For example:

	my $obj = Win32::OLE->new(['my.machine.com', 'Program.Id']);

If the PROGID is a I<program id> then Win32::OLE will try to resolve the
corresponding I<class id> locally.  If the I<program id> is not registered
locally then the remote registry is queried.  This will only succeed if
the local process has read access to the remote registry.  The safest
(and fastest) method is to specify the C<class id> directly.

=item Win32::OLE->EnumAllObjects([CALLBACK])

This class method returns the number Win32::OLE objects currently in
existance.  It will call the optional CALLBACK function for each of
these objects:

	$Count = Win32::OLE->EnumAllObjects(sub {
	    my $Object = shift;
	    my $Class = Win32::OLE->QueryObjectType($Object);
	    printf "# Object=%s Class=%s\n", $Object, $Class;
	});

The EnumAllObjects() method is primarily a debugging tool.  It can be
used e.g. in an END block to check if all external connections have
been properly destroyed.

=item Win32::OLE->FreeUnusedLibraries()

The FreeUnusedLibraries() class method unloads all unused OLE
resources.  These are the libraries of those classes of which all
existing objects have been destroyed.  The unloading of object
libraries is really only important for long running processes that
might instantiate a huge number of B<different> objects over time.

Be aware that objects implemented in Visual Basic have a buggy
implementation of this functionality: They pretend to be unloadable
while they are actually still running their cleanup code.  Unloading
the DLL at that moment typically produces an access violation.  The
probability for this problem can be reduced by calling the
SpinMessageLoop() method and sleep()ing for a few seconds.

=item Win32::OLE->GetActiveObject(CLASS[, DESTRUCTOR])

The GetActiveObject() class method returns an OLE reference to a
running instance of the specified OLE automation server.  It returns
C<undef> if the server is not currently active.  It will croak if
the class is not even registered.  The optional DESTRUCTOR method takes
either a method name or a code reference.  It is executed when the last
reference to this object goes away.  It is generally considered C<impolite>
to stop applications that you did not start yourself.

=item Win32::OLE->GetObject(MONIKER[, DESTRUCTOR])

The GetObject() class method returns an OLE reference to the specified
object.  The object is specified by a pathname optionally followed by
additional item subcomponent separated by exclamation marks '!'.  The
optional DESTRUCTOR argument has the same semantics as the DESTRUCTOR in
new() or GetActiveObject().

=item Win32::OLE->Initialize([COINIT])

The Initialize() class method can be used to specify an alternative
apartment model for the Perl thread.  It must be called B<before> the
first OLE object is created.  If the C<Win32::OLE::Const> module is
used then the call to the Initialize() method must be made from a BEGIN
block before the first C<use> statement for the C<Win32::OLE::Const>
module.

Valid values for COINIT are:

  Win32::OLE::COINIT_APARTMENTTHREADED  - single threaded
  Win32::OLE::COINIT_MULTITHREADED      - the default
  Win32::OLE::COINIT_OLEINITIALIZE      - single threaded, additional OLE stuff

COINIT_OLEINITIALIZE is sometimes needed when an OLE object uses
additional OLE compound document technologies not available from the
normal COM subsystem (for example MAPI.Session seems to require it).
Both COINIT_OLEINITIALIZE and COINIT_APARTMENTTHREADED create a hidden
top level window and a message queue for the Perl process.  This may
create problems with other application, because Perl normally doesn't
process its message queue.  This means programs using synchronous
communication between applications (such as DDE initiation), may hang
until Perl makes another OLE method call/property access or terminates.
This applies to InstallShield setups and many things started to shell
associations.  Please try to utilize the C<Win32::OLE-E<gt>SpinMessageLoop>
and C<Win32::OLE-E<gt>Uninitialize> methods if you can not use the default
COINIT_MULTITHREADED model.

=item OBJECT->Invoke(METHOD[, ARGS])

The Invoke() object method is an alternate way to invoke OLE
methods.  It is normally equivalent to C<$OBJECT->METHOD(@ARGS)>.  This
function must be used if the METHOD name contains characters not valid
in a Perl variable name (like foreign language characters).  It can
also be used to invoke the default method of an object even if the
default method has not been given a name in the type library.  In this
case use <undef> or C<''> as the method name.  To invoke an OLE objects
native Invoke() method (if such a thing exists), please use:

	$Object->Invoke('Invoke', @Args);

=item Win32::OLE->LastError()

The LastError() class method returns the last recorded OLE
error.  This is a dual value like the C<$!> variable: in a numeric
context it returns the error number and in a string context it returns
the error message.  The error number is a signed HRESULT value.  Please
use the L<HRESULT(ERROR)> function to convert an unsigned hexadecimal
constant to a signed HRESULT.

The last OLE error is automatically reset by a successful OLE
call.  The numeric value can also explicitly be set by a call (which will
discard the string value):

	Win32::OLE->LastError(0);

=item OBJECT->LetProperty(NAME,ARGS,VALUE)

In Win32::OLE property assignment using the hash syntax is equivalent
to the Visual Basic C<Set> syntax (I<by reference> assignment):

	$Object->{Property} = $OtherObject;

corresponds to this Visual Basic statement:

	Set Object.Property = OtherObject

To get the I<by value> treatment of the Visual Basic C<Let> statement

	Object.Property = OtherObject

you have to use the LetProperty() object method in Perl:

	$Object->LetProperty($Property, $OtherObject);

LetProperty() also supports optional arguments for the property assignment.
See L<OBJECT->SetProperty(NAME,ARGS,VALUE)> for details.

=item Win32::OLE->MessageLoop()

The MessageLoop() class method will run a standard Windows message
loop, dispatching messages until the QuitMessageLoop() class method is
called.  It is used to wait for OLE events.

=item Win32::OLE->Option(OPTION)

The Option() class method can be used to inspect and modify
L<Module Options>.  The single argument form retrieves the value of
an option:

	my $CP = Win32::OLE->Option('CP');

A single call can be used to set multiple options simultaneously:

	Win32::OLE->Option(CP => CP_ACP, Warn => 3);

=item Win32::OLE->QueryObjectType(OBJECT)

The QueryObjectType() class method returns a list of the type library
name and the objects class name.  In a scalar context it returns the
class name only.  It returns C<undef> when the type information is not
available.

=item Win32::OLE->QuitMessageLoop()

The QuitMessageLoop() class method posts a (user-level) "Quit" message
to the current threads message loop.  QuitMessageLoop() is typically
called from an event handler.  The MessageLoop() class method will
return when it receives this "Quit" method.

=item OBJECT->SetProperty(NAME,ARGS,VALUE)

The SetProperty() method allows to modify properties with arguments,
which is not supported by the hash syntax.  The hash form

	$Object->{Property} = $Value;

is equivalent to

	$Object->SetProperty('Property', $Value);

Arguments must be specified between the property name and the new value:

	$Object->SetProperty('Property', @Args, $Value);

It is not possible to use "named argument" syntax with this function
because the new value must be the last argument to SetProperty().

This method hides any native OLE object method called SetProperty().
The native method will still be available through the Invoke() method:

	$Object->Invoke('SetProperty', @Args);

=item Win32::OLE->SpinMessageLoop

This class method retrieves all pending messages from the message queue
and dispatches them to their respective window procedures.  Calling this
method is only necessary when not using the COINIT_MULTITHREADED model.
All OLE method calls and property accesses automatically process the
message queue.

=item Win32::OLE->Uninitialize

The Uninitialize() class method uninitializes the OLE subsystem.  It
also destroys the hidden top level window created by OLE for single
threaded apartments.  All OLE objects will become invalid after this call!
It is possible to call the Initialize() class method again with a different
apartment model after shutting down OLE with Uninitialize().

=item Win32::OLE->WithEvents(OBJECT[, HANDLER[, INTERFACE]])

This class method enables and disables the firing of events by the
specified OBJECT.  If no HANDLER is specified, then events are
disconnected.  For some objects Win32::OLE is not able to
automatically determine the correct event interface.  In this case the
INTERFACE argument must contain either the COCLASS name of the OBJECT
or the name of the event DISPATCH interface.  Please read the L<Events>
section below for detailed explanation of the Win32::OLE event
support.

=back

Whenever Perl does not find a method name in the Win32::OLE package it
is automatically used as the name of an OLE method and this method call
is dispatched to the OLE server.

There is one special hack built into the module: If a method or property
name could not be resolved with the OLE object, then the default method
of the object is called with the method name as its first parameter.  So

	my $Sheet = $Worksheets->Table1;
or
	my $Sheet = $Worksheets->{Table1};

is resolved as

	my $Sheet = $Worksheet->Item('Table1');

provided that the $Worksheets object doesnot have a C<Table1> method
or property.  This hack has been introduced to call the default method
of collections which did not name the method in their type library.  The
recommended way to call the "unnamed" default method is:

	my $Sheet = $Worksheets->Invoke('', 'Table1');

This special hack is disabled under C<use strict 'subs';>.

=head2 Object methods and properties

The object returned by the new() method can be used to invoke
methods or retrieve properties in the same fashion as described
in the documentation for the particular OLE class (eg. Microsoft
Excel documentation describes the object hierarchy along with the
properties and methods exposed for OLE access).

Optional parameters on method calls can be omitted by using C<undef>
as a placeholder.  A better way is to use named arguments, as the
order of optional parameters may change in later versions of the OLE
server application.  Named parameters can be specified in a reference
to a hash as the last parameter to a method call.

Properties can be retrieved or set using hash syntax, while methods
can be invoked with the usual perl method call syntax.  The C<keys>
and C<each> functions can be used to enumerate an object's properties.
Beware that a property is not always writable or even readable (sometimes
raising exceptions when read while being undefined).

If a method or property returns an embedded OLE object, method
and property access can be chained as shown in the examples below.

=head2 Functions

The following functions are not exported by default.

=over 8

=item HRESULT(ERROR)

The HRESULT() function converts an unsigned number into a signed HRESULT
error value as used by OLE internally.  This is necessary because Perl
treats all hexadecimal constants as unsigned.  To check if the last OLE
function returned "Member not found" (0x80020003) you can write:

	if (Win32::OLE->LastError == HRESULT(0x80020003)) {
	    # your error recovery here
	}

=item in(COLLECTION)

If COLLECTION is an OLE collection object then C<in $COLLECTION>
returns a list of all members of the collection.  This is a shortcut
for C<Win32::OLE::Enum->All($COLLECTION)>.  It is most commonly used in
a C<foreach> loop:

	foreach my $value (in $collection) {
	    # do something with $value here
	}

=item valof(OBJECT)

Normal assignment of Perl OLE objects creates just another reference
to the OLE object.  The valof() function explictly dereferences the
object (through the default method) and returns the value of the object.

	my $RefOf = $Object;
	my $ValOf = valof $Object;
        $Object->{Value} = $NewValue;

Now $ValOf still contains the old value wheras $RefOf would
resolve to the $NewValue because it is still a reference to
$Object.

The valof() function can also be used to convert Win32::OLE::Variant
objects to Perl values.

=item with(OBJECT, PROPERTYNAME => VALUE, ...)

This function provides a concise way to set the values of multiple
properties of an object.  It iterates over its arguments doing
C<$OBJECT->{PROPERTYNAME} = $VALUE> on each trailing pair.

=back

=head2 Overloading

The Win32::OLE objects can be overloaded to automatically convert to
their values whenever they are used in a bool, numeric or string
context.  This is not enabled by default.  You have to request it
through the OVERLOAD pseudoexport:

	use Win32::OLE qw(in valof with OVERLOAD);

You can still get the original string representation of an object
(C<Win32::OLE=0xDEADBEEF>), e.g. for debugging, by using the
C<overload::StrVal()> method:

	print overload::StrVal($object), "\n";

Please note that C<OVERLOAD> is a global setting.  If any module enables
Win32::OLE overloading then it's active everywhere.

=head2 Events

The Win32::OLE module now contains B<ALPHA> level event support.  This
support is only available when Perl is running in a single threaded
apartment.  This can most easily be assured by using the C<EVENTS>
pseudo-import:

	use Win32::OLE qw(EVENTS);

which implicitly does something like:

	use Win32::OLE;
	Win32::OLE->Initialize(Win32::OLE::COINIT_OLEINITIALIZE);

The current interface to OLE events should be considered experimental
and is subject to change.  It works as expected for normal OLE
applications, but OLE control events often don't seem to work yet.

Events must be enabled explicitly for an OLE object through the
Win32::OLE->WithEvents() class method.  The Win32::OLE module uses the
IProvideClassInfo2 interface to determine the default event source of
the object.  If this interface is not supported, then the user must
specify the name of the event source explicitly in the WithEvents()
method call.  It is also possible to specify the class name of the
object as the third parameter.  In this case Win32::OLE will try to
look up the default source interface for this COCLASS.

The HANDLER argument to Win32::OLE->WithEvents() can either be a CODE
reference or a package name.  In the first case, all events will invoke
this particular function.  The first two arguments to this function will
be the OBJECT itself and the name of the event.  The remaining arguments
will be event specific.

	sub Event {
	    my ($Obj,$Event,@Args) = @_;
	    print "Event triggered: '$Event'\n";
	}
	Win32::OLE->WithEvents($Obj, \&Event);

Alternatively the HANDLER argument can specify a package name.  When the
OBJECT fires an event, Win32::OLE will try to find a function of the same
name as the event in this package.  This function will be called with the
OBJECT as the first argument followed again by the event specific parameters:

	package MyEvents;
	sub EventName1 {
	    my ($Obj,@Args) = @_;
	    print "EventName1 event triggered\n";
	}

	package main;
	Win32::OLE->WithEvents($Obj, 'MyEvents', 'IEventInterface');

If Win32::OLE doesn't find a function with the name of the event then nothing
happens.

Event parameters passed I<by reference> are handled specially.  They are not
converted to the corresponding Perl datatype but passed as Win32::OLE::Variant
objects.  You can assign a new value to these objects with the help of the
Put() method.  This value will be passed back to the object when the event
function returns:

	package MyEvents;
	sub BeforeClose {
	    my ($self,$Cancel) = @_;
	    $Cancel->Put(1) unless $MayClose;
	}

Direct assignment to $Cancel would have no effect on the original value and
would therefore not command the object to abort the closing action.

=head2 Module Options

The following module options can be accessed and modified with the
C<Win32::OLE->Option> class method.  In earlier versions of the Win32::OLE
module these options were manipulated directly as class variables.  This
practice is now deprecated.

=over 8

=item CP

This variable is used to determine the codepage used by all
translations between Perl strings and Unicode strings used by the OLE
interface.  The default value is CP_ACP, which is the default ANSI
codepage.  Other possible values are CP_OEMCP, CP_MACCP, CP_UTF7 and
CP_UTF8.  These constants are not exported by default.

=item LCID

This variable controls the locale idnetifier used for all OLE calls.
It is set to LOCALE_NEUTRAL by default.  Please check the
L<Win32::OLE::NLS> module for other locale related information.

=item Variant

This options controls how method calls and property accessors return
values of type VT_CY and VT_DECIMAL are being returned.  By default
VT_CY values are turned into strings and VT_DECIMAL values into
floating point numbers.  If the C<Variant> option is enabled, these
values are returned as Win32::OLE::Variant objects, just like VT_DATE
and VT_ERROR values.  If the Win32::OLE::Variant module is also
loaded, then all values should still behave as before in string and in
numeric context.

The only reason that the C<Variant> behavior is not the default is that
this is an incompatible change that might break existing programs.

=item Warn

This variable determines the behavior of the Win32::OLE module when
an error happens.  Valid values are:

	0	Ignore error, return undef
	1	Carp::carp if $^W is set (-w option)
	2	always Carp::carp
	3	Carp::croak

The error number and message (without Carp line/module info) are
available through the C<Win32::OLE->LastError> class method.

Alternatively the Warn option can be set to a CODE reference.  E.g.

    Win32::OLE->Option(Warn => 3);

is equivalent to

    Win32::OLE->Option(Warn => \&Carp::croak);

This can even be used to emulate the VisualBasic C<On Error Goto
Label> construct:

    Win32::OLE->Option(Warn =>  sub {goto CheckError});
    # ... your normal OLE code here ...

  CheckError:
    # ... your error handling code here ...

=item _NewEnum

This option enables additional enumeration support for collection
objects.  When the C<_NewEnum> option is set, all collections will
receive one additional property: C<_NewEnum>.  The value of this
property will be a reference to an array containing all the elements
of the collection.  This option can be useful when used in conjunction
with an automatic tree traversal program, like C<Data::Dumper> or an
object tree browser.  The value of this option should be either 1
(enabled) or 0 (disabled, default).

    Win32::OLE->Option(_NewEnum => 1);
    # ...
    my @sheets = @{$Excel->Worksheets->{_NewEnum}};

In normal application code, this would be better written as:

    use Win32::OLE qw(in);
    # ...
    my @sheets = in $Excel->Worksheets;

=item _Unique

The C<_Unique> options guarantees that Win32::OLE will maintain a
one-to-one mapping between Win32::OLE objects and the native COM/OLE
objects.  Without this option, you can query the same property twice
and get two different Win32::OLE objects for the same underlying COM
object.

Using a unique proxy makes life easier for tree traversal algorithms
to recognize they already visited a particular node.  This option
comes at a price: Win32::OLE has to maintain a global hash of all
outstanding objects and their corresponding proxies.  Identity checks
on COM objects can also be expensive if the objects reside
out-of-process or even on a different computer.  Therefore this option
is off by default unless the program is being run in the debugger.

Unfortunately, this option doesn't always help.  Some programs will
return new COM objects for even the same property when asked for it
multiple times (especially for collections).  In this case, there is
nothing Win32::OLE can do to detect that these objects are in fact
identical (because they aren't at the COM level).

The C<_Unique> option can be set to either 1 (enabled) or 0 (disabled,
default).

=back

=head1 EXAMPLES

Here is a simple Microsoft Excel application.

	use Win32::OLE;

	# use existing instance if Excel is already running
	eval {$ex = Win32::OLE->GetActiveObject('Excel.Application')};
	die "Excel not installed" if $@;
	unless (defined $ex) {
	    $ex = Win32::OLE->new('Excel.Application', sub {$_[0]->Quit;})
		    or die "Oops, cannot start Excel";
	}

        # get a new workbook
        $book = $ex->Workbooks->Add;

	# write to a particular cell
	$sheet = $book->Worksheets(1);
	$sheet->Cells(1,1)->{Value} = "foo";

        # write a 2 rows by 3 columns range
        $sheet->Range("A8:C9")->{Value} = [[ undef, 'Xyzzy', 'Plugh' ],
                                           [ 42,    'Perl',  3.1415  ]];

        # print "XyzzyPerl"
        $array = $sheet->Range("A8:C9")->{Value};
	for (@$array) {
	    for (@$_) {
		print defined($_) ? "$_|" : "<undef>|";
	    }
	    print "\n";
	}

	# save and exit
        $book->SaveAs( 'test.xls' );
	undef $book;
	undef $ex;

Please note the destructor specified on the Win32::OLE->new method.  It ensures
that Excel will shutdown properly even if the Perl program dies.  Otherwise
there could be a process leak if your application dies after having opened
an OLE instance of Excel.  It is the responsibility of the module user to
make sure that all OLE objects are cleaned up properly!

Here is an example of using Variant data types.

	use Win32::OLE;
	use Win32::OLE::Variant;
	$ex = Win32::OLE->new('Excel.Application', \&OleQuit) or die "oops\n";
	$ex->{Visible} = 1;
	$ex->Workbooks->Add;
	# should generate a warning under -w
	$ovR8 = Variant(VT_R8, "3 is a good number");
	$ex->Range("A1")->{Value} = $ovR8;
	$ex->Range("A2")->{Value} = Variant(VT_DATE, 'Jan 1,1970');

	sub OleQuit {
	    my $self = shift;
	    $self->Quit;
	}

The above will put value "3" in cell A1 rather than the string
"3 is a good number".  Cell A2 will contain the date.

Similarly, to invoke a method with some binary data, you can
do the following:

	$obj->Method( Variant(VT_UI1, "foo\000b\001a\002r") );

Here is a wrapper class that basically delegates everything but
new() and DESTROY().  The wrapper class shown here is another way to
properly shut down connections if your application is liable to die
without proper cleanup.  Your own wrappers will probably do something
more specific to the particular OLE object you may be dealing with,
like overriding the methods that you may wish to enhance with your
own.

	package Excel;
	use Win32::OLE;

	sub new {
	    my $s = {};
	    if ($s->{Ex} = Win32::OLE->new('Excel.Application')) {
		return bless $s, shift;
	    }
	    return undef;
	}

	sub DESTROY {
	    my $s = shift;
	    if (exists $s->{Ex}) {
		print "# closing connection\n";
		$s->{Ex}->Quit;
		return undef;
	    }
	}

	sub AUTOLOAD {
	    my $s = shift;
	    $AUTOLOAD =~ s/^.*:://;
	    $s->{Ex}->$AUTOLOAD(@_);
	}

	1;

The above module can be used just like Win32::OLE, except that
it takes care of closing connections in case of abnormal exits.
Note that the effect of this specific example can be easier accomplished
using the optional destructor argument of Win32::OLE::new:

	my $Excel = Win32::OLE->new('Excel.Application', sub {$_[0]->Quit;});

Note that the delegation shown in the earlier example is not the same as
true subclassing with respect to further inheritance of method calls in your
specialized object.  See L<perlobj>, L<perltoot> and L<perlbot> for details.
True subclassing (available by setting C<@ISA>) is also feasible,
as the following example demonstrates:

	#
	# Add error reporting to Win32::OLE
	#

	package Win32::OLE::Strict;
	use Carp;
	use Win32::OLE;

	use strict qw(vars);
	use vars qw($AUTOLOAD @ISA);
	@ISA = qw(Win32::OLE);

	sub AUTOLOAD {
	    my $obj = shift;
	    $AUTOLOAD =~ s/^.*:://;
	    my $meth = $AUTOLOAD;
	    $AUTOLOAD = "SUPER::" . $AUTOLOAD;
	    my $retval = $obj->$AUTOLOAD(@_);
	    unless (defined($retval) || $AUTOLOAD eq 'DESTROY') {
		my $err = Win32::OLE::LastError();
		croak(sprintf("$meth returned OLE error 0x%08x",$err))
		  if $err;
	    }
	    return $retval;
	}

	1;

This package inherits the constructor new() from the Win32::OLE
package.  It is important to note that you cannot later rebless a
Win32::OLE object as some information about the package is cached by
the object.  Always invoke the new() constructor through the right
package!

Here's how the above class will be used:

	use Win32::OLE::Strict;
	my $Excel = Win32::OLE::Strict->new('Excel.Application', 'Quit');
	my $Books = $Excel->Workbooks;
	$Books->UnknownMethod(42);

In the sample above the call to UnknownMethod() will be caught with

	UnknownMethod returned OLE error 0x80020009 at test.pl line 5

because the Workbooks object inherits the class C<Win32::OLE::Strict> from the
C<$Excel> object.

=head1 NOTES

=head2 Hints for Microsoft Office automation

=over 8

=item Documentation

The object model for the Office applications is defined in the Visual Basic
reference guides for the various applications.  These are typically not
installed by default during the standard installation.  They can be added
later by rerunning the setup program with the custom install option.

=item Class, Method and Property names

The names have been changed between different versions of Office.  For
example C<Application> was a method in Office 95 and is a property in
Office97.  Therefore it will not show up in the list of property names
C<keys %$object> when querying an Office 95 object.

The class names are not always identical to the method/property names
producing the object.  E.g. the C<Workbook> method returns an object of
type C<Workbook> in Office 95 and C<_Workbook> in Office 97.

=item Moniker (GetObject support)

Office applications seem to implement file monikers only.  For example
it seems to be impossible to retrieve a specific worksheet object through
C<GetObject("File.XLS!Sheet")>.  Furthermore, in Excel 95 the moniker starts
a Worksheet object and in Excel 97 it returns a Workbook object.  You can use
either the Win32::OLE::QueryObjectType class method or the $object->{Version}
property to write portable code.

=item Enumeration of collection objects

Enumerations seem to be incompletely implemented.  Office 95 application don't
seem to support neither the Reset() nor the Clone() methods.  The Clone()
method is still unimplemented in Office 97.  A single walk through the
collection similar to Visual Basics C<for each> construct does work however.

=item Localization

Starting with Office 97 Microsoft has changed the localized class, method and
property names back into English.  Note that string, date and currency
arguments are still subject to locale specific interpretation.  Perl uses the
system default locale for all OLE transaction whereas Visual Basic uses a
type library specific locale.  A Visual Basic script would use "R1C1" in string
arguments to specify relative references.  A Perl script running on a German
language Windows would have to use "Z1S1".  Set the LCID module option
to an English locale to write portable scripts.  This variable should
not be changed after creating the OLE objects; some methods seem to randomly
fail if the locale is changed on the fly.

=item SaveAs method in Word 97 doesn't work

This is an known bug in Word 97.  Search the MS knowledge base for Word /
Foxpro incompatibility.  That problem applies to the Perl OLE interface as
well.  A workaround is to use the WordBasic compatibility object.  It doesn't
support all the options of the native method though.

    $Word->WordBasic->FileSaveAs($file);

The problem seems to be fixed by applying the Office 97 Service Release 1.

=item Randomly failing method calls

It seems like modifying objects that are not selected/activated is sometimes
fragile.  Most of these problems go away if the chart/sheet/document is
selected or activated before being manipulated (just like an interactive
user would automatically do it).

=back

=head2 Incompatibilities

There are some incompatibilities with the version distributed by Activeware
(as of build 306).

=over 8

=item 1

The package name has changed from "OLE" to "Win32::OLE".

=item 2

All functions of the form "Win32::OLEFoo" are now "Win32::OLE::Foo",
though the old names are temporarily accomodated.  Win32::OLECreateObject()
was changed to Win32::OLE::CreateObject(), and is now called
Win32::OLE::new() bowing to established convention for naming constructors.
The old names should be considered deprecated, and will be removed in the
next version.

=item 3

Package "OLE::Variant" is now "Win32::OLE::Variant".

=item 4

The Variant function is new, and is exported by default.  So are
all the VT_XXX type constants.

=item 5

The support for collection objects has been moved into the package
Win32::OLE::Enum.  The C<keys %$object> method is now used to enumerate
the properties of the object.

=back

=head2 Bugs and Limitations

=over 8

=item *

To invoke a native OLE method with the same name as one of the
Win32::OLE methods (C<Dispatch>, C<Invoke>, C<SetProperty>, C<DESTROY>,
etc.), you have to use the C<Invoke> method:

	$Object->Invoke('Dispatch', @AdditionalArgs);

The same is true for names exported by the Exporter or the Dynaloader
modules, e.g.: C<export>, C<export_to_level>, C<import>,
C<_push_tags>, C<export_tags>, C<export_ok_tags>, C<export_fail>,
C<require_version>, C<dl_load_flags>,
C<croak>, C<bootstrap>, C<dl_findfile>, C<dl_expandspec>,
C<dl_find_symbol_anywhere>, C<dl_load_file>, C<dl_find_symbol>,
C<dl_undef_symbols>, C<dl_install_xsub> and C<dl_error>.

=back

=head1 SEE ALSO

The documentation for L<Win32::OLE::Const>, L<Win32::OLE::Enum>,
L<Win32::OLE::NLS> and L<Win32::OLE::Variant> contains additional
information about OLE support for Perl on Win32.

=head1 AUTHORS

Originally put together by the kind people at Hip and Activeware.

Gurusamy Sarathy <gsar@activestate.com> subsequently fixed several
major bugs, memory leaks, and reliability problems, along with some
redesign of the code.

Jan Dubois <jand@activestate.com> pitched in with yet more massive redesign,
added support for named parameters, and other significant enhancements.
He's been hacking on it ever since.

Please send questions about problems with this module to the
Perl-Win32-Users mailinglist at ActiveState.com.  The mailinglist charter
requests that you put an [OLE] tag somewhere on the subject line (for OLE
related questions only, of course).

=head1 COPYRIGHT

    (c) 1995 Microsoft Corporation. All rights reserved.
    Developed by ActiveWare Internet Corp., now known as
    ActiveState Tool Corp., http://www.ActiveState.com

    Other modifications Copyright (c) 1997-2000 by Gurusamy Sarathy
    <gsar@activestate.com> and Jan Dubois <jand@activestate.com>

    You may distribute under the terms of either the GNU General Public
    License or the Artistic License, as specified in the README file.

=head1 VERSION

Version 0.1703	  6 September 2005

=cut
