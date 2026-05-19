# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from functools import lru_cache
from config import Settings, settings as global_settings

@lru_cache()
def get_settings() -> Settings:
    return global_settings