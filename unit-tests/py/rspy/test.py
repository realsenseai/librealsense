# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

"""
This module is for formatting and writing unit-tests in python. The general format is as follows
1. Use start() to start a test and give it, as an argument, the name of the test
2. Use whatever check functions are relevant to test the run
3. Use finish() to signal the end of the test
4. Repeat stages 1-3 as the number of tests you want to run in the file
5. Use print_results_and_exit() to print the number of tests and assertions that passed/failed in the correct format
   before exiting with 0 if all tests passed or with 1 if there was a failed test

In addition you may want to use the 'info' functions in this module to add more detailed
messages in case of a failed check
"""

import os, sys, subprocess, threading, traceback, platform, math, re

from rspy import log

n_assertions = 0
n_failed_assertions = 0
n_tests = 0
n_failed_tests = 0
failed_tests = []
test_failed = False
test_in_progress = None
test_info = {} # Dictionary for holding additional information to print in case of a failed check.


# These are 'on_fail' argument possible values:
ABORT = 'abort'  # call test.abort()
RAISE = 'raise'  # raise an exception
LOG   = 'log'    # log it and continue


class CheckFailed( Exception ):
    """
    This is the raised exception when RAISE is specified above
    """
    def __init__( self, message ):
        super().__init__( message )


# if --context flag was sent, the test is running under a specific context which could affect its run
context = []
if '--context' in sys.argv:
    context_index = sys.argv.index( '--context' )
    try:
        context = sys.argv.pop(context_index + 1).split()
    except IndexError:
        log.f( "Received --context flag but no context" )
    sys.argv.pop( context_index )

# If --rslog flag was sent, enable LibRS logging (LOG_DEBUG, etc.)
try:
    sys.argv.remove( '--rslog' )
    import pyrealsense2 as rs
    rs.log_to_console( rs.log_severity.debug )
except ValueError as e:
    pass  # No --rslog passed in

if '--nested' in sys.argv:
    nested_index = sys.argv.index( '--nested' )
    try:
        log.nested = sys.argv.pop(nested_index + 1)
    except IndexError:
        log.f( "Received --nested flag but no nested name" )
    sys.argv.pop( nested_index )
    # Use a special prompt when interactive mode is requested (-i)
    if sys.flags.interactive and not hasattr( sys, 'ps1' ):
        sys.ps1 = '___ready\n'  # sys.ps2 will get the default '...'


def set_env_vars( env_vars ):
    """
    We want certain environment variables set when we get here. We assume they're not set.

    However, it is impossible to change the current running environment to see them. Instead, we rerun ourselves
    in a child process that inherits the environment we set.

    To do this, we depend on a specific argument in sys.argv that tells us this is the rerun (meaning child
    process). When we see it, we assume the variables are set and don't do anything else.

    For this to work well, the environment variable requirement (set_env_vars call) should appear as one of the
    first lines of the test.

    :param env_vars: A dictionary where the keys are the name of the environment variable and the values are the
        wanted values in string form (environment variables must be strings)
    """
    if sys.argv[-1] != 'rerun':
        log.d( 'environment variables needed:', env_vars )
        for env_var, val in env_vars.items():
            os.environ[env_var] = val
        cmd = [sys.executable]
        #
        # PYTHON FLAGS
        #
        #     -u     : force the stdout and stderr streams to be unbuffered; same as PYTHONUNBUFFERED=1
        # With buffering we may end up losing output in case of crashes! (in Python 3.7 the text layer of the
        # streams is unbuffered, but we assume 3.6)
        cmd += ['-u']
        #if 'site' not in sys.modules:
        #    #     -S     : don't imply 'import site' on initialization
        #    cmd += ["-S"]
        #
        if sys.flags.verbose:
            #     -v     : verbose (trace import statements)
            cmd += ["-v"]
        #
        cmd += [sys.argv[0]]
        #
        # SCRIPT FLAGS
        #
        # Pass in the same args as the current script got:
        cmd += log.original_args
        #
        # And add a flag 'rerun' that we'll see next time we get here in the subprocess
        cmd += ["rerun"]
        log.d( f'[pid {os.getpid()}] running: {cmd}' )
        p = subprocess.run( cmd,
                            stdout=None,
                            stderr=subprocess.STDOUT,
                            universal_newlines=True )
        sys.exit( p.returncode )
    log.d( f'[pid {os.getpid()}] rerun detected' )
    sys.argv = sys.argv[:-1]  # Remove the rerun


