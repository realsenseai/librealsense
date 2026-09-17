// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include <src/option.h>

#include <functional>
#include <memory>


namespace realdds {
class dds_option;
}  // namespace realdds


namespace librealsense {


    // A facade for a realdds::dds_option exposing librealsense interface
    class rs_dds_option : public option_base
    {
        std::shared_ptr< realdds::dds_option > _dds_opt;
        rs2_option_type const _rs_type;

    public:
        typedef std::function< void(rsutils::json value) > set_option_callback;
        typedef std::function< rsutils::json() > query_option_callback;
        typedef std::function< bool() > locked_predicate; // e.g options that cannot change while streaming

    private:
        set_option_callback _set_opt_cb;
        query_option_callback _query_opt_cb;
        locked_predicate _locked;

    public:
        rs_dds_option(const std::shared_ptr< realdds::dds_option >& dds_opt,
            set_option_callback set_opt_cb,
            query_option_callback query_opt_cb);

        void set_locked_predicate( locked_predicate is_locked ) { _locked = std::move( is_locked ); }

        rsutils::json get_value() const noexcept override;
        rs2_option_type get_value_type() const noexcept override { return _rs_type; }
        virtual void set_value(rsutils::json) override;

        virtual void set(float value) override;

        virtual float query() const override;

        bool is_read_only() const override;
        virtual bool is_enabled() const override;
        virtual const char* get_description() const override;
        virtual const char* get_value_description(float) const override;
    };

    template<class T>
    class rs_dds_option_items_desc : public rs_dds_option
    {
    private:
        std::map<float, std::string> _item_desc;

    public:
        rs_dds_option_items_desc(const std::shared_ptr< realdds::dds_option >& dds_opt,
            const std::map<float, std::string>& description_per_value,
            set_option_callback set_opt_cb,
            query_option_callback query_opt_cb)
            : rs_dds_option(dds_opt, set_opt_cb, query_opt_cb)
            , _item_desc(description_per_value)
        { }

        const char* get_value_description(float val) const override 
        { 
            auto it = _item_desc.find(val);
            if (it != _item_desc.end())
            {
                return it->second.c_str();
            }
            return nullptr;
        }

        void set_description(float val, const std::string& desc)
        {
            _item_desc[val] = desc;
        }
    };
}// namespace librealsense
