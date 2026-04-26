# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

import re, os, subprocess, time, sys, platform
from abc import ABC, abstractmethod

from rspy import log, file

# this script is in unit-test/py/rspy
unit_tests_dir = os.path.dirname( os.path.dirname( os.path.dirname( os.path.abspath( __file__ ) ) ) )
# the full path to the directory that should hold the unit-tests logs. It is updated in run-unit-tests when we know
# the target directory. If None we assume the output should go to stdout
logdir = None

# LibCI needs a directory in which to look for a configuration and/or collaterals: we call that our "home"
# This is always a directory called "LibCI", but may be in different locations, in this order of priority:
#     1. C:\LibCI  (on the LibCI machine)
#     2. ~/LibCI   (in Windows, ~ is likely C:\Users\<username>)
if platform.system() == 'Linux':
    home = '/usr/local/lib/ci'
else:
    home = 'C:\\LibCI'
if not os.path.isdir( home ):
    home = os.path.normpath( os.path.expanduser( '~/LibCI' ))
#
# Configuration (git config format) is kept in this file:
configfile = home + os.sep + 'configfile'
#
# And exceptions for configuration specs are stored here:
exceptionsfile = home + os.sep + 'exceptions.specs'


def run( cmd, stdout = None, timeout = 200, append = False ):
    """
    Wrapper function for subprocess.run.
    If the child process times out or ends with a non-zero exit status an exception is raised!

    :param cmd: the command and argument for the child process, as a list
    :param stdout: path of file to direct the output of the process to (None to disable)
    :param timeout: number of seconds to give the process before forcefully ending it (None to disable)
    :param append: if True and stdout is not None, the log of the test will be appended to the file instead of
                   overwriting it
    :return: the output written by the child, if stdout is None -- otherwise N/A
    """
    log.d( 'running:', cmd )
    handle = None
    start_time = time.time()
    try:
        log.debug_indent()
        if stdout is None:
            sys.stdout.flush()
        elif stdout and stdout != subprocess.PIPE:
            if append:
                handle = open( stdout, "a" )
                handle.write(
                    "\n----------TEST-SEPARATOR----------\n\n" )
                handle.flush()
            else:
                handle = open( stdout, "w" )
            stdout = handle
        rv = subprocess.run( cmd,
                             stdout=stdout,
                             stderr=subprocess.STDOUT,
                             universal_newlines=True,
                             timeout=timeout,
                             check=True )
        result = rv.stdout
        if not result:
            result = []
        else:
            result = result.split( '\n' )
        return result
    finally:
        if handle:
            handle.close()
        log.debug_unindent()
        run_time = time.time() - start_time
        log.d( "test took", run_time, "seconds" )


class TestConfig( ABC ):  # Abstract Base Class
    """
    Configuration for a test, encompassing any metadata needed to control its run, like retries etc.
    """

    def __init__( self, context ):
        self._configurations = list()
        self._priority = 1000
        self._tags = set()
        self._types = set()  # usage: test:type <type> or test:type !<type>
        self._flags = set()
        self._timeout = 200
        self._retries = 0
        self._context = context
        self._donotrun = False

    def debug_dump( self ):
        if self._donotrun:
            log.d( 'THIS TEST WILL BE SKIPPED (donotrun specified)' )
        if self._priority != 1000:
            log.d( 'priority:', self._priority )
        if self._timeout != 200:
            log.d( 'timeout:', self._timeout )
        if self._retries != 0:
            log.d( 'retries:', self._retries )
        if len( self._tags ) > 1:
            log.d( 'tags:', self._tags )
        if self._flags:
            log.d( 'flags:', self._flags )
        if len( self._configurations ) > 1:
            log.d( len( self._configurations ), 'configurations' )
            # don't show them... they are output separately

    @property
    def configurations( self ):
        return self._configurations

    @property
    def priority( self ):
        return self._priority

    @property
    def timeout( self ):
        return self._timeout

    @property
    def retries( self ):
        return self._retries

    @property
    def tags( self ):
        return self._tags

    @property
    def types( self ):
        return self._types

    @property
    def flags( self ):
        return self._flags

    @property
    def context( self ):
        return self._context

    @property
    def donotrun( self ):
        return  self._donotrun

