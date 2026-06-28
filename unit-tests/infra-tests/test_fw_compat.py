# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for rspy/fw_compat.py gold-recovery FW selection:

- gold_recovery_key(): D400 SKUs share one image keyed by product line ('D400'); D500 SKUs are
  keyed by SKU ('D555'), parsed prefix-agnostically from the device name.
- download_gold_fw(): returns None gracefully (never raises) on an unresolvable key, a missing
  fw_fallback.json entry, a network failure, or a suspiciously small download; and returns the
  cached path without re-downloading when the image is already on disk.
"""

from unittest.mock import patch, MagicMock
import pytest
from rspy import fw_compat


class TestGoldRecoveryKey:
    def test_d400_keyed_by_product_line(self):
        assert fw_compat.gold_recovery_key( "D400", "RealSense D435" ) == "D400"
        assert fw_compat.gold_recovery_key( "D400", "RealSense D455" ) == "D400"
        assert fw_compat.gold_recovery_key( "D400", None ) == "D400"

    def test_d500_keyed_by_sku_prefix_agnostic(self):
        for name in ( "Intel RealSense D555 Recovery", "RealSense D555 Recovery",
                      "RealSense D555", "D555 Recovery" ):
            assert fw_compat.gold_recovery_key( "D500", name ) == "D555"
        assert fw_compat.gold_recovery_key( "D500", "RealSense D585S" ) == "D585"

    def test_unresolvable_returns_none(self):
        assert fw_compat.gold_recovery_key( "D500", None ) is None
        assert fw_compat.gold_recovery_key( "D500", "no model here" ) is None


class TestDownloadGoldFw:
    @pytest.fixture(autouse=True)
    def _home(self):
        # download_gold_fw builds the cache path from libci.home; give it a stable value.
        with patch.object( fw_compat.libci, "home", "/ci" ):
            yield

    def test_unresolvable_key_returns_none(self):
        # not D400 and no SKU in the name -> no key -> None (must not raise)
        assert fw_compat.download_gold_fw( "D500", None ) is None

    @patch( "rspy.fw_compat._load_gold_recovery_fw_map", return_value={} )
    def test_missing_entry_returns_none(self, _map):
        # also covers a missing/malformed fw_fallback.json, which _load_*_map maps to {}
        assert fw_compat.download_gold_fw( "D400" ) is None

    @patch( "rspy.fw_compat._load_gold_recovery_fw_map",
            return_value={ "D400": "https://x/img.bin" } )
    @patch( "rspy.fw_compat.os.path.isfile", return_value=True )
    def test_cached_returns_path_without_download(self, _isfile, _map):
        with patch( "rspy.fw_compat.urllib.request.urlopen" ) as urlopen:
            path = fw_compat.download_gold_fw( "D400" )
            urlopen.assert_not_called()
        assert path and path.endswith( "img.bin" )

    @patch( "rspy.fw_compat._load_gold_recovery_fw_map",
            return_value={ "D400": "https://x/img.bin" } )
    @patch( "rspy.fw_compat.os.makedirs" )
    @patch( "rspy.fw_compat.os.path.isfile", return_value=False )
    def test_network_failure_returns_none(self, _isfile, _mk, _map):
        with patch( "rspy.fw_compat.urllib.request.urlopen", side_effect=OSError( "boom" ) ):
            assert fw_compat.download_gold_fw( "D400" ) is None

    @patch( "rspy.fw_compat._load_gold_recovery_fw_map",
            return_value={ "D400": "https://x/img.bin" } )
    @patch( "rspy.fw_compat.os.makedirs" )
    @patch( "rspy.fw_compat.os.path.isfile", return_value=False )
    def test_small_download_discarded(self, _isfile, _mk, _map):
        resp = MagicMock()
        resp.__enter__.return_value.read.return_value = b"x" * 1024  # < 512 KB -> bad download
        with patch( "rspy.fw_compat.urllib.request.urlopen", return_value=resp ):
            assert fw_compat.download_gold_fw( "D400" ) is None