def find_first_device_or_exit():
    """
    :return: the first device that was found, if no device is found the test is skipped. That way we can still run
        the unit-tests when no device is connected and not fail the tests that check a connected device
    """
    import pyrealsense2 as rs
    c = rs.context()
    if not c.devices.size():  # if no device is connected we skip the test
        log.f("No device found")
    dev = c.devices[0]
    log.d( 'found', dev )
    log.d( 'in', rs )
    return dev, c


def find_devices_by_product_line_or_exit( product_line ):
    """
    :param product_line: The product line of the wanted devices
    :return: A list of devices of specific product line that was found, if no device is found the test is skipped.
        That way we can still run the unit-tests when no device is connected
        and not fail the tests that check a connected device
    """
    import pyrealsense2 as rs
    c = rs.context()
    devices_list = c.query_devices(product_line)
    if devices_list.size() == 0:
        log.f( "No device of the", product_line, "product line was found" )
    log.d( 'found', devices_list.size(), product_line, 'devices:', [dev for dev in devices_list] )
    log.d( 'in', rs )
    return devices_list


def find_two_devices_by_product_line_or_exit( product_line ):
    """
    Find exactly two devices of the specified product line, ensuring they have different serial numbers.
    
    This is the main entry point for tests that require two RealSense devices simultaneously.
    If fewer than two devices are available, the test will be skipped (not failed) with a clear message.
    
    :param product_line: The product line of the wanted devices (e.g., rs.product_line.D400)
    :return: A tuple of (dev1, dev2, context) where dev1 and dev2 are two different devices
    
    Example usage:
        import pyrealsense2 as rs
        dev1, dev2, ctx = test.find_two_devices_by_product_line_or_exit(rs.product_line.D400)
        # dev1 and dev2 are guaranteed to have different serial numbers
    """
    import pyrealsense2 as rs
    c = rs.context()
    devices_list = c.query_devices(product_line)
    
    if devices_list.size() < 2:
        log.f( f"Test requires 2 devices; found {devices_list.size()} device(s) of {product_line} product line" )
    
    # Get the first two devices
    dev1 = devices_list[0]
    dev2 = devices_list[1]
    
    # Verify they have different serial numbers (sanity check)
    sn1 = dev1.get_info(rs.camera_info.serial_number) if dev1.supports(rs.camera_info.serial_number) else None
    sn2 = dev2.get_info(rs.camera_info.serial_number) if dev2.supports(rs.camera_info.serial_number) else None
    
    if sn1 and sn2 and sn1 == sn2:
        log.f( f"Found two devices but they have the same serial number: {sn1}" )
    
    log.d( 'found 2 devices:' )
    log.d( '  device 1:', dev1, 'SN:', sn1 )
    log.d( '  device 2:', dev2, 'SN:', sn2 )
    log.d( 'in', rs )
    
    return dev1, dev2, c


class two_devices:
    """
    Context manager for tests that require two RealSense devices simultaneously.
    
    This provides a clean interface for multi-device tests with automatic device discovery
    and graceful failure handling when insufficient devices are available.
    
    Usage example:
        import pyrealsense2 as rs
        from rspy import test
        
        with test.two_devices(rs.product_line.D400) as (dev1, dev2):
            # Your test code here using dev1 and dev2
            # Both devices are guaranteed to have different serial numbers
            sn1 = dev1.get_info(rs.camera_info.serial_number)
            sn2 = dev2.get_info(rs.camera_info.serial_number)
            test.check(sn1 != sn2, "Serial numbers should be different")
    
    The context manager handles device cleanup automatically at the end of the 'with' block.
    If fewer than 2 devices are found, the test will exit gracefully (skip, not fail).
    """
    
    def __init__(self, product_line):
        """
        :param product_line: The product line of devices to search for (e.g., rs.product_line.D400)
        """
        self.product_line = product_line
        self.dev1 = None
        self.dev2 = None
        self.context = None
    
    def __enter__(self):
        """
        Called when entering the 'with' statement. Discovers and returns two devices.
        """
        self.dev1, self.dev2, self.context = find_two_devices_by_product_line_or_exit(self.product_line)
        return self.dev1, self.dev2
    
    def __exit__(self, exc_type, exc_value, traceback):
        """
        Called when exiting the 'with' statement. Cleanup happens here if needed.
        """
        # Device cleanup is handled by pyrealsense2 automatically
        # This method is here for future extensibility (e.g., explicit device reset)
        pass


def print_stack():
    """
    Function for printing the current call stack. Used when an assertion fails
    """
    # Avoid stack trace into print_stack():
    #     File "C:/work/git/lrs\unit-tests\py\rspy\test.py", line 124, in check
    #       print_stack()
    #     File "C:/work/git/lrs\unit-tests\py\rspy\test.py", line 87, in print_stack
    #       stack = traceback.format_stack()
    print_stack_( traceback.format_stack()[:-2] )


