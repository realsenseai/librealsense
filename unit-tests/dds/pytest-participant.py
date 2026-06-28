# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import logging
import pyrealdds as dds
from rspy import test, config_file
import rspy.log
from pytest_check import check

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.dds,
    pytest.mark.flaky( retries=2 ),
]

if rspy.log.nested is not None:
    dds.debug( log.isEnabledFor( logging.DEBUG ), rspy.log.nested )

    def start_participant():
        global participant
        participant = dds.participant()
        check.is_true( not participant )

        participant.init( config_file.get_domain_from_config_file_or_default(), 'participant-server' )

        check.is_true( participant )
        check.is_true( participant.is_valid() )

    def stop_participant():
        global participant
        del participant

else:
    log.nested = 'C  '

    @pytest.fixture(scope='module')
    def remote_server():
        with test.remote.fork( script=__file__, nested_indent='  S' ) as remote:
            yield remote

    ###############################################################################################################
    # The client
    #

    import threading

    dds.debug( log.isEnabledFor( logging.DEBUG ), 'C  ' )

    def test_participant_added_and_removed(remote_server):
        # setup client
        server_added = False
        participants_changed = threading.Event()
        def on_participant_added( guid, name ):
            nonlocal server_added
            if name == 'participant-server':
                server_added = True
                participants_changed.set()
        server_removed = False
        def on_participant_removed( guid, name ):
            nonlocal server_removed
            if name == 'participant-server':
                server_removed = True
                participants_changed.set()

        participant = dds.participant()
        participant.init( config_file.get_domain_from_config_file_or_default(), 'participant' )

        listener = participant.create_listener()
        listener.on_participant_added( on_participant_added )
        listener.on_participant_removed( on_participant_removed )

        # We can see a new participant
        check.is_false( server_added )
        participants_changed.clear()
        remote_server.run( 'start_participant()' )
        check.is_true( participants_changed.wait( 3 ) )
        check.is_true( server_added )

        # And its removal
        check.is_false( server_removed )
        participants_changed.clear()
        remote_server.run( 'stop_participant()' )
        check.is_true( participants_changed.wait( 3 ) )
        check.is_true( server_removed )

        listener = None
        participant = None
