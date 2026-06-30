/*********************************************************************
*
* Software License Agreement (BSD License)
*
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of Willow Garage, Inc. nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*
*********************************************************************/

#ifndef PLUGINLIB__CLASS_LOADER_IMP_HPP_
#define PLUGINLIB__CLASS_LOADER_IMP_HPP_

#include <cstdlib>
#include <list>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
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

#include "ament_index_cpp/get_package_prefix.hpp"
#include "ament_index_cpp/get_package_share_directory.hpp"
#include "ament_index_cpp/get_resource.hpp"
#include "ament_index_cpp/get_resources.hpp"
#include "class_loader/class_loader.hpp"
#include "rcpputils/shared_library.hpp"
#include "rcutils/logging_macros.h"

#include "./class_loader.hpp"
#include "./impl/filesystem_helper.hpp"
#include "./impl/split.hpp"

#ifdef _WIN32
#define CLASS_LOADER_IMPL_OS_PATHSEP ";"
#else
#define CLASS_LOADER_IMPL_OS_PATHSEP ":"
#endif

namespace pluginlib
{

template<class T>
ClassLoader<T>::ClassLoader(
  std::string package,
  std::string base_class,
  std::string attrib_name,
  std::vector<std::string> plugin_xml_paths)
: plugin_xml_paths_(plugin_xml_paths),
  package_(package),
  base_class_(base_class),
  attrib_name_(attrib_name),
  // NOTE: The parameter to the class loader enables/disables on-demand class
  // loading/unloading.
  // Leaving it off for now... libraries will be loaded immediately and won't
  // be unloaded until class loader is destroyed or force unload.
  lowlevel_class_loader_(false)
  /***************************************************************************/
{
  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader", "Creating ClassLoader, base = %s, address = %p",
    base_class.c_str(), static_cast<void *>(this));
  try {
    ament_index_cpp::get_package_prefix(package_);
  } catch (const ament_index_cpp::PackageNotFoundError & exception) {
    // rethrow as class loader exception, package name is in the error message already.
    throw pluginlib::ClassLoaderException(exception.what());
  }

  if (0 == plugin_xml_paths_.size()) {
    plugin_xml_paths_ = getPluginXmlPaths(package_, attrib_name_);
  }
  classes_available_ = determineAvailableClasses(plugin_xml_paths_);
  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
    "Finished constructring ClassLoader, base = %s, address = %p",
    base_class.c_str(), static_cast<void *>(this));
}

template<class T>
ClassLoader<T>::~ClassLoader()
/***************************************************************************/
{
  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
    "Destroying ClassLoader, base = %s, address = %p",
    getBaseClassType().c_str(), static_cast<void *>(this));
}


template<class T>
T * ClassLoader<T>::createClassInstance(const std::string & lookup_name, bool auto_load)
/***************************************************************************/
{
  // Note: This method is deprecated
  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
    "In deprecated call createClassInstance(), lookup_name = %s, auto_load = %i.",
    (lookup_name.c_str()), auto_load);

  if (auto_load && !isClassLoaded(lookup_name)) {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "Autoloading class library before attempting to create instance.");
    loadLibraryForClass(lookup_name);
  }

  try {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "Attempting to create instance through low-level MultiLibraryClassLoader...");
    T * obj = lowlevel_class_loader_.createUnmanagedInstance<T>(getClassType(lookup_name));
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "Instance created with object pointer = %p", static_cast<void *>(obj));

    return obj;
  } catch (const class_loader::CreateClassException & ex) {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "CreateClassException about to be raised for class %s",
      lookup_name.c_str());
    throw pluginlib::CreateClassException(ex.what());
  }
}

#if defined(HAS_CPP11_MEMORY) && HAS_CPP11_MEMORY
template<class T>
std::shared_ptr<T> ClassLoader<T>::createSharedInstance(const std::string & lookup_name)
/***************************************************************************/
{
  return createUniqueInstance(lookup_name);
}
#endif

