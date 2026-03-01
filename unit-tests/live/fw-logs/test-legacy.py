# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# DDS devices have not implemented firmware_logger interface yet. Only D400 devices use legacy logging format
# test:device each(D400*)
# test:donotrun:!nighlty

from rspy import log, test
from rspy import librs as rs
import xml.etree.ElementTree as ET
import os.path

# Working directory changes between manual runs and CI runs
path = os.path.dirname( os.path.realpath(__file__) )

if log.is_debug_on():
    rs.log_to_console( rs.log_severity.debug )

with test.closure( 'Create the logger', on_fail=test.ABORT ):
    context = rs.context()
    dev = context.query_devices()[0]
    logger = dev.as_firmware_logger()
    test.check( logger )
    raw_message = logger.create_message()
    parsed_message = logger.create_parsed_message()

with test.closure( 'Load unsupported definitions file' ):
    events = ET.fromstring(
             """<Format>
                  <Event id="0" numberOfArguments="0" format="Event0" />
                </Format>""" )
    events_path = os.path.join( path, 'events.xml' )
    ET.ElementTree( events ).write( events_path )
    definitions = ET.fromstring(
                  """<Format>
                       <Source id="0" Name="source1">
                       <File Path="" />
                         <Module id="1" verbosity="0" Name="source0module1" />
                       </Source>
                       <Source id="1" Name="source2">
                       <File Path="" />
                         <Module id="1" verbosity="0" Name="source1module1" />
                       </Source>
                     </Format>""" )
    definitions[0][0].set( "Path", events_path )
    definitions[1][0].set( "Path", events_path )
    definitions_path = os.path.join( path, 'definitions.xml' )
    ET.ElementTree( definitions ).write( definitions_path )
    with open( definitions_path, 'r') as f:
        definitions = f.read()
    logger.init_parser( definitions )
    logger.get_firmware_log( raw_message ) # Get a log entry from the camera with unknown content
    test.check_throws( lambda: logger.parse_log( raw_message, parsed_message ), RuntimeError, 'FW logs parser expect one formatting options, have 2' )
    
with test.closure( 'Load supported definitions file' ):
    # Events based on real events file, representing logs we are most likely to receive
    # Use all entry options - Event, File, Thread, Enum
    events = ET.fromstring(
             """<Format>
                  <Event id="37" numberOfArguments="3" format="Event37 Arg1:{0} Arg2:{1}, Arg3:0x{2:x}" />
                  <Event id="49" numberOfArguments="3" format="Event49 Arg1:{0} EnumArg2: {1,ETSystemSubStates} EnumArg3: {2,ETSystemSubStates}" />
                  <Event id="90" numberOfArguments="3" format="Event90 Arg1:0x{0:x}, Arg2:{1}, Arg3:0x{2:x}" />
                  <Event id="272" numberOfArguments="1" format="Event272 Arg1 0x{0:x}" />
                  <Event id="277" numberOfArguments="2" format="Event277 Arg1 0x{0:x}, Arg2 0x{1:x}" />
                  <Event id="304" numberOfArguments="1" format="Event304 Arg1 0x{0:x}" />
                  <Event id="380" numberOfArguments="1" format="Event380 Arg1:{0:x}" />
                  <File id="13" Name="File13" />
                  <File id="29" Name="File29" />
                  <File id="49" Name="File49" />
                  <Thread id="0" Name="Thread0" />
                  <Thread id="7" Name="Thread7" />
                  <Enums>
                    <Enum Name="ETSystemSubStates">
                      <EnumValue Key="0" Value="Enum1Litteral0" />
                      <EnumValue Key="1" Value="Enum1Litteral1" />
                      <EnumValue Key="2" Value="Enum1Litteral2" />
                      <EnumValue Key="4" Value="Enum1Litteral4" />
                      <EnumValue Key="8" Value="Enum1Litteral8" />
                      <EnumValue Key="16" Value="Enum1Litteral16" />
                      <EnumValue Key="32" Value="Enum1Litteral32" />
                      <EnumValue Key="64" Value="Enum1Litteral64" />
                      <EnumValue Key="128" Value="Enum1Litteral128" />
                    </Enum>
                  </Enums>
                </Format>""" )
    events_path = os.path.join( path, 'events.xml' )
    ET.ElementTree( events ).write( events_path )
    definitions = ET.fromstring(
          """<Format>
               <Source id="0" Name="DS5">
                 <File Path="" />
               </Source>
             </Format>""" )
    definitions[0][0].set( "Path", events_path )
    definitions_path = os.path.join( path, 'definitions.xml' )
    ET.ElementTree( definitions ).write( definitions_path )
    with open( definitions_path, 'r') as f:
        definitions = f.read()
    logger.init_parser( definitions )
    try:
        logger.get_firmware_log( raw_message ) # Get a log entry from the camera with unknown content
        logger.parse_log( raw_message, parsed_message )
        log.d( 'Parsed message: ', parsed_message.get_message() )
    except:
        test.unexpected_exception()

with test.closure( 'Load events file directly' ):
    with open( events_path, 'r') as f:
        events = f.read()
    logger.init_parser( events )
    try:
        logger.get_firmware_log( raw_message ) # Get a log entry from the camera with unknown content
        logger.parse_log( raw_message, parsed_message )
        log.d( 'Parsed message: ', parsed_message.get_message() )
    except:
        test.unexpected_exception()
        
with test.closure( 'All done' ):
    if os.path.exists( events_path ):
      os.remove( events_path )
    if os.path.exists( definitions_path ):
      os.remove( definitions_path )
    del dev
    del context

test.print_results()
