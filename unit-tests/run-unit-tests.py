#!python3

# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

import sys, os, subprocess, re, platform, getopt, time

# Add our py/ module directory so we can find our own libraries
current_dir = os.path.dirname( os.path.abspath( __file__ ) )
sys.path.append( os.path.join( current_dir, 'py' ))

from rspy import log, file, repo, libci
from rspy.signals import register_signal_handlers

# Python's default list of paths to look for modules includes user-intalled. We want
# to avoid those to take only the pyrealsense2 we actually compiled!
#
# Rather than rebuilding the whole sys.path, we instead remove:
from site import getusersitepackages   # not the other stuff, like quit(), exit(), etc.!
#log.d( 'site packages=', getusersitepackages() )
#log.d( 'sys.path=', sys.path )
#log.d( 'removing', [p for p in sys.path if file.is_inside( p, getusersitepackages() )])
sys.path = [p for p in sys.path if not file.is_inside( p, getusersitepackages() )]
#log.d( 'modified=', sys.path )


def usage():
    ourname = os.path.basename( sys.argv[0] )
    print( 'Syntax: ' + ourname + ' [options] [dir]' )
    print( '        dir: location of executable tests to run' )
    print( 'Options:' )
    print( '        --debug              Turn on debugging information (does not include LibRS debug logs; see --rslog)' )
    print( '        -v, --verbose        Errors will dump the log to stdout' )
    print( '        -q, --quiet          Suppress output; rely on exit status (0=no failures)' )
    print( '        -s, --stdout         Do not redirect stdout to logs' )
    print( '        -r, --regex          Run all tests whose name matches the following regular expression' )
    print( '        --skip-regex         Skip all tests whose name matches the following regular expression' )
    print( '        -t, --tag            Run all tests with the following tag. If used multiple times runs all tests matching' )
    print( '                             all tags. e.g. -t tag1 -t tag2 will run tests who have both tag1 and tag2' )
    print( '                             tests automatically get tagged with \'exe\' or \'py\' and based on their location' )
    print( '                             inside unit-tests/, e.g. unit-tests/func/test-hdr.py gets [func, py]' )
    print( '        --live               Find only live tests (with test:device directives)' )
    print( '        --not-live           Find only tests that are NOT live (without test:device directives)' )
    print( '        --list-tags          Print out all available tags. This option will not run any tests' )
    print( '        --list-tests         Print out all available tests. This option will not run any tests' )
    print( '                             If both list-tags and list-tests are specified each test will be printed along' )
    print( '                             with its tags' )
    print( '        --no-exceptions      Do not load the LibCI/exceptions.specs file' )
    print( '        --context <>         The context to use for test configuration' )
    print( '        --repeat <#>         Repeat each test <#> times' )
    print( '        --retry <#>          Retry each test <#> times (unless test specified more)' )
    print( '        --config <>          Ignore test configurations; use the one provided' )
    print( '        --device <>          Run only on the specified devices; ignore any test that does not match (implies --live)' )
    print( '        --exclude-device <>  Exclude the specified devices from testing (space-separated list)' )
    print( '        --no-reset           Do not try to reset any devices, with or without a hub' )
    print( '        --hub-reset          If a hub is available, reset the hub itself' )
    print( '        --custom-fw-d400          If custom fw provided flash it if its different that the current fw installed' )
    print( '        --custom-fw-d555          If custom fw provided flash it if its different that the current fw installed' )
    print( '        --rslog              Enable LibRS logging (LOG_DEBUG etc.) to console in each test' )
    print( '        --skip-disconnected  Skip live test if required device is disconnected (only applies w/o a hub)' )
    print( '        --test-dir <>        Path to test dir; default: librealsense/unit-tests' )
    print( '                             ex: --test-dir $HOME/my_ws/my_tests; logs are stored in the same dir' )
    print( 'Examples:' )
    print( 'Running: python run-unit-tests.py -s' )
    print( '    Runs all tests, but direct their output to the console rather than log files' )
    print( 'Running: python run-unit-tests.py --list-tests --list-tags' )
    print( "    Will find all tests and print for each one what tags it has in the following format:" )
    print( '        <test-name> has tags: <tags-separated-by-spaces>' )
    print( 'Running: python run-unit-tests.py -r name -t log ~/my-build-directory' )
    print( "    Will run all tests whose name contains 'name' and who have the tag 'log' while searching for the" )
    print( "    exe files in the provided directory. Each test will create its own .log file to which its" )
    print( "    output will be written." )
    sys.exit( 2 )
    
