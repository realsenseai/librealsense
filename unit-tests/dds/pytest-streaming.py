# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import logging
import pyrealdds as dds
from rspy import test, config_file
from time import sleep
import json
import flexible
from pytest_check import check

log = logging.getLogger(__name__)
log.nested = 'C  '

pytestmark = [
    pytest.mark.dds,
    pytest.mark.flaky( retries=2 ),
]

dds.debug( log.isEnabledFor( logging.DEBUG ), 'C  ' )

sample1 = { 'id' : 'test-message', 'n' : 1, 'content' : { 'int' : 1, 'float' : 2.0, 'array' : [ 1, 2.0, 'hello' ] } }
sample1_json = json.dumps( sample1 )
sample2 = { 'id' : 'test-message-2', 'n' : 2 }
sample2_json = json.dumps( sample2 )
sample3 = { 'id' : 'test-message', 'n' : 3, 'hmm' : [] }
sample3_json = json.dumps( sample3 )

@pytest.fixture(scope='module')
def remote_and_participant():
    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), "test-streaming" )

    import os.path
    cwd = os.path.dirname(os.path.realpath(__file__))
    remote_script = os.path.join( cwd, 'streaming-server.py' )
    with test.remote( remote_script, nested_indent="  S" ) as remote:
        remote.wait_until_ready()
        try:
            yield remote, participant
        finally:
            del participant

#############################################################################################
#
def test_1w1r_one_message(remote_and_participant):
    remote, participant = remote_and_participant
    remote.run( 'topic = flexible.writer( participant, "1w1r" )' )
    # NOTE: technically, we could combine everything into a single command:
    #      Topic("blah").write( ... )
    # and it would do everything, including destroy the Topic instance with the writer etc.
    # This doesn't work! If the writer is destroyed before we've had a chance to get the message,
    # the message will get lost...
    topic = flexible.reader( participant, "1w1r" )
    # We may wait for the data before we even see the writer... so wait for the writer first!
    topic.wait_for_writers()
    # Same for the writer:
    remote.run( 'topic.wait_for_readers()' )
    remote.run( 'topic.write( """' + sample1_json + '""" )' )
    msg = topic.read()
    check.is_true( topic.empty() )
    check.equal( msg.json_data(), sample1 )
    remote.run( 'topic.stop()' )
    topic.wait_for_writers( 0 )
    topic = None
#
#############################################################################################
#
def test_1w1r_n_messages(remote_and_participant):
    remote, participant = remote_and_participant
    remote.run( 'topic = flexible.writer( participant, "1w1rNm" )' )
    topic = flexible.reader( participant, "1w1rNm" )
    topic.wait_for_writers()
    remote.run( 'topic.wait_for_readers()' )
    remote.run( 'topic.write( """' + sample1_json + '""" )' )
    remote.run( 'topic.write( """' + sample2_json + '""" )' )
    remote.run( 'topic.write( """' + sample3_json + '""" )' )
    msg1 = topic.read()
    msg2 = topic.read()
    msg3 = topic.read()
    check.is_true( topic.empty() )
    check.equal( msg1.json_data(), sample1 )
    check.equal( msg2.json_data(), sample2 )
    check.equal( msg3.json_data(), sample3 )
    remote.run( 'topic.stop()' )
    topic.wait_for_writers( 0 )
    topic = None
#
#############################################################################################
#
def test_1w2r_one_message(remote_and_participant):
    remote, participant = remote_and_participant
    remote.run( 'topic = flexible.writer( participant, "1w2r" )' )
    r1 = flexible.reader( participant, "1w2r" )
    r2 = flexible.reader( participant, r1.handle )
    r1.wait_for_writers()
    r2.wait_for_writers()
    remote.run( 'topic.wait_for_readers( 2 )' )
    remote.run( 'topic.write( """' + sample1_json + '""" )' )
    msg1 = r1.read()
    msg2 = r2.read()
    check.is_true( r1.empty() )
    check.is_true( r2.empty() )
    check.equal( msg1.json_data(), sample1 )
    check.equal( msg2.json_data(), sample1 )
    remote.run( 'topic.stop()' )
    r1.wait_for_writers( 0 )
    r2.wait_for_writers( 0 )
    r1 = r2 = None
#
#############################################################################################
