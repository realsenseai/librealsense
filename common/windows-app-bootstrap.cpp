// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

// This file converts the call to WinMain to a call to cross-platform main
// We need WinMain on Windows to offer proper Windows application and not console application
// Should not be used in CMake on Linux


#include <Windows.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <iostream>
#include <csignal>

#include "os.h"
#include <rsutils/os/os.h>
#include "metadata-helper.h"
#include "rendering.h"
#include <rsutils/string/windows.h>

#include <delayimp.h>

#ifdef _MSC_VER
#pragma warning(disable: 4244) // Prevent warning for wchar->char string conversion
#endif

// Use OS hook to modify message box behaviour
// This lets us implement "report" functionality if Viewer is crashing
HHOOK hhk;
// When launching messagebox, replace standard Cancel button with "report"
LRESULT CALLBACK cbtproc(INT code, WPARAM wParam, LPARAM lParam)
{
    HWND window = (HWND)wParam;
    if (code == HCBT_ACTIVATE)
    {
        if (GetDlgItem(window, IDCANCEL) != NULL)
        {
            SetDlgItemText(window, IDCANCEL, L"Report");
        }
        UnhookWindowsHookEx(hhk);
    }
    else CallNextHookEx(hhk, code, wParam, lParam);
    return 0;
}

bool should_intercept(int code)
{
    if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP) return false;
    return true;
}

std::string exception_code_to_string(int code, struct _EXCEPTION_POINTERS *ep)
{
    if (code == EXCEPTION_ACCESS_VIOLATION) return "Access Violation!";
    else if (code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED) return "Array out of bounds access error!";
    else if (code == EXCEPTION_DATATYPE_MISALIGNMENT) return "Read / write of misaligned data!";
    else if (code == EXCEPTION_FLT_DENORMAL_OPERAND) return "Denormal floating point operation!";
    else if (code == EXCEPTION_FLT_DIVIDE_BY_ZERO) return "Unhandled floating point division by zero!";
    else if (code == EXCEPTION_FLT_INEXACT_RESULT) return "Inexact floating point result!";
    else if (code == EXCEPTION_FLT_INVALID_OPERATION) return "Invalid floating point operation!";
    else if (code == EXCEPTION_FLT_OVERFLOW) return "Floating point overflow!";
    else if (code == EXCEPTION_FLT_STACK_CHECK) return "Floating point stack overflow / underflow!";
    else if (code == EXCEPTION_FLT_UNDERFLOW) return "Floating point underflow!";
    else if (code == EXCEPTION_ILLEGAL_INSTRUCTION) return "Illegal CPU instruction!\nPossibly newer CPU architecture is required";
    else if (code == EXCEPTION_IN_PAGE_ERROR) return "In page error!\nPossibly network connection error when running the program over network";
    else if (code == EXCEPTION_INT_DIVIDE_BY_ZERO) return "Unhandled integer division by zero!";
    else if (code == EXCEPTION_INT_OVERFLOW) return "Integer overflow!";
    else if (code == EXCEPTION_INVALID_DISPOSITION) return "Invalid disposition error!";
    else if (code == EXCEPTION_NONCONTINUABLE_EXCEPTION) return "Noncontinuable exception occured!";
    else if (code == EXCEPTION_PRIV_INSTRUCTION) return "Error due to invalid call to priviledged instruction!";
    else if (code == EXCEPTION_STACK_OVERFLOW) return "Stack overflow error!";
    else if (code == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND)) {
        auto details = (DelayLoadInfo*)ep->ExceptionRecord->ExceptionInformation[0];
        std::string dll_name = details->szDll;
        std::string fname = details->dlp.szProcName;
        return rsutils::string::from() << "Could not find " << dll_name << " library required for " << fname
                                       << ".\nMake sure all program dependencies are reachable or download standalone "
                                          "version of the App from our GitHub";
    }
    else
        return rsutils::string::from() << "Unknown error (" << code << ")!";
}

// Show custom error message box with OK and Report options
// Report will prompt new GitHub issue
void report_error(std::string error)
{
    std::wstring ws(error.begin(), error.end());
    SetWindowsHookEx(WH_CBT, &cbtproc, 0, GetCurrentThreadId());
    auto button = MessageBox(NULL, ws.c_str(), L"Something went wrong...", MB_ICONERROR | MB_OKCANCEL);
    if (button == IDCANCEL)
    {
        std::stringstream ss;
        rs2_error* e = nullptr;

        ss << "| | |\n";
        ss << "|---|---|\n";
        ss << "|**librealsense**|" << rs2::api_version_to_string(rs2_get_api_version(&e)) << (rs2::is_debug() ? " DEBUG" : " RELEASE") << "|\n";
        ss << "|**OS**|" << rsutils::os::get_os_name() << "|\n\n";
        ss << "RealSense Viewer / Depth Quality Tool has crashed with the following error message:\n";
        ss << "```\n";
        ss << error;
        ss << "\n```";

        std::string link = "https://github.com/realsenseai/librealsense/issues/new?body=" + rs2::url_encode(ss.str());
        rs2::open_url(link.c_str());
    }
}

// Global crash handler will be triggered when exception escapes from thread
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep)
{
    auto code = ep->ExceptionRecord->ExceptionCode;

    if (should_intercept(code))
    {
        std::string error = "Unhandled exception escaping from a worker thread!\nError type: ";
        error += exception_code_to_string(code, ep);
        report_error(error);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

// Actual main - implemented in the cross-platform section of the App
int main(int argv, const char** argc);

int filter(unsigned int code, struct _EXCEPTION_POINTERS *ep)
{
    if (should_intercept(code))
    {
        auto error = exception_code_to_string(code, ep);
        std::cerr << "Program terminated due to an unrecoverable SEH exception:\n" << error;
        return EXCEPTION_EXECUTE_HANDLER;
    }
    else return EXCEPTION_CONTINUE_SEARCH;
}

// Wrapper around main. Must be in a function without destructors (that does not require stack unwinding)
int run_main(int argv, const char** argc)
{
    SetUnhandledExceptionFilter(CrashHandler);

    int res = 0;
    __try
    {
        res = main(argv, argc);
    }
    __except (filter(GetExceptionCode(), GetExceptionInformation()))
    {
        res = EXIT_FAILURE;
    }
    return res;
}

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_ HINSTANCE hPrevInstance,
    _In_ LPSTR     lpCmdLine,
    _In_ int       nCmdShow
)
{
    int argCount;

    std::shared_ptr<LPWSTR> szArgList(CommandLineToArgvW(GetCommandLine(), &argCount), LocalFree);
    if (szArgList == NULL) return main(0, nullptr);

    std::vector<std::string> args;
    for (int i = 0; i < argCount; i++)
    {
        std::string s = rsutils::string::windows::win_to_utf( szArgList.get()[i] );

        if (s == rs2::metadata_helper::get_command_line_param())
        {
            try
            {
                rs2::metadata_helper::instance().enable_metadata();
                exit(0);
            }
            catch (...) { exit(-1); }
        }

        args.push_back(s);
    }

    std::vector<const char*> argc;
    std::transform(args.begin(), args.end(), std::back_inserter(argc), [](const std::string& s) { return s.c_str(); });

    // Redirect CERR to string stream
    std::stringstream ss;
    std::cerr.rdbuf(ss.rdbuf());

    int res = run_main(static_cast<int>(argc.size()), argc.data());
    
    if (res == EXIT_FAILURE)
    {
        report_error(ss.str());
    }

    return res;
}