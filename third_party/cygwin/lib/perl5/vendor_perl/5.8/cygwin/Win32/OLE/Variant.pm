# The documentation is at the __END__

package Win32::OLE::Variant;
require Win32::OLE;  # Make sure the XS bootstrap has been called

use strict;
use vars qw(@ISA @EXPORT @EXPORT_OK);

use Exporter;
@ISA = qw(Exporter);

@EXPORT = qw(
	     Variant
	     VT_EMPTY VT_NULL VT_I2 VT_I4 VT_R4 VT_R8 VT_CY VT_DATE VT_BSTR
	     VT_DISPATCH VT_ERROR VT_BOOL VT_VARIANT VT_UNKNOWN VT_DECIMAL VT_UI1
	     VT_ARRAY VT_BYREF
	    );

@EXPORT_OK = qw(CP_ACP CP_OEMCP nothing nullstring);

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
sub VT_DECIMAL {14;}	# Officially not allowed in VARIANTARGs
sub VT_UI1 {17;}

sub VT_ARRAY {0x2000;}
sub VT_BYREF {0x4000;}


# For backward compatibility
sub CP_ACP   {0;}     # ANSI codepage
sub CP_OEMCP {1;}     # OEM codepage

use overload
    # '+' => 'Add', '-' => 'Sub', '*' => 'Mul', '/' => 'Div',
    '""'     => sub {$_[0]->As(VT_BSTR)},
    '0+'     => sub {$_[0]->As(VT_R8)},
    fallback => 1;

sub Variant {
    return Win32::OLE::Variant->new(@_);
}

sub nothing {
    return Win32::OLE::Variant->new(VT_DISPATCH);
}

sub nullstring {
    return Win32::OLE::Variant->new(VT_BSTR);
}

1;

__END__

=head1 NAME

Win32::OLE::Variant - Create and modify OLE VARIANT variables

=head1 SYNOPSIS

	use Win32::OLE::Variant;
	my $var = Variant(VT_DATE, 'Jan 1,1970');
	$OleObject->{value} = $var;
	$OleObject->Method($var);


=head1 DESCRIPTION

The IDispatch interface used by the Perl OLE module uses a universal
argument type called VARIANT.  This is basically an object containing
a data type and the actual data value.  The data type is specified by
the VT_xxx constants.

=head2 Functions

=over 8

=item nothing()

The nothing() function returns an empty VT_DISPATCH variant.  It can be
used to clear an object reference stored in a property

	use Win32::OLE::Variant qw(:DEFAULT nothing);
	# ...
	$object->{Property} = nothing;

This has the same effect as the Visual Basic statement

	Set object.Property = Nothing

The nothing() function is B<not> exported by default.

=item nullstring()

The nullstring() function returns a VT_BSTR variant with a NULL string
pointer.  This is B<not> the same as a VT_BSTR variant with an empty
string "".  The nullstring() value is the same as the vbNullString
constant in Visual Basic.

The nullstring() function is B<not> exported by default.

=item Variant(TYPE, DATA)

This is just a function alias of the C<Win32::OLE::Variant->new()>
method (see below).  This function is exported by default.

=back

=head2 Methods

=over 8

=item new(TYPE, DATA)

This method returns a Win32::OLE::Variant object of the specified
TYPE that contains the given DATA.  The Win32::OLE::Variant object
can be used to specify data types other than IV, NV or PV (which are
supported transparently).  See L<Variants> below for details.

For VT_EMPTY and VT_NULL variants, the DATA argument may be omitted.
For all non-VT_ARRAY variants DATA specifies the initial value.

To create a SAFEARRAY variant, you have to specify the VT_ARRAY flag in
addition to the variant base type of the array elemnts.  In this cases
DATA must be a list specifying the dimensions of the array.  Each element
can be either an element count (indices 0 to count-1) or an array
reference pointing to the lower and upper array bounds of this dimension:

	my $Array = Win32::OLE::Variant->new(VT_ARRAY|VT_R8, [1,2], 2);