# get os and directories for future use
# NOTE: WSL will read as 'Linux' but the build is Windows-based!
system = platform.system()
if system == 'Linux' and "microsoft" not in platform.release().lower():
    linux = True
else:
    linux = False

# Parse command-line:
try:
    opts, args = getopt.getopt( sys.argv[1:], 'hvqr:st:',
                                longopts=['help', 'verbose', 'debug', 'quiet', 'regex=', 'stdout', 'tag=', 'list-tags',
                                          'list-tests', 'no-exceptions', 'context=', 'repeat=', 'retry=', 'config=', 'no-reset', 'hub-reset',
                                          'rslog', 'skip-disconnected', 'live', 'not-live', 'device=', 'exclude-device=', 'test-dir=','skip-regex=','custom-fw-d400=','custom-fw-d555='] )
except getopt.GetoptError as err:
    log.e( err )  # something like "option -a not recognized"
    usage()
regex = None
to_stdout = False
required_tags = set()
list_tags = False
list_tests = False
no_exceptions = False
context = []
repeat = 1
retries = 0
forced_configurations = None
device_set = None
exclude_device_set = None
no_reset = False
hub_reset = False
skip_disconnected = False
custom_fw_path=''
custom_fw_d555_path=''
rslog = False
only_live = False
only_not_live = False
test_dir = current_dir
test_dir_log =  False
skip_regex = None
for opt, arg in opts:
    if opt in ('-h', '--help'):
        usage()
    elif opt in ('-v', '--verbose'):
        log.verbose_on()
    elif opt in ('-q', '--quiet'):
        log.quiet_on()
    elif opt in ('-r', '--regex'):
        regex = arg
    elif opt in ('-s', '--stdout'):
        to_stdout = True
    elif opt in ('-t', '--tag'):
        required_tags.add( arg )
    elif opt == '--list-tags':
        list_tags = True
    elif opt == '--list-tests':
        list_tests = True
    elif opt == '--no-exceptions':
        no_exceptions = True
    elif opt == '--context':
        context = arg.split()  # list of contexts
    elif opt == '--repeat':
        if not arg.isnumeric()  or  int(arg) < 1:
            log.e( "--repeat must be a number greater than 0" )
            usage()
        repeat = int(arg)
    elif opt == '--retry':
        if not arg.isnumeric()  or  int(arg) < 0:
            log.e( "--retry must be a number greater than or equal to 0" )
            usage()
        retries = int(arg)
    elif opt == '--config':
        forced_configurations = [[arg]]
    elif opt == '--device':
        if only_not_live:
            log.e( "--device and --not-live are mutually exclusive" )
            usage()
        only_live = True
        device_set = arg.split()
    elif opt == '--exclude-device':
        exclude_device_set = arg.split()
    elif opt == '--no-reset':
        no_reset = True
    elif opt == '--hub-reset':
        hub_reset = True
    elif opt == '--rslog':
        rslog = True
    elif opt == '--skip-disconnected':
        skip_disconnected = True
    elif opt == '--live':
        if only_not_live:
            log.e( "--live and --not-live are mutually exclusive" )
            usage()
        only_live = True
    elif opt == '--not-live':
        if only_live:
            log.e( "--live and --not-live are mutually exclusive" )
            usage()
        only_not_live = True
    elif opt == '--test-dir':
        test_dir = os.path.abspath(arg)
        libci.unit_tests_dir = test_dir
        log.i(f'Tests dir changed from default to: {test_dir}')
        test_dir_log = True
    elif opt in ('--skip-regex'):
        skip_regex = arg
    elif opt == '--custom-fw-d400':
        custom_fw_path = arg  # Store the custom firmware path
        log.i(f"custom D400 firmware path was provided ${custom_fw_path}")
    elif opt == '--custom-fw-d555':
        custom_fw_d555_path = arg  # Store the custom D555 firmware path
        log.i(f"custom D555 firmware path was provided ${custom_fw_d555_path}")