def print_stack_( stack ):
    """
    Function for printing the current call stack. Used when an assertion fails
    """
    log.e( 'Traceback (most recent call last):' )
    for line in stack:
        log.out( line[:-1], line_prefix = '    ' )  # format_stack() adds \n


"""
The following functions are for asserting test cases:
The check family of functions tests an expression and continues the test whether the assertion succeeded or failed.
The require family are equivalent but execution is aborted if the assertion fails. In this module, the require family
is used by sending abort=True to check functions
"""


def _count_check():
    global n_assertions
    n_assertions += 1


def check_passed():
    """
    Function for when a check passes
    :return: always True (so you can 'return check_passed()'
    """
    _count_check()
    print_info( error = False )
    reset_info()
    return True


def check_failed( on_fail=LOG, description=None ):
    """
    Function for when a check fails
    :return: always False (so you can 'return check_failed()'
    """
    if description:
        log.out( f'    {description}' )
    _count_check()
    global n_failed_assertions, test_failed
    n_failed_assertions += 1
    test_failed = True
    print_info()
    if on_fail == ABORT:
        abort()
    if on_fail == RAISE:
        raise CheckFailed( f'on_fail=RAISE' )
    if on_fail != LOG:
        log.e( f'Invalid \'on_fail\' argument \'{on_fail}\': should be {ABORT}, {RAISE}, or {LOG}' )
    return False


def abort():
    log.f( "Aborting" )


def check( exp, description=None, on_fail=LOG ):
    """
    Basic function for asserting expressions.
    :param exp: An expression to be asserted, if false the assertion failed
    :param on_fail: How to behave on failure: see constants above
    :return: True if assertion passed, False otherwise
    """
    if not exp:
        print_stack()
        return check_failed( on_fail, description=( description or f'Got \'{exp}\'' ))
    return check_passed()


def check_false( exp, description=None, on_fail=LOG ):
    """
    Opposite of check()
    """
    if exp:
        print_stack()
        return check_failed( on_fail, description=( description or f'Expecting False; got \'{exp}\'' ))
    return check_passed()


def check_equal( result, expected, on_fail=LOG ):
    """
    Used for asserting a variable has the expected value
    :param result: The actual value of a variable
    :param expected: The expected value of the variable
    :param on_fail: How to behave on failure; see constants above
    :return: True if assertion passed, False otherwise
    """
    if expected is not None  and  result is not None  and  type(expected) != type(result):
        raise RuntimeError( f'incompatible types passed to check_equal( {type(result)}, {type(expected)} )' )
    if result != expected:
        print_stack()
        if type(expected) == list:
            i = 0
            if len(result) != len(expected):
                log.out( f'        left  : {result} len {len(result)}' )
                log.out( f'        right : {expected} len {len(expected)}' )
            else:
                log.out( f'        list diffs, size={len(expected)}:' )
                n = 0
                w = 5
                for res, exp in zip(result, expected):
                    if res != exp:
                        if n <= 5:
                            w = max( len(str(res)), w )
                        else:
                            break;
                        n += 1
                n = 0
                for res, exp in zip(result, expected):
                    if res != exp:
                        if n <= 5:
                            log.out( f'{i:13} : {res:>{w}}  ->  {exp}' )
                        n += 1
                    i += 1
                if n > 5:
                    log.out( f'                ... and {n-5} more' )
        else:
            log.out( f'        left  : {result}' )
            log.out( f'        right : {expected}' )
        return check_failed( on_fail )
    return check_passed()

check_equal_lists = check_equal

# Recursive function to compare two JSON objects with epsilon for floats
def check_equal_jsons(json1, json2, epsilon=1e-6, path="root"):
    """
    Compares two JSON-like objects with support for float tolerance and ignoring field order.
    :param json1: The actual JSON object.
    :param json2: The expected JSON object.
    :param epsilon: The tolerance for float comparison.
    :param path: The current path in the JSON structure for logging mismatches.
    :return: True if JSONs are equal within tolerance, False otherwise.
    """
    def log_difference(path, j1, j2):
        print(f"Mismatch at {path}:")
        print(f"        left  : {j1}")
        print(f"        right : {j2}")

    # Normalize dictionaries by sorting their keys, so order doesn't matter
    if isinstance(json1, dict) and isinstance(json2, dict):
        if set(json1.keys()) != set(json2.keys()):
            log_difference(path, json1, json2)
            return False
        # Recursively compare each key-value pair
        for key in json1:
            if not check_equal_jsons(json1[key], json2[key], epsilon, path=f"{path}.{key}"):
                return False

    # Compare lists by their length and then by recursively comparing each item
    elif isinstance(json1, list) and isinstance(json2, list):
        if len(json1) != len(json2):
            log_difference(path, json1, json2)
            return False
        # Sort lists of dictionaries by a normalized key structure to ensure order-independence
        sorted_json1 = sorted(json1, key=lambda x: str(x) if isinstance(x, (dict, list)) else x)
        sorted_json2 = sorted(json2, key=lambda x: str(x) if isinstance(x, (dict, list)) else x)
        for i, (item1, item2) in enumerate(zip(sorted_json1, sorted_json2)):
            if not check_equal_jsons(item1, item2, epsilon, path=f"{path}[{i}]"):
                return False

    # Compare floats with epsilon tolerance
    elif isinstance(json1, float) and isinstance(json2, float):
        if abs(json1 - json2) > epsilon:
            log_difference(path, json1, json2)
            return False

    # Direct comparison for other types
    else:
        if json1 != json2:
            log_difference(path, json1, json2)
            return False

    return True



