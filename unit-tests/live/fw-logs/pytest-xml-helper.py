# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Currently testing with D585S because this is the only module supporting some of the features like module verbosity and version verification.
# When more D500 models will be available most of the test cases can be generelized, but D585S version verification should still be checked specifically.

import os
import re

import pytest
import pyrealsense2 as rs

pytestmark = [
    pytest.mark.device("D585S"),
    pytest.mark.skip(reason="until fw issue resolved"),
]


def test_xml_helper(test_device, tmp_path):
    dev, _ = test_device
    logger = rs.firmware_logger(dev)

    fw_version = dev.get_info( rs.camera_info.firmware_version )
    smcu_version = dev.get_info( rs.camera_info.smcu_fw_version )

    events_path = os.path.join( str(tmp_path), "events.xml" )
    events2_path = os.path.join( str(tmp_path), "events2.xml" )

    # test empty XML
    empty_xml = ""
    with pytest.raises(RuntimeError, match=re.escape("Cannot find XML root")):
        logger.init_parser(empty_xml)

    # test root is not Format
    xml = """<Source id="0" Name="test" />"""
    with pytest.raises(RuntimeError, match=re.escape("XML root should be 'Format'")):
        logger.init_parser( xml )

    # test source
    xml = """<Format>
               <Source/>
             </Format>"""
    with pytest.raises(RuntimeError, match=re.escape("Can't find attribute 'id' in node Source")):
        logger.init_parser( xml )

    xml = """<Format>
               <Source id="0"/>
             </Format>"""
    with pytest.raises(RuntimeError, match=re.escape("Can't find attribute 'Name' in node Source")):
        logger.init_parser( xml )

    xml = """<Format>
               <Source id="3" Name="invalid" />
             </Format>"""
    with pytest.raises(RuntimeError, match=re.escape("Supporting source id 0 to 2. Found source (3, invalid)")):
        logger.init_parser( xml )

    xml = """<Format>
               <Source id="-1" Name="invalid" />
             </Format>"""
    with pytest.raises(RuntimeError, match=re.escape("Supporting source id 0 to 2. Found source (-1, invalid)")):
        logger.init_parser( xml )

    # test module
    events_xml = """<Format/>"""
    with open( events_path, "w" ) as events_file:
        events_file.write( events_xml )

    xml = f"""<Format>
               <Source id="0" Name="test" >
                 <File Path="{events_path}" />
                 <Module id="32" />
               </Source>
             </Format>"""
    with pytest.raises(RuntimeError, match=re.escape("Can't find attribute 'verbosity' in node Module")):
        logger.init_parser( xml )

    xml = f"""<Format>
               <Source id="0" Name="test" >
                 <File Path="{events_path}" />
                 <Module id="32" verbosity="0" />
               </Source>
             </Format>"""
    with pytest.raises(RuntimeError, match=re.escape("Supporting module id 0 to 31. Found module 32 in source (0, test)")):
        logger.init_parser( xml )

    xml = f"""<Format>
               <Source id="0" Name="test" >
                 <File Path="{events_path}" />
                 <Module id="-1" verbosity="0" />
               </Source>
             </Format>"""
    with pytest.raises(RuntimeError, match=re.escape("Supporting module id 0 to 31. Found module -1 in source (0, test)")):
        logger.init_parser( xml )

    # test version verification
    # Bad HKR and SMCU versions
    events_xml = "<Format version='1.2'/>"
    with open( events_path, "w" ) as events_file:
        events_file.write( events_xml )

    events2_xml = "<Format version='3.4'/>"
    with open( events2_path, "w" ) as events2_file:
        events2_file.write( events2_xml )

    xml = f"""<Format>
               <Source id="0" Name="HKR" >
                 <File Path="{events_path}" />
                 <Module id="0" verbosity="0" />
               </Source>
               <Source id="1" Name="SMCU" >
                 <File Path="{events2_path}" />
                 <Module id="0" verbosity="0" />
               </Source>
             </Format>"""

    expected_error = "Source HKR expected version " + fw_version + " but xml file version is 1.2"
    with pytest.raises(RuntimeError, match=re.escape(expected_error)):
        logger.init_parser( xml )

    # Fix HKR version, fail on SMCU
    events_xml = "<Format version='" + fw_version + "'/>"
    with open( events_path, "w" ) as events_file:
        events_file.write( events_xml )

    expected_error = "Source SMCU expected version " + smcu_version + " but xml file version is 3.4"
    with pytest.raises(RuntimeError, match=re.escape(expected_error)):
        logger.init_parser( xml )

    # Both versions OK
    events2_xml = "<Format version='" + smcu_version + "'/>"
    with open( events2_path, "w" ) as events2_file:
        events2_file.write( events2_xml )

    assert logger.init_parser( xml )

    # test verbosity level
    # Number is OK (range not checked)
    xml = f"""<Format>
               <Source id="0" Name="HKR" >
                 <File Path="{events_path}" />
                 <Module id="0" verbosity="55" />
               </Source>
               <Source id="1" Name="SMCU" >
                 <File Path="{events2_path}" />
                 <Module id="0" verbosity="0" />
               </Source>
             </Format>"""
    assert logger.init_parser( xml )

    # Starting with a digit but is not a number
    xml = f"""<Format>
               <Source id="0" Name="test" >
                 <File Path="{events_path}" />
                 <Module id="0" verbosity="0A" />
               </Source>
               <Source id="1" Name="test" >
                 <File Path="{events2_path}" />
                 <Module id="0" verbosity="0" />
               </Source>
             </Format>"""
    with pytest.raises(RuntimeError, match=re.escape("Bad verbosity level 0A")):
        logger.init_parser( xml )

    # Valid verbosity keywords combined
    xml = f"""<Format>
               <Source id="0" Name="test" >
                 <File Path="{events_path}" />
                 <Module id="0" verbosity="DEBUG|INFO|ERROR" />
               </Source>
               <Source id="1" Name="test" >
                 <File Path="{events2_path}" />
                 <Module id="0" verbosity="VERBOSE|FATAL" />
               </Source>
             </Format>"""
    assert logger.init_parser( xml )

    # Not one of the valid verbosity keywords
    xml = f"""<Format>
               <Source id="0" Name="test" >
                 <File Path="{events_path}" />
                 <Module id="0" verbosity="TEST" />
               </Source>
               <Source id="1" Name="test" >
                 <File Path="{events2_path}" />
                 <Module id="0" verbosity="0" />
               </Source>
             </Format>"""
    with pytest.raises(RuntimeError, match=re.escape("Illegal verbosity TEST. Expecting NONE, VERBOSE, DEBUG, INFO, WARNING, ERROR or FATAL")):
        logger.init_parser( xml )

    # test module events file
    xml = f"""<Format>
               <Source id="0" Name="test" >
                 <File Path="{events_path}" />
                 <Module id="0" verbosity="0" Path="{events_path}" />
               </Source>
               <Source id="1" Name="test" >
                 <File Path="{events2_path}" />
                 <Module id="0" verbosity="0" />
               </Source>
             </Format>"""
    assert logger.init_parser( xml )

    # test live log messages received
    # Number is OK (range not checked)
    xml = f"""<Format>
               <Source id="0" Name="HKR" >
                 <File Path="{events_path}" />
                 <Module id="0" verbosity="63" />
                 <Module id="1" verbosity="63" />
                 <Module id="2" verbosity="63" />
                 <Module id="3" verbosity="63" />
                 <Module id="4" verbosity="63" />
                 <Module id="5" verbosity="63" />
                 <Module id="6" verbosity="63" />
                 <Module id="7" verbosity="63" />
                 <Module id="8" verbosity="63" />
                 <Module id="9" verbosity="63" />
               </Source>
               <Source id="1" Name="SMCU" >
                 <File Path="{events2_path}" />
                 <Module id="0" verbosity="0" />
               </Source>
             </Format>"""

    assert logger.init_parser( xml )
    logger.start_collecting()
    message = logger.create_message()
    for i in range(10):
        assert logger.get_firmware_log( message )
    logger.stop_collecting()
