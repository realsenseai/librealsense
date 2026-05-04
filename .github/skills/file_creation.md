# Safe Source File Generation

When creating new code files, it is critical to ensure they are placed in the project's source tree and NOT in build or temporary directories. 

## Protocol

### 1. Diagnose Environment
Before calling `create_file`, always inspect the Current Working Directory (CWD).

**Red Flags (Build Directory Indicators):**
*   Presence of `CMakeCache.txt`, `CMakeFiles/` folder.
*   Presence of `.sln`, `.vcxproj`, or `.o` / `.obj` files in the root of CWD.
*   The directory name is `build`, `out`, `bin`, or `Debug`/`Release`.
*   Presence of `ZERO_CHECK.vcxproj` or `ALL_BUILD.vcxproj`.

### 2. Locate Source Root
If you are in a build directory, identify the path to the Source Root.
*   Look for `.git/`, `LICENSE`, `README.md`, or the top-level `CMakeLists.txt`.
*   Calculate the relative path (e.g., `../` or `../../`).

### 3. Verify Target Path
Ensure the parent directory of the file you intend to create exists in the *Source* location.

### 4. Execute with Relative Paths
Use the calculated relative prefix when calling `create_file`.

*   Bad: `filePath: "examples/my_example.cpp"` (If CWD is `build/`, this pollutes the build artifact folder).
*   Good: `filePath: "../examples/my_example.cpp"` (Escapes build dir to reach the actual source tree).

## Required Copyright Header

Every new source/header file **must** begin with this exact two-line header:

```
// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) YEAR RealSense, Inc. All Rights Reserved.
```

**Critical rules:**
*   `YEAR` **must be the actual current calendar year** -- run `(Get-Date).Year` (PowerShell) or `date +%Y` (bash) to confirm the year if unsure.
*   **Never copy the year from a nearby or template file.** Template files may be years old.
*   Wrong: `// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.` (stale year copied from an existing file)
*   Right: `// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.` (actual current year -- adjust as needed)

## Important Tips
*   **Git Ignore Rule**: Never create files in folders that are typically git-ignored (like `build/`).
*   **Consistency**: If you see existing source files being referenced with `../` (e.g. `../src/main.cpp`), likely you are in a build folder and should follow that pattern for new files.
