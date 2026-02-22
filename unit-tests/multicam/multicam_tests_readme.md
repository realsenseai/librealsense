# Multi-Device Test Infrastructure for librealsense

## Overview

This documentation describes the infrastructure for unit tests that require multiple RealSense devices connected simultaneously. The infrastructure supports any number of devices (typically 2-10 devices, but no hard limit is imposed).

## What Was Added

### 1. Test Helper Functions (unit-tests/py/rspy/test.py)

#### `find_devices_or_exit(count, product_line=None)`
**NEW:** General-purpose function to find N devices, optionally filtered by product line.

**IMPORTANT:** This function integrates with the `devices` module to properly support Acroname hubs and port management.

**Parameters:**
- `count`: Number of devices required (any positive integer)
- `product_line`: Optional. The product line to filter by (e.g., `"D400"`, `"D500"`). If None, any devices.

**Returns:**
- Tuple of `(device_list, serial_numbers)` where device_list contains N device handles, and serial_numbers is a list of their serial numbers

**Behavior:**
- If fewer than `count` devices are found, the test fails with a clear error message
- Displays: "Test requires N devices; found X device(s)"
- Verifies all devices have different serial numbers
- Works with Acroname hubs through the devices module

**Example:**
```python
from rspy import test

# Get 3 D400 devices
devices, sns = test.find_devices_or_exit(3, "D400")
# devices is a list of 3 device handles with different serial numbers

# Get any 5 devices
devices, sns = test.find_devices_or_exit(5)
# devices is a list of 5 device handles (can be mixed product lines)
```

#### `find_two_devices_by_product_line_or_exit(product_line)`
Finds exactly two devices of the specified product line and ensures they have different serial numbers.

**NOTE:** This is a convenience wrapper around `find_devices_or_exit()` for backward compatibility. **For new code, consider using `multiple_devices(2, product_line)` context manager instead.**

**IMPORTANT:** This function integrates with the `devices` module to properly support Acroname hubs and port management.

**Parameters:**
- `product_line`: The product line to search for as a string (e.g., `"D400"`, `"D500"`)

**Returns:**
- Tuple of `(dev1, dev2, serial_numbers)` where dev1 and dev2 are two different devices, and serial_numbers is a list of their serial numbers

**Behavior:**
- If fewer than 2 devices are found, the test fails with a clear error message
- Displays: "Test requires 2 devices; found X device(s)"
- Verifies devices have different serial numbers
- Works with Acroname hubs through the devices module

**Example:**
```python
from rspy import test

dev1, dev2, sns = test.find_two_devices_by_product_line_or_exit("D400")
# dev1 and dev2 are guaranteed to be different devices
```

#### `find_any_two_devices_or_exit()`
Finds any two RealSense devices regardless of product line, ensuring they have different serial numbers.

**NOTE:** This is a convenience wrapper around `find_devices_or_exit()` for backward compatibility. **For new code, consider using `multiple_devices(2)` context manager instead.**

**IMPORTANT:** This function integrates with the `devices` module to properly support Acroname hubs and port management.

**Parameters:**
- None

**Returns:**
- Tuple of `(dev1, dev2, serial_numbers)` where dev1 and dev2 are two different devices (may be different product lines), and serial_numbers is a list of their serial numbers

**Example:**
```python
from rspy import test

dev1, dev2, sns = test.find_any_two_devices_or_exit()
# dev1 and dev2 can be D400 + D500, D435 + D455, etc.
```

#### `multiple_devices` Context Manager Class
**NEW:** Provides a clean context manager interface for multi-device tests with automatic device discovery and cleanup.

**IMPORTANT:** This context manager integrates with the `devices` module to properly support Acroname hubs and port management.

**Usage (N Devices of Same Product Line):**
```python
from rspy import test

# Find 3 D400 devices
with test.multicam(3, "D400") as devices:
    dev1, dev2, dev3 = devices
    # All devices are D400 series with different serial numbers
    # Your test code here

# Or iterate over them:
with test.multicam(5, "D500") as devices:
    for i, dev in enumerate(devices, 1):
        sn = dev.get_info(rs.camera_info.serial_number)
        print(f"Device {i}: {sn}")
```

**Usage (N Devices of Any Product Line):**
```python
from rspy import test

# Find any 4 RealSense devices
with test.multicam(4) as devices:
    # devices is a list of 4 device handles (can be mixed product lines)
    # Your test code here
```

