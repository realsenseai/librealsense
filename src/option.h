// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.
#pragma once

#include "core/option-interface.h"
#include "librealsense-exception.h"

#include <rsutils/string/from.h>
#include <rsutils/lazy.h>

#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <tuple>
#include <atomic>

namespace librealsense
{
    class observable_option
    {
    public:
        void add_observer(std::function<void(float)> callback)
        {
            _callbacks.push_back(callback);
        }

        void notify(float val)
        {
            for (auto callback : _callbacks)
            {
                callback(val);
            }
        }

    private:
        std::vector<std::function<void(float)>> _callbacks;
    };

    class readonly_option : virtual public option
    {
    public:
        bool is_read_only() const override { return true; }

        void set(float) override
        {
            throw not_implemented_exception("This option is read-only!");
        }

        void enable_recording(std::function<void(const option&)> record_action) override
        {
            //empty
        }
    };

    class const_value_option : public readonly_option, public extension_snapshot
    {
    public:
        const_value_option( std::string const & desc, float val )
            : _val( [val]() { return val; } )
            , _desc( desc )
        {
        }

        const_value_option( std::string const & desc, rsutils::lazy< float > && val )
            : _val( std::move( val ) )
            , _desc( desc )
        {
        }

        float query() const override { return *_val; }
        option_range get_range() const override { return { *_val, *_val, 0, *_val }; }
        bool is_enabled() const override { return true; }

        const char* get_description() const override { return _desc.c_str(); }

        void update(std::shared_ptr<extension_snapshot> ext) override
        {
            if (auto opt = As<option>(ext))
            {
                auto new_val = opt->query();
                _val = rsutils::lazy< float >( [new_val]() { return new_val; } );
                _desc = opt->get_description();
            }
        }
    private:
        rsutils::lazy< float > _val;
        std::string _desc;
    };

    class LRS_EXTENSION_API option_base : public virtual option
    {
    public:
        option_base(const option_range& opt_range)
            : _opt_range(opt_range)
        {}

        bool is_valid(float value) const;

        option_range get_range() const override;

        virtual void enable_recording(std::function<void(const option&)> recording_action) override;
    protected:
        const option_range _opt_range;
        std::function<void(const option&)> _recording_function = [](const option&) {};
    };

    template<class T>
    class enum_option : public virtual option
    {
    public:
        const char* get_value_description(float val) const override
        {
            return get_string((T)((int)val));
        }
    };

    class option_description : public virtual option
    {
    public:
        option_description(std::string description)
            :_description(description) {}

        const char* get_description() const override
        {
            return _description.c_str();
        }

    protected:
        std::string _description;
    };

    template<class T>
    class cascade_option : public T, public observable_option
    {
    public:
        template <class... Args>
        cascade_option(Args&&... args) :
            T(std::forward<Args>(args)...) {}

        void set(float value) override
        {
            auto old = T::query();
            T::set(value);
            try
            {
                notify(value);
            }
            catch (...)
            {
                if (old != value)
                    T::set(old);
                LOG_WARNING("An exception was thrown while notifying inside cascase_option::set");
                throw;
            }

        }

        virtual void set_with_no_signal(float value)
        {
            T::set(value);
        }
    };

    template<class T>
    class LRS_EXTENSION_API ptr_option : public option_base
    {
    public:
        ptr_option(T min, T max, T step, T def, T* value, const std::string& desc)
            : option_base({ static_cast<float>(min),
                            static_cast<float>(max),
                            static_cast<float>(step),
                            static_cast<float>(def), }),
            _min(min), _max(max), _step(step), _def(def), _value(value), _desc(desc)
        {
            static_assert((std::is_arithmetic<T>::value), "ptr_option class supports arithmetic built-in types only");
            _on_set = [](float x) {};
        }