#ifndef PLUGINLIB__DISABLE_BOOST_FUNCTIONS
template<class T>
boost::shared_ptr<T> ClassLoader<T>::createInstance(const std::string & lookup_name)
/***************************************************************************/
{
  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
    "Attempting to create managed instance for class %s.",
    lookup_name.c_str());

  if (!isClassLoaded(lookup_name)) {
    loadLibraryForClass(lookup_name);
  }

  try {
    std::string class_type = getClassType(lookup_name);
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader", "%s maps to real class type %s",
      lookup_name.c_str(), class_type.c_str());

    boost::shared_ptr<T> obj = lowlevel_class_loader_.createInstance<T>(class_type);

    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "boost::shared_ptr to object of real type %s created.",
      class_type.c_str());

    return obj;
  } catch (const class_loader::CreateClassException & ex) {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "Exception raised by low-level multi-library class loader when attempting "
      "to create instance of class %s.",
      lookup_name.c_str());
    throw pluginlib::CreateClassException(ex.what());
  }
}
#endif

#if defined(HAS_CPP11_MEMORY) && HAS_CPP11_MEMORY
template<class T>
UniquePtr<T> ClassLoader<T>::createUniqueInstance(const std::string & lookup_name)
{
  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
    "Attempting to create managed (unique) instance for class %s.",
    lookup_name.c_str());

  if (!isClassLoaded(lookup_name)) {
    loadLibraryForClass(lookup_name);
  }

  try {
    std::string class_type = getClassType(lookup_name);
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader", "%s maps to real class type %s",
      lookup_name.c_str(), class_type.c_str());

    UniquePtr<T> obj = lowlevel_class_loader_.createUniqueInstance<T>(class_type);

    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "std::unique_ptr to object of real type %s created.",
      class_type.c_str());

    return obj;
  } catch (const class_loader::CreateClassException & ex) {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "Exception raised by low-level multi-library class loader when attempting "
      "to create instance of class %s.",
      lookup_name.c_str());
    throw pluginlib::CreateClassException(ex.what());
  }
}
#endif

template<class T>
T * ClassLoader<T>::createUnmanagedInstance(const std::string & lookup_name)
/***************************************************************************/
{
  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
    "Attempting to create UNMANAGED instance for class %s.",
    lookup_name.c_str());

  if (!isClassLoaded(lookup_name)) {
    loadLibraryForClass(lookup_name);
  }

  T * instance = 0;
  try {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "Attempting to create instance through low level multi-library class loader.");
    std::string class_type = getClassType(lookup_name);
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader", "%s maps to real class type %s",
      lookup_name.c_str(), class_type.c_str());
    instance = lowlevel_class_loader_.createUnmanagedInstance<T>(class_type);
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "Instance of type %s created.",
      class_type.c_str());
  } catch (const class_loader::CreateClassException & ex) {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "Exception raised by low-level multi-library class loader when attempting "
      "to create UNMANAGED instance of class %s.",
      lookup_name.c_str());
    throw pluginlib::CreateClassException(ex.what());
  }
  return instance;
}

template<class T>
std::vector<std::string> ClassLoader<T>::getPluginXmlPaths(
  const std::string & package,
  const std::string & attrib_name)
/***************************************************************************/
{
  // Pull possible files from manifests of packages which depend on this package and export class
  std::vector<std::string> paths;
  {
    // the convention is to create an ament resource which a concatenation of
    // the package name, "pluginlib", and the attribute being exported
    // __ is used as the concatenation delimiter because it cannot be in a
    // package name
    std::string resource_name = package + "__pluginlib__" + attrib_name;
    auto plugin_packages_with_prefixes = ament_index_cpp::get_resources(resource_name);
    for (const auto & package_prefix_pair : plugin_packages_with_prefixes) {
      // it is also convention to place the relative path to the plugin xml in
      // the ament resource file
      std::string resource_content;
      {
        using ament_index_cpp::get_resource;
        if (!get_resource(resource_name, package_prefix_pair.first, resource_content)) {
          RCUTILS_LOG_WARN_NAMED("pluginlib.ClassLoader",
            "unexpectedly not able to find ament resource '%s' for package '%s'",
            resource_name.c_str(),
            package_prefix_pair.first.c_str()
          );
          continue;
        }
      }
      // the content may contain multiple plugin description files
      std::stringstream ss(resource_content);
      std::string line;
      while (std::getline(ss, line, '\n')) {
        if (!line.empty()) {
          // store the prefix for the package with a plugin and the relative path
          // to the plugin xml file
          paths.push_back(package_prefix_pair.second + "/" + line);
        }
      }
    }
  }
  return paths;
}

