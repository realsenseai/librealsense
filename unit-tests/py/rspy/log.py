# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

import sys, os

# Set this string to show a "nesting" idetifier that can be used to distinguish output
# from this process from others...
nested = None


# We're usually the first to be imported, and so the first see the original arguments as passed
# into sys.argv... remember them before we change:
# (NOTE: sys.orig_argv is available as of 3.10)
original_args = sys.argv[1:]


def _write( s ):
    """
    When s is long, write() doesn't seem to work right and only part of the string gets written!
    """
    x = 0
    chunk = 8192
    while( x < len(s) ):
        sys.stdout.write( s[x:x+chunk] )
        x += chunk


# Set up the default output system; if not a terminal, disable colors!
def _stream_has_color( stream ):
    if not hasattr(stream, "isatty"):
        return False
    if not stream.isatty():
        return False # auto color only on TTYs
    try:
        import curses
        curses.setupterm()
        return curses.tigetnum( "colors" ) > 2
    except:
        # guess false in case of error
        return False


def find_flag( arg ):
    for x in range( 1, len(sys.argv) ):
        a = sys.argv[x]
        if a == arg:
            return x
        if a == '--':
            break
    return None


_have_no_color = False
if find_flag( '--color' ):
    sys.argv.remove( '--color' )
    _have_color = True
elif find_flag( '--no-color' ):
    sys.argv.remove( '--no-color' )
    _have_color = False
    _have_no_color = True
else:
    _have_color = _stream_has_color( sys.stdout )
if _have_color:
    red = '\033[91m'
    yellow = '\033[93m'
    gray = '\033[90m'
    reset = '\033[0m'
    cr = '\033[G'
    clear_eos = '\033[J'
    clear_eol = '\033[K'
    _progress = ''
    def out( *args, sep = ' ', end = '\n', line_prefix = None, color = None ):
        global _progress, reset
        s = indent( sep.join( [str(s) for s in args] ), line_prefix )
        if color:
            s = color + s + reset
        if end:
            clear_to_eol = len(_progress) > 0  and  end[-1] == '\n'
            if clear_to_eol:
                _write( s + clear_eol + end )
                progress( *_progress )
            else:
                _write( s + end )
        else:
            _write( s )
    def progress(*args):
        global _progress
        sys.stdout.flush()
        sys.stdout.write( '\0337' )  # save cursor
        print( *args, end = clear_eol )
        sys.stdout.write( '\0338' )  # restore cursor
        sys.stdout.flush()
        _progress = args
else:
    red = yellow = gray = reset = cr = clear_eos = ''
    def out( *args, sep = ' ', end = '\n', line_prefix = None, color = None ):
        s = indent( sep.join( [str(s) for s in args] ), line_prefix )
        if end:
            _write( s + end )
        else:
            _write( s )
    def progress(*args):
        if args:
            print( *args )
        sys.stdout.flush()

def is_color_on():
    global _have_color
    return _have_color

def is_color_disabled():
    global _have_no_color
    return _have_no_color


def quiet_on():
    global out
    def out(*args):
        pass


def indent( str, line_prefix = '    ' ):
    global nested
    if nested:
        line_prefix = '[' + nested + '] ' + ( line_prefix or '' )
    if line_prefix:
        str = line_prefix + str.replace( '\n', '\n' + line_prefix )
    return str


_verbose_on = False
def v(*args):
    pass
def verbose_on():
    global v, _verbose_on
    def v(*args):
        global gray, reset
        out( gray + '-V-', *args, reset )
    _verbose_on = True
def is_verbose_on():
    global _verbose_on
    return _verbose_on


_debug_on = False
_debug_indent = ''
def d(*args):
    # Return whether info was output
    return False
def debug_on():
    global d, _debug_on, _debug_indent
    def d( *args ):
        global gray
        out( *args, line_prefix = "-D- " + _debug_indent, color = gray )
        return True
    _debug_on = True
def is_debug_on():
    global _debug_on
    return _debug_on
if find_flag( '--debug' ):
    sys.argv.remove( '--debug' )
    debug_on()
def debug_indent( n = 1, indentation = '  ' ):
    global _debug_indent
    _debug_indent += n * indentation
def debug_unindent( n = 1, indentation = '  ' ):
    global _debug_indent
    _debug_indent = _debug_indent[:-n * len(indentation)]


def i( *args ):
    out( '-I-', *args)


def f( *args ):
    out( *args, line_prefix = red + '-F-' + reset + ' ' )
    sys.exit(1)


# We track the number of errors
_n_errors = 0
def e( *args ):
    global red, reset
    out( *args, line_prefix = red + '-E-' + reset + ' ' )
    global _n_errors
    _n_errors = _n_errors + 1

def n_errors():
    global _n_errors
    return _n_errors

def reset_errors():
    global _n_errors
    _n_errors = 0


# We track the number of warnings
_n_warnings = 0
def w(*args):
    global red, reset
    out( *args, line_prefix = yellow + '-W-' + reset + ' ' )
    global _n_warnings
    _n_warnings = _n_warnings + 1

def n_warnings():
    global _n_warnings
    return _n_warnings

def reset_warnings():
    global _n_warnings
    _n_warnings = 0


def split():
    """
    Output an easy-to-distinguish line separating text above from below.
    Currently a line of "_____"
    """
    try:
        screen_width = os.get_terminal_size().columns
    except:
        # this happens under github actions, for example, or when a terminal does not exist
        screen_width = 60
    out()
    out( '_' * screen_width )

