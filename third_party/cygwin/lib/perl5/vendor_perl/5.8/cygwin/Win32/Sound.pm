#######################################################################
#
# Win32::Sound - An extension to play with Windows sounds
# 
# Author: Aldo Calpini <dada@divinf.it>
# Version: 0.47
# Info:
#       http://www.divinf.it/dada/perl
#       http://www.perl.com/CPAN/authors/Aldo_Calpini
#
#######################################################################
# Version history: 
# 0.01 (19 Nov 1996) file created
# 0.03 (08 Apr 1997) first release
# 0.30 (20 Oct 1998) added Volume/Format/Devices/DeviceInfo
#                    (thanks Dave Roth!)
# 0.40 (16 Mar 1999) added the WaveOut object
# 0.45 (09 Apr 1999) added $! support, documentation et goodies
# 0.46 (25 Sep 1999) fixed small bug in DESTROY, wo was used without being
#		     initialized (Gurusamy Sarathy <gsar@activestate.com>)
# 0.47 (22 May 2000) support for passing Unicode string to Play()
#                    (Doug Lankshear <dougl@activestate.com>)

package Win32::Sound;

# See the bottom of this file for the POD documentation.  
# Search for the string '=head'.

require Exporter;       # to export the constants to the main:: space
require DynaLoader;     # to dynuhlode the module.

@ISA= qw( Exporter DynaLoader );
@EXPORT = qw(
    SND_ASYNC
    SND_NODEFAULT
    SND_LOOP
    SND_NOSTOP
);

#######################################################################
# This AUTOLOAD is used to 'autoload' constants from the constant()
# XS function.  If a constant is not found then control is passed
# to the AUTOLOAD in AutoLoader.
#

sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    #reset $! to zero to reset any current errors.
    local $! = 0;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {

    # [dada] This results in an ugly Autoloader error

    #if ($! =~ /Invalid/) {
    #    $AutoLoader::AUTOLOAD = $AUTOLOAD;
    #    goto &AutoLoader::AUTOLOAD;
    #} else {
    
    # [dada] ... I prefer this one :)

        ($pack, $file, $line) = caller;
        undef $pack; # [dada] and get rid of "used only once" warning...
        die "Win32::Sound::$constname is not defined, used at $file line $line.";

    #}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}


#######################################################################
# STATIC OBJECT PROPERTIES
#
$VERSION="0.47"; 
undef unless $VERSION; # [dada] to avoid "possible typo" warning

#######################################################################
# METHODS
#

sub Version { $VERSION }

sub Volume {
    my(@in) = @_;
    # Allows '0%'..'100%'   
    $in[0] =~ s{ ([\d\.]+)%$ }{ int($1*100/255) }ex if defined $in[0];
    $in[1] =~ s{ ([\d\.]+)%$ }{ int($1*100/255) }ex if defined $in[1];
    _Volume(@in);
}

#######################################################################
# dynamically load in the Sound.dll module.
#

bootstrap Win32::Sound;

#######################################################################
# Win32::Sound::WaveOut
#

package Win32::Sound::WaveOut;

sub new {
    my($class, $one, $two, $three) = @_;
    my $self = {};
    bless($self, $class);
    
    if($one !~ /^\d+$/ 
    and not defined($two)
    and not defined($three)) {
        # Looks like a file
        $self->Open($one);
    } else {
        # Default format if not given
        $self->{samplerate} = ($one   or 44100);
        $self->{bits}       = ($two   or 16);
        $self->{channels}   = ($three or 2);
        $self->OpenDevice();
    }
    return $self;
}

sub Volume {
    my(@in) = @_;
    # Allows '0%'..'100%'   
    $in[0] =~ s{ ([\d\.]+)%$ }{ int($1*255/100) }ex if defined $in[0];
    $in[1] =~ s{ ([\d\.]+)%$ }{ int($1*255/100) }ex if defined $in[1];
    _Volume(@in);
}

sub Pitch {
    my($self, $pitch) = @_;
    my($int, $frac);
    if(defined($pitch)) {
        $pitch =~ /(\d+).?(\d+)?/;
        $int = $1;
        $frac = $2 or 0;
        $int = $int << 16;
        $frac = eval("0.$frac * 65536");
        $pitch = $int + $frac;
        return _Pitch($self, $pitch);
    } else {
        $pitch = _Pitch($self);
        $int = ($pitch & 0xFFFF0000) >> 16;
        $frac = $pitch & 0x0000FFFF;
        return eval("$int.$frac");
    }
}

sub PlaybackRate {
    my($self, $rate) = @_;
    my($int, $frac);
    if(defined($rate)) {
        $rate =~ /(\d+).?(\d+)?/;
        $int = $1;
        $frac = $2 or 0;
        $int = $int << 16;
        $frac = eval("0.$frac * 65536");
        $rate = $int + $frac;
        return _PlaybackRate($self, $rate);
    } else {
        $rate = _PlaybackRate($self);
        $int = ($rate & 0xFFFF0000) >> 16;
        $frac = $rate & 0x0000FFFF;
        return eval("$int.$frac");
    }
}

# Preloaded methods go here.

#Currently Autoloading is not implemented in Perl for win32
# Autoload methods go after __END__, and are processed by the autosplit program.

1;
__END__


=head1 NAME

Win32::Sound - An extension to play with Windows sounds

=head1 SYNOPSIS

    use Win32::Sound;
    Win32::Sound::Volume('100%');
    Win32::Sound::Play("file.wav");
    Win32::Sound::Stop();
    
    # ...and read on for more fun ;-)

=head1 FUNCTIONS

=over 4

=item B<Win32::Sound::Play(SOUND, [FLAGS])>

Plays the specified sound: SOUND can the be name of a WAV file
or one of the following predefined sound names:

    SystemDefault
    SystemAsterisk
    SystemExclamation
    SystemExit
    SystemHand
    SystemQuestion
    SystemStart

Additionally, if the named sound could not be found, the 
function plays the system default sound (unless you specify the 
C<SND_NODEFAULT> flag). If no parameters are given, this function 
stops the sound actually playing (see also Win32::Sound::Stop).

FLAGS can be a combination of the following constants:

=over 4

=item C<SND_ASYNC>

The sound is played asynchronously and the function 
returns immediately after beginning the sound
(if this flag is not specified, the sound is
played synchronously and the function returns
when the sound ends).

=item C<SND_LOOP>

The sound plays repeatedly until it is stopped.
You must also specify C<SND_ASYNC> flag.

=item C<SND_NODEFAULT>

No default sound is used. If the specified I<sound>
cannot be found, the function returns without
playing anything.

=item C<SND_NOSTOP>

If a sound is already playing, the function fails.
By default, any new call to the function will stop
previously playing sounds.

=back

=item B<Win32::Sound::Stop()>

Stops the sound currently playing.

=item B<Win32::Sound::Volume()>

Returns the wave device volume; if 
called in an array context, returns left
and right values. Otherwise, returns a single
32 bit value (left in the low word, right 
in the high word).
In case of error, returns C<undef> and sets
$!.

Examples:

    ($L, $R) = Win32::Sound::Volume();
    if( not defined Win32::Sound::Volume() ) {
        die "Can't get volume: $!";
    }

=item B<Win32::Sound::Volume(LEFT, [RIGHT])>

Sets the wave device volume; if two arguments
are given, sets left and right channels 
independently, otherwise sets them both to
LEFT (eg. RIGHT=LEFT). Values range from
0 to 65535 (0xFFFF), but they can also be
given as percentage (use a string containing 
a number followed by a percent sign).

Returns C<undef> and sets $! in case of error,
a true value if successful.

Examples:

    Win32::Sound::Volume('50%');
    Win32::Sound::Volume(0xFFFF, 0x7FFF);
    Win32::Sound::Volume('100%', '50%');
    Win32::Sound::Volume(0);

=item B<Win32::Sound::Format(filename)>

Returns information about the specified WAV file format;
the array contains:

=over

=item * sample rate (in Hz)

=item * bits per sample (8 or 16)

=item * channels (1 for mono, 2 for stereo)

=back

Example:

    ($hz, $bits, $channels) 
        = Win32::Sound::Format("file.wav");


=item B<Win32::Sound::Devices()>

Returns all the available sound devices;
their names contain the type of the
device (WAVEOUT, WAVEIN, MIDIOUT,
MIDIIN, AUX or MIXER) and 
a zero-based ID number: valid devices
names are for example:

    WAVEOUT0
    WAVEOUT1
    WAVEIN0
    MIDIOUT0
    MIDIIN0
    AUX0
    AUX1
    AUX2

There are also two special device
names, C<WAVE_MAPPER> and C<MIDI_MAPPER>
(the default devices for wave output
and midi output).

Example:

    @devices = Win32::Sound::Devices();

=item Win32::Sound::DeviceInfo(DEVICE)

Returns an associative array of information 
about the sound device named DEVICE (the
same format of Win32::Sound::Devices).

The content of the array depends on the device
type queried. Each device type returns B<at least> 
the following information:

    manufacturer_id
    product_id
    name
    driver_version

For additional data refer to the following
table:

    WAVEIN..... formats
                channels
    
    WAVEOUT.... formats
                channels
                support
                
    MIDIOUT.... technology
                voices
                notes
                channels
                support
                
    AUX........ technology
                support
                
    MIXER...... destinations
                support

The meaning of the fields, where not
obvious, can be evinced from the 
Microsoft SDK documentation (too long
to report here, maybe one day... :-).

Example:

    %info = Win32::Sound::DeviceInfo('WAVE_MAPPER');
    print "$info{name} version $info{driver_version}\n";

=back

=head1 THE WaveOut PACKAGE

Win32::Sound also provides a different, more
powerful approach to wave audio data with its 
C<WaveOut> package. It has methods to load and
then play WAV files, with the additional feature
of specifying the start and end range, so you
can play only a portion of an audio file.

Furthermore, it is possible to load arbitrary
binary data to the soundcard to let it play and
save them back into WAV files; in a few words,
you can do some sound synthesis work.

=head2 FUNCTIONS

=over

=item new Win32::Sound::WaveOut(FILENAME)

=item new Win32::Sound::WaveOut(SAMPLERATE, BITS, CHANNELS)

=item new Win32::Sound::WaveOut()

This function creates a C<WaveOut> object; the
first form opens the specified wave file (see
also C<Open()> ), so you can directly C<Play()> it.

The second (and third) form opens the
wave output device with the format given
(or if none given, defaults to 44.1kHz,
16 bits, stereo); to produce something
audible you can either C<Open()> a wave file
or C<Load()> binary data to the soundcard
and then C<Write()> it.

=item Close()

Closes the wave file currently opened.

=item CloseDevice()

Closes the wave output device; you can change
format and reopen it with C<OpenDevice()>.

=item GetErrorText(ERROR)

Returns the error text associated with
the specified ERROR number; note it only
works for wave-output-specific errors.

=item Load(DATA)

Loads the DATA buffer in the soundcard.
The format of the data buffer depends
on the format used; for example, with
8 bit mono each sample is one character,
while with 16 bit stereo each sample is
four characters long (two 16 bit values
for left and right channels). The sample
rate defines how much samples are in one
second of sound. For example, to fit one
second at 44.1kHz 16 bit stereo your buffer
must contain 176400 bytes (44100 * 4).

=item Open(FILE)

Opens the specified wave FILE.

=item OpenDevice()

Opens the wave output device with the
current sound format (not needed unless
you used C<CloseDevice()>).

=item Pause()

Pauses the sound currently playing; 
use C<Restart()> to continue playing.

=item Play( [FROM, TO] )

Plays the opened wave file. You can optionally
specify a FROM - TO range, where FROM and TO
are expressed in samples (or use FROM=0 for the
first sample and TO=-1 for the last sample).
Playback happens always asynchronously, eg. in 
the background.

=item Position()

Returns the sample number currently playing;
note that the play position is not zeroed
when the sound ends, so you have to call a
C<Reset()> between plays to receive the
correct position in the current sound.

=item Reset()

Stops playing and resets the play position
(see C<Position()>).

=item Restart()

Continues playing the sound paused by C<Pause()>.

=item Save(FILE, [DATA])

Writes the DATA buffer (if not given, uses the 
buffer currently loaded in the soundcard) 
to the specified wave FILE.

=item Status()

Returns 0 if the soundcard is currently playing,
1 if it's free, or C<undef> on errors.

=item Unload()

Frees the soundcard from the loaded data.

=item Volume( [LEFT, RIGHT] )

Gets or sets the volume for the wave output device.
It works the same way as Win32::Sound::Volume.

=item Write()

Plays the data currently loaded in the soundcard;
playback happens always asynchronously, eg. in 
the background.

=back

=head2 THE SOUND FORMAT

The sound format is stored in three properties of
the C<WaveOut> object: C<samplerate>, C<bits> and
C<channels>.
If you need to change them without creating a 
new object, you should close before and reopen 
afterwards the device.

    $WAV->CloseDevice();
    $WAV->{samplerate} = 44100; # 44.1kHz
    $WAV->{bits}       = 8;     # 8 bit
    $WAV->{channels}   = 1;     # mono
    $WAV->OpenDevice();

You can also use the properties to query the
sound format currently used.

=head2 EXAMPLE

This small example produces a 1 second sinusoidal
wave at 440Hz and saves it in F<sinus.wav>:

    use Win32::Sound;
    
    # Create the object
    $WAV = new Win32::Sound::WaveOut(44100, 8, 2);
    
    $data = ""; 
    $counter = 0;
    $increment = 440/44100;
    
    # Generate 44100 samples ( = 1 second)
    for $i (1..44100) {

        # Calculate the pitch 
        # (range 0..255 for 8 bits)
        $v = sin($counter/2*3.14) * 128 + 128;    

        # "pack" it twice for left and right
        $data .= pack("cc", $v, $v);

        $counter += $increment;
    }
    
    $WAV->Load($data);       # get it
    $WAV->Write();           # hear it
    1 until $WAV->Status();  # wait for completion
    $WAV->Save("sinus.wav"); # write to disk
    $WAV->Unload();          # drop it

=head1 VERSION

Win32::Sound version 0.46, 25 Sep 1999.

=head1 AUTHOR

Aldo Calpini, C<dada@divinf.it>

Parts of the code provided and/or suggested by Dave Roth.

=cut


