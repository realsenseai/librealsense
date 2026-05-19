/*
 * Copyright (c) 2009, Willow Garage, Inc.
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

#ifndef PLUGINLIB__CLASS_LOADER_HPP_
#define PLUGINLIB__CLASS_LOADER_HPP_

#include <map>
#include <string>
#include <vector>

/* This is a workaround to MSVC incorrectly reporting the __cplusplus version
 * as explained in:
 * https://blogs.msdn.microsoft.com/vcblog/2018/04/09/msvc-now-correctly-reports-__cplusplus/
 *
 * I'm hesitant to currently switch on the /Zc:__cplusplus switch, as there are
 * reports of code (incorrectly) assuming it should always be set to 199711L.
 */
#if defined(_MSC_VER)
# define HAS_CPP11_MEMORY (_MSC_VER >= 1900)
#else
# define HAS_CPP11_MEMORY (__cplusplus >= 201103L)
#endif

#if defined(HAS_CPP11_MEMORY) && HAS_CPP11_MEMORY
# include <memory>
#endif

#ifndef PLUGINLIB__DISABLE_BOOST_FUNCTIONS
#include <boost/shared_ptr.hpp>
#endif

#include "class_loader/multi_library_class_loader.hpp"
#include "pluginlib/class_desc.hpp"
#include "pluginlib/class_loader_base.hpp"
#include "pluginlib/exceptions.hpp"
#include "tinyxml2.h"  // NOLINT

namespace pluginlib
{

#if defined(HAS_CPP11_MEMORY) && HAS_CPP11_MEMORY
template<typename T>
using UniquePtr = class_loader::ClassLoader::UniquePtr<T>;
#endif

/// A class to help manage and load classes.
template<class T>
class ClassLoader : public ClassLoaderBase
{
public:
  typedef typename std::map<std::string, ClassDesc>::iterator ClassMapIterator;

public:
  /**
   * \param package The package containing the base class
   * \param base_class The type of the base class for classes to be loaded
   * \param attrib_name The attribute to search for in manifext.xml files, defaults to "plugin"
   * \param plugin_xml_paths The list of paths of plugin.xml files, defaults to be crawled via
   *   ros::package::getPlugins()
   * \throws pluginlib::ClassLoaderException if package manifest cannot be found
   */
  ClassLoader(
    std::string package,
    std::string base_class,
    std::string attrib_name = std::string("plugin"),
    std::vector<std::string> plugin_xml_paths = std::vector<std::string>());

  ~ClassLoader();

  /// Create an instance of a desired class, optionally loading the associated library too.
  /**
   * \param lookup_name The name of the class to load
   * \param auto_load Specifies whether or not to automatically load the
   *   library containing the class, set to true by default.
   * \throws pluginlib::LibraryLoadException when the library associated
   *   with the class cannot be loaded
   * \throws pluginlib::CreateClassException when the class cannot be instantiated
   * \return An instance of the class
   * \deprecated use either createInstance() or createUnmanagedInstance()
   */
  [[deprecated]]
  T * createClassInstance(
    const std::string & lookup_name,
    bool auto_load = true);

#if defined(HAS_CPP11_MEMORY) && HAS_CPP11_MEMORY
  /// Create an instance of a desired class.
  /**
   * Implicitly calls loadLibraryForClass() to increment the library counter.
   *
   * Deleting the instance and calling unloadLibraryForClass() is automatically
   * handled by the shared pointer.
   * \param lookup_name The name of the class to load
   * \throws pluginlib::LibraryLoadException when the library associated with
   *   the class cannot be loaded
   * \throws pluginlib::CreateClassException when the class cannot be instantiated
   * \return An instance of the class
   */
  std::shared_ptr<T> createSharedInstance(const std::string & lookup_name);
#endif

#ifndef PLUGINLIB__DISABLE_BOOST_FUNCTIONS
  /// Create an instance of a desired class.
  /**
   * Deprecated, use createSharedInstance() instead.
   * Same as createSharedInstance() except it returns a boost::shared_ptr.
   */
  [[deprecated]]
  boost::shared_ptr<T> createInstance(const std::string & lookup_name);
#endif

#if defined(HAS_CPP11_MEMORY) && HAS_CPP11_MEMORY
  /// Create an instance of a desired class.
  /**
   * Implicitly calls loadLibraryForClass() to increment the library counter.
   *
   * Deleting the instance and calling unloadLibraryForClass() is automatically
   * handled by the unique pointer.
   *
   * If you release the wrapped pointer you must manually call the original
   * deleter when you want to destroy the released pointer.
   *
   * \param lookup_name The name of the class to load.
   * \throws pluginlib::LibraryLoadException when the library associated with
   *   the class cannot be loaded.
   * \throws pluginlib::CreateClassException when the class cannot be instantiated
   * \return An instance of the class
   */
  UniquePtr<T> createUniqueInstance(const std::string & lookup_name);
#endif

