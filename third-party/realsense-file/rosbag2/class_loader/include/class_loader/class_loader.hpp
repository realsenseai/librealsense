/*
 * Software License Agreement (BSD License)
 *
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
 *     * Neither the name of the copyright holders nor the names of its
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

#ifndef CLASS_LOADER__CLASS_LOADER_HPP_
#define CLASS_LOADER__CLASS_LOADER_HPP_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// TODO(mikaelarguedas) remove this once console_bridge complies with this
// see https://github.com/ros/console_bridge/issues/55
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
#include "console_bridge/console.h"
#ifdef __clang__
# pragma clang diagnostic pop
#endif

#include "class_loader/class_loader_core.hpp"
#include "class_loader/register_macro.hpp"
#include "class_loader/visibility_control.hpp"

namespace class_loader
{

/// Returns a platform specific version of a basic library name
/**
 * On *nix platforms the library name is prefixed with `lib`.
 * On all platforms the output of class_loader::systemLibrarySuffix() is appended.
 *
 * @param[in] library_name name to add the prefix
 * @return library name with the prefix added
 */
CLASS_LOADER_PUBLIC
std::string systemLibraryFormat(const std::string & library_name);

/**
 * @class ClassLoader
 * @brief This class allows loading and unloading of dynamically linked libraries which contain class
 * definitions from which objects can be created/destroyed during runtime (i.e. class_loader).
 * Libraries loaded by a ClassLoader are only accessible within scope of that ClassLoader object.
 */
class ClassLoader
{
public:
  template<typename Base>
  using DeleterType = std::function<void (Base *)>;

  template<typename Base>
  using UniquePtr = std::unique_ptr<Base, DeleterType<Base>>;

  /**
   * @brief Constructor for ClassLoader
   *
   * @param library_path - The path of the runtime library to load
   * @param ondemand_load_unload - Indicates if on-demand (lazy) unloading/loading of libraries
   * occurs as plugins are created/destroyed.
   */
  CLASS_LOADER_PUBLIC
  explicit ClassLoader(const std::string & library_path, bool ondemand_load_unload = false);

  /**
   * @brief Destructor for ClassLoader. All libraries opened by this ClassLoader are unloaded automatically.
   */
  CLASS_LOADER_PUBLIC
  virtual ~ClassLoader();

  /**
   * @brief Indicates which classes (i.e. class_loader) that can be loaded by this object
   *
   * @return vector of strings indicating names of instantiable classes derived from <Base>
   */
  template<class Base>
  std::vector<std::string> getAvailableClasses() const
  {
    return class_loader::impl::getAvailableClasses<Base>(this);
  }

  /**
   * @brief Generates an instance of loadable classes (i.e. class_loader).
   *
   * It is not necessary for the user to call loadLibrary() as it will be invoked automatically
   * if the library is not yet loaded (which typically happens when in "On Demand Load/Unload" mode).
   *
   * @param derived_class_name The name of the class we want to create (@see getAvailableClasses())
   * @return A std::shared_ptr<Base> to newly created plugin object
   */
  template<class Base>
  std::shared_ptr<Base> createInstance(const std::string & derived_class_name)
  {
    return std::shared_ptr<Base>(
      createRawInstance<Base>(derived_class_name, true),
      std::bind(&ClassLoader::onPluginDeletion<Base>, this, std::placeholders::_1)
    );
  }

  /// Generates an instance of loadable classes (i.e. class_loader).
  /**
   * It is not necessary for the user to call loadLibrary() as it will be
   * invoked automatically if the library is not yet loaded (which typically
   * happens when in "On Demand Load/Unload" mode).
   *
   * If you release the wrapped pointer you must manually call the original
   * deleter when you want to destroy the released pointer.
   *
   * @param derived_class_name
   *   The name of the class we want to create (@see getAvailableClasses()).
   * @return A std::unique_ptr<Base> to newly created plugin object.
   */
  template<class Base>
  UniquePtr<Base> createUniqueInstance(const std::string & derived_class_name)
  {
    Base * raw = createRawInstance<Base>(derived_class_name, true);
    return std::unique_ptr<Base, DeleterType<Base>>(
      raw,
      std::bind(&ClassLoader::onPluginDeletion<Base>, this, std::placeholders::_1)
    );
  }

