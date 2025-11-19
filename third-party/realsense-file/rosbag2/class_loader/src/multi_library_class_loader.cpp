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

#include "class_loader/multi_library_class_loader.hpp"

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace class_loader
{

class MultiLibraryClassLoaderImpl
{
public:
  bool enable_ondemand_loadunload_;
  LibraryToClassLoaderMap active_class_loaders_;
  std::mutex loader_mutex_;
};

MultiLibraryClassLoader::MultiLibraryClassLoader(bool enable_ondemand_loadunload)
: impl_(new MultiLibraryClassLoaderImpl())
{
  impl_->enable_ondemand_loadunload_ = enable_ondemand_loadunload;
}

MultiLibraryClassLoader::~MultiLibraryClassLoader()
{
  shutdownAllClassLoaders();
  delete impl_;
}

std::vector<std::string> MultiLibraryClassLoader::getRegisteredLibraries() const
{
  std::vector<std::string> libraries;
  for (auto & it : impl_->active_class_loaders_) {
    if (it.second != nullptr) {
      libraries.push_back(it.first);
    }
  }
  return libraries;
}

ClassLoader * MultiLibraryClassLoader::getClassLoaderForLibrary(const std::string & library_path)
{
  return impl_->active_class_loaders_[library_path];
}

ClassLoaderVector MultiLibraryClassLoader::getAllAvailableClassLoaders() const
{
  ClassLoaderVector loaders;
  for (auto & it : impl_->active_class_loaders_) {
    loaders.push_back(it.second);
  }
  return loaders;
}

bool MultiLibraryClassLoader::isLibraryAvailable(const std::string & library_name) const
{
  std::vector<std::string> available_libraries = getRegisteredLibraries();
  return available_libraries.end() != std::find(
    available_libraries.begin(), available_libraries.end(), library_name);
}

void MultiLibraryClassLoader::loadLibrary(const std::string & library_path)
{
  if (!isLibraryAvailable(library_path)) {
    impl_->active_class_loaders_[library_path] =
      new class_loader::ClassLoader(library_path, isOnDemandLoadUnloadEnabled());
  }
}

void MultiLibraryClassLoader::shutdownAllClassLoaders()
{
  for (auto & library_path : getRegisteredLibraries()) {
    unloadLibrary(library_path);
  }
}

int MultiLibraryClassLoader::unloadLibrary(const std::string & library_path)
{
  int remaining_unloads = 0;
  if (isLibraryAvailable(library_path)) {
    ClassLoader * loader = getClassLoaderForLibrary(library_path);
    remaining_unloads = loader->unloadLibrary();
    if (remaining_unloads == 0) {
      impl_->active_class_loaders_[library_path] = nullptr;
      delete (loader);
    }
  }
  return remaining_unloads;
}

bool MultiLibraryClassLoader::isOnDemandLoadUnloadEnabled() const
{
  return impl_->enable_ondemand_loadunload_;
}

}  // namespace class_loader