class TestConfigFromText( TestConfig ):
    """
    Configuration for a test -- from any text-based syntax with a given prefix, e.g. for python:
        #test:usb2
        #test:device L500* D400*
        #test:device D400_CAM_SYNC    # requires 2x D400 devices (excludes D405)
        #test:retries 3
        #test:priority 0
    And, for C++ the prefix could be:
        //#test:...
    """

    def __init__( self, source, line_prefix, context ):
        """
        :param source: The absolute path to the text file
        :param line_prefix: A regex to denote a directive (must be first thing in a line), which will
            be immediately followed by the directive itself and optional arguments
        :param context: context in which to configure the test
        """
        TestConfig.__init__( self, context )

        self.derive_config_from_text( source, line_prefix )
        self.derive_tags_from_path( source )

    def derive_config_from_text( self, source, line_prefix ):
        # Configuration is made up of directives:
        #     #test:<directive>[:[!]<context>] <param>*
        # If a context is not specified, the directive always applies. Any directive with a context
        # will only get applied if we're running under the context it specifies (! means not, so
        # !nightly means when not under nightly).
        regex  = r'^' + line_prefix
        regex += r'([^\s:]+)'          # 1: directive
        regex += r'(?::(\S+))?'        # 2: optional context
        regex += r'((?:\s+\S+)*?)'     # 3: params
        regex += r'\s*(?:#\s*(.*))?$'  # 4: optional comment
        for line in file.grep( regex, source ):
            match = line['match']
            directive = match.group( 1 )
            directive_context = match.group( 2 )
            text_params = match.group( 3 ).strip()
            params = [s for s in text_params.split()]
            comment = match.group( 4 )
            if directive_context:
                not_context = directive_context.startswith('!')
                if not_context:
                    directive_context = directive_context[1:]
                # not_context | directive_ctx in context | RESULT
                # ----------- | ------------------------ | ------
                #      0      |            0             | IGNORE
                #      0      |            1             | USE
                #      1      |            0             | USE
                #      1      |            1             | IGNORE
                if not_context == (self._context and directive_context in self._context):
                    # log.d( "directive", line['line'], "ignored because of context mismatch with running context",
                    #       self._context)
                    continue
            if directive == 'device':
                # log.d( '    configuration:', params )
                params_lower_list = text_params.lower().split()
                if not params:
                    log.e( source + '+' + str( line['index'] ) + ': device directive with no devices listed' )
                elif sum(s.startswith('each(') for s in params_lower_list) > 1:
                    log.e( source + '+' + str(
                            line['index'] ) + ': each() cannot be used multiple times in same line', params )
                elif params_lower_list[0].startswith('each('):
                    if not re.fullmatch( r'each\(.+\)', params_lower_list[0], re.IGNORECASE ):
                        log.e( source + '+' + str( line['index'] ) + ': invalid \'each\' syntax:', params )
                    else:
                        for param in params_lower_list[1:]:
                            if not param.startswith("!"):
                                log.e(source + '+' + str(line['index']) + ': invalid syntax:', params,
                                      '. All device names after \'' + params[0] +
                                      '\' must start with \'!\' in order to skip them')
                                break
                        else:
                            self._configurations.append( params )
                else:
                    self._configurations.append( params )
            elif directive == 'priority':
                if len( params ) == 1 and params[0].isdigit():
                    self._priority = int( params[0] )
                else:
                    log.e( source + '+' + str( line['index'] ) + ': priority directive with invalid parameters:',
                           params )
            elif directive == 'timeout':
                if len( params ) == 1 and params[0].isdigit():
                    self._timeout = int( params[0] )
                else:
                    log.e( source + '+' + str( line['index'] ) + ': timeout directive with invalid parameters:',
                           params )
            elif directive == 'retries':
                if len( params ) == 1 and params[0].isdigit():
                    self._retries = int( params[0] )
                else:
                    log.e( source + '+' + str( line['index'] ) + ': timeout directive with invalid parameters:',
                           params )
            elif directive == 'tag':
                self._tags.update( map( str.lower, params ))  # tags are case-insensitive
            elif directive == 'type':
                self._types.update( map( str.lower, params ))
            elif directive == 'flag':
                self._flags.update( params )
            elif directive == 'donotrun':
                if params:
                    log.e( source + '+' + str( line['index'] ) + ': donotrun directive should not have parameters:',
                           params )
                self._donotrun = True
            else:
                log.e( source + '+' + str( line['index'] ) + ': invalid directive "' + directive + '"; ignoring' )

    def derive_tags_from_path( self, source ):
        # we need the relative path starting at the unit-tests directory
        relative_path = re.split( r"[/\\]unit-tests[/\\]", source )[-1]
        sub_dirs = re.split( r"[/\\]", relative_path )[:-1] # last element will be the name of the test
        self._tags.update( sub_dirs )


