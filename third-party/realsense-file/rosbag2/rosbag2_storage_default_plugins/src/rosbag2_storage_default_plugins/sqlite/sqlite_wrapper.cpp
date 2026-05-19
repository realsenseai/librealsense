// Copyright 2018, Bosch Software Innovations GmbH.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rosbag2_storage_default_plugins/sqlite/sqlite_wrapper.hpp"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "rcutils/types.h"
#include "rosbag2_storage/serialized_bag_message.hpp"

#include "rosbag2_storage_default_plugins/sqlite/sqlite_exception.hpp"

#include "../logging.hpp"

namespace rosbag2_storage_plugins
{

SqliteWrapper::SqliteWrapper(
  const std::string & uri, rosbag2_storage::storage_interfaces::IOFlag io_flag)
: db_ptr(nullptr)
{
  if (io_flag == rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY) {
    int rc = sqlite3_open_v2(
      uri.c_str(), &db_ptr,
      SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr);
    if (rc != SQLITE_OK) {
      std::stringstream errmsg;
      errmsg << "Could not read-only open database. SQLite error (" <<
        rc << "): " << sqlite3_errstr(rc);
      throw SqliteException{errmsg.str()};
    }
    // throws an exception if the database is not valid.
    prepare_statement("PRAGMA schema_version;")->execute_and_reset();
  } else {
    int rc = sqlite3_open_v2(
      uri.c_str(), &db_ptr,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);
    if (rc != SQLITE_OK) {
      std::stringstream errmsg;
      errmsg << "Could not read-write open database. SQLite error (" <<
        rc << "): " << sqlite3_errstr(rc);
      throw SqliteException{errmsg.str()};
    }
    // RealSense recording performance optimizations:
    // The original rosbag2 settings (WAL + synchronous=NORMAL) cause periodic FPS drops
    // when recording multiple streams, because SQLite blocks on disk flushes.
    //
    // MEMORY journal: SQLite uses a "journal" — a temporary copy of data before changes —
    //   to recover from crashes. By default this journal is a file on disk.
    //   MEMORY keeps it in RAM instead, avoiding disk I/O for journal writes.
    //   Risk: if the process crashes (killed/segfault) or computer loses power
    //   mid-recording, the .db3 file may be corrupt — but for sensor recording
    //   you'd just re-record.
    prepare_statement("PRAGMA journal_mode = MEMORY;")->execute_and_reset();
    // synchronous=OFF: don't wait for the OS to confirm data is written to disk (no fsync).
    //   Data goes to the OS cache and gets flushed to disk in the background.
    //   Same tradeoff as above: power loss could corrupt the file, but that's acceptable
    //   for sensor recording where you'd just re-record.
    prepare_statement("PRAGMA synchronous = OFF;")->execute_and_reset();
    // Larger page cache (64MB instead of the default ~2MB).
    //   Keeps more data in memory, reducing disk reads during write-heavy workloads.
    //   Negative value means the size is in KB.
    prepare_statement("PRAGMA cache_size = -64000;")->execute_and_reset();
  }

  sqlite3_extended_result_codes(db_ptr, 1);
}

SqliteWrapper::SqliteWrapper()
: db_ptr(nullptr) {}

SqliteWrapper::~SqliteWrapper()
{
  const int rc = sqlite3_close(db_ptr);
  if (rc != SQLITE_OK) {
    ROSBAG2_STORAGE_DEFAULT_PLUGINS_LOG_ERROR_STREAM(
      "Could not close open database. Error code: " << rc <<
        " Error message: " << sqlite3_errstr(rc));
  }
}

SqliteStatement SqliteWrapper::prepare_statement(const std::string & query)
{
  return std::make_shared<SqliteStatementWrapper>(db_ptr, query);
}

size_t SqliteWrapper::get_last_insert_id()
{
  return sqlite3_last_insert_rowid(db_ptr);
}

SqliteWrapper::operator bool()
{
  return db_ptr != nullptr;
}

}  // namespace rosbag2_storage_plugins