def check_between( result, min, max, on_fail=LOG ):
    """
    Used for asserting a variable is between two values
    :param result: The actual value of a variable
    :param min: The minimum expected value of the result
    :param max: The maximum expected value of the result
    :param on_fail: How to behave on failure; see constants above
    :return: True if assertion passed, False otherwise
    """
    if result < min  or  result > max:
        print_stack()
        log.out( "       result :", result )
        log.out( "      between :", min, '-', max )
        return check_failed( on_fail )
    return check_passed()


def check_approx_abs( result, expected, abs_err, on_fail=LOG ):
    """
    Used for asserting a variable has the expected value, plus/minus 'abs_err'
    :param result: The actual value of a variable
    :param expected: The expected value of the result
    :param abs_err: How far away from expected we're allowed to get
    :param on_fail: How to behave on failure; see constants above
    :return: True if assertion passed, False otherwise
    """
    return check_between( result, expected - abs_err, expected + abs_err, on_fail )


def unreachable( on_fail=LOG ):
    """
    Used to assert that a certain section of code (exp: an if block) is not reached
    :param on_fail: How to behave; see constants above
    """
    print_stack()
    check_failed( on_fail )


def _unexpected_exception( type, e, tb ):
    print_stack_( traceback.format_list( traceback.extract_tb( tb )))
    for line in traceback.format_exception_only( type, e ):
        log.out( line[:-1], line_prefix = '    ' )
    check_failed( description='Unexpected exception!' )


def unexpected_exception():
    """
    Used to assert that an except block is not reached. It's different from unreachable because it expects
    to be in an except block and prints the stack of the error and not the call-stack for this function
    """
    type,e,tb = sys.exc_info()
    return _unexpected_exception( type, e, tb )


def check_float_lists( result, expected, epsilon=1e-6, on_fail=LOG ):
    """
    Like check_equal_lists but checks that floats diff is less then epsilon, not exactly equal
    :param result: The actual list
    :param expected: The expected list
    :param epsilon:  allowed difference between appropriate elements in the lists.
    :param on_fail: How to behave on failure; see constants above
    :return: True if assertion passed, False otherwise
    """
    failed = False
    if len(result) != len(expected):
        failed = True
        log.out("Check float lists failed due to lists of different sizes:")
        log.out("The resulted list has", len(result), "elements, but the expected list has", len(expected), "elements")
    i = 0
    for res, exp in zip(result, expected):
        if math.fabs( res - exp ) > epsilon:
            failed = True
            log.out("Check float lists failed due to unequal elements:")
            log.out("The difference between elements of index", i, "in both lists was larger than epsilon", epsilon)
        i += 1
    if failed:
        print_stack()
        log.out( "    result list  :", result )
        log.out( "    expected list:", expected )
        return check_failed( on_fail )
    return check_passed()


def check_exception( exception, expected_type, expected_msg=None, on_fail=LOG ):
    """
    Used to assert a certain type of exception was raised, placed in the except block
    :param exception: The exception that was raised
    :param expected_type: The expected type of exception
    :param expected_msg: The expected message in the exception; can be re.Pattern: use re.compile(...)
    :param on_fail: How to behave on failure; see constants above
    :return: True if assertion passed, False otherwise
    """
    failed = False
    if type(exception) != expected_type:
        failed = [ "        raised exception was", type(exception),
                 "\n        but expected", expected_type,
                 "\n      With message:", str(exception) ]
    elif expected_msg is not None:
        if isinstance( expected_msg, str ):
            if str(exception) != expected_msg:
                failed = [ "        exception message:", str(exception),
                         "\n        but we expected  :", expected_msg ]
        elif isinstance( expected_msg, re.Pattern ):
            if not expected_msg.fullmatch( str(exception) ):
                failed = [ "        exception message :", str(exception),
                         "\n        but expected regex:", expected_msg.pattern ]
        else:
            raise RuntimeError( f"exception message should be string or compiled regex (got {type(expected_msg)})" )
    if failed:
        print_stack()
        log.out( *failed )
        return check_failed( on_fail )
    log.d( 'expected exception:', exception )
    return check_passed()


