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

#include "rosbag2_storage_default_plugins/sqlite/sqlite_storage.hpp"

#include <sys/stat.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rcpputils/filesystem_helper.hpp"

#include "rosbag2_storage/metadata_io.hpp"
#include "rosbag2_storage/serialized_bag_message.hpp"
#include "rosbag2_storage_default_plugins/sqlite/sqlite_statement_wrapper.hpp"
#include "rosbag2_storage_default_plugins/sqlite/sqlite_exception.hpp"

#include "../logging.hpp"

namespace
{
std::string to_string(rosbag2_storage::storage_interfaces::IOFlag io_flag)
{
  switch (io_flag) {
    case rosbag2_storage::storage_interfaces::IOFlag::APPEND:
      return "APPEND";
    case rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY:
      return "READ_ONLY";
    case rosbag2_storage::storage_interfaces::IOFlag::READ_WRITE:
      return "READ_WRITE";
    default:
      return "UNKNOWN";
  }
}

bool is_read_write(const rosbag2_storage::storage_interfaces::IOFlag io_flag)
{
  return io_flag == rosbag2_storage::storage_interfaces::IOFlag::READ_WRITE;
}

constexpr const auto FILE_EXTENSION = ".db3";

// Minimum size of a sqlite3 database file in bytes (84 kiB).
constexpr const uint64_t MIN_SPLIT_FILE_SIZE = 86016;
}  // namespace