def find_build_dir( dir ):
    """
    Given a directory we know must be within the build tree, go up the tree until we find
    a file we know must be in the root build directory...

    :return: the build directory if found, or None otherwise
    """
    build_dir = dir
    while True:
        if os.path.isfile( os.path.join( build_dir, 'CMakeCache.txt' )):
            log.d( 'assuming build dir path:', build_dir )
            return build_dir
        base = os.path.dirname( build_dir )
        if base == build_dir:
            log.d( 'could not find CMakeCache.txt; cannot assume build dir from', dir )
            break
        build_dir = base

if len( args ) > 1:
    usage()
exe_dir = None                 # the directory in which we expect to find exes
if len( args ) == 1:
    exe_dir = args[0]
    if not os.path.isdir( exe_dir ):
        log.f( 'Not a directory:', exe_dir )
    build_dir = find_build_dir( exe_dir )
else:
    build_dir = repo.build    # may not actually contain exes
    #log.d( 'repo.build:', build_dir )

# Python scripts should be able to find the pyrealsense2 .pyd or else they won't work. We don't know
# if the user (Travis included) has pyrealsense2 installed but even if so, we want to use the one we compiled.
# we search the librealsense repository for the .pyd file (.so file in linux)
pyrs = ""
pyd_dirs = set()
pyrs_search_dir = exe_dir or build_dir or repo.root
for pyd in file.find( pyrs_search_dir, linux and r'.*python.*\.so$' or r'(^|/)py.*\.pyd$' ):
    if re.search( r'(^|/)pyrealsense2', pyd ):
        if pyrs:
            raise RuntimeError( f'found more than one possible pyrealsense2!\n    previous: {pyrs}\n    and:      {pyd}' )
        pyrs = pyd
    # The path is relative; make it absolute so it can be found by tests
    pyd_dirs.add( os.path.dirname( os.path.join( pyrs_search_dir, pyd )))
pyrs_path = None
if pyrs:
    # The path is relative; make it absolute and add to PYTHONPATH so it can be found by tests
    pyrs_path = os.path.join( pyrs_search_dir, pyrs )
    # We need to add the directory not the file itself
    pyrs_path = os.path.dirname( pyrs_path )
elif len(pyd_dirs) == 1:
    # Maybe we found other libraries, like pyrsutils?
    log.d( 'did not find pyrealsense2' )
    pyrs_path = next(iter(pyd_dirs))
if pyrs_path:
    log.d( 'found python libraries in:', pyd_dirs )
    if not exe_dir:
        build_dir = find_build_dir( pyrs_path )
        exe_dir = pyrs_path

# Try to assume exe directory from inside build directory. Only works if there is only one location with tests
if not exe_dir and build_dir:
    mask = r'(^|/)test-[^/.]*'
    if linux:
        mask += r'$'
    else:
        mask += r'\.exe'
    for executable in file.find( build_dir, mask ):
        executable = os.path.join( build_dir, executable )
        #log.d( 'found exe=', executable )
        if not file.is_executable( executable ):
            continue
        dir_with_test = os.path.dirname( executable )
        if exe_dir and exe_dir != dir_with_test:
            log.f( "Ambiguous executable tests in 2 directories:\n\t", exe_dir, "\n\t", dir_with_test,
                    "\n\tSpecify the directory manually..." )
        exe_dir = dir_with_test