  /// Create an instance of a desired class.
  /**
   * Implicitly calls loadLibraryForClass() to increment the library counter.
   *
   * \attention The ownership is transfered to the caller, which is responsible
   *   for deleting the instance and calling unloadLibraryForClass()
   *   (in order to decrement the associated library counter and unloading it
   *   if it is no more used).
   * \param lookup_name The name of the class to load
   * \throws pluginlib::LibraryLoadException when the library associated with
   *   the class cannot be loaded
   * \throws pluginlib::CreateClassException when the class cannot be instantiated
   * \return An instance of the class
   */
  T * createUnmanagedInstance(const std::string & lookup_name);

  /// Return a list of all available plugin manifest paths for this ClassLoader's base class type.
  /**
   * \return A vector of strings corresponding to the paths of all available plugin manifests
   */
  std::vector<std::string> getPluginXmlPaths();

  /// Return a list of all available classes for this ClassLoader's base class type.
  /**
   * \return A vector of strings corresponding to the names of all available classes
   */
  std::vector<std::string> getDeclaredClasses();

  /// Strip the package name off of a lookup name.
  /**
   * \param lookup_name The name of the plugin
   * \return The name of the plugin stripped of the package name
   */
  virtual std::string getName(const std::string & lookup_name);

  /// Given the lookup name of a class, return the type of the associated base class.
  /**
   * \return The type of the associated base class
   */
  virtual std::string getBaseClassType() const;

  /// Given the lookup name of a class, return the type of the derived class associated with it.
  /**
   * \param lookup_name The name of the class
   * \return The name of the associated derived class
   */
  virtual std::string getClassType(const std::string & lookup_name);

  /// Given the lookup name of a class, return its description.
  /**
   * \param lookup_name The lookup name of the class
   * \return The description of the class
   */
  virtual std::string getClassDescription(const std::string & lookup_name);

  /// Given the name of a class, return the path to its associated library.
  /**
   * \param lookup_name The name of the class
   * \return The path to the associated library
   */
  virtual std::string getClassLibraryPath(const std::string & lookup_name);

  /// Given the name of a class, return name of the containing package.
  /**
   * \param lookup_name The name of the class
   * \return The name of the containing package
   */
  virtual std::string getClassPackage(const std::string & lookup_name);

  /// Given the name of a class, return the path of the associated plugin manifest.
  /**
   * \param lookup_name The name of the class
   * \return The path of the associated plugin manifest
   */
  virtual std::string getPluginManifestPath(const std::string & lookup_name);

  /// Return the libraries that are registered and can be loaded.
  /**
   * \return A vector of strings corresponding to the names of registered libraries
   */
  virtual std::vector<std::string> getRegisteredLibraries();

  /// Check if the library for a given class is currently loaded.
  /**
   * \param lookup_name The lookup name of the class to query
   * \return True if the class is loaded, false otherwise
   */
  bool isClassLoaded(const std::string & lookup_name);

  /// Check if the class associated with a plugin name is available to be loaded.
  /**
   * \param lookup_name The name of the plugin
   * \return true if the plugin is available, false otherwise
   */
  virtual bool isClassAvailable(const std::string & lookup_name);

