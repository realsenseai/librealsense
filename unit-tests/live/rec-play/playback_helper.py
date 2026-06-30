# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.


import time
import logging
from pytest_check import check
from rspy.timer import Timer

log = logging.getLogger(__name__)

'''
This class is used to wait and verify for a requested playback status arrived within a requested timeout.
It blocks the calling thread and samples the playback status using a playback status callback from LRS API
      
Usage should look like this:
    psv = PlaybackStatusVerifier( device );
    ...Start Streaming...
    psv.wait_for_status( timeout=5, required_status=rs.playback_status.playing, sample_interval=0.05 )

'''
class PlaybackStatusVerifier:

    def __init__( self, dev ):
        self._current_status = None
        self._statuses = []
        self._status_changes_cnt = 0
        playback_dev = dev.as_playback()
        '''
        Note: LRS set_status_changed_callback function is a 'register' and not a 'set' function
              meaning multiple set called will add a callback to the callback list
        '''
        playback_dev.set_status_changed_callback( self.__signal_on_status_change )

    def __signal_on_status_change( self, playback_status ):
        log.debug(f'playback status callback invoked with {playback_status}')
        self._status_changes_cnt += 1
        self._current_status = playback_status
        self._statuses.append(playback_status)


    '''
    This function goal is to catch the first time the playback status match the required status,
    We sample the status at a user-defined interval low enough for not missing the event and compare the status to the required one.
    
    Note: Limitation - This function is build to catch statues change at 'sample_interval' time, 
    If the required status can arrive and change within this sample_interval, this function can miss it.
    The user needs to make sure that the 'sample_interval' is lower than the possible time for this status to arrive and be overridden by a new status.
    '''
    def wait_for_status( self, timeout, required_status, sample_interval=0.01 ):
        self._status_changes_cnt = 0
        status_changes_cnt = self._status_changes_cnt
        self._current_status = None
        required_status_detected = False
        wait_for_event_timer = Timer(timeout)
        wait_for_event_timer.start()
        log.debug(f'timeout set to {timeout} [sec]')
        while not wait_for_event_timer.has_expired():
            if required_status == self._current_status:
                log.debug('Required status "' + str(required_status) + '" detected!')
                required_status_detected = True
                break
            time.sleep( sample_interval )

        check.is_true(required_status_detected, 'Check failed, Timeout on waiting for ' + str(required_status) )

        '''If the status changes too fast let say , Stopped -> Playing --> Stopped  within 1 sample time we can miss
        it. So if we already failed on the status we add a check to indicate it in the test result that our sample
        interval may be too long and we may have missed the required status '''
        if not required_status_detected:
            check.is_true( status_changes_cnt + 1 < self._status_changes_cnt,
                    'Multiple status changes detected, expecting a single change, got '+ str( self._status_changes_cnt - status_changes_cnt ) +
                        ' changes, consider lowering the sample interval' )

    def wait_for_status_changes( self, counter, timeout, sample_interval=0.01 ):
        wait_for_event_timer = Timer(timeout)
        wait_for_event_timer.start()
        required_status_detected = False
        log.debug(f'timeout set to {timeout} [sec]')
        while not wait_for_event_timer.has_expired():
            if counter == self._status_changes_cnt:
                required_status_detected = True
                break
            time.sleep( sample_interval )
        check.is_true(required_status_detected, 'Check failed, Timeout on waiting for status change')

    def get_statuses(self):
        return self._statuses