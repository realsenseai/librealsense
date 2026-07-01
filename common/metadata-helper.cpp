// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "metadata-helper.h"

#ifdef WIN32

#include <Windows.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <iterator>

#include <regex>
#include <iostream>
#include <sstream>
#include <functional>
#include <map>

#include <librealsense2/rs.hpp>
#include <rsutils/string/windows.h>

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#endif

namespace rs2
{
#ifdef WIN32
    struct device_id
    {
        std::string vid, pid, mi, guid, sn;
    };

    inline bool equal(const std::string& a, const std::string& b)
    {
        return strcasecmp(a.c_str(), b.c_str()) == 0;
    }

    inline bool operator==(const device_id& a, const device_id& b)
    {
        return equal(a.vid, b.vid) &&
               equal(a.pid, b.pid) &&
               equal(a.guid, b.guid) &&
               equal(a.mi, b.mi) &&
               equal(a.sn, b.sn);
    }

    class windows_metadata_helper : public metadata_helper
    {
        using super = metadata_helper;

    public:
        static bool parse_device_id(const std::string& id, device_id* res)
        {
            static const std::regex regex("vid_([0-9a-f]+)&pid_([0-9a-f]+)&mi_([0-9a-f]+)#[0-9a-f]&([0-9a-f]+)&[\\s\\S]*\\{([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\\}", std::regex_constants::icase);

            std::match_results<std::string::const_iterator> match;

            if (std::regex_search(id, match, regex) && match.size() > 5)
            {
                res->vid = match[1];
                res->pid = match[2];
                res->mi = match[3];
                res->sn = match[4];
                res->guid = match[5];
                return true;
            }
            return false;
        }

