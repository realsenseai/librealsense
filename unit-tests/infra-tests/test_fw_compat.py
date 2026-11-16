# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for rspy/fw_compat.download_gold_fw(): returns None gracefully (never raises) on no URL, a
network failure, or a suspiciously small download; returns a local path as-is; and returns the
cached path without re-downloading when the image is already on disk.
"""

from unittest.mock import patch, MagicMock
import pytest
from rspy import fw_compat


class TestDownloadGoldFw:
    @pytest.fixture(autouse=True)
    def _home(self):
        # download_gold_fw builds the cache path from libci.home; give it a stable value.
        with patch.object( fw_compat.libci, "home", "/ci" ):
            yield

    def test_no_url_returns_none(self):
        assert fw_compat.download_gold_fw( None ) is None
        assert fw_compat.download_gold_fw( "" ) is None

    def test_local_path_returned_as_is(self, tmp_path):
        img = tmp_path / "golden.img"
        img.write_bytes( b"x" )
        assert fw_compat.download_gold_fw( str( img ) ) == str( img )

    @patch( "rspy.fw_compat.os.path.isfile", side_effect=lambda p: p.endswith( "img.bin" ) and p.startswith( "/ci" ) )
    def test_cached_returns_path_without_download(self, _isfile):
        with patch( "rspy.fw_compat.urllib.request.urlopen" ) as urlopen:
            path = fw_compat.download_gold_fw( "https://x/img.bin" )
            urlopen.assert_not_called()
        assert path and path.endswith( "img.bin" )

    @patch( "rspy.fw_compat.os.makedirs" )
    @patch( "rspy.fw_compat.os.path.isfile", return_value=False )
    def test_network_failure_returns_none(self, _isfile, _mk):
        with patch( "rspy.fw_compat.urllib.request.urlopen", side_effect=OSError( "boom" ) ):
            assert fw_compat.download_gold_fw( "https://x/img.bin" ) is None

    @patch( "rspy.fw_compat.os.makedirs" )
    @patch( "rspy.fw_compat.os.path.isfile", return_value=False )
    def test_small_download_discarded(self, _isfile, _mk):
        resp = MagicMock()
        resp.__enter__.return_value.read.return_value = b"x" * 1024  # < 512 KB -> bad download
        with patch( "rspy.fw_compat.urllib.request.urlopen", return_value=resp ):
            assert fw_compat.download_gold_fw( "https://x/img.bin" ) is None