if not to_stdout:
    # If no test executables were found, put the logs directly in the build directory
    if not test_dir_log:
        logdir = os.path.join( exe_dir or build_dir or os.path.join( repo.root, 'build' ), 'unit-tests' )
    else:
        logdir = os.path.join( test_dir, 'logs' )
    os.makedirs( logdir, exist_ok=True )
    libci.logdir = logdir
        
n_tests = 0
n_failed_tests = 0

# Figure out which sys.path we want the tests to see, assuming we have Python tests
# PYTHONPATH is what Python will ADD to sys.path for child processes BEFORE any standard python paths
# (We can simply change `sys.path` but any child python scripts won't see it; we change the environment instead)
#
os.environ["PYTHONPATH"] = os.path.join( current_dir, 'py' )
#
for dir in pyd_dirs:
    os.environ["PYTHONPATH"] += os.pathsep + dir


def serial_numbers_to_string( sns ):
    return ' '.join( [f'{devices.get(sn).name}_{sn}' for sn in sns] )



def configuration_str( configuration, repetition=0, retry=0, sns=None, prefix='', suffix='' ):
    """ Return a string repr (with a prefix and/or suffix) of the configuration or '' if it's None """
    s = ''
    if configuration is not None:
        s += '[' + ' '.join( configuration )
        if sns is not None:
            s += ' -> ' + serial_numbers_to_string( sns )
        s += ']'
    elif sns is not None:
        s += '[' + serial_numbers_to_string( sns ) + ']'
    if repetition:
        s += f'[rep {repetition+1}]'
    if retry:
        s += f'[retry {retry}]'
    if s:
        s = prefix + s + suffix
    return s


def check_log_for_fails( path_to_log, testname, configuration=None, repetition=1, sns=None ):
    # Normal logs are expected to have in last line:
    #     "All tests passed (11 assertions in 1 test case)"
    # Tests that have failures, however, will show:
    #     "test cases: 1 | 1 failed
    #      assertions: 9 | 6 passed | 3 failed"
    # We make sure we look at the log written by the last run of the test by ignoring anything before the last
    # line with "----...---" that separate between 2 separate runs of he test
    if path_to_log is None:
        return False
    results = None
    for ctx in file.grep( r'test cases:\s*(\d+) \|\s*(\d+) (passed|failed)|^----------TEST-SEPARATOR----------$',
                          path_to_log ):
        m = ctx['match']
        if m.string == "----------TEST-SEPARATOR----------":
            results = None
        else:
            results = m

    if not results:
        return False

    total = int( results.group( 1 ) )
    passed = int( results.group( 2 ) )
    if results.group( 3 ) == 'failed':
        # "test cases: 1 | 1 failed"
        passed = total - passed
    if passed < total:
        if total == 1 or passed == 0:
            desc = 'failed'
        else:
            desc = str( total - passed ) + ' of ' + str( total ) + ' failed'

        if log.is_verbose_on():
            log.e( log.red + testname + log.reset + ': ' + configuration_str( configuration, repetition, suffix=' ', sns=sns ) + desc )
            log.i( 'Log: >>>' )
            log.out()
            file.cat( path_to_log )
            log.out( '<<<' )
        else:
            log.e( log.red + testname + log.reset + ': ' + configuration_str( configuration, repetition, sns=sns,
                                                                              suffix=' ' ) + desc + '; see ' + path_to_log )
        return True
    return False


