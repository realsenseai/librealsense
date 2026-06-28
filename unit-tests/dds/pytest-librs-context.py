# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

import pytest
import re
import logging
import pyrealsense2 as rs

log = logging.getLogger(__name__)
log.nested = 'C  '

pytestmark = [
    pytest.mark.dds,
]

if log.isEnabledFor(logging.DEBUG):
    rs.log_to_console( rs.log_severity.debug )

#############################################################################################
#
def test_multiple_participants_on_same_domain_should_fail():
    contexts = []
    contexts.append( rs.context( { 'dds': { 'enabled': True, 'domain': 124, 'participant': 'context1' }} ))
    # another context, same domain and name -> OK
    contexts.append( rs.context( { 'dds': { 'enabled': True, 'domain': 124, 'participant': 'context1' }} ))
    # without a name -> pick up the name from the existing participant (default is "librealsense")
    contexts.append( rs.context( { 'dds': { 'enabled': True, 'domain': 124 }} ))
    # same name, different domain -> different participant; should be OK:
    contexts.append( rs.context( { 'dds': { 'enabled': True, 'domain': 125, 'participant': 'context1' }} ))
    with pytest.raises( RuntimeError, match=re.escape( "A DDS participant 'context1' already exists in domain 124; cannot create 'context2'" ) ):
        rs.context( { 'dds': { 'enabled': True, 'domain': 124, 'participant': 'context2' }} )
    del contexts
#
#############################################################################################
