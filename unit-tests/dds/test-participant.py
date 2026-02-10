# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022-4 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

from rspy import log, test, config_file
import pyrealdds as dds

with test.remote.fork( nested_indent='  S' ) as remote:
    if remote is None:  # we're the fork
        dds.debug( log.is_debug_on(), log.nested )

        def start_participant():
            global participant
            participant = dds.participant()
            test.check( not participant )

            participant.init( config_file.get_domain_from_config_file(), 'participant-server' )

            test.check( participant )
            test.check( participant.is_valid() )

        def stop_participant():
            global participant
            del participant

        raise StopIteration()  # the remote is now interactive

    ###############################################################################################################
    # The client
    #

    import threading

    dds.debug( log.is_debug_on(), 'C  ' )
    log.nested = 'C  '

    with test.closure( 'setup client' ):

        server_added = False
        participants_changed = threading.Event()
        def on_participant_added( guid, name ):
            global server_added
            if name == 'participant-server':
                server_added = True
                participants_changed.set()
        server_removed = False
        def on_participant_removed( guid, name ):
            global server_removed
            if name == 'participant-server':
                server_removed = True
                participants_changed.set()

        participant = dds.participant()
        participant.init( config_file.get_domain_from_config_file(), 'participant-client' )

        listener = participant.create_listener()
        listener.on_participant_added( on_participant_added )
        listener.on_participant_removed( on_participant_removed )

    with test.closure( 'We can see a new participant' ):
        test.check_false( server_added )
        participants_changed.clear()
        remote.run( 'start_participant()' )
        test.check( participants_changed.wait( 3 ) )
        test.check( server_added )

    with test.closure( 'And its removal' ):
        test.check_false( server_removed )
        participants_changed.clear()
        remote.run( 'stop_participant()' )
        test.check( participants_changed.wait( 3 ) )
        test.check( server_removed )

    listener = None
    participant = None

test.print_results()