template<class T>
std::map<std::string, ClassDesc> ClassLoader<T>::determineAvailableClasses(
  const std::vector<std::string> & plugin_xml_paths)
/***************************************************************************/
{
  // mas - This method requires major refactoring...
  // not only is it really long and confusing but a lot of the comments do not
  // seem to be correct.
  // With time I keep correcting small things, but a good rewrite is needed.

  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader", "Entering determineAvailableClasses()...");
  std::map<std::string, ClassDesc> classes_available;

  // Walk the list of all plugin XML files (variable "paths") that are exported by the build system
  for (std::vector<std::string>::const_iterator it = plugin_xml_paths.begin();
    it != plugin_xml_paths.end(); ++it)
  {
    try {
      processSingleXMLPluginFile(*it, classes_available);
    } catch (const pluginlib::InvalidXMLException & e) {
      RCUTILS_LOG_ERROR_NAMED("pluginlib.ClassLoader",
        "Skipped loading plugin with error: %s.",
        e.what());
    }
  }

  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader", "Exiting determineAvailableClasses()...");
  return classes_available;
}

template<class T>
std::string ClassLoader<T>::extractPackageNameFromPackageXML(const std::string & package_xml_path)
/***************************************************************************/
{
  tinyxml2::XMLDocument document;
  document.LoadFile(package_xml_path.c_str());
  tinyxml2::XMLElement * doc_root_node = document.FirstChildElement("package");
  if (NULL == doc_root_node) {
    RCUTILS_LOG_ERROR_NAMED("pluginlib.ClassLoader",
      "Could not find a root element for package manifest at %s.",
      package_xml_path.c_str());
    return "";
  }

  assert(document.RootElement() == doc_root_node);

  tinyxml2::XMLElement * package_name_node = doc_root_node->FirstChildElement("name");
  if (NULL == package_name_node) {
    RCUTILS_LOG_ERROR_NAMED("pluginlib.ClassLoader",
      "package.xml at %s does not have a <name> tag! Cannot determine package "
      "which exports plugin.",
      package_xml_path.c_str());
    return "";
  }

  return package_name_node->GetText();
}

template<class T>
std::vector<std::string> ClassLoader<T>::getAllLibraryPathsToTry(
  const std::string & library_name,
  const std::string & exporting_package_name)