def get_tests():
    global regex, build_dir, exe_dir, pyrs, test_dir, linux, context, list_only, skip_regex
    if regex:
        run_pattern = re.compile( regex )

    if skip_regex:
        skip_pattern = re.compile( skip_regex )

    # In Linux, the build targets are located elsewhere than on Windows
    # Go over all the tests from a "manifest" we take from the result of the last CMake
    # run (rather than, for example, looking for test-* in the build-directory):
    manifestfile = os.path.join( build_dir, 'CMakeFiles', 'TargetDirectories.txt' )
    if os.path.isfile( manifestfile ) and exe_dir:
        # log.d( manifestfile )
        for manifest_ctx in file.grep( r'(?<=unit-tests/build/)\S+(?=/CMakeFiles/test-\S+.dir$)', manifestfile ):
            # We need to first create the test name so we can see if it fits the regex
            testdir = manifest_ctx['match'].group( 0 )  # "log/internal/test-all"
            # log.d( testdir )
            testparent = os.path.dirname( testdir )  # "log/internal"
            if testparent:
                testname = 'test-' + testparent.replace( '/', '-' ) + '-' + os.path.basename( testdir )[
                                                                            5:]  # "test-log-internal-all"
            else:
                testname = testdir  # no parent folder so we get "test-all"

            if regex and not run_pattern.search( testname ):
                continue
            
            if skip_regex and skip_pattern.search( testname ):
                continue

            exe = os.path.join( exe_dir, testname )
            if linux:
                if not os.path.isfile( exe ):
                    exe = os.path.join( build_dir, 'unit-tests', 'build', testdir, testname )
            else:
                exe += '.exe'

            yield libci.ExeTest( testname, exe, context )
    elif list_only:
        # We want to list all tests, even if they weren't built.
        # So we look for the source files instead of using the manifest
        for cpp_test in file.find( test_dir, r'(^|/)test-.*\.cpp' ):
            testparent = os.path.dirname( cpp_test )  # "log/internal" <-  "log/internal/test-all.py"
            if testparent:
                testname = 'test-' + testparent.replace( '/', '-' ) + '-' + os.path.basename( cpp_test )[
                                                                            5:-4]  # remove .cpp
            else:
                testname = os.path.basename( cpp_test )[:-4]

            if regex and not run_pattern.search( testname ):
                continue

            if skip_regex and skip_pattern.search( testname ):
                continue

            yield libci.ExeTest( testname, context = context )

    # Python unit-test scripts are in the same directory as us... we want to consider running them
    # (we may not if they're live and we have no pyrealsense2.pyd):
    for py_test in file.find( test_dir, r'(^|/)test-.*\.py' ):
        testparent = os.path.dirname( py_test )  # "log/internal" <-  "log/internal/test-all.py"
        if testparent:
            testname = 'test-' + testparent.replace( '/', '-' ) + '-' + os.path.basename( py_test )[5:-3]  # remove .py
        else:
            testname = os.path.basename( py_test )[:-3]

        if regex and not run_pattern.search( testname ):
            continue

        if skip_regex and skip_pattern.search( testname ):
            continue

        yield libci.PyTest( testname, py_test, context )


def prioritize_tests( tests ):
    return sorted( tests, key=lambda t: t.config.priority )


def devices_by_test_config( test, exceptions ):
    """
    Yield <configuration,serial-numbers> pairs for each valid configuration under which the
    test should run.

    The <configuration> is a list of ('test:device') designations, e.g. ['L500*', 'D415'].
    The <serial-numbers> is a set of device serial-numbers that fit this configuration.

    :param test: The test (of class type Test) we're interested in
    """
    global forced_configurations, device_set
    
    # Special handling for multi-device tests with wildcard configuration - enable all ports
    if (test.config.multi_device and 
        (forced_configurations is None) and 
        test.config.configurations and 
        test.config.configurations[0] == ['*', '*']):
        log.d( f'{test.name}: multi-device test with wildcard detected - enabling all ports and using all devices' )
        if devices.hub:
            devices.enable_all()
            import time
            time.sleep(2)  # Give devices time to enumerate
        all_devices = set(devices.enabled())
        if device_set:
            all_devices = all_devices.intersection(device_set)
        # Multi-device tests require at least 2 devices
        if len(all_devices) >= 2:
            # Return a single configuration with all devices
            yield ['*', '*'], all_devices
        elif len(all_devices) == 1:
            log.w( log.yellow + test.name + log.reset + ': multi-device test requires at least 2 devices, but only 1 found' )
        # If no devices or insufficient devices, don't yield anything (test will be skipped)
        return
    
    for configuration in ( forced_configurations  or  test.config.configurations ):
        try:
            for serial_numbers in devices.by_configuration( configuration, exceptions, device_set ):
                if not serial_numbers:
                    log.d( 'configuration:', configuration_str( configuration ), 'has no matching device; ignoring' )
                else:
                    yield configuration, serial_numbers
        except RuntimeError as e:
            if devices.hub:
                log.e( log.red + test.name + log.reset + ': ' + str( e ) )
            else:
                log.w( log.yellow + test.name + log.reset + ': ' + str( e ) )
            continue