class TestConfigFromCpp( TestConfigFromText ):
    def __init__( self, source, context ):
        TestConfigFromText.__init__( self, source, r'//#\s*test:', context )
        self._tags.add( 'exe' )


class TestConfigFromPy( TestConfigFromText ):
    def __init__( self, source, context ):
        TestConfigFromText.__init__( self, source, r'#\s*test:', context )
        self._tags.add( 'py' )


class Test( ABC ):  # Abstract Base Class
    """
    Abstract class for a test. Holds the name of the test
    """

    def __init__( self, testname ):
        # log.d( 'found', testname )
        self._name = testname
        self._config = None
        self._ran = False

    @abstractmethod
    def run_test( self, configuration = None, log_path = None, opts = set() ):
        pass

    def debug_dump( self ):
        if self._config:
            self._config.debug_dump()

    @property
    def config( self ):
        return self._config

    @property
    def name( self ):
        return self._name

    @property
    def ran( self ):
        return self._ran

    def get_log( self ):
        global logdir
        if not logdir:
            path = None
        else:
            path = logdir + os.sep + self.name + ".log"
        return path

    def is_live( self ):
        """
        Returns True if the test configurations specify devices (test has a 'device' directive)
        """
        return self._config and len( self._config.configurations ) > 0

    def find_source_path( self ):
        """
        :return: The relative path from unit-tests directory to the test's source file (cpp or py). If the source
                 file is not found None will be returned
        """
        # TODO: this is limited to a structure in which .cpp files and directories do not share names
        # For example:
        #     unit-tests/
        #         func/
        #             ...
        #         test-func.cpp
        # test-func.cpp will not be found!

        global unit_tests_dir
        split_testname = self.name.split( '-' )
        path = unit_tests_dir
        relative_path = ""
        found_test_dir = False

        while not found_test_dir:
            # index 0 should be 'test' as tests always start with it
            found_test_dir = True
            for i in range( 2,
                            len( split_testname ) ):  # Checking if the next part of the test name is a sub-directory
                possible_sub_dir = '-'.join( split_testname[1:i] )  # The next sub-directory could have several words
                sub_dir_path = path + os.sep + possible_sub_dir
                if os.path.isdir( sub_dir_path ):
                    path = sub_dir_path
                    relative_path += possible_sub_dir + os.sep
                    del split_testname[1:i]
                    found_test_dir = False
                    break

        path += os.sep + '-'.join( split_testname )
        relative_path += '-'.join( split_testname )
        if os.path.isfile( path + ".cpp" ):
            relative_path += ".cpp"
        elif os.path.isfile( path + ".py" ):
            relative_path += ".py"
        else:
            log.w( log.red + self.name + log.reset + ':',
                   'No matching .cpp or .py file was found; no configuration will be used!' )
            return None

        return relative_path


