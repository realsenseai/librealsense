// Copyright (c) 2019, Open Source Robotics Foundation, Inc.
// All rights reserved.
//
// Software License Agreement (BSD License 2.0)
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//  * Neither the name of the copyright holders nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This file is originally from:
// https://github.com/ros/pluginlib/blob/1a4de29fa55173e9b897ca8ff57ebc88c047e0b3/pluginlib/include/pluginlib/impl/filesystem_helper.hpp

/*! \file filesystem_helper.hpp
 * \brief Cross-platform filesystem helper functions and additional emulation of [std::filesystem](https://en.cppreference.com/w/cpp/filesystem).
 *
 * If std::filesystem is not available the necessary functions are emulated.
 * Note: Once std::filesystem is supported on all ROS2 platforms, this class
 * can be deprecated in favor of the built-in functionality.
 */

#ifndef RCPPUTILS__FILESYSTEM_HELPER_HPP_
#define RCPPUTILS__FILESYSTEM_HELPER_HPP_

#include <limits.h>
#include <sys/stat.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

/**
 * \def RCPPUTILS_IMPL_OS_DIRSEP
 *
 * A definition for this platforms string path separator
 */

#ifdef _WIN32
#  define RCPPUTILS_IMPL_OS_DIRSEP '\\'
#else
#  define RCPPUTILS_IMPL_OS_DIRSEP '/'
#endif

#ifdef _WIN32
#  include <windows.h>
#  include <direct.h>
#  include <fileapi.h>
#  include <io.h>
#  define access _access_s
#else
#  include <dirent.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

#include "rcpputils/split.hpp"

namespace rcpputils
{
namespace fs
{

static constexpr const char kPreferredSeparator = RCPPUTILS_IMPL_OS_DIRSEP;

/**
 * \brief Drop-in replacement for [std::filesystem::path](https://en.cppreference.com/w/cpp/filesystem/path).
 *
 * It must conform to the same standard described and cannot include methods that are not
 * incorporated there.
 */
class path
{
public:
  /**
    * \brief Constructs an empty path.
    */
  path()
  : path("")
  {}

  /**
   * \brief Conversion constructor from a std::string path.
   *
   * \param p A string path split by the platform's string path separator.
   */
  path(const std::string & p)  // NOLINT(runtime/explicit): this is a conversion constructor
  : path_(p), path_as_vector_(split(p, kPreferredSeparator))
  {
    std::replace(path_.begin(), path_.end(), '\\', kPreferredSeparator);
    std::replace(path_.begin(), path_.end(), '/', kPreferredSeparator);
  }

  /**
    * \brief Copy constructor.
    */
  path(const path & p) = default;

  /**
   * \brief Get the path delimited using this system's path separator.
   *
   * \return The path as a string
   */
  std::string string() const
  {
    return path_;
  }

  /**
   * \brief Check if this path exists.
   *
   * \return True if the path exists, false otherwise.
   */
  bool exists() const
  {
    return access(path_.c_str(), 0) == 0;
  }

  /**
   * \brief Check if the path exists and it is a directory.
   *
   * \return True if the path is an existing directory, false otherwise.
   */
  bool is_directory() const noexcept
  {
    struct stat stat_buffer;
    const auto rc = stat(path_.c_str(), &stat_buffer);

    if (rc != 0) {
      return false;
    }

#ifdef _WIN32
    return (stat_buffer.st_mode & S_IFDIR) == S_IFDIR;
#else
    return S_ISDIR(stat_buffer.st_mode);
#endif
  }

  /**
   * \brief Check if the path is a regular file.
   *
   * \return True if the file is an existing regular file, false otherwise.
   */
  bool is_regular_file() const noexcept
  {
    struct stat stat_buffer;
    const auto rc = stat(path_.c_str(), &stat_buffer);

    if (rc != 0) {
      return false;
    }

#ifdef _WIN32
    return (stat_buffer.st_mode & S_IFREG) == S_IFREG;
#else
    return S_ISREG(stat_buffer.st_mode);
#endif
  }

  /**
  * \brief Return the size of the file in bytes.
  *
  * \return size of file in bytes
  * \throws std::system_error
  */
  uint64_t file_size() const
  {
    if (is_directory()) {
      auto ec = std::make_error_code(std::errc::is_a_directory);
      throw std::system_error{ec, "cannot get file size"};
    }

    struct stat stat_buffer;
    const auto rc = stat(path_.c_str(), &stat_buffer);

    if (rc != 0) {
      std::error_code ec{errno, std::system_category()};
      errno = 0;
      throw std::system_error{ec, "cannot get file size"};
    } else {
      return static_cast<uint64_t>(stat_buffer.st_size);
    }
  }

  /**
  * \brief Check if the path is empty.
  *
  * \return True if the path is empty, false otherwise.
  */
  bool empty() const
  {
    return path_.empty();
  }

