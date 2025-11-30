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

#include <string>
#include <vector>

#include "class_loader/meta_object.hpp"
#include "class_loader/class_loader.hpp"

namespace class_loader
{
namespace impl
{

typedef std::vector<class_loader::ClassLoader *> ClassLoaderVector;

class AbstractMetaObjectBaseImpl
{
public:
  ClassLoaderVector associated_class_loaders_;
  std::string associated_library_path_;
  std::string base_class_name_;
  std::string class_name_;
  std::string typeid_base_class_name_;
};

AbstractMetaObjectBase::AbstractMetaObjectBase(
  const std::string & class_name, const std::string & base_class_name,
  const std::string & typeid_base_class_name)
: impl_(new AbstractMetaObjectBaseImpl())
{
  impl_->associated_library_path_ = "Unknown";
  impl_->base_class_name_ = base_class_name;
  impl_->class_name_ = class_name;
  impl_->typeid_base_class_name_ = typeid_base_class_name;
  CONSOLE_BRIDGE_logDebug(
    "class_loader.impl.AbstractMetaObjectBase: Creating MetaObject %p "
    "(base = %s, derived = %s, library path = %s)",
    this, baseClassName().c_str(), className().c_str(), getAssociatedLibraryPath().c_str());
}

AbstractMetaObjectBase::~AbstractMetaObjectBase()
{
  CONSOLE_BRIDGE_logDebug(
    "class_loader.impl.AbstractMetaObjectBase: "
    "Destroying MetaObject %p (base = %s, derived = %s, library path = %s)",
    this, baseClassName().c_str(), className().c_str(), getAssociatedLibraryPath().c_str());
  delete impl_;
}

const std::string & AbstractMetaObjectBase::className() const
{
  return impl_->class_name_;
}

const std::string & AbstractMetaObjectBase::baseClassName() const
{
  return impl_->base_class_name_;
}

const std::string & AbstractMetaObjectBase::typeidBaseClassName() const
{
  return impl_->typeid_base_class_name_;
}

const std::string & AbstractMetaObjectBase::getAssociatedLibraryPath() const
{
  return impl_->associated_library_path_;
}

void AbstractMetaObjectBase::setAssociatedLibraryPath(const std::string & library_path)
{
  impl_->associated_library_path_ = library_path;
}

void AbstractMetaObjectBase::addOwningClassLoader(ClassLoader * loader)
{
  ClassLoaderVector & v = impl_->associated_class_loaders_;
  if (std::find(v.begin(), v.end(), loader) == v.end()) {
    v.push_back(loader);
  }
}

void AbstractMetaObjectBase::removeOwningClassLoader(const ClassLoader * loader)
{
  ClassLoaderVector & v = impl_->associated_class_loaders_;
  ClassLoaderVector::iterator itr = std::find(v.begin(), v.end(), loader);
  if (itr != v.end()) {
    v.erase(itr);
  }
}

bool AbstractMetaObjectBase::isOwnedBy(const ClassLoader * loader) const
{
  const ClassLoaderVector & v = impl_->associated_class_loaders_;
  auto it = std::find(v.begin(), v.end(), loader);
  return it != v.end();
}

bool AbstractMetaObjectBase::isOwnedByAnybody() const
{
  return impl_->associated_class_loaders_.size() > 0;
}

size_t AbstractMetaObjectBase::getAssociatedClassLoadersCount() const
{
  return impl_->associated_class_loaders_.size();
}

ClassLoader * AbstractMetaObjectBase::getAssociatedClassLoader(size_t index) const
{
  return impl_->associated_class_loaders_[index];
}

}  // namespace impl

}  // namespace class_loader