        ptr_option(T min, T max, T step, T def, T* value, const std::string& desc,
            const std::map<float, std::string>& description_per_value)
            : option_base({ static_cast<float>(min),
                            static_cast<float>(max),
                            static_cast<float>(step),
                            static_cast<float>(def), }),
            _min(min), _max(max), _step(step), _def(def), _value(value), _desc(desc), _item_desc(description_per_value)
        {
            static_assert((std::is_arithmetic<T>::value), "ptr_option class supports arithmetic built-in types only");
            _on_set = [](float x) {};
        }


        void set(float value) override
        {
            T val = static_cast<T>(value);
            if ((_max < val) || (_min > val))
                throw invalid_value_exception( rsutils::string::from() << "Given value " << value << " is outside ["
                                                                       << _min << "," << _max << "] range!" );
            *_value = val;
            _on_set(value);
        }

        float query() const override
        {
            return static_cast<float>(*_value);
        }

        bool is_enabled() const override { return true; }

        void enable_recording(std::function<void(const option&)> record_action) override {}

        const char* get_description() const override { return _desc.c_str(); }

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

        void on_set(std::function<void(float)> on_set) { _on_set = on_set; }
    private:
        T _min, _max, _step, _def; // stored separately so that logic can be done in base type
        T* _value;
        std::string _desc;
        std::map<float, std::string> _item_desc;
        std::function<void(float)> _on_set;
    };

    class LRS_EXTENSION_API float_option : public option_base
    {
    public:
        float_option(option_range range) : option_base(range), _value(range.def) {}

        void set(float value) override;
        float query() const override { return _value; }
        bool is_enabled() const override { return true; }
        // TODO: expose this outwards
        const char* get_description() const override { return "A simple custom option for a processing block or software device"; }
    protected:
        float _value;
    };

    template<class T>
    class float_option_with_description : public float_option, public option_description, public enum_option<T>
    {
    public:
        float_option_with_description(option_range range, std::string description)
            :float_option(range), option_description(description) {}

        const char* get_description() const override { return option_description::get_description(); }
    };

    class readonly_float_option : public float_option
    {
    public:
        readonly_float_option(const option_range& range)
            : float_option(range) {}

        bool is_read_only() const override { return true; }
        const char* get_description() const override { return "A simple read-only custom option for a software device"; }
        void set(float) override
        {
            // TODO: Use get_description() to give a more useful error message when user-supplied descriptions are implemented
            throw not_implemented_exception("This option is read-only!");
        }

        void update(float val) { float_option::set(val); }
    };

    class LRS_EXTENSION_API bool_option : public float_option
    {
    public:
        bool_option(bool default_on = true) : float_option(option_range{ 0, 1, 1, default_on ? 1.f : 0.f }) {}
        bool is_true() { return (_value > _opt_range.min); }
        // TODO: expose this outwards
        const char* get_description() const override { return "A simple custom option for a processing block"; }

        using ptr = std::shared_ptr< bool_option >;
    };


    /** Wrapper for another option -- forwards all API calls to the proxied option
    *such that specific functionality can be easily overriden */
    class proxy_option : public option
    {
    public:
        explicit proxy_option(std::shared_ptr<option> proxy_option)
            : _proxy(proxy_option)
        {}

        const char* get_value_description(float val) const override
        {
            return _proxy->get_value_description(val);
        }
        const char* get_description() const override
        {
            return _proxy->get_description();
        }
        virtual void set(float value) override
        {
            return _proxy->set(value);
        }
        float query() const override
        {
            return _proxy->query();
        }

        option_range get_range() const override
        {
            return _proxy->get_range();
        }

        bool is_enabled() const override
        {
            return  _proxy->is_enabled();
        }

        bool is_read_only() const override
        {
            return  _proxy->is_read_only();
        }

        void enable_recording(std::function<void(const option&)> record_action) override
        {
            _recording_function = record_action;
        }
    protected:
        std::shared_ptr<option> _proxy;
        std::function<void(const option&)> _recording_function = [](const option&) {};
    };