This creates a 2-dimensional SAFEARRAY of doubles with 4 elements:
(1,0), (1,1), (2,0) and (2,1).

A special case is the the creation of one-dimensional VT_UI1 arrays with
a string DATA argument:

	my $String = Variant(VT_ARRAY|VT_UI1, "String");

This creates a 6 element character array initialized to "String".  For
backward compatibility VT_UI1 with a string initializer automatically
implies VT_ARRAY.  The next line is equivalent to the previous example:

	my $String = Variant(VT_UI1, "String");

If you really need a single character VT_UI1 variant, you have to create
it using a numeric intializer:

	my $Char = Variant(VT_UI1, ord('A'));

=item As(TYPE)

C<As> converts the VARIANT to the new type before converting to a
Perl value.  This take the current LCID setting into account.  For
example a string might contain a ',' as the decimal point character.
Using C<$variant->As(VT_R8)> will correctly return the floating
point value.

The underlying variant object is NOT changed by this method.

=item ChangeType(TYPE)

This method changes the type of the contained VARIANT in place.  It
returns the object itself, not the converted value.

=item Copy([DIM])

This method creates a copy of the object.  If the original variant had
the VT_BYREF bit set then the new object will contain a copy of the
referenced data and not a reference to the same old data.  The new
object will not have the VT_BYREF bit set.

	my $Var = Variant(VT_I4|VT_ARRAY|VT_BYREF, [1,5], 3);
	my $Copy = $Var->Copy;

The type of C<$Copy> is now VT_I4|VT_ARRAY and the value is a copy of
the other SAFEARRAY.  Changes to elements of C<$Var> will not be reflected
in C<$Copy> and vice versa.

The C<Copy> method can also be used to extract a single element of a
VT_ARRAY | VT_VARIANT object.  In this case the array indices must be
specified as a list DIM:

	my $Int = $Var->Copy(1, 2);

C<$Int> is now a VT_I4 Variant object containing the value of element (1,2).

=item Currency([FORMAT[, LCID]])

This method converts the VARIANT value into a formatted curency string.  The
FORMAT can be either an integer constant or a hash reference.  Valid constants
are 0 and LOCALE_NOUSEROVERRIDE.  You get the value of LOCALE_NOUSEROVERRIDE
from the Win32::OLE::NLS module:

	use Win32::OLE::NLS qw(:LOCALE);

LOCALE_NOUSEROVERRIDE tells the method to use the system default currency
format for the specified locale, disregarding any changes that might have
been made through the control panel application.

The hash reference could contain the following keys:

	NumDigits	number of fractional digits
	LeadingZero	whether to use leading zeroes in decimal fields
	Grouping	size of each group of digits to the left of the decimal
	DecimalSep	decimal separator string
	ThousandSep	thousand separator string
	NegativeOrder	see L<Win32::OLE::NLS/LOCALE_ICURRENCY>
	PositiveOrder	see L<Win32::OLE::NLS/LOCALE_INEGCURR>
	CurrencySymbol	currency symbol string

For example:

	use Win32::OLE::Variant;
	use Win32::OLE::NLS qw(:DEFAULT :LANG :SUBLANG :DATE :TIME);
	my $lcidGerman = MAKELCID(MAKELANGID(LANG_GERMAN, SUBLANG_NEUTRAL));
	my $v = Variant(VT_CY, "-922337203685477.5808");
	print $v->Currency({CurrencySymbol => "Tuits"}, $lcidGerman), "\n";

will print:

	-922.337.203.685.477,58 Tuits

=item Date([FORMAT[, LCID]])