/***************************************************************************/
{
  // To determine the common prefix of the paths to try, the prefix of the
  // exporting package is retrieved.
  // To that, various library folder names are added (lib, lib64, etc...)
  // Additionally, "libexec" like folders are checked, using the package name
  // as the libexec folder name within the library name.
  // Finally the library name (just the file name, stripped of extra relative)
  // with various extensions is concatenated to the various library directories.
  //
  // For example, if the package was 'rviz' and the library_name was
  // 'librviz_default_plugins', these paths might be tried:
  //
  //   - <prefix for rviz>/lib/librviz_default_plugins.so
  //   - <prefix for rviz>/lib64/librviz_default_plugins.so
  //   - <prefix for rviz>/bin/rviz_default_plugins.dll
  //   - <prefix for rviz>/lib/rviz/librviz_default_plugins.so
  //   - <prefix for rviz>/lib64/rviz/librviz_default_plugins.so
  //
  // The extension, e.g. `.so`, might be different based on the operating
  // system, e.g. it might be `.dylib` on macOS or `.dll` on Windows.
  // Similarly, the library might have the `lib` prefix added or removed.
  // Also, the library name might have a `d` added if the library is built
  // debug, depending on the system.

  // TODO(wjwwood): probably should avoid "searching" and just embed the
  // relative path to the libraries in the ament index, since CMake knows it
  // at build time...

  const std::string path_separator = getPathSeparator();

  std::vector<std::string> all_paths;  // result of all pairs to search

  std::string package_prefix = ament_index_cpp::get_package_prefix(exporting_package_name);

  // Setup the directories to look in.
  std::vector<std::string> all_search_paths = {
    // for now just try lib and lib64 (and their respective "libexec" directories)
    package_prefix + path_separator + "lib",
    package_prefix + path_separator + "lib64",
    package_prefix + path_separator + "bin",  // also look in bin, for dll's on Windows
    package_prefix + path_separator + "lib" + path_separator + exporting_package_name,
    package_prefix + path_separator + "lib64" + path_separator + exporting_package_name,
    package_prefix + path_separator + "bin" + path_separator + exporting_package_name,
  };

  std::string stripped_library_name = stripAllButFileFromPath(library_name);

  std::string library_name_alternative;  // either lib<library> or <library> without lib prefix
  const char * lib_prefix = "lib";
  if (library_name.rfind(lib_prefix, 0) == 0) {
    library_name_alternative = library_name.substr(strlen(lib_prefix));
    RCUTILS_LOG_WARN_NAMED("pluginlib.ClassLoader",
      "given plugin name '%s' should be '%s' for better portability",
      library_name.c_str(),
      library_name_alternative.c_str());
  } else {
    library_name_alternative = lib_prefix + library_name;
  }
  std::string stripped_library_name_alternative = stripAllButFileFromPath(library_name_alternative);

  try {
    // Setup the relative file paths to pair with the search directories above.
    std::vector<std::string> all_relative_library_paths = {
      rcpputils::get_platform_library_name(library_name),
      rcpputils::get_platform_library_name(library_name_alternative),
      rcpputils::get_platform_library_name(stripped_library_name),
      rcpputils::get_platform_library_name(stripped_library_name_alternative)
    };
    std::vector<std::string> all_relative_debug_library_paths = {
      rcpputils::get_platform_library_name(library_name, true),
      rcpputils::get_platform_library_name(library_name_alternative, true),
      rcpputils::get_platform_library_name(stripped_library_name, true),
      rcpputils::get_platform_library_name(stripped_library_name_alternative, true)
    };

    for (auto && current_search_path : all_search_paths) {
      for (auto && current_library_path : all_relative_library_paths) {
        all_paths.push_back(current_search_path + path_separator + current_library_path);
      }
      for (auto && current_library_path : all_relative_debug_library_paths) {
        all_paths.push_back(current_search_path + path_separator + current_library_path);
      }
    }
  } catch (const std::runtime_error & ex) {
    throw std::runtime_error{ex.what()};
  }

  for (auto && path : all_paths) {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "[search path for '%s']: '%s'",
      library_name.c_str(),
      path.c_str());
  }

  return all_paths;
}

template<class T>
bool ClassLoader<T>::isClassLoaded(const std::string & lookup_name)
/***************************************************************************/
{
  return lowlevel_class_loader_.isClassAvailable<T>(getClassType(lookup_name));
}

template<class T>
std::string ClassLoader<T>::getBaseClassType() const
/***************************************************************************/
{
  return base_class_;
}

template<class T>
std::string ClassLoader<T>::getClassDescription(const std::string & lookup_name)
/***************************************************************************/
{
  ClassMapIterator it = classes_available_.find(lookup_name);
  if (it != classes_available_.end()) {
    return it->second.description_;
  }
  return "";
}

template<class T>
std::string ClassLoader<T>::getClassType(const std::string & lookup_name)
/***************************************************************************/
{
  ClassMapIterator it = classes_available_.find(lookup_name);
  if (it != classes_available_.end()) {
    return it->second.derived_class_;
  }
  return "";
}