        static void foreach_device_path(const std::vector<device_id>& devices,
            std::function<void(const device_id&,  /* matched device */
                std::wstring /* registry key of Device Parameters for that device */)> action)
        {
            std::map<std::string, std::vector<device_id>> guid_to_devices;
            for (auto&& d : devices)
            {
                guid_to_devices[d.guid].push_back(d);
            }

            for (auto&& kvp : guid_to_devices)
            {
                auto guid = kvp.first;

                std::stringstream ss;
                ss << "SYSTEM\\CurrentControlSet\\Control\\DeviceClasses\\{" << guid << "}";
                auto s = ss.str();
                std::wstring prefix(s.begin(), s.end());

                HKEY key;
                if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, prefix.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS)
                {
                    // Don't forget to release in the end:
                    std::shared_ptr<void> raii(key, RegCloseKey);

                    TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name
                    DWORD    cchClassName = MAX_PATH;  // size of class string
                    DWORD    cSubKeys = 0;               // number of subkeys
                    DWORD    cbMaxSubKey;              // longest subkey size
                    DWORD    cchMaxClass;              // longest class string
                    DWORD    cValues;              // number of values for key
                    DWORD    cchMaxValue;          // longest value name
                    DWORD    cbMaxValueData;       // longest value data
                    DWORD    cbSecurityDescriptor; // size of security descriptor
                    FILETIME ftLastWriteTime;      // last write time

                    DWORD retCode = RegQueryInfoKey(
                        key,                    // key handle
                        achClass,                // buffer for class name
                        &cchClassName,           // size of class string
                        NULL,                    // reserved
                        &cSubKeys,               // number of subkeys
                        &cbMaxSubKey,            // longest subkey size
                        &cchMaxClass,            // longest class string
                        &cValues,                // number of values for this key
                        &cchMaxValue,            // longest value name
                        &cbMaxValueData,         // longest value data
                        &cbSecurityDescriptor,   // security descriptor
                        &ftLastWriteTime);       // last write time

                    for (auto i = 0ul; i < cSubKeys; i++)
                    {
                        TCHAR achKey[MAX_KEY_LENGTH];
                        DWORD cbName = MAX_KEY_LENGTH;
                        retCode = RegEnumKeyEx(key, i,
                            achKey,
                            &cbName,
                            NULL,
                            NULL,
                            NULL,
                            &ftLastWriteTime);
                        if (retCode == ERROR_SUCCESS)
                        {
                            std::wstring suffix = achKey;
                            device_id rdid;

                            std::string a = rsutils::string::windows::win_to_utf( suffix.c_str() );

                            if (parse_device_id(a, &rdid))
                            {
                                for (auto&& did : kvp.second)
                                {
                                    if (rdid == did)
                                    {
                                        std::wstringstream ss;
                                        ss << prefix << "\\" << suffix << "\\#GLOBAL\\Device Parameters";
                                        auto path = ss.str();
                                        action(rdid, path);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Heuristic that determines how many media-pins UVC device is expected to have
        static int number_of_mediapins(const std::string& vid, const std::string& pid, const std::string& mi)
        {
            // Generic to all RealSense vendor ID cameras:
            // Return the max metadata-key count on every interface (all MIs), for all cameras, so new PIDs need no update.
            // Unneeded pins create a very small overhead (~5KB per pin that exists but does not need metadata),
            // Current max count is 4 - depth+IR+color1+color2.
            static const std::string VID_REALSENSE_CAMERA = "38E5";
            if (equal(vid, VID_REALSENSE_CAMERA)) return 4;

            // Not changing previous (D400) cameras behavior
            if (mi == "00")
            {
                // D405 has 3 media-pins
                if (equal(pid, "0b5b")) return 3;
                else return 2; // D400 has two
            }
            return 1; // RGB has one
        }

        bool is_running_as_admin()
        {
            BOOL result = FALSE;
            PSID admin_group = NULL;

            SID_IDENTIFIER_AUTHORITY ntauthority = SECURITY_NT_AUTHORITY;
            if (!AllocateAndInitializeSid(
                &ntauthority,
                2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0,
                &admin_group))
            {
                rs2::log(RS2_LOG_SEVERITY_WARN, "Unable to query permissions - AllocateAndInitializeSid failed");
                return false;
            }
            std::shared_ptr<void> raii(admin_group, FreeSid);

            if (!CheckTokenMembership(NULL, admin_group, &result))
            {
                rs2::log(RS2_LOG_SEVERITY_WARN, "Unable to query permissions - CheckTokenMembership failed");
                return false;
            }

            return result == TRUE;
        }

        void enable_metadata_with_new_admin_process()
        {
            wchar_t szPath[MAX_PATH];
            if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)))
            {
                SHELLEXECUTEINFO sei = { sizeof(sei) };

                sei.lpVerb = L"runas";
                sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                sei.lpFile = szPath;
                sei.hwnd = NULL;
                sei.nShow = SW_NORMAL;
                auto cmd_line = get_command_line_param();
                std::wstring wcmd(cmd_line.begin(), cmd_line.end());
                sei.lpParameters = wcmd.c_str();

                if (!ShellExecuteEx(&sei))
                {
                    auto errstr = std::system_category().message(GetLastError());
                    std::string msg = "Unable to elevate to admin privilege to enable metadata! " + errstr;
                    rs2::log(RS2_LOG_SEVERITY_WARN, msg.c_str());
                }
                else
                {
                    WaitForSingleObject(sei.hProcess, INFINITE);
                    DWORD exitCode = 0;
                    GetExitCodeProcess(sei.hProcess, &exitCode);
                    CloseHandle(sei.hProcess);
                    if (exitCode)
                        throw std::runtime_error("Failed to set metadata registry keys!");
                }
            }
            else
            {
                rs2::log(RS2_LOG_SEVERITY_WARN, "Unable to fetch module name!");
            }
        }

        bool is_enabled(std::string id) const override
        {
            device_id did;
            if( ! parse_device_id( id, &did ) )
                // If it's not parsed, it's not a valid USB device; return the default
                return super::is_enabled( id );

            bool res = false;
            foreach_device_path({ did }, [&res, did](const device_id&, std::wstring path) {

                HKEY key;
                if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS)
                {
                    // Don't forget to release in the end:
                    std::shared_ptr<void> raii(key, RegCloseKey);

                    bool found = true;
                    DWORD len = sizeof(DWORD);//size of data

                    for (int i = 0; i < number_of_mediapins(did.vid, did.pid, did.mi); i++)
                    {
                        std::wstringstream ss; ss << L"MetadataBufferSizeInKB" << i;
                        std::wstring metadatakey = ss.str();

                        DWORD MetadataBufferSizeInKB = 0;
                        if (RegQueryValueEx(
                            key,
                            metadatakey.c_str(),
                            NULL,
                            NULL,
                            (LPBYTE)(&MetadataBufferSizeInKB),
                            &len) != ERROR_SUCCESS)
                            rs2::log(RS2_LOG_SEVERITY_DEBUG, "Unable to read metadata registry key!");

                        found = found && MetadataBufferSizeInKB;
                    }

                    if (found) res = true;
                }
            });

            return res;
        }

        void enable_metadata() override
        {
            if (!is_running_as_admin())
            {
                enable_metadata_with_new_admin_process();
            }
            else
            {
                std::vector<device_id> dids;

                rs2::context ctx;
                auto list = ctx.query_devices();
                for (uint32_t i = 0; i < list.size(); i++)
                {
                    try
                    {
                        rs2::device dev = list[i];

                        if (dev.supports(RS2_CAMERA_INFO_PRODUCT_LINE))
                        {
                            std::string product = dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE);
                            if (can_support_metadata(product))
                            {
                                for (auto sen : dev.query_sensors())
                                {
                                    if (sen.supports(RS2_CAMERA_INFO_PHYSICAL_PORT))
                                    {
                                        std::string port = sen.get_info(RS2_CAMERA_INFO_PHYSICAL_PORT);
                                        device_id did;
                                        if (parse_device_id(port, &did))
                                            dids.push_back(did);
                                    }
                                }
                            }
                        }
                    }
                    catch (...) {}
                }

                bool failure = false;
                foreach_device_path(dids, [&failure](const device_id& did, std::wstring path) {
                    HKEY key;
                    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_WRITE | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS)
                    {
                        // Don't forget to release in the end:
                        std::shared_ptr<void> raii(key, RegCloseKey);

                        bool found = true;
                        DWORD len = sizeof(DWORD);//size of data

                        for (int i = 0; i < number_of_mediapins(did.vid, did.pid, did.mi); i++)
                        {
                            std::wstringstream ss; ss << L"MetadataBufferSizeInKB" << i;
                            std::wstring metadatakey = ss.str();

                            DWORD MetadataBufferSizeInKB = 5;
                            if (RegSetValueEx(key, metadatakey.c_str(), 0, REG_DWORD,
                                (const BYTE*)&MetadataBufferSizeInKB, sizeof(DWORD)) != ERROR_SUCCESS)
                            {
                                rs2::log(RS2_LOG_SEVERITY_DEBUG, "Unable to write metadata registry key!");
                                failure = true;
                            }
                        }
                    }
                    });
                if (failure) throw std::runtime_error("Unable to write to metadata registry key!");
            }
        }
    };
#endif

    metadata_helper& metadata_helper::instance()
    {
#ifdef WIN32
        static windows_metadata_helper instance;
#else
        static metadata_helper instance;
#endif
        return instance;
    }
}
