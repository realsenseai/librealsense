// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include <rsutils/os/special-folder.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <locale.h>

#ifdef _WIN32

#include <rsutils/os/hresult.h>
#include <KnownFolders.h>
#include <shlobj.h>

#elif defined __linux__ || defined __APPLE__

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#endif
#include <stdexcept>


namespace rsutils {
namespace os {


std::string get_special_folder( special_folder f )
{
    setlocale(LC_ALL, ".UTF8");
    std::string res;

#ifdef _WIN32


    if( f == special_folder::temp_folder )
    {
        char buf[MAX_PATH+2];  // "The maximum possible return value is MAX_PATH+1"
        auto len = GetTempPathA( sizeof( buf ), buf );  // "return the length of the string, not including the terminating null character"
        if( len > 0 )  // zero on failure
            res.assign( buf, len );
        else
            throw std::runtime_error( "failed to get temp folder" );
    }
    else if( f == special_folder::user_documents )
    {
        // The user's Documents folder location may get overridden, as we know OneDrive does in certain
        // circumstances. In such cases, the new function, SHGetKnownFolderPath, does not always return the new
        // path, while the deprecated function does.
        CHAR path[MAX_PATH];
        CHECK_HR( SHGetFolderPathA( NULL, CSIDL_PERSONAL, NULL, 0, path ) );
        res = path;
        res += "\\";
    }
    else
    {
        GUID folder;
        switch( f )
        {
        case special_folder::user_desktop:
            folder = FOLDERID_Desktop;
            break;
        case special_folder::user_documents:
            folder = FOLDERID_Documents;
            break;
        case special_folder::user_pictures:
            folder = FOLDERID_Pictures;
            break;
        case special_folder::user_videos:
            folder = FOLDERID_Videos;
            break;
        case special_folder::app_data:
            folder = FOLDERID_RoamingAppData;
            break;
        default:
            throw std::invalid_argument( "special_folder value (" + std::to_string( (int)f ) + ") is not supported" );
        }

        PWSTR path = NULL;
        HRESULT hr = SHGetKnownFolderPath( folder, KF_FLAG_DEFAULT_PATH, NULL, &path );
        CHECK_HR_STR( "SHGetKnownFolderPath", hr );  // throws runtime_error
        char str[1024];
        size_t len;
        auto error = wcstombs_s( &len, str, path, 1023 );
        CoTaskMemFree( path );
        if( error )
            throw std::runtime_error( "failed to convert special folder: errno=" + std::to_string( error ) );
        res = str;
        res += "\\";
    }


#elif defined __linux__ || defined __APPLE__


    if( f == special_folder::temp_folder )
    {
        const char * tmp_dir = getenv( "TMPDIR" );
        res = tmp_dir ? tmp_dir : "/tmp/";
    }
    else
    {
        const char * home_dir = getenv( "HOME" );
        if( ! home_dir )
        {
            struct passwd * pw = getpwuid( getuid() );
            if( pw && pw->pw_dir )
                home_dir = pw->pw_dir;
            else
                throw std::runtime_error( "failed to get special folder " + std::to_string( (int)f ) );
        }
        if( home_dir )
        {
            res = home_dir;
            switch( f )
            {
            case special_folder::user_desktop:
                res += "/Desktop/";
                break;
            case special_folder::user_documents:
                res += "/Documents/";
                break;
            case special_folder::user_pictures:
                res += "/Pictures/";
                break;
            case special_folder::user_videos:
                res += "/Videos/";
                break;
            case special_folder::app_data:
                res += "/.";
                break;
            default:
                throw std::invalid_argument( "special_folder value (" + std::to_string( (int) f ) + ") is not supported" );
            }
        }
    }


#endif  // defined __linux__ || defined __APPLE__
    return res;
}


}  // namespace os
}  // namespace rsutils