template<class T>
std::string ClassLoader<T>::getClassLibraryPath(const std::string & lookup_name)
/***************************************************************************/
{
  if (classes_available_.find(lookup_name) == classes_available_.end()) {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "Class %s has no mapping in classes_available_.",
      lookup_name.c_str());
    return "";
  }
  ClassMapIterator it = classes_available_.find(lookup_name);
  std::string library_name = it->second.library_name_;
  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
    "Class %s maps to library %s in classes_available_.",
    lookup_name.c_str(), library_name.c_str());

  std::vector<std::string> paths_to_try =
    getAllLibraryPathsToTry(library_name, it->second.package_);

  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
    "Iterating through all possible paths where %s could be located...",
    library_name.c_str());
  for (auto it = paths_to_try.begin(); it != paths_to_try.end(); it++) {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader", "Checking path %s ", it->c_str());
    if (pluginlib::impl::fs::exists(*it)) {
      RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader", "Library %s found at explicit path %s.",
        library_name.c_str(), it->c_str());
      return *it;
    }
  }
  return "";
}

template<class T>
std::string ClassLoader<T>::getClassPackage(const std::string & lookup_name)
/***************************************************************************/
{
  ClassMapIterator it = classes_available_.find(lookup_name);
  if (it != classes_available_.end()) {
    return it->second.package_;
  }
  return "";
}

template<class T>
std::vector<std::string> ClassLoader<T>::getPluginXmlPaths()
/***************************************************************************/
{
  return plugin_xml_paths_;
}

template<class T>
std::vector<std::string> ClassLoader<T>::getDeclaredClasses()
/***************************************************************************/
{
  std::vector<std::string> lookup_names;
  for (ClassMapIterator it = classes_available_.begin(); it != classes_available_.end(); ++it) {
    lookup_names.push_back(it->first);
  }

  return lookup_names;
}

template<class T>
std::string ClassLoader<T>::getErrorStringForUnknownClass(const std::string & lookup_name)
/***************************************************************************/
{
  std::string declared_types;
  std::vector<std::string> types = getDeclaredClasses();
  for (unsigned int i = 0; i < types.size(); i++) {
    declared_types = declared_types + std::string(" ") + types[i];
  }
  return "According to the loaded plugin descriptions the class " + lookup_name +
         " with base class type " + base_class_ + " does not exist. Declared types are " +
         declared_types;
}

template<class T>
std::string ClassLoader<T>::getName(const std::string & lookup_name)
/***************************************************************************/
{
  // remove the package name to get the raw plugin name
  std::vector<std::string> result = pluginlib::impl::split(lookup_name, "/|:");
  return result.back();
}

template<class T>
std::string
ClassLoader<T>::getPackageFromPluginXMLFilePath(const std::string & plugin_xml_file_path)
/***************************************************************************/
{
  // Note: This method takes an input a path to a plugin xml file and must determine which
  // package the XML file came from. This is not necessarily the same thing as the member
  // variable "package_". The plugin xml file can be located anywhere in the source tree for a
  // package

  // catkin and ament:
  // 1. Find nearest encasing package.xml
  // 2. Extract name of package from package.xml

  std::string package_name;
  pluginlib::impl::fs::path p(plugin_xml_file_path);
  pluginlib::impl::fs::path parent = p.parent_path();

  // Figure out exactly which package the passed XML file is exported by.
  while (true) {
    if (pluginlib::impl::fs::exists(parent / "package.xml")) {
      std::string package_file_path = (parent / "package.xml").string();
      return extractPackageNameFromPackageXML(package_file_path);
    }

    // Recursive case - hop one folder up
    parent = parent.parent_path();

    // Base case - reached root and cannot find what we're looking for
    if (parent.string().empty()) {
      return "";
    }
  }

  return package_name;
}

template<class T>
std::string ClassLoader<T>::getPathSeparator()
/***************************************************************************/
{
  return std::string(1, pluginlib::impl::fs::path::preferred_separator);
}


template<class T>
std::string ClassLoader<T>::getPluginManifestPath(const std::string & lookup_name)
/***************************************************************************/
{
  ClassMapIterator it = classes_available_.find(lookup_name);
  if (it != classes_available_.end()) {
    return it->second.plugin_manifest_path_;
  }
  return "";
}


template<class T>
std::vector<std::string> ClassLoader<T>::getRegisteredLibraries()
/***************************************************************************/
{
  return lowlevel_class_loader_.getRegisteredLibraries();
}