    /** \brief auto_disabling_control class provided a control
    * that disable auto-control when changing the auto disabling control value */
    class auto_disabling_control : public proxy_option
    {
    public:
        explicit auto_disabling_control(std::shared_ptr<option> auto_disabling,
            std::shared_ptr<option> affected_option,
            std::vector<float> move_to_manual_values = { 1.f },
            float manual_value = 0.f)
            : proxy_option(auto_disabling), _affected_control(affected_option)
            , _move_to_manual_values(move_to_manual_values), _manual_value(manual_value)
        {}

        void set( float value ) override;

    private:
        std::weak_ptr<option>   _affected_control;
        std::vector<float>      _move_to_manual_values;
        float                   _manual_value;
    };

    /** \brief gated_option class will permit the user to
    *  perform only read (query) of the read_only option when its affecting_option is set */
    class gated_option : public proxy_option
    {
    public:
        explicit gated_option(std::shared_ptr<option> leading_to_read_only,
            std::vector<std::pair<std::shared_ptr<option>, std::string>> gated_options)
            : proxy_option(leading_to_read_only)
        {
            for (auto& gated : gated_options)
            {
                _gated_options.push_back(gated);
            }
        }

        void set( float value ) override;

    private:
        std::vector < std::pair<std::weak_ptr<option>, std::string> >  _gated_options;
    };

    /** \brief gated_by_value_option class will permit the user to
     *  perform only read (query) of the read_only option when its affecting_option value is not as needed.
     *  Receives a tuple of < option, value write is allowed in, explenation string in case of failure to read >*/
    class gated_by_value_option : public proxy_option
    {
    public:
        explicit gated_by_value_option(
            std::shared_ptr< option > leading_to_read_only,
            std::vector< std::tuple< std::shared_ptr< option >, float, std::string > > gating_options )
            : proxy_option( leading_to_read_only )
        {
            for( auto & gate : gating_options )
            {
                _gating_options.push_back( gate );
            }
        }

        void set( float requested_option_value ) override;

    private:
        std::vector< std::tuple< std::weak_ptr< option >, float, std::string > > _gating_options;
    };

    // Read-only while is_locked() holds; set() then throws reason instead of reaching the device.
    class locked_option : public proxy_option
    {
    public:
        locked_option( std::shared_ptr< option > proxy, std::function< bool() > is_locked, std::string reason )
            : proxy_option( std::move( proxy ) )
            , _is_locked( std::move( is_locked ) )
            , _reason( std::move( reason ) )
        {
        }

        void set( float value ) override;
        bool is_read_only() const override { return _is_locked() || proxy_option::is_read_only(); }

    private:
        std::function< bool() > _is_locked;
        std::string _reason;
    };


    /** \brief class provided a control
    * that changes min distance value when changing max distance value */
    class max_distance_option : public proxy_option, public observable_option
    {
    public:
        explicit max_distance_option(std::shared_ptr<option> max_option,
            std::shared_ptr<option> min_option)
            : proxy_option(max_option), _min_option(min_option)
        {}

        void set( float value ) override;

    private:
        std::weak_ptr<option>   _min_option;
    };

    /** \brief class provided a control
    * that changes max distance value when changing min distance value */
    class min_distance_option : public proxy_option, public observable_option
    {
    public:
        explicit min_distance_option(std::shared_ptr<option> min_option,
            std::shared_ptr<option> max_option)
            : proxy_option(min_option), _max_option(max_option)
        {}

        void set( float value ) override;

    private:
        std::weak_ptr<option>   _max_option;
    };

    class sensor_base;

    class enable_motion_correction : public option_base
    {
    public:
        void set(float value) override;

        float query() const override;

        bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Enable/Disable Automatic Motion Data Correction";
        }

        enable_motion_correction(sensor_base* mm_ep, const option_range& opt_range);

    private:
        std::atomic<bool>  _is_active;
    };

}