def check_throws( _lambda, expected_type, expected_msg=None, on_fail=LOG ):
    """
    We expect the lambda, when called, to raise an exception!
    """
    if not callable( _lambda ):
        raise RuntimeError( "expecting a function, not " + _lambda )
    try:
        _lambda()
    except Exception as e:
        return check_exception( e, expected_type, expected_msg, on_fail )
    print_stack()
    log.out( f'        expected {expected_type} but no exception was thrown' )
    return check_failed( on_fail )


def check_frame_drops(frame, previous_frame_number, allowed_drops = 1, allow_frame_counter_reset = False):
    """
    Used for checking frame drops while streaming
    :param frame: Current frame being checked
    :param previous_frame_number: Number of the previous frame
    :param allowed_drops: Maximum number of frame drops we accept
    :return: False if dropped too many frames or frames were out of order, True otherwise
    """
    global test_in_progress
    if test_in_progress is None:
        return True
    frame_number = frame.get_frame_number()
    failed = False
    # special case for D400, because the depth sensor may reset itself
    if previous_frame_number > 0 and not (allow_frame_counter_reset and frame_number < 5):
        dropped_frames = frame_number - (previous_frame_number + 1)
        if dropped_frames > allowed_drops:
            log.out( dropped_frames, "frame(s) before", frame, "were dropped" )
            failed = True
        elif dropped_frames < 0:
            log.out( "Frames repeated or out of order. Got", frame, "after frame", previous_frame_number )
            failed = True
    if failed:
        fail() 
        return False
    reset_info()
    return True


class Information:
    """
    Class representing the information stored in test_info dictionary
    """
    def __init__(self, value, persistent = False):
        self.value = value
        self.persistent = persistent


def info( name, value, persistent = False ):
    """
    This function is used to store additional information to print in case of a failed test. This information is
    erased after the next check. The information is stored in the dictionary test_info, Keys are names (strings)
    and the items are of Information class
    If information with the given name is already stored it will be replaced
    :param name: The name of the variable
    :param value: The value this variable stores
    :param persistent: If this parameter is True, the information stored will be kept after the following check
        and will only be erased at the end of the test (or when reset_info is called with True)
    """
    global test_info
    test_info[name] = Information(value, persistent)
    return value


def reset_info(persistent = False):
    """
    erases the stored information
    :param persistent: If this parameter is True, even the persistent information will be erased
    """
    global test_info
    if persistent:
        test_info.clear()
    else:
        new_info = test_info.copy()
        for name, information in test_info.items():
            if not information.persistent:
                new_info.pop(name)
        test_info = new_info


def print_info( error = True ):
    global test_info
    if not test_info: # No information is stored
        return
    logger = error and log.out or log.d
    for name, information in test_info.items():
        logger( f"        {name} : {information.value}" )
    reset_info()


def fail():
    """
    Function for manually failing a test in case you want a specific test that does not fit any check function
    """
    check_test_in_progress()
    global test_failed
    if not test_failed:
        test_failed = True


def check_test_in_progress( in_progress = True ):
    global test_in_progress
    actually_in_progress = test_in_progress is not None
    if actually_in_progress != in_progress:
        if actually_in_progress:
            raise RuntimeError( "test case is already running" )
        else:
            raise RuntimeError( "no test case is running" )


def start(*test_name):
    """
    Used at the beginning of each test to reset the global variables
    :param test_name: Any number of arguments that combined give the name of this test
    """
    print_separator()
    global n_tests, test_failed, test_in_progress
    n_tests += 1
    test_failed = False
    test_in_progress = test_name
    reset_info( persistent = True )
    log.i( 'Test:', *test_name )


def finish( on_fail=LOG ):
    """
    Used at the end of each test to check if it passed and print the answer
    """
    check_test_in_progress()
    global test_failed, failed_tests, n_failed_tests, test_in_progress
    if test_failed:
        n_failed_tests += 1
        failed_tests.append( test_in_progress )
        log.e("Test failed")
        if on_fail == ABORT:
            abort()
        if on_fail == RAISE:
            # This is a test failure, not a check failure, so we don't use CheckFailed
            raise RuntimeError( f'test "{test_in_progress}" failed' )
    else:
        log.i("Test passed")
    test_in_progress = None