  /// Generates an instance of loadable classes (i.e. class_loader).
  /**
   * It is not necessary for the user to call loadLibrary() as it will be
   * invoked automatically if the library is not yet loaded (which typically
   * happens when in "On Demand Load/Unload" mode).
   *
   * Creating an unmanaged instance disables dynamically unloading libraries when
   * managed pointers go out of scope for all class loaders in this process.
   *
   * @param derived_class_name
   *   The name of the class we want to create (@see getAvailableClasses()).
   * @return An unmanaged (i.e. not a shared_ptr) Base* to newly created plugin object.
   */
  template<class Base>
  Base * createUnmanagedInstance(const std::string & derived_class_name)
  {
    return createRawInstance<Base>(derived_class_name, false);
  }

  /**
   * @brief Indicates if a plugin class is available
   *
   * @param Base - polymorphic type indicating base class
   * @param class_name - the name of the plugin class
   * @return true if yes it is available, false otherwise
   */
  template<class Base>
  bool isClassAvailable(const std::string & class_name) const
  {
    std::vector<std::string> available_classes = getAvailableClasses<Base>();
    return std::find(
      available_classes.begin(), available_classes.end(), class_name) != available_classes.end();
  }

  /**
   * @brief Gets the full-qualified path and name of the library associated with this class loader
   *
   * @return the full-qualified path and name of the library
   */
  CLASS_LOADER_PUBLIC
  const std::string & getLibraryPath() const;

  /**
   * @brief Indicates if a library is loaded within the scope of this ClassLoader.
   *
   * Note that the library may already be loaded internally through another ClassLoader,
   * but until loadLibrary() method is called, the ClassLoader cannot create objects from said library.
   * If we want to see if the library has been opened by somebody else, @see isLibraryLoadedByAnyClassloader()
   *
   * @param library_path The path to the library to load
   * @return true if library is loaded within this ClassLoader object's scope, otherwise false
   */
  CLASS_LOADER_PUBLIC
  bool isLibraryLoaded() const;

  /**
   * @brief Indicates if a library is loaded by some entity in the plugin system (another ClassLoader),
   * but not necessarily loaded by this ClassLoader.
   *
   * @return true if library is loaded within the scope of the plugin system, otherwise false
   */
  CLASS_LOADER_PUBLIC
  bool isLibraryLoadedByAnyClassloader() const;

  /**
   * @brief Indicates if the library is to be loaded/unloaded on demand...meaning that only to load
   * a lib when the first plugin is created and automatically shut it down when last active plugin
   * is destroyed.
   *
   * @return true if ondemand load and unload is active, otherwise false
   */
  CLASS_LOADER_PUBLIC
  bool isOnDemandLoadUnloadEnabled() const;

  /**
   * @brief  Attempts to load a library on behalf of the ClassLoader. If the library is already
   * opened, this method has no effect. If the library has been already opened by some other entity
   * (i.e. another ClassLoader or global interface), this object is given permissions to access any
   * plugin classes loaded by that other entity.
   */
  CLASS_LOADER_PUBLIC
  void loadLibrary();