class PyTest( Test ):
    """
    Class for python tests. Hold the path to the script of the test
    """

    def __init__( self, testname, path_to_test, context = None ):
        """
        :param testname: name of the test
        :param path_to_test: the relative path from the current directory to the path
        :param context: context in which the test will run
        """
        global unit_tests_dir
        Test.__init__( self, testname )
        self.path_to_script = unit_tests_dir + os.sep + path_to_test
        self._config = TestConfigFromPy( self.path_to_script, context )

    def debug_dump( self ):
        log.d( 'script:', self.path_to_script )
        Test.debug_dump( self )

    def command( self, to_file ):
        """
        :param to_file: True if stdout is redirected to a file (so colors make no sense)
        """
        cmd = [sys.executable]
        #
        # PYTHON FLAGS
        #
        #     -u     : force the stdout and stderr streams to be unbuffered; same as PYTHONUNBUFFERED=1
        # With buffering we may end up losing output in case of crashes! (in Python 3.7 the text layer of the
        # streams is unbuffered, but we assume 3.6)
        cmd += ['-u']
        #
        if sys.flags.verbose:
            cmd += ["-v"]
        #
        cmd += [self.path_to_script]
        #
        # SCRIPT FLAGS
        #
        # If the script has a custom-arguments flag, then we don't pass any of the standard options
        if 'custom-args' not in self.config.flags:
            #
            if log.is_debug_on():
                cmd += ['--debug']
            #
            if to_file:
                pass
            elif log.is_color_on():
                cmd += ['--color']
            elif log.is_color_disabled():
                cmd += ['--no-color']
            #
            if self.config.context:
                cmd += ['--context', ' '.join(self.config.context)]
        return cmd

    def run_test( self, configuration = None, log_path = None, opts = set() ):
        try:
            cmd = self.command( to_file = log_path and log_path != subprocess.PIPE )
            if opts:
                cmd += [opt for opt in opts]
            run( cmd, stdout=log_path, append=self.ran, timeout=self.config.timeout )
        finally:
            self._ran = True
            # Small delay to allow any async background commands to complete before port reset
            time.sleep( 0.1 )


class ExeTest( Test ):
    """
    Class for c/cpp tests. Hold the path to the executable for the test
    """

    def __init__( self, testname, exe = None, context = None ):
        """
        :param testname: name of the test
        :param exe: full path to executable
        :param context: context in which the test will run
        """
        global unit_tests_dir
        Test.__init__( self, testname )
        self.exe = exe

        relative_test_path = self.find_source_path()
        if relative_test_path:
            self._config = TestConfigFromCpp( unit_tests_dir + os.sep + relative_test_path, context )
        else:
            self._config = TestConfig(context)
            self._config.tags.add( 'exe' )

    def debug_dump( self ):
        if self.exe:
            if not os.path.isfile( self.exe ):
                log.d( "exe does not exist: " + self.exe )
            else:
                log.d( 'exe:', self.exe )
        Test.debug_dump( self )

    def command( self, to_file ):
        """
        :param to_file: True if stdout is redirected to a file (so colors make no sense)
        """
        cmd = [self.exe]
        if 'custom-args' not in self.config.flags:
            # Assume we're a Catch2 exe, so:
            # if sys.flags.verbose:
            #    cmd +=
            if log.is_debug_on():
                cmd += ['-d', 'yes']  # show durations for each test-case
                # cmd += ['--success']  # show successful assertions in output
                cmd += ['--debug']
            # if not to_file and log.is_color_on():
            #    cmd += ['--use-colour', 'yes']
            if self.config.context:
                cmd += ['--context', ' '.join(self.config.context)]
        return cmd

    def run_test( self, configuration = None, log_path = None, opts = set() ):
        if not self.exe:
            raise RuntimeError("Tried to run test " + self.name + " with no exe file provided")
        try:
            cmd = self.command( to_file = log_path and log_path != subprocess.PIPE )
            if opts:
                cmd += [opt for opt in opts]
            run( cmd, stdout=log_path, append=self.ran, timeout=self.config.timeout )
        finally:
            self._ran = True
            # Small delay to allow any async background commands to complete before port reset
            time.sleep( 0.1 )