namespace rosbag2_storage_plugins
{
SqliteStorage::~SqliteStorage()
{
  if (active_transaction_) {
    commit_transaction();
  }
}

void SqliteStorage::open(
  const std::string & uri, rosbag2_storage::storage_interfaces::IOFlag io_flag)
{
  if (is_read_write(io_flag)) {
    relative_path_ = uri + FILE_EXTENSION;

    // READ_WRITE requires the DB to not exist.
    if (rcpputils::fs::path(relative_path_).exists()) {
      throw std::runtime_error(
              "Failed to create bag: File '" + relative_path_ + "' already exists!");
    }
  } else {  // APPEND and READ_ONLY
    relative_path_ = uri;

    // APPEND and READ_ONLY require the DB to exist
    if (!rcpputils::fs::path(relative_path_).exists()) {
      throw std::runtime_error(
              "Failed to read from bag: File '" + relative_path_ + "' does not exist!");
    }
  }

  try {
    database_ = std::make_unique<SqliteWrapper>(relative_path_, io_flag);
  } catch (const SqliteException & e) {
    throw std::runtime_error("Failed to setup storage. Error: " + std::string(e.what()));
  }

  // initialize only for READ_WRITE since the DB is already initialized if in APPEND.
  if (is_read_write(io_flag)) {
    initialize();
  }

  // Reset the read and write statements in case the database changed.
  // These will be reinitialized lazily on the first read or write.
  read_statement_ = nullptr;
  write_statement_ = nullptr;

  ROSBAG2_STORAGE_DEFAULT_PLUGINS_LOG_INFO_STREAM(
    "Opened database '" << relative_path_ << "' for " << to_string(io_flag) << ".");
}

void SqliteStorage::activate_transaction()
{
  if (active_transaction_) {
    return;
  }

  ROSBAG2_STORAGE_DEFAULT_PLUGINS_LOG_DEBUG_STREAM("begin transaction");
  database_->prepare_statement("BEGIN TRANSACTION;")->execute_and_reset();

  active_transaction_ = true;
}

void SqliteStorage::commit_transaction()
{
  if (!active_transaction_) {
    return;
  }

  ROSBAG2_STORAGE_DEFAULT_PLUGINS_LOG_DEBUG_STREAM("commit transaction");
  database_->prepare_statement("COMMIT;")->execute_and_reset();

  active_transaction_ = false;
}

void SqliteStorage::write(std::shared_ptr<const rosbag2_storage::SerializedBagMessage> message)
{
  if (!write_statement_) {
    prepare_for_writing();
  }
  auto topic_entry = topics_.find(message->topic_name);
  if (topic_entry == end(topics_)) {
    throw SqliteException(
            "Topic '" + message->topic_name +
            "' has not been created yet! Call 'create_topic' first.");
  }

  write_statement_->bind(message->time_stamp, topic_entry->second, message->serialized_data);
  write_statement_->execute_and_reset();
}

void SqliteStorage::write(
  const std::vector<std::shared_ptr<const rosbag2_storage::SerializedBagMessage>> & messages)
{
  if (!write_statement_) {
    prepare_for_writing();
  }

  activate_transaction();

  for (auto & message : messages) {
    write(message);
  }

  commit_transaction();
}

bool SqliteStorage::has_next()
{
  if (!read_statement_) {
    prepare_for_reading();
  }

  return current_message_row_ != message_result_.end();
}

std::shared_ptr<rosbag2_storage::SerializedBagMessage> SqliteStorage::read_next()
{
  if (!read_statement_) {
    prepare_for_reading();
  }

  auto bag_message = std::make_shared<rosbag2_storage::SerializedBagMessage>();
  bag_message->serialized_data = std::get<0>(*current_message_row_);
  bag_message->time_stamp = std::get<1>(*current_message_row_);
  bag_message->topic_name = std::get<2>(*current_message_row_);

  ++current_message_row_;
  return bag_message;
}

std::vector<rosbag2_storage::TopicMetadata> SqliteStorage::get_all_topics_and_types()
{
  if (all_topics_and_types_.empty()) {
    fill_topics_and_types();
  }

  return all_topics_and_types_;
}

uint64_t SqliteStorage::get_bagfile_size() const
{
  const auto bag_path = rcpputils::fs::path{get_relative_file_path()};

  return bag_path.exists() ? bag_path.file_size() : 0u;
}

void SqliteStorage::initialize()
{
  std::string create_stmt = "CREATE TABLE topics(" \
    "id INTEGER PRIMARY KEY," \
    "name TEXT NOT NULL," \
    "type TEXT NOT NULL," \
    "serialization_format TEXT NOT NULL," \
    "offered_qos_profiles TEXT NOT NULL);";
  database_->prepare_statement(create_stmt)->execute_and_reset();
  create_stmt = "CREATE TABLE messages(" \
    "id INTEGER PRIMARY KEY," \
    "topic_id INTEGER NOT NULL," \
    "timestamp INTEGER NOT NULL, " \
    "data BLOB NOT NULL);";
  database_->prepare_statement(create_stmt)->execute_and_reset();
  create_stmt = "CREATE INDEX timestamp_idx ON messages (timestamp ASC);";
  database_->prepare_statement(create_stmt)->execute_and_reset();
}

void SqliteStorage::create_topic(const rosbag2_storage::TopicMetadata & topic)
{
  if (topics_.find(topic.name) == std::end(topics_)) {
    auto insert_topic =
      database_->prepare_statement(
      "INSERT INTO topics (name, type, serialization_format, offered_qos_profiles) "
      "VALUES (?, ?, ?, ?)");
    insert_topic->bind(
      topic.name, topic.type, topic.serialization_format, topic.offered_qos_profiles);
    insert_topic->execute_and_reset();
    topics_.emplace(topic.name, static_cast<int>(database_->get_last_insert_id()));
  }
}

void SqliteStorage::remove_topic(const rosbag2_storage::TopicMetadata & topic)
{
  if (topics_.find(topic.name) != std::end(topics_)) {
    auto delete_topic =
      database_->prepare_statement(
      "DELETE FROM topics where name = ? and type = ? and serialization_format = ?");
    delete_topic->bind(topic.name, topic.type, topic.serialization_format);
    delete_topic->execute_and_reset();
    topics_.erase(topic.name);
  }
}

void SqliteStorage::prepare_for_writing()
{
  write_statement_ = database_->prepare_statement(
    "INSERT INTO messages (timestamp, topic_id, data) VALUES (?, ?, ?);");
}

void SqliteStorage::prepare_for_reading()
{
  if (!storage_filter_.topics.empty()) {
    // Construct string for selected topics
    std::string topic_list{""};
    for (auto & topic : storage_filter_.topics) {
      topic_list += "'" + topic + "'";
      if (&topic != &storage_filter_.topics.back()) {
        topic_list += ",";
      }
    }

    read_statement_ = database_->prepare_statement(
      "SELECT data, timestamp, topics.name "
      "FROM messages JOIN topics ON messages.topic_id = topics.id "
      "WHERE topics.name IN (" + topic_list + ")"
      "ORDER BY messages.timestamp;");
  } else {
    read_statement_ = database_->prepare_statement(
      "SELECT data, timestamp, topics.name "
      "FROM messages JOIN topics ON messages.topic_id = topics.id "
      "ORDER BY messages.timestamp;");
  }
  message_result_ = read_statement_->execute_query<
    std::shared_ptr<rcutils_uint8_array_t>, rcutils_time_point_value_t, std::string>();
  current_message_row_ = message_result_.begin();
}

void SqliteStorage::fill_topics_and_types()
{
  auto statement = database_->prepare_statement(
    "SELECT name, type, serialization_format FROM topics ORDER BY id;");
  auto query_results = statement->execute_query<std::string, std::string, std::string>();

  for (auto result : query_results) {
    all_topics_and_types_.push_back(
      {std::get<0>(result), std::get<1>(result), std::get<2>(result), ""});
  }
}

std::string SqliteStorage::get_storage_identifier() const
{
  return "sqlite3";
}

std::string SqliteStorage::get_relative_file_path() const
{
  return relative_path_;
}

uint64_t SqliteStorage::get_minimum_split_file_size() const
{
  return MIN_SPLIT_FILE_SIZE;
}

rosbag2_storage::BagMetadata SqliteStorage::get_metadata()
{
  rosbag2_storage::BagMetadata metadata;
  metadata.storage_identifier = get_storage_identifier();
  metadata.relative_file_paths = {get_relative_file_path()};

  metadata.message_count = 0;
  metadata.topics_with_message_count = {};

  auto statement = database_->prepare_statement(
    "SELECT name, type, serialization_format, COUNT(messages.id), MIN(messages.timestamp), "
    "MAX(messages.timestamp) "
    "FROM messages JOIN topics on topics.id = messages.topic_id "
    "GROUP BY topics.name;");
  auto query_results = statement->execute_query<
    std::string, std::string, std::string, int, rcutils_time_point_value_t,
    rcutils_time_point_value_t>();

  rcutils_time_point_value_t min_time = INT64_MAX;
  rcutils_time_point_value_t max_time = 0;
  for (auto result : query_results) {
    metadata.topics_with_message_count.push_back(
      {
        {std::get<0>(result), std::get<1>(result), std::get<2>(result), ""},
        static_cast<size_t>(std::get<3>(result))
      });

    metadata.message_count += std::get<3>(result);
    min_time = std::get<4>(result) < min_time ? std::get<4>(result) : min_time;
    max_time = std::get<5>(result) > max_time ? std::get<5>(result) : max_time;
  }

  if (metadata.message_count == 0) {
    min_time = 0;
    max_time = 0;
  }

  metadata.starting_time =
    std::chrono::time_point<std::chrono::high_resolution_clock>(std::chrono::nanoseconds(min_time));
  metadata.duration = std::chrono::nanoseconds(max_time) - std::chrono::nanoseconds(min_time);
  metadata.bag_size = get_bagfile_size();

  return metadata;
}

void SqliteStorage::set_filter(
  const rosbag2_storage::StorageFilter & storage_filter)
{
  storage_filter_ = storage_filter;
}

void SqliteStorage::reset_filter()
{
  storage_filter_ = rosbag2_storage::StorageFilter();
}

}  // namespace rosbag2_storage_plugins

//#include "pluginlib/class_list_macros.hpp"  // NOLINT
//PLUGINLIB_EXPORT_CLASS(
//  rosbag2_storage_plugins::SqliteStorage,
//  rosbag2_storage::storage_interfaces::ReadWriteInterface)