  /**
   * @brief  Attempts to unload a library loaded within scope of the ClassLoader. If the library is
   * not opened, this method has no effect. If the library is opened by other another ClassLoader,
   * the library will NOT be unloaded internally -- however this ClassLoader will no longer be able
   * to instantiate class_loader bound to that library. If there are plugin objects that exist in
   * memory created by this classloader, a warning message will appear and the library will not be
   * unloaded. If loadLibrary() was called multiple times (e.g. in the case of multiple threads or
   * purposefully in a single thread), the user is responsible for calling unloadLibrary() the same
   * number of times. The library will not be unloaded within the context of this classloader until
   * the number of unload calls matches the number of loads.
   *
   * @return The number of times more unloadLibrary() has to be called for it to be unbound from this ClassLoader
   */
  CLASS_LOADER_PUBLIC
  int unloadLibrary();

private:
  /**
   * @brief Callback method when a plugin created by this class loader is destroyed
   *
   * @param obj - A pointer to the deleted object
   */
  template<class Base>
  void onPluginDeletion(Base * obj)
  {
    CONSOLE_BRIDGE_logDebug(
      "class_loader::ClassLoader: Calling onPluginDeletion() for obj ptr = %p.\n",
      reinterpret_cast<void *>(obj));
    if (nullptr == obj) {
      return;
    }
    std::lock_guard<std::recursive_mutex> lock(plugin_ref_count_mutex_);
    delete (obj);
    assert(plugin_ref_count_ > 0);
    --plugin_ref_count_;
    if (plugin_ref_count_ == 0 && isOnDemandLoadUnloadEnabled()) {
      if (!ClassLoader::hasUnmanagedInstanceBeenCreated()) {
        unloadLibraryInternal(false);
      } else {
        CONSOLE_BRIDGE_logWarn(
          "class_loader::ClassLoader: "
          "Cannot unload library %s even though last shared pointer went out of scope. "
          "This is because createUnmanagedInstance was used within the scope of this process, "
          "perhaps by a different ClassLoader. "
          "Library will NOT be closed.",
          getLibraryPath().c_str());
      }
    }
  }

  /// Generates an instance of loadable classes (i.e. class_loader).
  /**
   * It is not necessary for the user to call loadLibrary() as it will be
   * invoked automatically if the library is not yet loaded (which typically
   * happens when in "On Demand Load/Unload" mode).
   *
   * @param derived_class_name
   *   The name of the class we want to create (@see getAvailableClasses()).
   * @param managed
   *   If true, the returned pointer is assumed to be wrapped in a smart
   *   pointer by the caller.
   * @return A Base* to newly created plugin object.
   */
  template<class Base>
  Base * createRawInstance(const std::string & derived_class_name, bool managed)
  {
    if (!managed) {
      this->setUnmanagedInstanceBeenCreated(true);
    }

    if (
      managed &&
      ClassLoader::hasUnmanagedInstanceBeenCreated() &&
      isOnDemandLoadUnloadEnabled())
    {
      CONSOLE_BRIDGE_logInform(
        "%s",
        "class_loader::ClassLoader: "
        "An attempt is being made to create a managed plugin instance (i.e. boost::shared_ptr), "
        "however an unmanaged instance was created within this process address space. "
        "This means libraries for the managed instances will not be shutdown automatically on "
        "final plugin destruction if on demand (lazy) loading/unloading mode is used."
      );
    }
    if (!isLibraryLoaded()) {
      loadLibrary();
    }

    Base * obj = class_loader::impl::createInstance<Base>(derived_class_name, this);
    assert(obj != NULL);  // Unreachable assertion if createInstance() throws on failure.

    if (managed) {
      std::lock_guard<std::recursive_mutex> lock(plugin_ref_count_mutex_);
      ++plugin_ref_count_;
    }

    return obj;
  }

  /**
   * @brief Getter for if an unmanaged (i.e. unsafe) instance has been created flag
   */
  CLASS_LOADER_PUBLIC
  static bool hasUnmanagedInstanceBeenCreated();

  CLASS_LOADER_PUBLIC
  static void setUnmanagedInstanceBeenCreated(bool state);

  /**
   * @brief As the library may be unloaded in "on-demand load/unload" mode, unload maybe called from
   * createInstance(). The problem is that createInstance() locks the plugin_ref_count as does
   * unloadLibrary(). This method is the implementation of unloadLibrary but with a parameter to
   * decide if plugin_ref_mutex_ should be locked.
   *
   * @param lock_plugin_ref_count - Set to true if plugin_ref_count_mutex_ should be locked, else false
   * @return The number of times unloadLibraryInternal has to be called again for it to be unbound
   *   from this ClassLoader
   */
  CLASS_LOADER_PUBLIC
  int unloadLibraryInternal(bool lock_plugin_ref_count);

private:
  bool ondemand_load_unload_;
  std::string library_path_;
  int load_ref_count_;
  std::recursive_mutex load_ref_count_mutex_;
  int plugin_ref_count_;
  std::recursive_mutex plugin_ref_count_mutex_;
  static bool has_unmananged_instance_been_created_;
};

}  // namespace class_loader


#endif  // CLASS_LOADER__CLASS_LOADER_HPP_
