# Two-Device Test Infrastructure for librealsense

## Overview

This documentation describes the infrastructure added to support unit tests that require two RealSense devices connected simultaneously.

## What Was Added

### 1. Test Helper Functions (unit-tests/py/rspy/test.py)

#### `find_two_devices_by_product_line_or_exit(product_line)`
Finds exactly two devices of the specified product line and ensures they have different serial numbers.

**Parameters:**
- `product_line`: The product line to search for (e.g., `rs.product_line.D400`)

**Returns:**
- Tuple of `(dev1, dev2, context)` where dev1 and dev2 are two different devices

**Behavior:**
- If fewer than 2 devices are found, the test exits gracefully (skip, not fail)
- Displays clear message: "Test requires 2 devices; found X device(s)"
- Verifies devices have different serial numbers

**Example:**
```python
import pyrealsense2 as rs
from rspy import test

dev1, dev2, ctx = test.find_two_devices_by_product_line_or_exit(rs.product_line.D400)
# dev1 and dev2 are guaranteed to be different devices
```

#### `find_any_two_devices_or_exit()`
Finds any two RealSense devices regardless of product line, ensuring they have different serial numbers.

**Parameters:**
- None

**Returns:**
- Tuple of `(dev1, dev2, context)` where dev1 and dev2 are two different devices (may be different product lines)

**Example:**
```python
from rspy import test

dev1, dev2, ctx = test.find_any_two_devices_or_exit()
# dev1 and dev2 can be D400 + L500, D435 + D455, etc.
```

#### `two_devices` Context Manager Class
Provides a clean context manager interface for two-device tests with automatic device discovery and cleanup.

**Usage (Same Product Line):**
```python
import pyrealsense2 as rs
from rspy import test

# Find two D400 devices
with test.two_devices(rs.product_line.D400) as (dev1, dev2):
    # Both devices are D400 series
    sn1 = dev1.get_info(rs.camera_info.serial_number)
    sn2 = dev2.get_info(rs.camera_info.serial_number)
    test.check(sn1 != sn2)
```

**Usage (Any Two Devices):**
```python
from rspy import test

# Find any two RealSense devices (can be different product lines)
with test.two_devices() as (dev1, dev2):
    # dev1 and dev2 can be D400 + L500, D435 + D455, etc.
    # Your test code here
```

**Benefits:**
- Clean syntax with automatic resource management
- Graceful failure handling
- Pythonic and familiar pattern
- Flexible: works with same or different product lines

### 2. Device Configuration Enhancement (unit-tests/py/rspy/devices.py)

#### Updated `by_configuration()` and `by_spec()` Functions
Enhanced to support multiple devices of the same type by allowing repeated device specifications, plus wildcard support for any device.

**Example Configurations:**
```python
#test:device D400* D400*    # Two D400 devices
#test:device * *            # Any two devices (can be different product lines)
#test:device D400* L500*    # One D400 and one L500
```

**Wildcard Support:**
- `*` alone = any/all devices (useful for product-agnostic tests)
- `D400*` = all D400 series devices
- `L500*` = all L500 series devices