def print_separator():
    """
    For use only in-between test-cases, this will separate them in some visual way so as
    to be easier to differentiate.
    """
    check_test_in_progress( False )
    log.out( '\n___' )


class closure:
    """
    Automatic wrapper around a test start/finish, with a try and unexpected_exception at the end
    """
    def __init__( self, *test_name, on_fail=LOG ):
        self._on_fail = on_fail
        self._name = test_name
    def __enter__( self ):
        start( *self._name )
        return self
    def __exit__( self, type, value, traceback ):
        if type is not None:
            # An exception was thrown
            if type != CheckFailed:  # if CheckFailed, the relevant info was already supplied
                _unexpected_exception( type, value, traceback )
        finish( on_fail=self._on_fail )
        # "If an exception is supplied, and the method wishes to suppress the exception (i.e.,
        # prevent it from being propagated), it should return a true value."
        # https://docs.python.org/3/reference/datamodel.html#with-statement-context-managers
        return True  # otherwise the exception will keep propagating


def print_results():
    """
    Used to print the results of the tests in the file. The format has to agree with the expected format in
    check_log_for_fails() in run-unit-tests and with the C++ format using Catch
    """
    print_separator()
    global n_assertions, n_tests, n_failed_assertions, n_failed_tests, failed_tests
    if n_failed_tests or n_failed_assertions:
        passed = n_assertions - n_failed_assertions
        log.out( f'test cases: {n_tests} | {log.red}{n_failed_tests} failed{log.reset}' )
        for name in failed_tests:
            log.d( f'    {name}' )
        log.out("assertions:", n_assertions, "|", passed, "passed |", n_failed_assertions, "failed")
        sys.exit(1)
    log.out("All tests passed (" + str(n_assertions) + " assertions in " + str(n_tests) + " test cases)")


def print_results_and_exit():
    print_results()
    sys.exit(0)


def nested_cmd( script, nested_indent='svr', interactive=False, cwd=None ):
    """
    Builds the command list for running a nested script, given the current context etc.

    :param script: the path to the other script
    :param nested_indent: some short identifier to help you distinguish the remote vs the local stdout
    """

    import sys
    cmd = [sys.executable]
    #
    # PYTHON FLAGS
    #
    #     -u     : force the stdout and stderr streams to be unbuffered; same as PYTHONUNBUFFERED=1
    # With buffering we may end up losing output in case of crashes! (in Python 3.7 the text layer of the
    # streams is unbuffered, but we assume 3.6)
    cmd += ['-u']
    #
    #     -v     : verbose (trace import statements); can be supplied multiple times to increase verbosity
    if sys.flags.verbose:
        cmd += ["-v"]
    #
    #     -i     : inspect interactively after running script; forces a prompt even if stdin does not appear
    #              to be a terminal
    if interactive:
        cmd += ["-i"]
    #
    if not os.path.isabs( script ):
        cwd = cwd or os.getcwd()
        script = os.path.join( cwd, script )
    cmd += [script]
    #
    if log.is_debug_on():
        cmd += ['--debug']
    #
    if log.is_color_on():
        cmd += ['--color']
    #
    cmd += ['--nested', nested_indent or '']
    #
    return cmd


