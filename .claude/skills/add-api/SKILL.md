---
name: add-api
description: Add new public API to the librealsense SDK (C/C++ headers, Windows exports, Python bindings, examples) without breaking existing API. Use before adding any function, struct, enum, option or metadata field to include/librealsense2.
---

# Adding New API to librealsense

## Rules

- **Add, don't change.** Never remove or change the signature, struct layout or enum values of existing public API without explicit user approval — the C API keeps ABI compatibility within a major version.
- **Enums**: add new values just before the `*_COUNT` entry; never renumber existing ones.
- Add only what was asked. No summary/design files; use an example instead of a doc unless one is really needed (`doc/<feature>.md`).
- New files get the copyright header with the current year (see the `file-creation` skill).

## Checklist — new C function

1. **Declare** in the matching C header `include/librealsense2/h/rs_*.h`, with a doc comment and a trailing `rs2_error** error` parameter.
2. **Implement** in `src/rs.cpp` using the API-call wrappers, which turn exceptions into `rs2_error`:
   ```cpp
   rs2_processing_block* rs2_create_mjpeg_decoder(rs2_error** error) BEGIN_API_CALL
   {
       return new rs2_processing_block{ std::make_shared<mjpeg_converter>(RS2_FORMAT_RGB8) };
   }
   NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)
   ```
   Use `HANDLE_EXCEPTIONS_AND_RETURN(<default>, <args>...)` when the function takes arguments, and `VALIDATE_NOT_NULL` / `VALIDATE_RANGE` for inputs.
3. **Export** the symbol in `src/realsense.def` next to related functions. Windows exports only what is listed there — a missing entry builds fine but fails to link for users.
4. **C++ wrapper**: add the method/class in `include/librealsense2/hpp/rs_*.hpp`, calling the C function and `error::handle(e)`.
5. **Python**: bind it in the matching `wrappers/python/pyrs_*.cpp` (e.g. `pyrs_processing.cpp`, `pyrs_device.cpp`, `pyrs_sensor.cpp`, `pyrs_frame.cpp`) with snake_case names and a docstring.
6. **Example / test**: a short C++ example in `examples/<feature>/` (with `CMakeLists.txt`) when the feature is user-facing, and/or a pytest test (see the `pytest-infra` skill).
7. **Build** the library and `pyrealsense2`, and try the new API from Python.

## Checklist — new enum value (option, metadata, stream, format...)

1. Add the value in the header before `*_COUNT` (e.g. `RS2_OPTION_*` in `rs_option.h`, `RS2_FRAME_METADATA_*` in `rs_frame.h`).
2. Add its string in `src/to-string.cpp` (`CASE( NAME )` in the matching `*_to_string` switch).
3. Implement it where it applies (e.g. the device/sensor option registration under `src/ds/...`, or the metadata parser).
4. A **new enum type** also needs `src/core/enum-helpers.h`, its `rs2_<type>_to_string` declared, exported in `src/realsense.def`, and a Python binding in `wrappers/python/c_files.cpp`.

## Before Finishing

- `git diff` shows no changed signatures, struct layouts or enum values in `include/librealsense2/`.
- Every new C function is in `src/realsense.def`.
- Library and Python bindings build; the new API was exercised.
