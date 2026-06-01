// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:add-file ../../common/rs-config.cpp

#include <unit-tests/catch.h>
#include <common/rs-config.h>
#include <rsutils/os/special-folder.h>

#include <string>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif


namespace {

static std::string make_test_config_path()
{
#ifdef _WIN32
    std::string pid = std::to_string( GetCurrentProcessId() );
#else
    std::string pid = std::to_string( getpid() );
#endif
    auto temp_dir = rsutils::os::get_special_folder( rsutils::os::special_folder::temp_folder );
    return temp_dir + "rs_config_test_" + pid + ".json";
}

// RAII wrapper: removes the file at path on destruction
struct scoped_file
{
    std::string path;

    explicit scoped_file( std::string p )
        : path( std::move( p ) )
    {
    }

    ~scoped_file() { std::remove( path.c_str() ); }
};

}  // namespace


// Verify that a value written via set() is persisted to disk and correctly
// loaded by a fresh config_file instance.
TEST_CASE( "config/regular_update", "[common]" )
{
    std::string path = make_test_config_path();
    scoped_file guard( path );

    {
        rs2::config_file cfg( path );
        cfg.set( "greeting", "hello_world" );
        // set() calls save() internally: writes a .pid.tmp file then atomically renames it
    }

    rs2::config_file reloaded( path );
    CHECK( reloaded.contains( "greeting" ) );
    CHECK( reloaded.get( "greeting", "" ) == std::string( "hello_world" ) );
}