  /**
  * \brief Check if the path is an absolute path.
  *
  * \return True if the path is absolute, false otherwise.
  */
  bool is_absolute() const
  {
    return path_.size() > 0 &&
           (path_.compare(0, 1, std::string(1, kPreferredSeparator)) == 0 ||
           this->is_absolute_with_drive_letter());
  }

  /**
  * \brief Const iterator to first element of this path.
  *
  * \return A const iterator to the first element.
  */
  std::vector<std::string>::const_iterator cbegin() const
  {
    return path_as_vector_.cbegin();
  }

  /**
  * Const iterator to one past the last element of this path.
  *
  * return A const iterator to one past the last element of the path.
  */
  std::vector<std::string>::const_iterator cend() const
  {
    return path_as_vector_.cend();
  }

  /**
  * \brief Get the parent directory of this path.
  *
  * \return A path to the parent directory.
  */
  path parent_path() const
  {
    // Edge case: empty path
    if (this->empty()) {
      return path("");
    }

    // Edge case: if path only consists of one part, then return '.' or '/'
    //            depending if the path is absolute or not
    if (1u == path_as_vector_.size()) {
      if (this->is_absolute()) {
        // Windows is tricky, since an absolute path may start with 'C:\\' or '\\'
        if (this->is_absolute_with_drive_letter()) {
          return path(path_as_vector_[0] + kPreferredSeparator);
        }
        return path(std::string(1, kPreferredSeparator));
      }
      return path(".");
    }

    // Edge case: with a path 'C:\\foo' we want to return 'C:\\' not 'C:'
    // Don't drop the root directory from an absolute path on Windows starting with a letter drive
    if (2u == path_as_vector_.size() && this->is_absolute_with_drive_letter()) {
      return path(path_as_vector_[0] + kPreferredSeparator);
    }

    path parent;
    for (auto it = this->cbegin(); it != --this->cend(); ++it) {
      if (!parent.empty() || it->empty()) {
        parent /= *it;
      } else {
        parent = *it;
      }
    }
    return parent;
  }

  /**
  * \brief Get the last element in this path.
  *
  * If this path points to a directory, it will return the directory name.
  *
  * \return The last element in this path
  */
  path filename() const
  {
    return path_.empty() ? path() : *--this->cend();
  }

  /**
  * \brief Get a relative path to the component including and following the last '.'.
  *
  * \return The string extension
  */
  path extension() const
  {
    const char * delimiter = ".";
    auto split_fname = split(this->string(), *delimiter);
    return split_fname.size() == 1 ? path("") : path("." + split_fname.back());
  }

  /**
  * \brief Concatenate a path and a string into a single path.
  *
  * \param other the string compnoent to concatenate
  * \return The combined path of this and other.
  */
  path operator/(const std::string & other)
  {
    return this->operator/(path(other));
  }

  /**
  * \brief Append a string component to this path.
  *
  * \param other the string component to append
  * \return *this
  */
  path & operator/=(const std::string & other)
  {
    this->operator/=(path(other));
    return *this;
  }

  /**
  * \brief Concatenate two paths together.
  *
  * \param other the path to append
  * \return The combined path.
  */
  path operator/(const path & other)
  {
    return path(*this).operator/=(other);
  }

  /**
  * \brief Append a string component to this path.
  *
  * \param other the string component to append
  * \return *this
  */
  path & operator/=(const path & other)
  {
    if (other.is_absolute()) {
      this->path_ = other.path_;
      this->path_as_vector_ = other.path_as_vector_;
    } else {
      this->path_ += kPreferredSeparator + other.string();
      this->path_as_vector_.insert(
        std::end(this->path_as_vector_),
        std::begin(other.path_as_vector_), std::end(other.path_as_vector_));
    }
    return *this;
  }

private:
  /// Returns true if the path is an absolute path with a drive letter on Windows
  bool is_absolute_with_drive_letter() const
  {
#ifdef _WIN32
    if (path_.empty()) {
      return false;
    }
    return 0 == path_.compare(1, 2, ":\\");
#else
    return false;  // only Windows contains absolute paths starting with drive letters
#endif
  }

