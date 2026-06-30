# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Float-tolerant, order-independent JSON equality comparison.

Pure helper extracted from rspy.test so pytest tests can use it without
pulling in the legacy rspy.test framework.
"""


def check_equal_jsons(json1, json2, epsilon=1e-6, path="root"):
    """Compare two JSON-like objects with float tolerance and ignoring field order.

    Returns True if equal within tolerance, False otherwise. Mismatches are
    printed to stdout with a path indicator for debugging.

    :param json1: The actual JSON object.
    :param json2: The expected JSON object.
    :param epsilon: The tolerance for float comparison.
    :param path: The current path in the JSON structure for logging mismatches.
    """
    def log_difference(p, j1, j2):
        print(f"Mismatch at {p}:")
        print(f"        left  : {j1}")
        print(f"        right : {j2}")

    if isinstance(json1, dict) and isinstance(json2, dict):
        if set(json1.keys()) != set(json2.keys()):
            log_difference(path, json1, json2)
            return False
        for key in json1:
            if not check_equal_jsons(json1[key], json2[key], epsilon, path=f"{path}.{key}"):
                return False

    elif isinstance(json1, list) and isinstance(json2, list):
        if len(json1) != len(json2):
            log_difference(path, json1, json2)
            return False
        sorted_json1 = sorted(json1, key=lambda x: str(x) if isinstance(x, (dict, list)) else x)
        sorted_json2 = sorted(json2, key=lambda x: str(x) if isinstance(x, (dict, list)) else x)
        for i, (item1, item2) in enumerate(zip(sorted_json1, sorted_json2)):
            if not check_equal_jsons(item1, item2, epsilon, path=f"{path}[{i}]"):
                return False

    elif isinstance(json1, float) and isinstance(json2, float):
        if abs(json1 - json2) > epsilon:
            log_difference(path, json1, json2)
            return False

    elif json1 != json2:
        log_difference(path, json1, json2)
        return False

    return True