**How It Works:**
- Each device spec in the configuration is matched to a unique device
- Repeating a spec (e.g., `D400*` twice) requests multiple devices of that type
- Devices are guaranteed to be unique (same device won't be matched twice)
- `*` matches any available device, making tests work across product lines

**Backward Compatibility:**
- Existing single-device tests continue to work unchanged
- Mixed configurations work: `#test:device D400* D455` (one D400, one D455)

### 3. Test Runner Integration (run-unit-tests.py)

**No changes required!** The existing test runner automatically handles two-device tests:

- Tests with insufficient devices are skipped (not failed)
- Clear warning messages indicate why tests were skipped
- Exit status remains 0 when tests are skipped due to missing hardware

### 4. Example Tests

#### unit-tests/live/d400/test-two-devices-streaming.py
A comprehensive example demonstrating:
- Two-device discovery and validation
- Simultaneous streaming from two devices
- Sequential streaming (stop one, start another)
- Independent sensor configuration
- Extensive inline documentation serves as a template
- D400-specific (requires two D400 devices)

#### unit-tests/live/frames/test-two-devices-frame-drops.py
Advanced frame drop detection test demonstrating:
- Product-agnostic testing (works with ANY two devices)
- Frame counter metadata analysis
- Simultaneous multi-sensor streaming
- Testing all common resolutions and frame rates
- Automatic adaptation to different device combinations

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
#test:device D400* L500*
```

The repeated spec (`D400*` appears twice) tells the infrastructure to allocate TWO devices matching the D400 pattern.

### Step 2: Use the Context Manager (Recommended)

**For specific product line:**
```python
import pyrealsense2 as rs
from rspy import test

with test.two_devices(rs.product_line.D400) as (dev1, dev2):
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

with test.two_devices() as (dev1, dev2):
    # Your test code here
    # dev1 and dev2 can be different product lines (D400 + L500, etc.)
    
    # Find common stream profiles supported by both devices
    # and test only those
```

### Alternative: Use Helper Function Directly

For tests needing more control:

```python
import pyrealsense2 as rs
from rspy import test

dev1, dev2, ctx = test.find_two_devices_by_product_line_or_exit(rs.product_line.D400)

# Your test code with manual device management
```

## Behavior in Different Environments

### With 2+ Devices Connected
- Test runs normally
- Two unique devices are allocated
- Both devices can stream simultaneously

### With 1 Device Connected
- Test is **skipped** (not failed)
- Clear message: "Test requires 2 devices; found 1 device(s)"
- Exit status remains 0 (success)

### With 0 Devices Connected
- Test is **skipped** (not failed)
- Clear message: "Test requires 2 devices; found 0 device(s)"
- Exit status remains 0 (success)

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
- D400 + L500 (different product lines)
- Any combination of two RealSense devices

### Different Device Types
Request one D400 and one D455:
```python
#test:device D400* D455
```

Request one D400 and one L500:
```python
#test:device D400* L500*
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

## Testing the Changes

### To test with the examples:

```bash
# Navigate to unit-tests directory
cd unit-tests

# Run the D400-specific streaming test (requires 2 D400 devices)
python run-unit-tests.py -r test-two-devices-streaming

# Run the product-agnostic frame drops test (works with any 2 devices)
python run-unit-tests.py -r test-two-devices-frame-drops

# Run with verbose output to see device discovery
python run-unit-tests.py -r test-two-devices-streaming -v

# Run with debug output
python run-unit-tests.py -r test-two-devices-frame-drops --debug
```

### Expected Output (with 2 devices):
```
Running test-live-d400-two-devices-streaming
  [D435_123456789 D435_987654321]
...
4 unit-test(s) completed successfully
```

### Expected Output (with 1 device):
```
Warning: test-live-d400-two-devices-streaming: Test requires 2 devices; found 1 device(s)
0 unit-test(s) completed successfully
```

## Design Decisions

### Why Context Manager?
- Pythonic and familiar pattern
- Automatic resource management
- Clean syntax reduces boilerplate

### Why Skip Instead of Fail?
- Tests shouldn't fail due to hardware unavailability
- Allows running test suite in various environments
- CI/CD friendly (single-device build machines)

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
   - Added `find_two_devices_by_product_line_or_exit()` function
   - Added `find_any_two_devices_or_exit()` function (product-agnostic)
   - Added `two_devices` context manager class (supports optional product_line parameter)

2. **unit-tests/py/rspy/devices.py**
   - Enhanced `by_configuration()` to support repeated device specs
   - Added wildcard `*` support in `by_spec()` for any-device matching
   - Updated documentation for two-device support

3. **unit-tests/live/d400/test-two-devices-streaming.py** (NEW)
   - Comprehensive example demonstrating all features
   - Serves as template for future two-device tests
   - Includes extensive inline documentation
   - D400-specific test

4. **unit-tests/live/frames/test-two-devices-frame-drops.py** (NEW)
   - Advanced frame drop detection test
   - Product-agnostic (works with any two devices)
   - Tests frame counter metadata
   - Multi-sensor and multi-resolution testing

5. **unit-tests/TWO_DEVICE_TESTS.md** (NEW)
   - Complete documentation and user guide

## Backward Compatibility

✅ All existing single-device tests continue to work unchanged

✅ No changes required to existing test files

✅ Test runner behavior unchanged for single-device tests

✅ Configuration syntax remains the same

## Future Enhancements

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