template<class T>
bool ClassLoader<T>::isClassAvailable(const std::string & lookup_name)
/***************************************************************************/
{
  return classes_available_.find(lookup_name) != classes_available_.end();
}

template<class T>
std::string ClassLoader<T>::joinPaths(const std::string & path1, const std::string & path2)
/***************************************************************************/
{
  pluginlib::impl::fs::path p1(path1);
  return (p1 / path2).string();
}

template<class T>
void ClassLoader<T>::loadLibraryForClass(const std::string & lookup_name)
/***************************************************************************/
{
  ClassMapIterator it = classes_available_.find(lookup_name);
  if (it == classes_available_.end()) {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "Class %s has no mapping in classes_available_.",
      lookup_name.c_str());
    throw pluginlib::LibraryLoadException(getErrorStringForUnknownClass(lookup_name));
  }

  std::string library_path = getClassLibraryPath(lookup_name);
  if ("" == library_path) {
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "No path could be found to the library containing %s.",
      lookup_name.c_str());
    std::ostringstream error_msg;
    error_msg << "Could not find library corresponding to plugin " << lookup_name <<
      ". Make sure the plugin description XML file has the correct name of the "
      "library and that the library actually exists.";
    throw pluginlib::LibraryLoadException(error_msg.str());
  }

  try {
    lowlevel_class_loader_.loadLibrary(library_path);
    it->second.resolved_library_path_ = library_path;
  } catch (const class_loader::LibraryLoadException & ex) {
    std::string error_string =
      "Failed to load library " + library_path + ". "
      "Make sure that you are calling the PLUGINLIB_EXPORT_CLASS macro in the "
      "library code, and that names are consistent between this macro and your XML. "
      "Error string: " + ex.what();
    throw pluginlib::LibraryLoadException(error_string);
  }
}

template<class T>
void ClassLoader<T>::processSingleXMLPluginFile(
  const std::string & xml_file, std::map<std::string,
  ClassDesc> & classes_available)
/***************************************************************************/
{
  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader", "Processing xml file %s...", xml_file.c_str());
  tinyxml2::XMLDocument document;
  document.LoadFile(xml_file.c_str());
  tinyxml2::XMLElement * config = document.RootElement();
  if (NULL == config) {
    throw pluginlib::InvalidXMLException(
            "XML Document '" + xml_file +
            "' has no Root Element. This likely means the XML is malformed or missing.");
    return;
  }
  if (!(strcmp(config->Value(), "library") == 0 ||
    strcmp(config->Value(), "class_libraries") == 0))
  {
    throw pluginlib::InvalidXMLException(
            "The XML document '" + xml_file + "' given to add must have either \"library\" or "
            "\"class_libraries\" as the root tag");
    return;
  }
  // Step into the filter list if necessary
  if (strcmp(config->Value(), "class_libraries") == 0) {
    config = config->FirstChildElement("library");
  }

  tinyxml2::XMLElement * library = config;
  while (library != NULL) {
    std::string library_path = library->Attribute("path");
    if (0 == library_path.size()) {
      RCUTILS_LOG_ERROR_NAMED("pluginlib.ClassLoader",
        "Failed to find Path Attirbute in library element in %s", xml_file.c_str());
      continue;
    }

    std::string package_name = getPackageFromPluginXMLFilePath(xml_file);
    if ("" == package_name) {
      RCUTILS_LOG_ERROR_NAMED("pluginlib.ClassLoader",
        "Could not find package manifest (neither package.xml or deprecated "
        "manifest.xml) at same directory level as the plugin XML file %s. "
        "Plugins will likely not be exported properly.\n)",
        xml_file.c_str());
    }

    tinyxml2::XMLElement * class_element = library->FirstChildElement("class");
    while (class_element) {
      std::string derived_class;
      if (class_element->Attribute("type") != NULL) {
        derived_class = std::string(class_element->Attribute("type"));
      } else {
        throw pluginlib::ClassLoaderException(
                "Class could not be loaded. Attribute 'type' in class tag is missing.");
      }

      std::string base_class_type;
      if (class_element->Attribute("base_class_type") != NULL) {
        base_class_type = std::string(class_element->Attribute("base_class_type"));
      } else {
        throw pluginlib::ClassLoaderException(
                "Class could not be loaded. Attribute 'base_class_type' in class tag is missing.");
      }

      std::string lookup_name;
      if (class_element->Attribute("name") != NULL) {
        lookup_name = class_element->Attribute("name");
        RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
          "XML file specifies lookup name (i.e. magic name) = %s.",
          lookup_name.c_str());
      } else {
        RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
          "XML file has no lookup name (i.e. magic name) for class %s, "
          "assuming lookup_name == real class name.",
          derived_class.c_str());
        lookup_name = derived_class;
      }

      // make sure that this class is of the right type before registering it
      if (base_class_type == base_class_) {
        // register class here
        tinyxml2::XMLElement * description = class_element->FirstChildElement("description");
        std::string description_str;
        if (description) {
          description_str = description->GetText() ? description->GetText() : "";
        } else {
          description_str = "No 'description' tag for this plugin in plugin description file.";
        }

        classes_available.insert(std::pair<std::string, ClassDesc>(lookup_name,
          ClassDesc(lookup_name, derived_class, base_class_type, package_name, description_str,
          library_path, xml_file)));
      }

      // step to next class_element
      class_element = class_element->NextSiblingElement("class");
    }
    library = library->NextSiblingElement("library");
  }
}