class remote:
    """
    Start another script, in a "remote" process, in interactive mode that you can control.
    I.e., you're running the script and can then give it commands, but not interpret any return values
    (unless you parse stdout).

    All stdout from the process is dumped to the current stdout. This includes the prompt (see below) and
    return values from functions: same as you'd see in a prompt!

    This is useful when you need finer control over another Python process, such as when running client-
    server tests.

    Usage:
        with test.remote( <script-name> ) as remote:
            remote.send( 'foo()' )
            ...
    Or possibly using try:
        remote = test.script( <script-name> )
        try:
            remote.start()
            remote.send( 'bar()' )
            ...
        finally:
            remote.stop()
    This way, proper "destruction" will occur, stop() will get called, and no hangups will occur.

    About the implementation:
    This takes advantage of the python interpreter's interactive mode (-i) after loading a script. This mode
    uses a default prompt (sys.ps1) that would normally be part of stdout. We don't usually want to see this.
    So, to remove, either the script has to have its own custom interactive loop (without -i) or we need to
    intercept the interactive mode and "cancel" the default prompt. The 'test' module does the latter when it
    sees '--nested' in the command-line.
    """

    class Error( RuntimeError ):
        """
        Raised when an exception is raised on the remote side, unexpectedly so not the same as CheckFailed
        """
        def __init__( self, message ):
            super().__init__( message )

    def __init__( self, script, interactive=True, name="remote", nested_indent="svr" ):
        self._script = script
        self._interactive = interactive
        self._name = name
        self._nested_indent = nested_indent
        self._cmd = nested_cmd( script, nested_indent=nested_indent, interactive=interactive )
        self._process = None
        self._thread = None
        self._status = None     # last return-code
        self._on_finish = None  # callback
        self._exception = None
        self._stdout = None

    def __enter__( self ):
        """
        Called when entering a 'with' statement. We automatically start and wait until ready:
        """
        self.start()
        return self

    def __exit__( self, exc_type, exc_value, traceback ):
        """
        Called when exiting scope of a 'with' statement, so we can properly clean up.
        NOTE: this effectively kills the process! If you want all output to be handled, use wait()
        """
        self.wait()

    def is_running( self ):
        return self._process and self._process.returncode is None or False

    def status( self ):
        return self._status

    def on_finish( self, callback ):
        self._on_finish = callback

    def _output_ready( self ):
        log.d( self._name, self._exception and 'raised an error' or 'is ready' )
        if self._events:
            event = self._events.pop(0)
            if event:
                event.set()
        else:
            # We raise the error here only as a last resort: we prefer handing it over to
            # the waiting thread on the event!
            self._raise_if_needed()

    def _output_reader( self ):
        """
        This is the worker function called from a thread to output the process stdout.
        It is in danger of HANGING UP our own process because readline blocks forever! To avoid this
        please follow the usage guidelines in the class notes.
        """
        nested_prefix = self._nested_indent and f'[{self._nested_indent}] ' or ''
        if not nested_prefix:
            x = -1
        missing_ready = None
        for line in iter( self._process.stdout.readline, '' ):
            # NOTE: line will include the terminating \n EOL
            # NOTE: so readline will return '' (with no EOL) when EOF is reached - the "sentinel"
            #       2nd argument to iter() - and we'll break out of the loop
            if line == '___ready\n':
                self._output_ready()
                continue
            if nested_prefix:
                x = line.find( nested_prefix )  # there could be color codes in the line
            if x < 0:
                if self._exception:
                    self._exception.append( line[:-1] )
                    # We cannot raise an error here -- it'll just exit the thread and not be
                    # caught in the main... Instead we have to wait until the remote is ready...
                elif line.startswith( 'Traceback '):
                    self._exception = [line[:-1]]
                elif line.startswith( '  File "' ):
                    # Some exception are syntax errors in the command, which would not have a 'Traceback'...
                    self._exception = [line[:-1]]
                if missing_ready and line.endswith( missing_ready ):
                    self._output_ready()
                    line = line[:-len(missing_ready)]
                    missing_ready = None
                    if not line:
                        continue
                if self._stdout is not None and not line.startswith( '-D-' ):
                    self._stdout.append( line[:-1] )
                else:
                    print( nested_prefix + line, end='', flush=True )
            else:
                if x > 0 and line[:x] == '___ready\n'[:x]:
                    missing_ready = '___ready\n'[x:]
                print( line, end='', flush=True )
        #
        log.d( self._name, 'stdout is finished' )
        self._terminate()
        if self._on_finish:
            self._on_finish( self._status )

    def start( self ):
        """
        Start the process
        """
        log.d( self._name, 'starting:', self._cmd )
        self._process = subprocess.Popen( self._cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True )
        self._thread = threading.Thread( target = remote._output_reader, args=(self,) )
        #
        # We allow waiting until the script is ready for input: see wait_until_ready()
        self._ready = threading.Event()
        self._events = [ self._ready ]
        #
        self._thread.start()

    def _raise_if_needed( self, on_fail=RAISE ):
        if self._exception:
            what = f'[{self._name}] ' + self._exception.pop()
            while self._exception and ( what.startswith( 'Invoked with:' ) or what.startswith( '  ' ) or what.startswith( '\n' )):
                what = self._exception.pop() + '\n  ' + what
            self._exception = None
            if on_fail == RAISE:
                raise remote.Error( what )
            print_stack_( traceback.format_stack()[:-2] )
            log.out( f'      {what}' )
            if on_fail == ABORT:
                self.wait()
                abort()
            if on_fail == LOG:
                pass
            else:
                raise ValueError( f'invalid failure handler "{how}" should be raise, abort, or log' )


    def wait_until_ready( self, timeout=10, on_fail=RAISE ):
        """
        The initial script can take a bit of time to load and run, and more if it does something "heavy". The user may want
        to wait until it's "ready" to take input...
        """
        if not self._ready.wait( timeout ):
            raise RuntimeError( f'{self._name} timed out' )
        self._raise_if_needed( on_fail=on_fail )
        if not self.is_running():
            raise RuntimeError( f'{self._name} exited with status {self._status}' )

    def run( self, command, timeout=5, on_fail=RAISE ):
        """
        Run a command asynchronously in the remote process
        :param command: the line, as if you typed it in an interactive shell
        :param timeout: if not None, how long to wait for the command to finish; otherwise returns immediately
        :param on_fail: how to handle exceptions on the server
                        'raise' to raise them as remote.Error()
                        'abort' to test.abort()
                        'log' to log and ignore
        """
        log.d( self._name, 'running:', command )
        assert self._interactive
        self._events.append( self._ready )
        self._ready.clear()
        self._process.stdin.write( command + '\n' )
        self._process.stdin.flush()
        if timeout:
            self.wait_until_ready( timeout=timeout, on_fail=on_fail )


    def capture_stdout( self ):
        """
        Start collecting stdout from the remote, if not already collecting it.
        Stdout collected will not be printed to our stdout, and will instead be returned in get_stdout()
        as a list of lines. Debug lines (-D-...) are filtered out.
        """
        if self._stdout is None:
            self._stdout = []


    def get_stdout( self, flush=True, stop=True, timeout=5 ):
        """
        Returns the stdout collected since capture_stdout(), as a list of lines.
        :param stop: if True, will stop collecting stdout
        :param flush: remote output may have not made it to us yet; if True, wait until we've seen everything
        """
        if flush:
            # Run empty command and wait until ready; will throw
            self.run( '', timeout=timeout )
        stdout = self._stdout
        if stop:
            self._stdout = None
        return stdout


    def wait( self, timeout=10 ):
        """
        Waits until all stdout has been consumed and the remote exited
        :param timeout: seconds before we stop waiting (default is big enough to be reasonable sure something
                        unusual is happening)
        :return: the exit status from the process
        """
        if self._thread and self._thread.is_alive():
            if self._interactive:
                self._process.stdin.write( 'exit()\n' )  # make sure we respond to it to avoid timeouts
                self._process.stdin.flush()
            if self._thread != threading.current_thread():
                log.d( 'waiting for', self._name, 'to finish...' )
                self._thread.join( timeout )
                if self._thread.is_alive():
                    log.d( self._name, 'waiting for thread join timed out after', timeout, 'seconds' )
        self._terminate()
        self._raise_if_needed()
        return self.status()

    def _terminate( self ):
        """
        Internal termination helper. The remote process will be killed.
        If you want to wait until it finishes and all stdout is consumed, use exit() or wait()
        """
        if self._process is not None:
            process = self._process
            self._process = None
            process.terminate()
            try:
                process.wait( timeout=0.2 )
                self._status = process.returncode
                log.d( self._name, 'exited with status', self._status )
            except subprocess.TimeoutExpired:
                log.d( self._name, 'process terminate timed out; no status' )
            finally:
                # Unexpected, but known to happen (process terminated while we're waiting for it to be ready)
                self._ready.set()

    def stop( self ):
        """
        Terminates the remote process.
        If you want to wait until it finishes and all stdout is consumed, use wait()
        """
        if self.is_running():
            log.d( 'stopping', self._name, 'process' )
            self._terminate()
            self._thread.join()
            self._thread = None


    class fork:
        """
        Using the remote is nice, but the two scripts live separately which could be annoying.
        Instead, we could do something similar to a 'fork' in Linux, e.g.:

        from rspy import log, test
        with test.remote.fork() as remote:
            if remote is None:
                log.i( "This is the forked code" )
                raise StopIteration()  # skip to the end of the with statement
            log.i( "This is the parent code" )
        """

        def __init__( self, script=None, **kwargs ):
            # We add a 'forked' to the command-line of the forked process
            if log.nested is not None:
                self._instance = None
            else:
                self._instance = remote( script or sys.argv[0], **kwargs )
                #self._instance._cmd += ['forked']

        def __enter__( self ):
            """
            Called when entering a 'with' statement. We automatically start and wait until ready:
            """
            if self._instance is not None:
                try:
                    self._instance.start()
                    self._instance.wait_until_ready()
                except:
                    if not self.__exit__( *sys.exc_info() ):
                        raise
            return self._instance  # return the remote; None if we're the fork

        def __exit__( self, exc_type, exc_value, traceback ):
            """
            Called when exiting scope of a 'with' statement, so we can properly clean up.
            """
            if exc_type is None:
                if self._instance is not None:
                    self._instance.wait()
            elif exc_type is StopIteration:
                # A StopIteration can be used to get out of the 'with' statement easily
                #
                # "If an exception is supplied, and the method wishes to suppress the exception (i.e.,
                # prevent it from being propagated), it should return a true value."
                # https://docs.python.org/3/reference/datamodel.html#with-statement-context-managers
                return True  # otherwise the exception will keep propagating
            elif self._instance is not None:
                self._instance.stop()
