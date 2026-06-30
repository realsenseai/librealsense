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

// Note: This header defines a simplication of a shared library

#ifndef CLASS_LOADER__META_OBJECT_HPP_
#define CLASS_LOADER__META_OBJECT_HPP_

#include <string>
#include <typeinfo>
#include <vector>

#include "class_loader/visibility_control.hpp"

namespace class_loader
{

class ClassLoader;  // Forward declaration

namespace impl
{

class AbstractMetaObjectBaseImpl;

/**
 * @class AbstractMetaObjectBase
 * @brief A base class for MetaObjects that excludes a polymorphic type parameter. Subclasses are class templates though.
 */
class CLASS_LOADER_PUBLIC AbstractMetaObjectBase
{
public:
  /**
   * @brief Constructor for the class
   */
  AbstractMetaObjectBase(
    const std::string & class_name,
    const std::string & base_class_name,
    const std::string & typeid_base_class_name = "UNSET");
  /**
   * @brief Destructor for the class. THIS MUST NOT BE VIRTUAL AND OVERRIDDEN BY
   * TEMPLATE SUBCLASSES, OTHERWISE THEY WILL PULL IN A REDUNDANT METAOBJECT
   * DESTRUCTOR OUTSIDE OF libclass_loader WITHIN THE PLUGIN LIBRARY! T
   */
  ~AbstractMetaObjectBase();

  /**
   * @brief Gets the literal name of the class.
   *
   * @return The literal name of the class as a C-string.
   */
  const std::string & className() const;

  /**
   * @brief gets the base class for the class this factory represents
   */
  const std::string & baseClassName() const;
  /**
   * @brief Gets the name of the class as typeid(BASE_CLASS).name() would return it
   */
  const std::string & typeidBaseClassName() const;

  /**
   * @brief Gets the path to the library associated with this factory
   *
   * @return Library path as a std::string
   */
  const std::string & getAssociatedLibraryPath() const;

  /**
   * @brief Sets the path to the library associated with this factory
   */
  void setAssociatedLibraryPath(const std::string & library_path);

  /**
   * @brief Associates a ClassLoader owner with this factory,
   *
   * @param loader Handle to the owning ClassLoader.
   */
  void addOwningClassLoader(ClassLoader * loader);

  /**
   * @brief Removes a ClassLoader that is an owner of this factory
   *
   * @param loader Handle to the owning ClassLoader.
   */
  void removeOwningClassLoader(const ClassLoader * loader);

  /**
   * @brief Indicates if the factory is within the usable scope of a ClassLoader
   *
   * @param loader Handle to the owning ClassLoader.
   * @return True if the factory is within the usable scope of a ClassLoader, false otherwise
   */
  bool isOwnedBy(const ClassLoader * loader) const;

  /**
   * @brief Indicates if the factory is within the usable scope of any ClassLoader
   *
   * @return true if the factory is within the usable scope of any ClassLoader, false otherwise
   */
  bool isOwnedByAnybody() const;

  /**
   * @brief Get the number of associated class Loaders
   *
   * @return number of associated class loaders
   */
  size_t getAssociatedClassLoadersCount() const;

  /**
   * @brief Get an associated ClassLoader pointer by index
   *
   * @param[in] index The index of the ClassLoader.
   * @return The ClassLoader pointer or undefined behaviour if the index is out of bounds
   */
  ClassLoader * getAssociatedClassLoader(size_t index) const;

protected:
  /**
   * This is needed to make base class polymorphic (i.e. have a vtable)
   */
  virtual void dummyMethod() {}

  AbstractMetaObjectBaseImpl * impl_;
};

/**
 * @class AbstractMetaObject
 * @brief Abstract base class for factories where polymorphic type variable indicates base class for
 *  plugin interface.
 *
 * @parm B The base class interface for the plugin
 */
template<class B>
class AbstractMetaObject : public AbstractMetaObjectBase
{
public:
  /**
   * @brief A constructor for this class
   *
   * @param name The literal name of the class.
   */
  AbstractMetaObject(const std::string & class_name, const std::string & base_class_name)
  : AbstractMetaObjectBase(class_name, base_class_name, typeid(B).name())
  {
  }

  /**
   * @brief Defines the factory interface that the MetaObject must implement.
   *
   * @return A pointer of parametric type B to a newly created object.
   */
  virtual B * create() const = 0;
  /// Create a new instance of a class.
  /// Cannot be used for singletons.

private:
  AbstractMetaObject();
  AbstractMetaObject(const AbstractMetaObject &);
  AbstractMetaObject & operator=(const AbstractMetaObject &);
};

/**
 * @class MetaObject
 * @brief The actual factory.
 *
 * @parm C The derived class (the actual plugin)
 * @parm B The base class interface for the plugin
 */
template<class C, class B>
class MetaObject : public AbstractMetaObject<B>
{
public:
  /**
   * @brief Constructor for the class
   */
  MetaObject(const std::string & class_name, const std::string & base_class_name)
  : AbstractMetaObject<B>(class_name, base_class_name)
  {
  }

  /**
   * @brief The factory interface to generate an object. The object has type C in reality, though a
   * pointer of the base class type is returned.
   *
   * @return A pointer to a newly created plugin with the base class type (type parameter B)
   */
  B * create() const
  {
    return new C;
  }
};

}  // namespace impl
}  // namespace class_loader

#endif  // CLASS_LOADER__META_OBJECT_HPP_