Converts the VARIANT into a formatted date string.  FORMAT can be either
one of the following integer constants or a format string:

	LOCALE_NOUSEROVERRIDE	system default date format for this locale
	DATE_SHORTDATE		use the short date format (default)
	DATE_LONGDATE		use the long date format
	DATE_YEARMONTH		use the year/month format
	DATE_USE_ALT_CALENDAR	use the alternate calendar, if one exists
	DATE_LTRREADING		left-to-right reading order layout
	DATE_RTLREADING		right-to left reading order layout

The constants are available from the Win32::OLE::NLS module:

	use Win32::OLE::NLS qw(:LOCALE :DATE);

The following elements can be used to construct a date format string.
Characters must be specified exactly as given below (e.g. "dd" B<not> "DD").
Spaces can be inserted anywhere between formating codes, other verbatim
text should be included in single quotes.

	d	day of month
	dd	day of month with leading zero for single-digit days
	ddd	day of week: three-letter abbreviation (LOCALE_SABBREVDAYNAME)
	dddd	day of week: full name (LOCALE_SDAYNAME)
	M	month
	MM	month with leading zero for single-digit months
	MMM	month: three-letter abbreviation (LOCALE_SABBREVMONTHNAME)
	MMMM	month: full name (LOCALE_SMONTHNAME)
	y	year as last two digits
	yy	year as last two digits with leading zero for years less than 10
	yyyy	year represented by full four digits
	gg	period/era string

For example:

	my $v = Variant(VT_DATE, "April 1 99");
	print $v->Date(DATE_LONGDATE), "\n";
	print $v->Date("ddd',' MMM dd yy"), "\n";

will print:

	Thursday, April 01, 1999
	Thu, Apr 01 99

=item Dim()

Returns a list of array bounds for a VT_ARRAY variant.  The list contains
an array reference for each dimension of the variant's SAFEARRAY.  This
reference points to an array containing the lower and upper bounds for
this dimension.  For example:

	my @Dim = $Var->Dim;

Now C<@Dim> contains the following list: C<([1,5], [0,2])>.

=item Get(DIM)

For normal variants C<Get> returns the value of the variant, just like the
C<Value> method.  For VT_ARRAY variants C<Get> retrieves the value of a single
array element.  In this case C<DIM> must be a list of array indices.  E.g.

	my $Val = $Var->Get(2,0);

As a special case for one dimensional VT_UI1|VT_ARRAY variants the C<Get>
method without arguments returns the character array as a Perl string.

	print $String->Get, "\n";

=item IsNothing()

Tests if the object is an empty VT_DISPATCH variant.  See also nothing().

=item IsNullString()

Tests if the object is an empty VT_BSTR variant.  See also nullstring().

=item LastError()

The use of the C<Win32::OLE::Variant->LastError()> method is deprecated.
Please use the C<Win32::OLE->LastError()> class method instead.

=item Number([FORMAT[, LCID]])

This method converts the VARIANT value into a formatted number string.  The
FORMAT can be either an integer constant or a hash reference.  Valid constants
are 0 and LOCALE_NOUSEROVERRIDE.  You get the value of LOCALE_NOUSEROVERRIDE
from the Win32::OLE::NLS module:

	use Win32::OLE::NLS qw(:LOCALE);

LOCALE_NOUSEROVERRIDE tells the method to use the system default number
format for the specified locale, disregarding any changes that might have
been made through the control panel application.

The hash reference could contain the following keys:

	NumDigits	number of fractional digits
	LeadingZero	whether to use leading zeroes in decimal fields
	Grouping	size of each group of digits to the left of the decimal
	DecimalSep	decimal separator string
	ThousandSep	thousand separator string
	NegativeOrder	see L<Win32::OLE::NLS/LOCALE_INEGNUMBER>

=item Put(DIM, VALUE)

The C<Put> method is used to assign a new value to a variant.  The value will
be coerced into the current type of the variant.  E.g.:

	my $Var = Variant(VT_I4, 42);
	$Var->Put(3.1415);

This changes the value of the variant to C<3> because the type is VT_I4.

