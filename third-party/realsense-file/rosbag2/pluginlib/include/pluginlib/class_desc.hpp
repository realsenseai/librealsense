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

#ifndef PLUGINLIB__CLASS_DESC_HPP_
#define PLUGINLIB__CLASS_DESC_HPP_

#include <string>

namespace pluginlib
{

/// Storage for information about a given class.
class ClassDesc
{
public:
  /**
   * \param lookup_name The lookup name of the class
   * \param derived_class The type of the derived class of the class
   * \param base_class The type of the class, corresponds to the type of the base class
   * \param package The package the class lives in
   * \param description A description for the class
   * \param library_name The name of the containing library for the class (not a full path!)
   * \param plugin_manifest_path The path to the plugin manifest file
   */
  ClassDesc(
    const std::string & lookup_name, const std::string & derived_class,
    const std::string & base_class, const std::string & package,
    const std::string & description, const std::string & library_name,
    const std::string & plugin_manifest_path)
  : lookup_name_(lookup_name),
    derived_class_(derived_class),
    base_class_(base_class),
    package_(package),
    description_(description),
    library_name_(library_name),
    resolved_library_path_("UNRESOLVED"),
    plugin_manifest_path_(plugin_manifest_path) {}

  std::string lookup_name_;
  std::string derived_class_;
  std::string base_class_;
  std::string package_;
  std::string description_;
  std::string library_name_;
  std::string resolved_library_path_;  // This is set by pluginlib::ClassLoader at load time.
  std::string plugin_manifest_path_;
};

}  // namespace pluginlib

#endif  // PLUGINLIB__CLASS_DESC_HPP_