**Benefits:**
- Clean syntax with automatic resource management
- Handles any number of devices (no hardcoded limits)
- Graceful failure handling
- Pythonic and familiar pattern
- Flexible: works with same or different product lines
- **Works with Acroname hubs** through devices module integration

**Note:** For two-device tests, you can use `test.multicam(2, "D400")` instead of the deprecated `two_devices` class.

### 2. Device Configuration Enhancement (unit-tests/py/rspy/devices.py)

#### Updated `by_configuration()` and `by_spec()` Functions
Enhanced to support multiple devices of the same type by allowing repeated device specifications, plus wildcard support for any device.

**Example Configurations:**
```python
#test:device D400* D400*           # Two D400 devices
#test:device D400* D400* D400*     # Three D400 devices
#test:device * * * * *             # Any five devices (can be different product lines)
#test:device D400* D500* D500*     # One D400, one D500, and one D500
```

**Wildcard Support:**
- `*` alone = any/all devices (useful for product-agnostic tests)
- `D400*` = all D400 series devices
- `D500*` = all D500 series devices

**How It Works:**
- Each device spec in the configuration is matched to a unique device
- Repeating a spec (e.g., `D400*` twice) requests multiple devices of that type
- Devices are guaranteed to be unique (same device won't be matched twice)
- `*` matches any available device, making tests work across product lines

**Backward Compatibility:**
- Existing single-device tests continue to work unchanged
- Mixed configurations work: `#test:device D400* D455` (one D400, one D455)

### 3. Test Runner Integration (run-unit-tests.py)

**No changes required!** The existing test runner automatically handles multicam tests:

- Tests with insufficient devices will fail
- Clear error messages indicate why tests failed
- Exit status will be non-zero when tests fail due to missing hardware

### 4. Example Tests

#### test-devices-enumeration.py
Device enumeration test demonstrating:
- Basic device discovery and validation
- Enumerating all connected devices
- Serial number verification
- Device uniqueness checks
- Works with any number of devices
- Minimal example for getting started

#### test-devices-streaming.py
Multi-stream validation test demonstrating:
- Multiple stream profiles on all connected devices (depth + color + IR)
- Stream synchronization across all devices
- Frame drop detection with metadata analysis
- Stream independence verification
- Product-agnostic (works with ALL connected devices)
- Scalable to many devices (2+)
- Serves as comprehensive template for multi-device tests


## How to Write a Two-Device Test

### Step 1: Add Test Configuration Directive

At the top of your test file, specify that two devices are needed.

**For same product line (e.g., two D400 devices):**
```python
#test:device D400* D400*
```

**For any two devices (product-agnostic):**
```python
#test:device * *
```

**For mixed product lines:**
```python
#test:device D400* D500*
```

The repeated spec (`D400*` appears twice) tells the infrastructure to allocate TWO devices matching the D400 pattern.

### Step 2: Use the multicam Context Manager (Recommended)

**For specific product line:**
```python
import pyrealsense2 as rs
from rspy import test

with test.multicam(2, "D400") as (dev1, dev2):
    # Your test code here
    # dev1 and dev2 are both D400 devices
    
    # Example: Open two pipelines simultaneously
    pipe1 = rs.pipeline()
    pipe2 = rs.pipeline()
    
    cfg1 = rs.config()
    cfg1.enable_device(dev1.get_info(rs.camera_info.serial_number))
    cfg1.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
    
    cfg2 = rs.config()
    cfg2.enable_device(dev2.get_info(rs.camera_info.serial_number))
    cfg2.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
    
    pipe1.start(cfg1)
    pipe2.start(cfg2)
    
    # Stream from both devices...
    
    pipe1.stop()
    pipe2.stop()
```

**For any two devices (product-agnostic):**
```python
from rspy import test

with test.multicam(2) as (dev1, dev2):
    # Your test code here
    # dev1 and dev2 can be different product lines (D400 + D500, etc.)
    
    # Find common stream profiles supported by both devices
    # and test only those
```

### Alternative: Use Helper Function Directly

For tests needing more control:

```python
from rspy import test

dev1, dev2, sns = test.find_two_devices_by_product_line_or_exit("D400")

# Your test code with manual device management
```

## Behavior in Different Environments

### With 2+ Devices Connected
- Test runs normally
- Two unique devices are allocated
- Both devices can stream simultaneously

### With 1 Device Connected
- Test **fails**
- Clear message: "Test requires 2 devices; found 1 device(s)"
- Exit status is non-zero (failure)

### With 0 Devices Connected
- Test **fails**
- Clear message: "Test requires 2 devices; found 0 device(s)"
- Exit status is non-zero (failure)

### In CI/CD Pipelines
- Tests requiring 2 devices won't break single-device environments
- Test run continues with other tests
- No false failures in CI

## Advanced Configuration

### Any Two Devices (Product-Agnostic)
Use wildcard to accept any two RealSense devices:
```python
#test:device * *
```
This configuration makes tests work with:
- D400 + D400 (same model)
- D435 + D455 (different D400 models)
- D400 + D500 (different product lines)
- Any combination of two RealSense devices

### Different Device Types
Request one D400 and one D455:
```python
#test:device D400* D455
```

Request one D400 and one D500:
```python
#test:device D400* D500*
```

### With Exclusions
Request two D400 devices, excluding D457:
```python
#test:device D400* D400* !D457
```

### Each Pattern
Run test once for each available D400 device (single device per run):
```python
#test:device each(D400*)
```

Note: `each()` is for single-device iteration, not for two-device tests.

## Running the Multi-Device Tests

### To test with the examples:

```bash
# Navigate to unit-tests directory
cd unit-tests

# Run the device enumeration test (works with all connected devices)
python run-unit-tests.py -r test-devices-enumeration

# Run the multi-stream test (works with all connected devices)
python run-unit-tests.py -r test-devices-streaming

# Run with verbose output to see device discovery
python run-unit-tests.py -r test-devices-streaming -v

# Run with debug output
python run-unit-tests.py -r test-devices-streaming --debug

# Run all multicam tests
python run-unit-tests.py -r devices
```

### Expected Output (with multiple devices):
```
Running test-devices-streaming
  Testing 3 devices...
...
4 unit-test(s) completed successfully
```

### Expected Output (with 1 device):
```
Running test-devices-streaming
Test requires 2 devices; found 1 device(s)
...
1 unit-test(s) FAILED!!!!!
```

## Design Decisions

### Why Context Manager?
- Pythonic and familiar pattern
- Automatic resource management
- Clean syntax reduces boilerplate

### Why Integrate with Devices Module?
- **Acroname Hub Support**: The devices module manages port enabling/disabling through Acroname hubs
- Without integration, multicam tests would fail when devices are connected through a hub
- Uses global device context instead of creating new rs.context() instances
- Proper port management and device lifecycle handling
- Consistent with single-device test infrastructure

### Why Fail When Devices Missing?
- Ensures test requirements are explicitly met
- Prevents false positives in CI/CD environments
- Clear signal when hardware setup is insufficient

### Why Repeat Device Spec?
- Clear and explicit intent (need 2 of same type)
- Leverages existing configuration syntax
- Backward compatible with all existing tests

### Why Wildcard `*` Support?
- Enables product-agnostic tests
- Same test code works with different device combinations
- Useful for testing cross-device functionality (sync, etc.)
- Reduces test maintenance for multi-product features

### Why No New Directive?
- Minimal changes to existing infrastructure
- Reuses proven device allocation logic
- Easier to maintain

## Files Modified/Created

1. **unit-tests/py/rspy/test.py**
   - Added `find_devices_or_exit()` function (integrates with devices module)
   - Added `multiple_devices` context manager class (supports any number of devices)
   - Added `find_two_devices_by_product_line_or_exit()` function (integrates with devices module, backward compatibility)
   - Added `find_any_two_devices_or_exit()` function (product-agnostic, integrates with devices module, backward compatibility)
   - **Removed**: `two_devices` context manager class (replaced by `multiple_devices(2, ...)` for clarity)
   - **Key Change**: All functions use `devices` module instead of creating new `rs.context()` for Acroname hub support

2. **unit-tests/py/rspy/devices.py**
   - Enhanced `by_configuration()` to support repeated device specs
   - Added wildcard `*` support in `by_spec()` for any-device matching
   - Updated documentation for two-device support

3. **unit-tests/multicam/test-devices-enumeration.py**
   - Device enumeration and validation test
   - Works with all connected devices
   - Minimal example for getting started

4. **unit-tests/multicam/test-devices-streaming.py**
   - Multi-stream simultaneous test (depth + color + IR)
   - Product-agnostic (works with all connected devices)
   - Frame drop detection and stream independence verification
   - Works with Acroname hubs

5. **unit-tests/multicam/multicam_tests_readme.md** (THIS FILE)
   - Complete documentation and user guide

## Backward Compatibility

✅ All existing single-device tests continue to work unchanged

✅ No changes required to existing test files

✅ Test runner behavior unchanged for single-device tests

✅ Configuration syntax remains the same

## How to Write a Multi-Device Test (N Devices)

### Step 1: Add Test Configuration Directive

At the top of your test file, specify how many devices are needed by repeating the spec.

**For N devices of same product line (e.g., three D400 devices):**
```python
#test:device D400* D400* D400*
```

**For N devices of any product line:**
```python
#test:device * * * *    # Four devices
```

**For mixed product lines:**
```python
#test:device D400* D400* D500*    # Two D400 + one D500
```

### Step 2: Use the multiple_devices Context Manager (Recommended)

```python
import pyrealsense2 as rs
from rspy import test

# For specific product line (3 D400 devices):
with test.multicam(3, "D400") as devices:
    dev1, dev2, dev3 = devices
    # Your test code here
    
    # Or iterate over all devices:
    for i, dev in enumerate(devices, 1):
        sn = dev.get_info(rs.camera_info.serial_number)
        print(f"Device {i}: {sn}")

# For any N devices (product-agnostic):
with test.multicam(4) as devices:
    # devices is a list of 4 device handles
    for dev in devices:
        # Test each device
        pass
```

### Alternative: Use find_devices_or_exit Directly

```python
from rspy import test

# Get 3 D400 devices
devices, sns = test.find_devices_or_exit(3, "D400")

# Get any 5 devices
devices, sns = test.find_devices_or_exit(5)

# Your test code with manual device management
```

## How to Write an All-Devices Test (Dynamic Count)

For tests that should work with ALL connected devices, regardless of count.

### Step 1: Add Test Configuration Directive

Use wildcard to test all devices together:

```python
#test:device *              # All devices of any type
#test:device D400*          # All D400 devices
```

**Note:** Don't use `each(*)` - that runs the test separately for each device. Use `*` to test all devices together in a single run.

### Step 2: Query and Test All Devices

**Recommended Pattern:**
```python
import pyrealsense2 as rs
from rspy import test, log, devices

# Ensure device database is populated
if not devices.all():
    devices.query(recycle_ports=False)

# Get all enabled devices
all_sns = list(devices.enabled())
device_count = len(all_sns)

log.i(f"Found {device_count} connected device(s)")

if device_count == 0:
    log.e("No devices connected - test cannot proceed")
    test.fail()
else:
    with test.closure("Test all connected devices"):
        for i, sn in enumerate(all_sns, 1):
            dev = devices.get(sn).handle
            
            # Test each device
            name = dev.get_info(rs.camera_info.name)
            log.i(f"Testing device {i}: {name} (SN: {sn})")
            
            # Your test logic here...
```

**Alternative: Use multiple_devices with Dynamic Count**
```python
from rspy import test, devices

# Query device count first
if not devices.all():
    devices.query(recycle_ports=False)

device_count = len(list(devices.enabled()))

if device_count > 0:
    # Use infrastructure with dynamic count
    with test.multicam(device_count) as devs:
        for i, dev in enumerate(devs, 1):
            # Test each device
            pass
```

### When to Use All-Devices Tests

- Exhaustive hardware validation across entire fleet
- Regression testing that should catch issues on any device
- Performance benchmarking across all available hardware
- Tests that don't require a specific device count

## Behavior in Different Environments

Potential future improvements (not implemented):

- Support for N devices (3+): `#test:device D400* D400* D400*`
- Helper for specific device pairs: `test.device_pair(sn1, sn2)`
- Automatic device reset between test cases
- Device pool management for parallel testing

## Questions or Issues?

For questions about:
- **Using the infrastructure**: See the example test and this README
- **Bugs or unexpected behavior**: Check error messages; they should be clear
- **Contributing new features**: Follow the existing patterns in test.py and devices.py