For VT_ARRAY type variants the indices for each dimension of the contained
SAFEARRAY must be specified in front of the new value:

	$Array->Put(1, 1, 2.7);

It is also possible to assign values to *every* element of the SAFEARRAY at
once using a single Put() method call:

	$Array->Put([[1,2], [3,4]]);

In this case the argument to Put() must be an array reference and the
dimensions of the Perl list-of-lists must match the dimensions of the
SAFEARRAY exactly.

The are a few special cases for one-dimensional VT_UI1 arrays: The VALUE
can be specified as a string instead of a number.  This will set the selected
character to the first character of the string or to '\0' if the string was
empty:

	my $String = Variant(VT_UI1|VT_ARRAY, "ABCDE");
	$String->Put(1, "123");
	$String->Put(3, ord('Z'));
	$String->Put(4, '');

This will set the value of C<$String> to C<"A1CZ\0">.  If the index is omitted
then the string is copied to the value completely.  The string is truncated
if it is longer than the size of the VT_UI1 array.  The result will be padded
with '\0's if the string is shorter:

	$String->Put("String");

Now C<$String> contains the value "Strin".

C<Put> returns the Variant object itself so that multiple C<Put> calls can be
chained together:

	$Array->Put(0,0,$First_value)->Put(0,1,$Another_value);

=item Time([FORMAT[, LCID]])

Converts the VARIANT into a formatted time string.  FORMAT can be either
one of the following integer constants or a format string:

	LOCALE_NOUSEROVERRIDE	system default time format for this locale
	TIME_NOMINUTESORSECONDS	don't use minutes or seconds
	TIME_NOSECONDS		don't use seconds
	TIME_NOTIMEMARKER	don't use a time marker
	TIME_FORCE24HOURFORMAT	always use a 24-hour time format

The constants are available from the Win32::OLE::NLS module:

	use Win32::OLE::NLS qw(:LOCALE :TIME);

The following elements can be used to construct a time format string.
Characters must be specified exactly as given below (e.g. "dd" B<not> "DD").
Spaces can be inserted anywhere between formating codes, other verbatim
text should be included in single quotes.

	h	hours; 12-hour clock
	hh	hours with leading zero for single-digit hours; 12-hour clock
	H	hours; 24-hour clock
	HH	hours with leading zero for single-digit hours; 24-hour clock
	m	minutes
	mm	minutes with leading zero for single-digit minutes
	s	seconds
	ss	seconds with leading zero for single-digit seconds
	t	one character time marker string, such as A or P
	tt	multicharacter time marker string, such as AM or PM

For example:

	my $v = Variant(VT_DATE, "April 1 99 2:23 pm");
	print $v->Time, "\n";
	print $v->Time(TIME_FORCE24HOURFORMAT|TIME_NOTIMEMARKER), "\n";
	print $v->Time("hh.mm.ss tt"), "\n";

will print:

	2:23:00 PM
	14:23:00
	02.23.00 PM

=item Type()

The C<Type> method returns the variant type of the contained VARIANT.

=item Unicode()

The C<Unicode> method returns a C<Unicode::String> object.  This contains
the BSTR value of the variant in network byte order.  If the variant is
not currently in VT_BSTR format then a VT_BSTR copy will be produced first.

=item Value()

The C<Value> method returns the value of the VARIANT as a Perl value.  The
conversion is performed in the same manner as all return values of
Win32::OLE method calls are converted.

=back

=head2 Overloading

The Win32::OLE::Variant package has overloaded the conversion to
string and number formats.  Therefore variant objects can be used in
arithmetic and string operations without applying the C<Value>
method first.

=head2 Class Variables

The Win32::OLE::Variant class used to have its own set of class variables
like C<$CP>, C<$LCID> and C<$Warn>.  In version 0.1003 and later of the
Win32::OLE module these variables have been eleminated.  Now the settings
of Win32::OLE are used by the Win32::OLE::Variant module too.  Please read
the documentation of the C<Win32::OLE-&gt;Option> class method.


