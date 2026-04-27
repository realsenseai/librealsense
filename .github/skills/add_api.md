# Adding New API to librealsense

## Overview
This skill documents best practices for adding new API functionality to librealsense

## Core Principles

### 1. API Additions Only - No Modifications
- **ONLY ADD** new API elements (functions, structures, enums)
- **NEVER DELETE** or modify existing API without explicit user approval
- Preserve backward compatibility at all costs
- Even when code appears incomplete, verify with user before any deletions

### 2. Minimal, Focused Changes
- Add only what's requested
- Don't create unnecessary summary or documentation files
- Keep changes surgical and targeted
- Use existing patterns and conventions

### 3. Proper File Locations

#### API Headers (`include/librealsense2/`)
- C headers: `include/librealsense2/h/*.h`
- C++ headers: `include/librealsense2/hpp/*.hpp`

#### Python Wrappers (`wrappers/python/`)
- **ALWAYS add Python bindings** when adding new C/C++ API
- Modify relevant files in `wrappers/python/`:
  - `pyrs_frame.cpp` - for frame-related APIs
  - `pyrs_device.cpp` - for device-related APIs
  - `pyrs_sensor.cpp` - for sensor-related APIs
- Use pybind11 syntax to expose C++ API to Python
- Follow existing naming conventions (snake_case for Python)
- Test Python bindings after implementation

#### Examples (`examples/`)
- Prefer **C++** examples (project standard)
- Place in appropriate subdirectory (e.g., `examples/object-detection/`)
- Include CMakeLists.txt for build integration
- Use clear, minimal code demonstrating the API
- **Optional**: Create Python example in `wrappers/python/examples/` if the feature is commonly used from Python

#### Documentation (`doc/`)
- Only create if truly necessary (usually not needed)
- Use `.md` format
- Place at repository root: `doc/feature-name.md`
- **NEVER** place in `build/` folder (not version controlled)

#### Internal Implementation (`src/`)
- Implementation files in appropriate subdirectories
- Follow existing naming conventions (kebab-case for files)

### 4. License Headers
- **Always use current year** (2026, not 2025 or earlier)
- Standard format:
  ```cpp
  // License: Apache 2.0. See LICENSE file in root directory.
  // Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
  ```

## Step-by-Step Process

### Phase 1: API Design
1. **Read existing API files first**
   - Understand current patterns
   - Use `get_file` to examine similar APIs
   - Check for naming conventions

2. **Add to public headers**
   - Add enum values to existing enums (at end of enum but before `_COUNT`)
   - Add structures at appropriate locations
   - Add function declarations
   - **Verify with git diff** that only additions were made

3. **Create C++ wrapper if needed**
   - New file: `include/librealsense2/hpp/rs_<feature>.hpp`
   - Follow existing wrapper patterns
   - Use inline functions when appropriate

### Phase 2: Verification
1. **Use git to verify changes**
   ```bash
   git diff --stat <file>
   # Should show: X insertions(+), 0 deletions(-)
   ```

2. **If deletions appear:**
   - STOP immediately
   - `git checkout <file>` to restore
   - Re-apply changes more carefully
   - Never assume the edit tool preserved content

### Phase 3: Examples (Instead of Documentation)
1. **Create example application**
   ```
   examples/<feature-name>/
   ├── CMakeLists.txt
   ├── rs-<feature-name>.cpp
   └── README.md (optional, brief)
   ```

2. **Example should demonstrate:**
   - Basic API usage
   - Common use cases
   - Best practices
   - Error handling

3. **Keep it simple:**
   - 100-200 lines of code
   - Clear comments
   - No external dependencies beyond librealsense

### Phase 4: Python Bindings
1. **Identify the appropriate wrapper file**
   - Frame-related: `wrappers/python/pyrs_frame.cpp`
   - Device-related: `wrappers/python/pyrs_device.cpp`
   - Sensor-related: `wrappers/python/pyrs_sensor.cpp`

2. **Add Python bindings using pybind11**
   - Expose new structures as Python classes
   - Expose new functions as Python methods
   - Use snake_case naming for Python
   - Add docstrings for Python help()

