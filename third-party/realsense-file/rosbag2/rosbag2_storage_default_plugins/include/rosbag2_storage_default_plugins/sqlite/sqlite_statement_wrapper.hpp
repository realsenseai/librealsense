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

#ifndef ROSBAG2_STORAGE_DEFAULT_PLUGINS__SQLITE__SQLITE_STATEMENT_WRAPPER_HPP_
#define ROSBAG2_STORAGE_DEFAULT_PLUGINS__SQLITE__SQLITE_STATEMENT_WRAPPER_HPP_

#include <sqlite3.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rosbag2_storage/serialized_bag_message.hpp"
#include "rosbag2_storage_default_plugins/sqlite/sqlite_exception.hpp"
#include "rosbag2_storage_default_plugins/visibility_control.hpp"

// This is necessary because of using stl types here. It is completely safe, because
// a) the member is not accessible from the outside
// b) there are no inline functions.
#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable:4251)
#endif

namespace rosbag2_storage_plugins
{

class ROSBAG2_STORAGE_DEFAULT_PLUGINS_PUBLIC SqliteStatementWrapper
  : public std::enable_shared_from_this<SqliteStatementWrapper>
{
public:
  SqliteStatementWrapper(sqlite3 * database, const std::string & query);
  SqliteStatementWrapper(const SqliteStatementWrapper &) = delete;
  SqliteStatementWrapper & operator=(const SqliteStatementWrapper &) = delete;
  ~SqliteStatementWrapper();

  template<typename ... Columns>
  class QueryResult
  {
public:
    using RowType = std::tuple<Columns ...>;
    class Iterator : public std::iterator<std::input_iterator_tag, RowType>
    {
public:
      static const int POSITION_END = -1;
      Iterator(std::shared_ptr<SqliteStatementWrapper> statement, int position)
      : statement_(statement), next_row_idx_(position), cached_row_idx_(POSITION_END - 1)
      {
        if (next_row_idx_ != POSITION_END) {
          if (statement_->step()) {
            ++next_row_idx_;
          } else {
            next_row_idx_ = POSITION_END;
          }
        }
      }

      Iterator(const Iterator &) = delete;
      Iterator & operator=(const Iterator &) = delete;

      Iterator(Iterator &&) = default;
      Iterator & operator=(Iterator &&) = default;

      Iterator & operator++()
      {
        if (next_row_idx_ != POSITION_END) {
          if (statement_->step()) {
            ++next_row_idx_;
          } else {
            next_row_idx_ = POSITION_END;
          }
          return *this;
        } else {
          throw SqliteException("Cannot increment result iterator beyond result set!");
        }
      }
      Iterator operator++(int)
      {
        auto old_value = *this;
        ++(*this);
        return old_value;
      }

      RowType operator*() const
      {
        if (next_row_idx_ == POSITION_END) {
          throw SqliteException("Cannot dereference iterator at end of result set!");
        }
        if (is_row_cache_valid()) {
          return row_cache_;
        }
        RowType row{};
        obtain_row_values(row);
        return row;
      }

      bool operator==(const Iterator & other) const
      {
        return statement_ == other.statement_ && next_row_idx_ == other.next_row_idx_;
      }
      bool operator!=(const Iterator & other) const
      {
        return !(*this == other);
      }

private:
      template<typename Indices = std::index_sequence_for<Columns ...>>
      void obtain_row_values(RowType & row) const
      {
        obtain_row_values_impl(row, Indices{});
        row_cache_ = row;
        cached_row_idx_ = next_row_idx_ - 1;
      }

      template<size_t I, size_t ... Is, typename RemainingIndices = std::index_sequence<Is ...>>
      void obtain_row_values_impl(RowType & row, std::index_sequence<I, Is ...>) const
      {
        statement_->obtain_column_value(I, std::get<I>(row));
        obtain_row_values_impl(row, RemainingIndices{});
      }
      void obtain_row_values_impl(RowType &, std::index_sequence<>) const {}  // end of recursion

      bool is_row_cache_valid() const
      {
        return cached_row_idx_ == next_row_idx_ - 1;
      }

      std::shared_ptr<SqliteStatementWrapper> statement_;
      int next_row_idx_;
      mutable int cached_row_idx_;
      mutable RowType row_cache_;
    };

    explicit QueryResult(std::shared_ptr<SqliteStatementWrapper> statement)
    : statement_(statement), is_already_accessed_(false)
    {}

    Iterator begin()
    {
      try_access_data();
      return Iterator(statement_, 0);
    }
    Iterator end()
    {
      return Iterator(statement_, Iterator::POSITION_END);
    }

    RowType get_single_line()
    {
      return *begin();
    }

private:
    void try_access_data()
    {
      if (is_already_accessed_) {
        throw SqliteException("Only one iterator per query result is supported!");
      }
      is_already_accessed_ = true;
    }

    std::shared_ptr<SqliteStatementWrapper> statement_;
    bool is_already_accessed_;
  };

  std::shared_ptr<SqliteStatementWrapper> execute_and_reset();
  template<typename ... Columns>
  QueryResult<Columns ...> execute_query();

  template<typename T1, typename T2, typename ... Params>
  std::shared_ptr<SqliteStatementWrapper> bind(T1 value1, T2 value2, Params ... values);
  std::shared_ptr<SqliteStatementWrapper> bind(int value);
  std::shared_ptr<SqliteStatementWrapper> bind(rcutils_time_point_value_t value);
  std::shared_ptr<SqliteStatementWrapper> bind(double value);
  std::shared_ptr<SqliteStatementWrapper> bind(const std::string & value);
  std::shared_ptr<SqliteStatementWrapper> bind(std::shared_ptr<rcutils_uint8_array_t> value);

  std::shared_ptr<SqliteStatementWrapper> reset();

private:
  bool step();
  bool is_query_ok(int return_code);

  void obtain_column_value(size_t index, int & value) const;
  void obtain_column_value(size_t index, rcutils_time_point_value_t & value) const;
  void obtain_column_value(size_t index, double & value) const;
  void obtain_column_value(size_t index, std::string & value) const;
  void obtain_column_value(size_t index, std::shared_ptr<rcutils_uint8_array_t> & value) const;

  template<typename T>
  void check_and_report_bind_error(int return_code, T value);
  void check_and_report_bind_error(int return_code);

  sqlite3_stmt * statement_;
  int last_bound_parameter_index_;
  std::vector<std::shared_ptr<rcutils_uint8_array_t>> written_blobs_cache_;
};

template<typename T1, typename T2, typename ... Params>
inline
std::shared_ptr<SqliteStatementWrapper>
SqliteStatementWrapper::bind(T1 value1, T2 value2, Params ... values)
{
  bind(value1);
  return bind(value2, values ...);
}

template<>
inline
void SqliteStatementWrapper::check_and_report_bind_error(int return_code, std::string value)
{
  if (return_code != SQLITE_OK) {
    throw SqliteException(
            "SQLite error when binding parameter " +
            std::to_string(last_bound_parameter_index_) + " to value '" + value +
            "'. Return code: " + std::to_string(return_code));
  }
}

template<typename T>
inline
void SqliteStatementWrapper::check_and_report_bind_error(int return_code, T value)
{
  check_and_report_bind_error(return_code, std::to_string(value));
}

template<typename ... Columns>
inline
SqliteStatementWrapper::QueryResult<Columns ...> SqliteStatementWrapper::execute_query()
{
  return QueryResult<Columns ...>(shared_from_this());
}

using SqliteStatement = std::shared_ptr<SqliteStatementWrapper>;

}  // namespace rosbag2_storage_plugins

#ifdef _WIN32
# pragma warning(pop)
#endif

#endif  // ROSBAG2_STORAGE_DEFAULT_PLUGINS__SQLITE__SQLITE_STATEMENT_WRAPPER_HPP_