  std::string path_;
  std::vector<std::string> path_as_vector_;
};

/**
 * \brief Check if the path is a regular file.
 *
 * \param p The path to check
 * \return True if the path exists, false otherwise.
 */
inline bool is_regular_file(const path & p) noexcept
{
  return p.is_regular_file();
}

/**
 * \brief Check if the path is a directory.
 *
 * \param p The path to check
 * \return True if the path is an existing directory, false otherwise.
 */
inline bool is_directory(const path & p) noexcept
{
  return p.is_directory();
}

/**
 * \brief Get the file size of the path.
 *
 * \param p The path to get the file size of.
 * \return The file size in bytes.
 *
 * \throws std::sytem_error
 */
inline uint64_t file_size(const path & p)
{
  return p.file_size();
}

/**
 * \brief Check if a path exists.
 *
 * \param path_to_check The path to check.
 * \return True if the path exists, false otherwise.
 */
inline bool exists(const path & path_to_check)
{
  return path_to_check.exists();
}


/**
 * \brief Create a directory with the given path p.
 *
 * This builds directories recursively and will skip directories if they are already created.
 * \return Return true if the directory is created, false otherwise.
 */
inline bool create_directories(const path & p)
{
  path p_built;
  int status = 0;

  for (auto it = p.cbegin(); it != p.cend() && status == 0; ++it) {
    if (!p_built.empty() || it->empty()) {
      p_built /= *it;
    } else {
      p_built = *it;
    }
    if (!p_built.exists()) {
#ifdef _WIN32
      status = _mkdir(p_built.string().c_str());
#else
      status = mkdir(p_built.string().c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
#endif
    }
  }
  return status == 0;
}

/**
 * \brief Remove the file or directory at the path p.
 *
 * \param p The path of the object to remove.
 * \return true if the file exists and it was successfully removed, false otherwise.
 */
inline bool remove(const path & p)
{
#ifdef _WIN32
  struct _stat s;
  if (_stat(p.string().c_str(), &s) == 0) {
    if (s.st_mode & S_IFDIR) {
      return _rmdir(p.string().c_str()) == 0;
    }
    if (s.st_mode & S_IFREG) {
      return ::remove(p.string().c_str()) == 0;
    }
  }
  return false;
#else
  return ::remove(p.string().c_str()) == 0;
#endif
}

/**
 * \brief Remove the directory at the path p and its content.
 *
 * Additionally to \sa remove, remove_all removes a directory and its containing files.
 *
 * \param The path of the directory to remove.
 * \return true if the directory exists and it was successfully removed, false otherwise.
 */
inline bool remove_all(const path & p)
{
  if (!is_directory(p)) {return remove(p);}

#ifdef _WIN32
  // We need a string of type PCZZTSTR, which is a double null terminated char ptr
  size_t length = p.string().size();
  TCHAR * temp_dir = new TCHAR[length + 2];
  memcpy(temp_dir, p.string().c_str(), length);
  temp_dir[length] = '\0';
  temp_dir[length + 1] = '\0';  // double null terminated

  SHFILEOPSTRUCT file_options;
  file_options.hwnd = nullptr;
  file_options.wFunc = FO_DELETE;  // delete (recursively)
  file_options.pFrom = temp_dir;
  file_options.pTo = nullptr;
  file_options.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;  // do not prompt user
  file_options.fAnyOperationsAborted = FALSE;
  file_options.lpszProgressTitle = nullptr;
  file_options.hNameMappings = nullptr;

  auto ret = SHFileOperation(&file_options);
  delete[] temp_dir;

  return 0 == ret && false == file_options.fAnyOperationsAborted;
#else
  DIR * dir = opendir(p.string().c_str());
  struct dirent * directory_entry;
  while ((directory_entry = readdir(dir)) != nullptr) {
    // Make sure to not call ".." or "." entries in directory (might delete everything)
    if (strcmp(directory_entry->d_name, ".") != 0 && strcmp(directory_entry->d_name, "..") != 0) {
      auto sub_path = rcpputils::fs::path(p) / directory_entry->d_name;
      // if directory, call recursively
      if (sub_path.is_directory() && !remove_all(sub_path)) {
        return false;
        // if not, call regular remove
      } else if (!remove(sub_path)) {
        return false;
      }
    }
  }
  closedir(dir);
  // directory is empty now, call remove
  remove(p);
  return !rcpputils::fs::exists(p);
#endif
}

/**
 * \brief Remove extension(s) from a path.
 *
 * An extension is defined as text starting from the end of a path to the first period (.) character.
 *
 * \param file_path The file path string.
 * \param n_times The number of extensions to remove if there are multiple extensions.
 * \return The path object.
 */
inline path remove_extension(const path & file_path, int n_times = 1)
{
  path new_path(file_path);
  for (int i = 0; i < n_times; i++) {
    const auto new_path_str = new_path.string();
    const auto last_dot = new_path_str.find_last_of('.');
    if (last_dot == std::string::npos) {
      return new_path;
    }
    new_path = path(new_path_str.substr(0, last_dot));
  }
  return new_path;
}

#undef RCPPUTILS_IMPL_OS_DIRSEP

/**
* \brief Convert the path to a string for ostream usage, such as in logging or string formatting.
*
* \param os The stream to send the path string to
* \param p The path to stringify
* \return The ostream, for chaining
*/
inline std::ostream & operator<<(std::ostream & os, const path & p)
{
  os << p.string();
  return os;
}

}  // namespace fs
}  // namespace rcpputils

#endif  // RCPPUTILS__FILESYSTEM_HELPER_HPP_
