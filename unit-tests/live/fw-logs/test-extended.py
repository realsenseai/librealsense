# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# DDS devices have not implemented firmware_logger interface yet
#test:device each(D500*) !D555
#test:donotrun:!nightly

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

with test.closure( 'Create xml files', on_fail=test.ABORT ):
    # Events based on real events file, representing logs we are most likely to receive
    events_real = ET.fromstring(
                  """<Format>
                       <Event id="50" numberOfArguments="0" format="Event50" />
                       <Event id="52" numberOfArguments="3" format="Event52 Arg1:{0}, Arg2:{1}, Arg3:{2}" />
                       <Event id="59" numberOfArguments="1" format="Event59 Arg1:{0}" />
                       <Event id="2549" numberOfArguments="0" format="Event2549" />
                       <File id="5" Name="File5" />
                       <Module id="2" Name="Module2" />
                     </Format>""" )
    events_real_path = os.path.join( path, 'events_real.xml' )
    ET.ElementTree( events_real ).write( events_real_path )
    
    events_dummy = ET.fromstring(
                   """<Format>
                        <Event id="1" numberOfArguments="0" format="Event1" />
                        <Event id="2" numberOfArguments="0" format="Event2" />
                        <File id="1" Name="File1" />
                        <File id="2" Name="File2" />
                        <Module id="1" Name="Module1" />
                        <Module id="2" Name="Module2" />
                      </Format>""" )
    events_dummy_path = os.path.join( path, 'events_dummy.xml' )
    ET.ElementTree( events_dummy ).write( events_dummy_path )
    
    # Expected events are from source1 module2, set verbosity to "enable all"
    definitions = ET.fromstring(
          """<Format>
               <Source id="0" Name="source1">
               <File Path="" />
                 <Module id="1" verbosity="0" Name="source1module1" Path=""/>
                 <Module id="2" verbosity="63" Name="source1module2" />
               </Source>
               <Source id="1" Name="source2">
               <File Path="" />
                 <Module id="1" verbosity="0" Name="source2module1" />
                 <Module id="2" verbosity="0" Name="source2module2" />
               </Source>
               <Source id="2" Name="source3">
               <File Path="" />
                 <Module id="1" verbosity="0" Name="source3module1" />
                 <Module id="2" verbosity="0" Name="source3module2" />
               </Source>
             </Format>""" )
    definitions[0][0].set( "Path", events_real_path )
    definitions[1][0].set( "Path", events_dummy_path )
    definitions[2][0].set( "Path", events_dummy_path )
    definitions_path = os.path.join( path, 'definitions.xml' )
    ET.ElementTree( definitions ).write( definitions_path )

with test.closure( 'Handle D585S file versions' ):
    import xml.etree.ElementTree as ET
    device_name = dev.get_info( rs.camera_info.name )
    if device_name.find( "D585S" ) != -1:
        fw_version = dev.get_info( rs.camera_info.firmware_version )
        tree = ET.parse( events_real_path )
        root = tree.getroot()
        test.check( root.tag, 'Format' )
        root.set( 'version', fw_version )
        tree.write( events_real_path )
        
        smcu_version = dev.get_info( rs.camera_info.smcu_fw_version )
        tree = ET.parse( events_dummy_path )
        root = tree.getroot()
        test.check( root.tag, 'Format' )
        root.set( 'version', smcu_version )
        tree.write( events_dummy_path )

with test.closure( 'Load unsupported definitions file' ):
    with open( events_real_path , 'r') as f:
        definitions = f.read()    
    test.check_throws( lambda: logger.init_parser( definitions ), RuntimeError, "Did not find 'Source' node with id 0" )

with test.closure( 'Load supported definitions file' ):
    with open( definitions_path , 'r') as f:
        definitions = f.read()
    logger.init_parser( definitions )
    logger.start_collecting()
    logger.get_firmware_log( raw_message ) # Get a log entry from the camera with unknown content
    logger.parse_log( raw_message, parsed_message )
    log.d( 'Parsed message: ', parsed_message.get_message() )
    logger.stop_collecting()
    
with test.closure( 'All done' ):
    if os.path.exists( events_real_path ):
      os.remove( events_real_path )
    if os.path.exists( events_dummy_path ):
      os.remove( events_dummy_path )
    if os.path.exists( definitions_path ):
      os.remove( definitions_path )
    del dev
    del context

test.print_results()