def test_wrapper_( test, configuration=None, repetition=1, curr_retry=0, max_retry = 0, sns=None ):
    global rslog
    #
    if not log.is_debug_on():
        conf_str = configuration_str( configuration, repetition, retry=curr_retry, prefix='  ', sns=sns )
        log.i( f'Running {test.name}{conf_str}' )
    #
    log_path = test.get_log()
    #
    opts = []
    if rslog:
        opts.append( '--rslog' )
    if test.name == "test-fw-update" and custom_fw_path:
        opts.append('--custom-fw-d400')
        opts.append(custom_fw_path)
    if test.name == "test-fw-update" and custom_fw_d555_path:
        opts.append('--custom-fw-d555')
        opts.append(custom_fw_d555_path)
    try:
        test.run_test( configuration = configuration, log_path = log_path, opts = opts )
    except FileNotFoundError as e:
        log.e( log.red + test.name + log.reset + ':', str( e ) + configuration_str( configuration, repetition, prefix=' ' ) )
    except subprocess.TimeoutExpired:
        log.e( log.red + test.name + log.reset + ':', configuration_str( configuration, repetition, suffix=' ' ) + 'timed out' )
    except subprocess.CalledProcessError as cpe:
        # An unexpected error occurred, if there are no more retries issue error
        if curr_retry == max_retry:
            if not check_log_for_fails( log_path, test.name, configuration, repetition, sns=sns ):
                # check_log_for_fails logs a more verbose message, but if it fails to do so log a general message here.
                log.e( log.red + test.name + log.reset + ':',
                       configuration_str( configuration, repetition, suffix=' ' ) + 'exited with non-zero value (' +
                           str( cpe.returncode ) + ')' )
    else:
        return True
    return False


def test_wrapper( test, configuration=None, repetition=1, serial_numbers=None ):
    global n_tests, n_failed_tests, retries
    n_tests += 1
    retry_count = max(test.config.retries, retries)
    for retry in range( retry_count + 1 ):
        if retry:
            if log.is_debug_on():
                log.debug_unindent()  # just to make it stand out a little more
                log.d( f'  Failed; retry #{retry}' )
                log.debug_indent()
            if no_reset or not serial_numbers:
                time.sleep(1)  # small pause between tries
            else:
                devices.enable_only( serial_numbers, recycle=True )
        if test_wrapper_( test, configuration, repetition, retry, retry_count, serial_numbers ):
            return True

    n_failed_tests += 1
    return False


def close_hubs():
    #
    # Disconnect from the hub -- if we don't it might crash on Linux...
    # Before that we close all ports, no need for cameras to stay on between LibCI runs
    if not list_only and not only_not_live:
        if devices.hub and devices.hub.is_connected():
            log.d("disconnecting from hub(s)")
            devices.hub.disable_ports()
            devices.wait_until_all_ports_disabled()
            devices.hub.disconnect()

