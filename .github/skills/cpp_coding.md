# C++ Coding Standards & Practices Skill

Use this protocol to ensure all C++ code modifications align with the project's engineering standards.

## Protocol

### 1. Enforce Language Standard
*   **Standard**: The core library need to complie with **C++14** compatibility (`cxx_std_14` — see `CMake/lrs_macros.cmake`). The public interface requires **C++11** (`cxx_std_11`). Examples and wrappers generally use **C++11**
*   **Safety**: Prefer strict Types over `auto` (unless type is obvious or iterator).
*   **No Macros**: Avoid C-style macros unless absolutely necessary.

### 2. Applied Naming Conventions
*   **General**: `snake_case` for almost everything (variables, functions, classes, namespaces, files).
*   **Constants**: `UPPER_CASE_WITH_UNDERSCORES`.
*   **Members**: Prefix with `_` (e.g., `_my_variable`).
*   **Templates**: `PascalCase` for types (e.g., `template<typename T>`).

### 3. Resource Management (RAII)
*   **Pointers**: Use `std::unique_ptr` by default. Use `std::shared_ptr` *only* when ownership is truly shared.
*   **Allocation**: Avoid raw `new`/`delete`. Use `std::make_unique` / `std::make_shared`.
*   **Value Semantics**: Prefer stack allocation and value semantics where possible.

### 4. Error Handling
*   **Exceptions**: Propagate errors explicitly. Use `throw rs2::error(...)` or `throw std::runtime_error(...)` for fatal states.
*   **Silence**: Never fail silently. Log errors or throw exceptions.

### 5. Modification Rules
*   **Style**: Do **NOT** reformat existing code solely for style. Only apply these rules to *new* or *refactored* lines.
*   **Consistency**: Match the surrounding code style if it deviates from the standard (local consistency wins for minor edits).

### 6. Performance Checks
*   **Hot Paths**: Check for hidden allocations in loops or streaming callbacks.
*   **Copying**: Minimize data copying; use const references (`const T&`) for non-primitive arguments.