3. **Test Python bindings**
   - Build pyrealsense2 module
   - Test in Python interpreter
   - Verify functionality matches C++/C API

### Phase 5: Implementation
1. Implement C API functions in `src/rs.cpp`
2. Add internal structures/classes as needed
3. Build and test

## Anti-Patterns to Avoid

### ❌ DON'T: Create Summary Files
```
docs/object-detection-api-summary.cpp  // WRONG - unnecessary
docs/implementation-guide.cpp          // WRONG - use examples instead
```

### ❌ DON'T: Use build/ Directory
```
build/docs/anything.md                 // WRONG - not version controlled
```

### ❌ DON'T: Modify Existing API
```cpp
// WRONG - modifying existing structure
typedef struct rs2_intrinsics {
    int width;
    int height;
    float ppx;
    float ppy;
    // DON'T add new fields here without approval!
} rs2_intrinsics;
```

### ❌ DON'T: Assume Edit Tool Preserves Content
- Always verify with `git diff`
- If in doubt, restore and redo
- Better safe than accidentally deleting code

## Correct Patterns

### ✅ DO: Add New Enums
```cpp
typedef enum rs2_frame_metadata_value {
    // ...existing values...
    RS2_FRAME_METADATA_EXISTING_LAST,
    RS2_FRAME_METADATA_NEW_FEATURE,     // ← ADD HERE
    RS2_FRAME_METADATA_COUNT            // ← Keep at end
} rs2_frame_metadata_value;
```

### ✅ DO: Add New Structures
```cpp
// At end of appropriate header file, before closing braces
typedef struct rs2_new_feature {
    int field1;
    float field2;
} rs2_new_feature;
```

### ✅ DO: Add New Functions
```cpp
// With full documentation
/**
 * Description of what the function does
 * \param[in] param1  Description
 * \param[out] error  Error handling
 * \return Description of return value
 */
int rs2_new_function(const rs2_frame* frame, rs2_error** error);
```

### ✅ DO: Create Example Application
```cpp
// examples/new-feature/rs-new-feature.cpp
// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <iostream>

int main() {
    try {
        // Demonstrate the new API
        rs2::pipeline pipe;
        pipe.start();
        
        while (true) {
            rs2::frameset frames = pipe.wait_for_frames();
            // Use new API here
        }
    }
    catch (const rs2::error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

## Verification Checklist

Before considering work complete:

- [ ] `git diff --stat` shows **only additions** (no deletions)
- [ ] All new files have 2026 copyright year
- [ ] No files created in `build/` directory
- [ ] Examples created instead of documentation (when needed)
- [ ] C++ example code preferred over C
- [ ] **Python bindings added** to appropriate `pyrs_*.cpp` file
- [ ] API follows existing naming conventions
- [ ] No modifications to existing API structures/functions
- [ ] Build completes successfully (including Python module)

## Common Scenarios

### Adding Metadata Fields
1. Add enum to `rs2_frame_metadata_value` (before `_COUNT`)
2. Implement parsing in appropriate backend
3. Create example showing how to read the metadata
4. Done - no documentation needed

### Adding New Frame Type
1. Add to `rs2_extension` enum
2. Create structure for frame data
3. Add accessor functions
4. Create example application
5. Implement in relevant backends

### Adding Option/Control
1. Add to `rs2_option` enum (if needed)
2. Add get/set functions (if needed)
3. Implement in device/sensor classes
4. Create example showing usage

## Quick Reference

| What to Add | Where | Notes |
|-------------|-------|-------|
| C API declarations | `include/librealsense2/h/*.h` | Only additions |
| C++ wrappers | `include/librealsense2/hpp/*.hpp` | Inline when possible |
| Python bindings | `wrappers/python/pyrs_*.cpp` | Always required |
| Examples | `examples/<feature>/` | C++ preferred |
| Python examples | `wrappers/python/examples/` | Optional |
| Documentation | `doc/*.md` | Only if truly necessary |
| Implementation | `src/**/*.cpp` | Follow existing structure |

## Remember

- **When in doubt, ASK** before modifying existing code
- **Always verify** with git diff
- **Examples over documentation** - show, don't tell
- **Current year** in license headers (2026)
- **No build/ directory** files in version control