=head2 Constants

These constants are exported by default:

	VT_EMPTY
	VT_NULL
	VT_I2
	VT_I4
	VT_R4
	VT_R8
	VT_CY
	VT_DATE
	VT_BSTR
	VT_DISPATCH
	VT_ERROR
	VT_BOOL
	VT_VARIANT
	VT_UNKNOWN
	VT_DECIMAL
	VT_UI1

	VT_ARRAY
	VT_BYREF

VT_DECIMAL is not on the official list of allowable OLE Automation
datatypes.  But even Microsoft ADO seems to sometimes return values
of Recordset fields in VT_DECIMAL format.

=head2 Variants

A Variant is a data type that is used to pass data between OLE
connections.

The default behavior is to convert each perl scalar variable into
an OLE Variant according to the internal perl representation.
The following type correspondence holds:

        C type          Perl type       OLE type
        ------          ---------       --------
          int              IV            VT_I4
        double             NV            VT_R8
        char *             PV            VT_BSTR
        void *           ref to AV       VT_ARRAY
           ?              undef          VT_ERROR
           ?        Win32::OLE object    VT_DISPATCH

Note that VT_BSTR is a wide character or Unicode string.  This presents a
problem if you want to pass in binary data as a parameter as 0x00 is
inserted between all the bytes in your data.  The C<Variant()> method
provides a solution to this.  With Variants the script writer can specify
the OLE variant type that the parameter should be converted to.  Currently
supported types are:

        VT_UI1     unsigned char
        VT_I2      signed int (2 bytes)
        VT_I4      signed int (4 bytes)
        VT_R4      float      (4 bytes)
        VT_R8      float      (8 bytes)
        VT_DATE    OLE Date
        VT_BSTR    OLE String
        VT_CY      OLE Currency
        VT_BOOL    OLE Boolean

When VT_DATE and VT_CY objects are created, the input parameter is treated
as a Perl string type, which is then converted to VT_BSTR, and finally to
VT_DATE of VT_CY using the C<VariantChangeType()> OLE API function.
See L<Win32::OLE/EXAMPLES> for how these types can be used.

=head2 Variant arrays

A variant can not only contain a single value but also a multi-dimensional
array of values (called a SAFEARRAY).  In this case the VT_ARRAY flag must
be added to the base variant type, e.g. C<VT_I4 | VT_ARRAY> for an array of
integers.  The VT_EMPTY and VT_NULL types are invalid for SAFEARRAYs.  It
is possible to create an array of variants: C<VT_VARIANT | VT_ARRAY>.  In this
case each element of the array can have a different type (including VT_EMPTY
and VT_NULL).  The elements of a VT_VARIANT SAFEARRAY cannot have either of the
VT_ARRAY or VT_BYREF flags set.

The lower and upper bounds for each dimension can be specified separately.
They do not have to have all the same lower bound (unlike Perl's arrays).

=head2 Variants by reference

Some OLE servers expect parameters passed by reference so that they
can be changed in the method call.  This allows methods to easily
return multiple values.  There is preliminary support for this in
the Win32::OLE::Variant module:

	my $x = Variant(VT_I4|VT_BYREF, 0);
	my $y = Variant(VT_I4|VT_BYREF, 0);
	$Corel->GetSize($x, $y);
	print "Size is $x by $y\n";

After the C<GetSize> method call C<$x> and C<$y> will be set to
the respective sizes.  They will still be variants.  In the print
statement the overloading converts them to string representation
automatically.

VT_BYREF is now supported for all variant types (including SAFEARRAYs).
It can also be used to pass an OLE object by reference:

	my $Results = $App->CreateResultsObject;
	$Object->Method(Variant(VT_DISPATCH|VT_BYREF, $Results));

=head1 AUTHORS/COPYRIGHT

This module is part of the Win32::OLE distribution.

=cut
