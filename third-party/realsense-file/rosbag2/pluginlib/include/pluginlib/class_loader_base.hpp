/*
 * Copyright (c) 2012, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PLUGINLIB__CLASS_LOADER_BASE_HPP_
#define PLUGINLIB__CLASS_LOADER_BASE_HPP_

#include <vector>
#include <string>

namespace pluginlib
{
/// Pure virtual base class of pluginlib::ClassLoader which is not templated.
/**
 * This allows the writing of non-templated manager code
 * which can call all the administrative functions of ClassLoaders -
 * everything except createClassInstance(), createInstance()
 * and createUnmanagedInstance().
 */
class ClassLoaderBase
{
public:
  /// Empty virtual destructor.
  virtual ~ClassLoaderBase() {}

  /// Return a list of all available plugin manifest paths.
  /**
   * \return A vector of strings corresponding to the paths of all available plugin manifests
   */
  virtual std::vector<std::string> getPluginXmlPaths() = 0;

  /// Return a list of all available classes for this ClassLoader's base class type.
  /**
   * \return A vector of strings corresponding to the names of all available classes
   */
  virtual std::vector<std::string> getDeclaredClasses() = 0;

  /// Refresh the list of all available classes for this ClassLoader's base class type.
  /**
   * \throws pluginlib::LibraryLoadException if package manifest cannot be found
   */
  virtual void refreshDeclaredClasses() = 0;

  /// Strip the package name off of a lookup name.
  /**
   * \param lookup_name The name of the plugin
   * \return The name of the plugin stripped of the package name
   */
  virtual std::string getName(const std::string & lookup_name) = 0;

  /// Check if the class associated with a plugin name is available to be loaded.
  /**
   * \param lookup_name The name of the plugin
   * \return True if the plugin is available, false otherwise
   */
  virtual bool isClassAvailable(const std::string & lookup_name) = 0;

  /// Given the lookup name of a class, return the type of the derived class associated with it.
  /**
   * \param lookup_name The name of the class
   * \return The name of the associated derived class
   */
  virtual std::string getClassType(const std::string & lookup_name) = 0;

  /// Given the lookup name of a class, return its description.
  /**
   * \param lookup_name The lookup name of the class
   * \return The description of the class
   */
  virtual std::string getClassDescription(const std::string & lookup_name) = 0;

  /// Given the lookup name of a class, return the type of the associated base class.
  /**
   * \return The type of the associated base class
   */
  virtual std::string getBaseClassType() const = 0;

  /// Given the name of a class, return name of the containing package.
  /**
   * \param lookup_name The name of the class
   * \return The name of the containing package
   */
  virtual std::string getClassPackage(const std::string & lookup_name) = 0;

  /// Given the name of a class, return the path of the associated plugin manifest.
  /**
   * \param lookup_name The name of the class
   * \return The path of the associated plugin manifest
   */
  virtual std::string getPluginManifestPath(const std::string & lookup_name) = 0;

  /// Check if a given class is currently loaded.
  /**
   * \param lookup_name The lookup name of the class to query
   * \return True if the class is loaded, false otherwise
   */
  virtual bool isClassLoaded(const std::string & lookup_name) = 0;

  /// Attempt to load a class with a given name.
  /**
   * \param lookup_name The lookup name of the class to load
   * \throws pluginlib::LibraryLoadException if the library for the
   *   class cannot be loaded
   */
  virtual void loadLibraryForClass(const std::string & lookup_name) = 0;

  /// Attempt to unload a class with a given name.
  /**
   * \param lookup_name The lookup name of the class to unload
   * \throws pluginlib::LibraryUnloadException if the library for the class
   *   cannot be unloaded
   * \return The number of pending unloads until the library is removed from memory
   */
  virtual int unloadLibraryForClass(const std::string & lookup_name) = 0;

  /// Return the libraries that are registered and can be loaded.
  /**
   * \return A vector of strings corresponding to the names of registered libraries
   */
  virtual std::vector<std::string> getRegisteredLibraries() = 0;

  /// Given the name of a class, return the path to its associated library.
  /**
   * \param lookup_name The name of the class
   * \return The path to the associated library
   */
  virtual std::string getClassLibraryPath(const std::string & lookup_name) = 0;
};
}  // namespace pluginlib

#endif  // PLUGINLIB__CLASS_LOADER_BASE_HPP_