template<class T>
void ClassLoader<T>::refreshDeclaredClasses()
/***************************************************************************/
{
  RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader", "Refreshing declared classes.");
  // determine classes not currently loaded for removal
  std::list<std::string> remove_classes;
  for (std::map<std::string, ClassDesc>::const_iterator it = classes_available_.begin();
    it != classes_available_.end(); it++)
  {
    std::string resolved_library_path = it->second.resolved_library_path_;
    std::vector<std::string> open_libs = lowlevel_class_loader_.getRegisteredLibraries();
    if (std::find(open_libs.begin(), open_libs.end(), resolved_library_path) != open_libs.end()) {
      remove_classes.push_back(it->first);
    }
  }

  while (!remove_classes.empty()) {
    classes_available_.erase(remove_classes.front());
    remove_classes.pop_front();
  }

  // add new classes
  plugin_xml_paths_ = getPluginXmlPaths(package_, attrib_name_);
  std::map<std::string, ClassDesc> updated_classes = determineAvailableClasses(plugin_xml_paths_);
  for (std::map<std::string, ClassDesc>::const_iterator it = updated_classes.begin();
    it != updated_classes.end(); it++)
  {
    if (classes_available_.find(it->first) == classes_available_.end()) {
      classes_available_.insert(std::pair<std::string, ClassDesc>(it->first, it->second));
    }
  }
}

template<class T>
std::string ClassLoader<T>::stripAllButFileFromPath(const std::string & path)
/***************************************************************************/
{
  std::string only_file;
  size_t c = path.find_last_of(getPathSeparator());
  if (std::string::npos == c) {
    return path;
  } else {
    return path.substr(c, path.size());
  }
}

template<class T>
int ClassLoader<T>::unloadLibraryForClass(const std::string & lookup_name)
/***************************************************************************/
{
  ClassMapIterator it = classes_available_.find(lookup_name);
  if (it != classes_available_.end() && it->second.resolved_library_path_ != "UNRESOLVED") {
    std::string library_path = it->second.resolved_library_path_;
    RCUTILS_LOG_DEBUG_NAMED("pluginlib.ClassLoader",
      "Attempting to unload library %s for class %s",
      library_path.c_str(), lookup_name.c_str());
    return unloadClassLibraryInternal(library_path);
  } else {
    throw pluginlib::LibraryUnloadException(getErrorStringForUnknownClass(lookup_name));
  }
}

template<class T>
int ClassLoader<T>::unloadClassLibraryInternal(const std::string & library_path)
/***************************************************************************/
{
  return lowlevel_class_loader_.unloadLibrary(library_path);
}

}  // namespace pluginlib

#endif  // PLUGINLIB__CLASS_LOADER_IMP_HPP_