# Run all tests
try:
    list_only = list_tags or list_tests
    if only_not_live:
        log.d( 'Only --not-live tests running; skipping device discovery' )
    elif not list_only:
        log.progress( '-I- Discovering devices ...' )
        if pyrs:
            sys.path.insert( 1, pyrs_path )  # Make sure we pick up the right pyrealsense2!
        from rspy import devices
        register_signal_handlers(close_hubs)
        disable_dds = "dds" not in context
        devices.query( hub_reset = hub_reset, disable_dds = disable_dds, rslog = rslog ) #resets the device
        devices.map_unknown_ports()
        #
        # Under a development environment (i.e., without a hub), we may only have one device connected
        # or even none and want to only show a warning for live tests:
        skip_live_tests = len( devices.all() ) == 0 and not devices.hub
        #
        exceptions = None
        if not skip_live_tests:
            if not no_exceptions and os.path.isfile( libci.exceptionsfile ):
                try:
                    log.d( 'loading device exceptions from:', libci.exceptionsfile )
                    log.debug_indent()
                    exceptions = devices.load_specs_from_file( libci.exceptionsfile )
                    exceptions = devices.expand_specs( exceptions )
                    log.d( '==>', exceptions )
                finally:
                    log.debug_unindent()
        #
        if device_set is not None:
            sns = set()  # convert the list of specs to a list of serial numbers
            ignored_list = list()
            for spec in device_set:
                included_devices = [sn for sn in devices.by_spec( spec, ignored_list )]
                if not included_devices:
                    log.f( f'No match for --device "{spec}"' )
                sns.update( included_devices )
            device_set = sns
            log.d( f'ignoring devices other than: {serial_numbers_to_string( device_set )}' )
        #
        if exclude_device_set is not None:
            excluded_sns = set()  # convert the list of exclude specs to a list of serial numbers
            ignored_list = list()
            for spec in exclude_device_set:
                excluded_devices = [sn for sn in devices.by_spec( spec, ignored_list )]
                excluded_sns.update( excluded_devices )
            
            if excluded_sns:
                log.d( f'excluding devices: {serial_numbers_to_string( excluded_sns )}' )
                if device_set is not None:
                    # Remove excluded devices from the included device set
                    device_set = device_set - excluded_sns
                    if not device_set:
                        log.f( 'All specified devices were excluded; no devices left to test' )
                    log.d( f'final device set after exclusions: {serial_numbers_to_string( device_set )}' )
                else:
                    # If no device_set was specified, we need to discover all devices and exclude the specified ones
                    all_devices = set(devices.all())
                    device_set = all_devices - excluded_sns
                    if not device_set:
                        log.f( 'All discovered devices were excluded; no devices left to test' )
                    else:
                        log.d( f'using all devices except excluded ones: {serial_numbers_to_string( device_set )}' )
        #
        log.progress()
    #
    # Automatically detect github actions based on environment variable
    #     see https://docs.github.com/en/actions/learn-github-actions/variables
    # We must do this before calculating the tests otherwise the context cannot be used in
    # directives...
    if 'gha' not in context:
        if os.environ.get( 'GITHUB_ACTIONS' ):
            context.append( 'gha' )
    #
    log.reset_errors()
    available_tags = set()
    tests = []
    failed_tests = []
    if context:
        log.i( 'Running under context:', context )
    if not to_stdout:
        log.i( 'Logs in:', libci.logdir )
    for test in prioritize_tests( get_tests() ):
        try:
            #
            if only_live and not test.is_live():
                log.d( f'{test.name} is not live; skipping' )
                continue
            if only_not_live and test.is_live():
                log.d( f'{test.name} is live; skipping' )
                continue
            #
            if test.config.donotrun:
                log.d( f'{test.name} is marked do-not-run; skipping' )
                continue
            #
            unfit_tags = []
            for tag in required_tags:
                if tag.startswith('!'):
                    if tag[1:] in test.config.tags:
                        unfit_tags.append( tag )
                elif tag not in test.config.tags:
                    unfit_tags.append( tag )
            if unfit_tags:
                log.d( f'{test.name} has {test.config.tags} which do not fit --tag {unfit_tags}; skipping' )
                continue
            #
            if 'Windows' in test.config.flags and linux:
                log.d( f'{test.name} has Windows flag and OS is Linux; skipping' )
                continue
            if 'Linux' in test.config.flags and not linux:
                log.d( f'{test.name} has Linux flag and OS is Windows; skipping' )
                continue
            #
            if to_stdout and not list_only:
                log.split()
            log.d( 'found', test.name, '...' )
            log.debug_indent()
            test.debug_dump()
            #
            available_tags.update( test.config.tags )
            tests.append( test )
            if list_only:
                n_tests += 1
                continue
            #
            if not test.is_live():
                test_ok = True
                for repetition in range(repeat):
                    test_ok = test_wrapper( test, repetition = repetition ) and test_ok
                if not test_ok:
                    failed_tests.append( test )
                continue
            #
            if skip_live_tests:
                if skip_disconnected:
                    log.w( test.name + ':', 'is live & no cameras were found; skipping due to --skip-disconnected' )
                else:
                    log.e( test.name + ':', 'is live and there are no cameras' )
                continue
            #
            test_ok = True
            for configuration, serial_numbers in devices_by_test_config( test, exceptions ):
                # Currently, with all of our tests, serial_numbers holds a single serial number
                # We will see multiple devices on serial_numbers only if the test specifies multiple devices in a
                # single line. For example: "test:device D435 D455" will require both devices simultaneity

                skip_test = False
                for sn in serial_numbers:
                    conn_type = devices.get(sn).connection_type.lower()
                    excluded_connections = [ t[1:].lower() for t in test.config.types if t.startswith('!') ]
                    required_connections = [ t.lower() for t in test.config.types if not t.startswith('!') ]
                    if conn_type in excluded_connections:
                        skip_test = True
                        break
                    if required_connections and conn_type not in required_connections:
                        skip_test = True
                        break

                if skip_test:
                    log.d( f'connection type does not fit {test.config.types}; skipping' )
                    continue

                for repetition in range(repeat):
                    try:
                        log.d( 'configuration:', configuration_str( configuration, repetition, sns=serial_numbers ) )
                        log.debug_indent()
                        should_reset = not no_reset
                        devices.enable_only( serial_numbers, recycle=should_reset )
                    except RuntimeError as e:
                        log.w( log.red + test.name + log.reset + ': ' + str( e ) )
                    else:
                        register_signal_handlers()
                        test_ok = test_wrapper( test, configuration, repetition, serial_numbers ) and test_ok
                    finally:
                        log.debug_unindent()
            if not test_ok:
                failed_tests.append( test )
            #
        finally:
            log.debug_unindent()

    if to_stdout and not list_only:
        log.split()
    log.progress()
    #
    if not n_tests:
        log.f( 'No unit-tests found!' )
    #
    if list_only:
        if list_tags and list_tests:
            for t in sorted( tests, key= lambda x: x.name ):
                print( t.name, "has tags:", ' '.join( t.config.tags ) )
        #
        elif list_tags:
            for t in sorted( list( available_tags ) ):
                print( t )
        #
        elif list_tests:
            for t in sorted( tests, key= lambda x: x.name ):
                print( t.name )
    #
    else:
        if failed_tests:
            log.out( log.red + str( n_failed_tests ) + log.reset, 'of', n_tests, 'test(s)',
                     log.red + 'failed!' + log.reset + log.clear_eos )
            log.d( 'Failed tests:\n    ' + '\n    '.join( [test.name for test in failed_tests] ))
            sys.exit( 1 )
        #
        log.out( str( n_tests ) + ' unit-test(s) completed successfully' + log.clear_eos )
#
finally:
    close_hubs()
#
sys.exit( 0 )