  /// Attempt to load the library containing a class with a given name.
  /**
   * The counter for the library uses (refcount) is also incremented.
   *
   * \param lookup_name The lookup name of the class to load
   * \throws pluginlib::LibraryLoadException if the library for the
   *   class cannot be loaded
   */
  virtual void loadLibraryForClass(const std::string & lookup_name);

  /// Refresh the list of all available classes for this ClassLoader's base class type.
  /**
   * \throws pluginlib::LibraryLoadException if package manifest cannot be found
   */
  virtual void refreshDeclaredClasses();

  /// Decrement the counter for the library containing a class with a given name.
  /**
   * Also try to unload the library, If the counter reaches zero.
   *
   * \param lookup_name The lookup name of the class to unload
   * \throws pluginlib::LibraryUnloadException if the library for the
   *   class cannot be unloaded
   * \return The number of pending unloads until the library is removed from memory
   */
  virtual int unloadLibraryForClass(const std::string & lookup_name);

private:
  /// Return the paths to plugin.xml files.
  /**
   * \throws pluginlib::LibraryLoadException if package manifest cannot be found
   * \return A vector of paths
   */
  std::vector<std::string> getPluginXmlPaths(
    const std::string & package,
    const std::string & attrib_name);

  /// Return the available classes.
  /**
   * \param plugin_xml_paths The vector of paths of plugin.xml files
   * \throws pluginlib::LibraryLoadException if package manifest cannot be found
   * \return A map of class names and the corresponding descriptions
   */
  std::map<std::string, ClassDesc> determineAvailableClasses(
    const std::vector<std::string> & plugin_xml_paths);

  /// Open a package.xml file and extract the package name (i.e. contents of <name> tag).
  /**
   * \param package_xml_path The path to the package.xml file
   * \return The name of the package if successful, otherwise an empty string
   */
  std::string extractPackageNameFromPackageXML(const std::string & package_xml_path);

  /// Get a list of paths to try to find a library.
  /**
   * As we transition from rosbuild to Catkin build systems, plugins can be
   * found in the old rosbuild place (pkg_name/lib usually) or somewhere in the
   * Catkin build space.
   */
  std::vector<std::string> getAllLibraryPathsToTry(
    const std::string & library_name,
    const std::string & exporting_package_name);

  /// Return an error message for an unknown class.
  /**
   * \param lookup_name The name of the class
   * \return The error message
   */
  std::string getErrorStringForUnknownClass(const std::string & lookup_name);

  /// Get the standard path separator for the native OS (e.g. "/" on *nix, "\" on Windows).
  std::string getPathSeparator();

  /// Get the package name from a path to a plugin XML file.
  std::string getPackageFromPluginXMLFilePath(const std::string & path);

  /// Join two filesystem paths together utilzing appropriate path separator.
  std::string joinPaths(const std::string & path1, const std::string & path2);

  /// Parse a plugin XML file.
  /**
   * Also insert the appropriate ClassDesc entries into the passes
   * classes_available map.
   */
  void processSingleXMLPluginFile(
    const std::string & xml_file, std::map<std::string,
    ClassDesc> & class_available);

  /// Strip all but the filename from an explicit file path.
  /**
   * \param path The path to strip
   * \return The basename of the path
   */
  std::string stripAllButFileFromPath(const std::string & path);


  /// Helper function for unloading a shared library.
  /**
   * \param library_path The exact path to the library to unload
   * \return The number of pending unloads until the library is removed from memory
   */
  int unloadClassLibraryInternal(const std::string & library_path);

private:
  std::vector<std::string> plugin_xml_paths_;
  // Map from library to class's descriptions described in XML.
  std::map<std::string, ClassDesc> classes_available_;
  std::string package_;
  std::string base_class_;
  std::string attrib_name_;
  class_loader::MultiLibraryClassLoader lowlevel_class_loader_;  // The underlying classloader
};

}  // namespace pluginlib

// Note: The implementation of the methods is in a separate file for clarity.
#include "./class_loader_imp.hpp"

#endif  // PLUGINLIB__CLASS_LOADER_HPP_
