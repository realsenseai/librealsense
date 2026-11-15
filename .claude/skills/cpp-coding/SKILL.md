---
name: cpp-coding
description: C++ coding standards for librealsense - language standard, naming, RAII, error handling, comments. Use before adding or modifying any C++ code.
---

# C++ Coding Standards & Practices Skill

Use this protocol to ensure all C++ code modifications align with the project's engineering standards.

## Protocol

### 1. Enforce Language Standard
*   **Standard**: The core library must compile with **C++14** compatibility (`cxx_std_14` — see `CMake/lrs_macros.cmake`). The public interface requires **C++11** (`cxx_std_11`). Examples and wrappers generally use **C++11**
*   **Safety**: Prefer strict Types over `auto` (unless type is obvious or iterator).
*   **No Macros**: Avoid C-style macros unless absolutely necessary.

### 2. Applied Naming Conventions
*   **General**: `snake_case` for variables, functions, classes and namespaces (e.g. `uvc_device`, `get_device_count()`).
*   **Files**: kebab-case (e.g. `backend-v4l2.h`, `device-model.cpp`).
*   **Constants**: `UPPER_CASE_WITH_UNDERSCORES`.
*   **Members**: Prefix with `_` (e.g., `_my_variable`).
*   **Templates**: `PascalCase` for type parameters (e.g., `template<typename T>`).
*   **Namespaces**: `librealsense` (core), `librealsense::platform` (platform layer); public C API types and enums use the `rs2_` prefix.
*   **Suffixes**: `*_interface` (abstract bases, e.g. `device_interface`), `*_factory`, `*_callback`, `*_model` (viewer UI classes).

### 3. Resource Management (RAII)
*   **Pointers**: Use `std::unique_ptr` by default. Use `std::shared_ptr` *only* when ownership is truly shared.
*   **Allocation**: Avoid raw `new`/`delete`. Use `std::make_unique` / `std::make_shared`.
*   **Value Semantics**: Prefer stack allocation and value semantics where possible.

### 4. Error Handling
*   **Exceptions**: Inside `src/`, throw the library exception types from `src/librealsense-exception.h` (e.g. `invalid_value_exception`, `wrong_api_call_sequence_exception`, `not_implemented_exception`); the C API wrappers translate them into `rs2_error`. `rs2::error` is the public C++ wrapper's type, used only in `include/librealsense2/hpp/` and client code.
*   **Silence**: Never fail silently. Log errors or throw exceptions.

### 5. Modification Rules
*   **Style**: Do **NOT** reformat existing code solely for style. Only apply these rules to *new* or *refactored* lines.
*   **Consistency**: Match the surrounding code style if it deviates from the standard (local consistency wins for minor edits).

### 6. Performance Checks
*   **Hot Paths**: Check for hidden allocations in loops or streaming callbacks.
*   **Copying**: Minimize data copying; use const references (`const T&`) for non-primitive arguments.

### 7. Comments
*   **Comments should be sparse.** Keep to 1–2 lines unless the code is large and genuinely complex.
*   **Intention:** Don't restate what the code already says, comment should convey the reason for the code.
*   **Locality:** The comment should refer to the local code. e.g. class description should not mention inheritors or where other logic live.
*   **Length:** Comment lines can be as long as the surrounding code. If code lines usually trim at 120 characters comment lines don't have to be 80 characters long.
*   **Generality:** Don't reference internal session reasoning e.g. "as you instructed" 
