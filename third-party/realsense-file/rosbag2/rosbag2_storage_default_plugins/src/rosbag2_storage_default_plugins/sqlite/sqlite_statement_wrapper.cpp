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

#include "rosbag2_storage_default_plugins/sqlite/sqlite_statement_wrapper.hpp"

#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "rcutils/logging_macros.h"
#include "rosbag2_storage/ros_helper.hpp"

#include "rosbag2_storage_default_plugins/sqlite/sqlite_exception.hpp"

namespace rosbag2_storage_plugins
{

SqliteStatementWrapper::SqliteStatementWrapper(sqlite3 * database, const std::string & query)
{
  sqlite3_stmt * statement;
  int return_code = sqlite3_prepare_v2(database, query.c_str(), -1, &statement, nullptr);
  if (return_code != SQLITE_OK) {
    std::stringstream errmsg;
    errmsg << "Error when preparing SQL statement '" << query << "'. SQLite error: (" <<
      return_code << "): " << sqlite3_errstr(return_code);

    throw SqliteException{errmsg.str()};
  }

  statement_ = statement;
  last_bound_parameter_index_ = 0;
}

SqliteStatementWrapper::~SqliteStatementWrapper()
{
  if (statement_) {
    sqlite3_finalize(statement_);
  }
}

std::shared_ptr<SqliteStatementWrapper> SqliteStatementWrapper::execute_and_reset()
{
  int return_code = sqlite3_step(statement_);
  if (!is_query_ok(return_code)) {
    std::stringstream errmsg;
    errmsg << "Error when processing SQL statement. SQLite error (" <<
      return_code << "): " << sqlite3_errstr(return_code);

    throw SqliteException{errmsg.str()};
  }
  return reset();
}

bool SqliteStatementWrapper::is_query_ok(int return_code)
{
  return return_code == SQLITE_OK || return_code == SQLITE_DONE || return_code == SQLITE_ROW;
}

std::shared_ptr<SqliteStatementWrapper> SqliteStatementWrapper::bind(int value)
{
  auto return_code = sqlite3_bind_int(statement_, ++last_bound_parameter_index_, value);
  check_and_report_bind_error(return_code, value);
  return shared_from_this();
}

std::shared_ptr<SqliteStatementWrapper>
SqliteStatementWrapper::bind(rcutils_time_point_value_t value)
{
  auto return_code = sqlite3_bind_int64(statement_, ++last_bound_parameter_index_, value);
  check_and_report_bind_error(return_code, value);
  return shared_from_this();
}

std::shared_ptr<SqliteStatementWrapper> SqliteStatementWrapper::bind(double value)
{
  auto return_code = sqlite3_bind_double(statement_, ++last_bound_parameter_index_, value);
  check_and_report_bind_error(return_code, value);
  return shared_from_this();
}

std::shared_ptr<SqliteStatementWrapper> SqliteStatementWrapper::bind(const std::string & value)
{
  auto return_code = sqlite3_bind_text(
    statement_, ++last_bound_parameter_index_, value.c_str(), -1, SQLITE_TRANSIENT);
  check_and_report_bind_error(return_code, value);
  return shared_from_this();
}

std::shared_ptr<SqliteStatementWrapper>
SqliteStatementWrapper::bind(std::shared_ptr<rcutils_uint8_array_t> value)
{
  written_blobs_cache_.push_back(value);
  auto return_code = sqlite3_bind_blob(
    statement_, ++last_bound_parameter_index_,
    value->buffer, static_cast<int>(value->buffer_length), SQLITE_STATIC);
  check_and_report_bind_error(return_code);
  return shared_from_this();
}

std::shared_ptr<SqliteStatementWrapper> SqliteStatementWrapper::reset()
{
  sqlite3_reset(statement_);
  sqlite3_clear_bindings(statement_);
  last_bound_parameter_index_ = 0;
  written_blobs_cache_.clear();
  return shared_from_this();
}

bool SqliteStatementWrapper::step()
{
  int return_code = sqlite3_step(statement_);
  if (return_code == SQLITE_ROW) {
    return true;
  } else if (return_code == SQLITE_DONE) {
    return false;
  } else {
    std::stringstream errmsg;
    errmsg << "Error reading SQL query. SQLite error (" <<
      return_code << "): " << sqlite3_errstr(return_code);

    throw SqliteException{errmsg.str()};
  }
}

void SqliteStatementWrapper::obtain_column_value(size_t index, int & value) const
{
  value = sqlite3_column_int(statement_, static_cast<int>(index));
}

void
SqliteStatementWrapper::obtain_column_value(size_t index, rcutils_time_point_value_t & value) const
{
  value = sqlite3_column_int64(statement_, static_cast<int>(index));
}

void SqliteStatementWrapper::obtain_column_value(size_t index, double & value) const
{
  value = sqlite3_column_double(statement_, static_cast<int>(index));
}

void SqliteStatementWrapper::obtain_column_value(size_t index, std::string & value) const
{
  value = reinterpret_cast<const char *>(sqlite3_column_text(statement_, static_cast<int>(index)));
}

void SqliteStatementWrapper::obtain_column_value(
  size_t index, std::shared_ptr<rcutils_uint8_array_t> & value) const
{
  auto data = sqlite3_column_blob(statement_, static_cast<int>(index));
  auto size = static_cast<size_t>(sqlite3_column_bytes(statement_, static_cast<int>(index)));
  value = rosbag2_storage::make_serialized_message(data, size);
}

void SqliteStatementWrapper::check_and_report_bind_error(int return_code)
{
  if (return_code != SQLITE_OK) {
    std::stringstream errmsg;
    errmsg << "Error when binding SQL parameter " <<
      last_bound_parameter_index_ << ". SQLite error (" <<
      return_code << "): " << sqlite3_errstr(return_code);

    throw SqliteException{errmsg.str()};
  }
}

}  // namespace rosbag2_storage_plugins
