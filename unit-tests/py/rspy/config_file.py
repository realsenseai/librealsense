# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import os
import json
import time

# Cache for domain value to avoid re-reading config file
_cached_domain = None

def get_config_path():
    if os.name == "nt":  # windows
        base_dir = os.environ.get("APPDATA")
    else:  # linux / macos / other unix-like
        base_dir = os.environ.get("HOME")

    config_path = os.path.join(base_dir, "realsense-config.json")
    return config_path


def get_config_file():
    config_path = get_config_path()

    try:
        with open(config_path, "r", encoding="utf-8") as f:
            config = json.load(f)
    except FileNotFoundError:
        raise FileNotFoundError(f"config file not found: {config_path}")
    except json.JSONDecodeError as e:
        raise ValueError(f"invalid json in {config_path}: {e}")
    
    return config
    
def generate_domain_from_time():
    """Generate a domain ID based on current time seconds to ensure uniqueness."""
    current_seconds = int(time.time()) % 232  # Keep it within DDS domain range (0-232)
    return current_seconds
    
def get_domain_from_config_file():
    global _cached_domain
    
    # Return cached value if already read
    if _cached_domain is not None:
        return _cached_domain
    
    try:
        # Read from file and cache the result
        config_file = get_config_file()
        domain = (config_file.get("context", {})
                             .get("dds", {})
                             .get("domain"))
        if domain is None:
            raise KeyError("Missing required config key: context.dds.domain")

        # Cache the domain value for future calls
        _cached_domain = domain
    
    except FileNotFoundError:
        # Fallback: generate domain from current time if config file not found
        _cached_domain = generate_domain_from_time()
    
    finally:
        return _cached_domain
