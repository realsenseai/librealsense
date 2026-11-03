// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#ifndef LIBREALSENSE_RS2_OPTIONS_HPP
#define LIBREALSENSE_RS2_OPTIONS_HPP

#include "rs_types.hpp"
#include "../h/rs_types.h"
#include "../h/rs_composite_option.h"
#include "../h/rs_dpp_header.h"   // DPP_HEADER_CURRENT_VERSION - see check_version_supported() below

#include <memory>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <limits>


namespace rs2
{
    class option_value
    {
        std::shared_ptr< const rs2_option_value > _value;

        // Set only for values taken out of an options-list, which is what carries the ranges
        std::shared_ptr< rs2_options_list > _list;
        int _index = -1;

    public:
        explicit option_value( rs2_option_value const * handle )
            : _value( handle, rs2_delete_option_value )
        {
        }
        option_value( rs2_option_value const * handle, std::shared_ptr< rs2_options_list > list, int index )
            : _value( handle, rs2_delete_option_value )
            , _list( std::move( list ) )
            , _index( index )
        {
        }
        option_value( option_value const & ) = default;
        option_value( option_value && ) = default;
        option_value() = default;

        enum invalid_t { invalid };
        option_value( rs2_option option_id, invalid_t )
            : _value( new rs2_option_value{ option_id, false, RS2_OPTION_TYPE_COUNT } ) {}

        option_value( rs2_option option_id, int64_t as_integer )
            : _value( new rs2_option_value{ option_id, true, RS2_OPTION_TYPE_INTEGER } )
        {
            const_cast< rs2_option_value * >( _value.get() )->as_integer = as_integer;
        }
        option_value( rs2_option option_id, float as_float )
            : _value( new rs2_option_value{ option_id, true, RS2_OPTION_TYPE_FLOAT } )
        {
            const_cast< rs2_option_value * >( _value.get() )->as_float = as_float;
        }
        option_value( rs2_option option_id, char const * as_string )
            : _value( new rs2_option_value{ option_id, true, RS2_OPTION_TYPE_STRING } )
        {
            const_cast< rs2_option_value * >( _value.get() )->as_string = as_string;
        }
        option_value( rs2_option option_id, bool as_boolean )
            : _value( new rs2_option_value{ option_id, true, RS2_OPTION_TYPE_BOOLEAN } )
        {
            const_cast<rs2_option_value *>(_value.get())->as_integer = as_boolean;
        }

        option_value & operator=( option_value const & ) = default;
        option_value & operator=( option_value && ) = default;

        rs2_option_value const * operator->() const { return _value.get(); }
        operator rs2_option_value const *() const { return _value.get(); }

        /**
        * \return whether a range is available with this value
        */
        bool has_range() const
        {
            rs2_option_range range;
            return get_range( range );
        }

        /**
        * The range of values this option accepts. Throws if no range is available.
        */
        option_range range() const
        {
            rs2_option_range range;
            if( ! get_range( range ) )
                throw std::runtime_error( std::string( "no range available for option " )
                                          + rs2_option_to_string( _value ? _value->id : RS2_OPTION_COUNT ) );
            return { range.min, range.max, range.def, range.step };
        }

    private:
        bool get_range( rs2_option_range & range ) const
        {
            if( ! _list )
                return false;
            rs2_error * e = nullptr;
            int const has_range = rs2_get_option_range_from_list( _list.get(), _index, &range, &e );
            error::handle( e );
            return has_range != 0;
        }
    };

    class options_list
    {
    public:
        options_list( options_list const & ) = default;
        options_list( options_list && ) = default;

        explicit options_list( std::shared_ptr< rs2_options_list > list )
            : _list( std::move( list ) )
        {
            rs2_error * e = nullptr;
            _size = rs2_get_options_list_size( _list.get(), &e );
            error::handle( e );
        }

        options_list()
            : _list( nullptr )
            , _size( 0 )
        {
        }

        option_value operator[]( size_t index ) const
        {
            rs2_error * e = nullptr;
            auto value = rs2_get_option_value_from_list( _list.get(), static_cast< int >( index ), &e );
            error::handle( e );
            return option_value( value, _list, static_cast< int >( index ) );
        }

        size_t size() const { return _size; }

        option_value front() const { return ( *this )[0]; }
        option_value back() const { return ( *this )[size() - 1]; }

        class iterator
        {
            iterator( const options_list & list, size_t index )
                : _list( list )
                , _index( index )
            {
            }

        public:
            option_value operator*() const { return _list[_index]; }
            
            bool operator!=( const iterator & other ) const
            {
                return other._index != _index || &other._list != &_list;
            }
            bool operator==( const iterator & other ) const
            {
                return ! ( *this != other );
            }

            iterator & operator++()
            {
                _index++;
                return *this;
            }

        private:
            friend options_list;
            const options_list & _list;
            size_t _index;
        };

        iterator begin() const { return iterator( *this, 0 ); }
        iterator end() const { return iterator( *this, size() ); }

        std::shared_ptr< rs2_options_list > get() const { return _list; };

    private:
        std::shared_ptr< rs2_options_list > _list;
        size_t _size;
    };
    
    class options_changed_callback : public rs2_options_changed_callback
    {
        std::function< void( const options_list & ) > _callback;

    public:
        explicit options_changed_callback( const std::function< void( const options_list & ) > & callback )
            : _callback( callback )
        {
        }

        void on_value_changed( rs2_options_list * list ) override
        {
            std::shared_ptr< rs2_options_list > sptr( list, rs2_delete_options_list );
            options_list opt_list( sptr );
            _callback( opt_list );
        }

        void release() override { delete this; }
    };

    class options
    {
    public:
        /**
        * check if particular option is supported
        * \param[in] option     option id to be checked
        * \return true if option is supported
        */
        bool supports(rs2_option option) const
        {
            rs2_error* e = nullptr;
            auto res = rs2_supports_option(_options, option, &e);
            error::handle(e);
            return res > 0;
        }

        /**
        * get option description
        * \param[in] option     option id to be checked
        * \return human-readable option description
        */
        const char* get_option_description(rs2_option option) const
        {
            rs2_error* e = nullptr;
            auto res = rs2_get_option_description(_options, option, &e);
            error::handle(e);
            return res;
        }

        /**
        * get option name
        * \param[in] option     option id to be checked
        * \return human-readable option name
        */
        const char* get_option_name(rs2_option option) const
        {
            rs2_error* e = nullptr;
            auto res = rs2_get_option_name(_options, option, &e);
            error::handle(e);
            return res;
        }

        /**
        * get option value description (in case specific option value hold special meaning)
        * \param[in] option     option id to be checked
        * \param[in] val      value of the option
        * \return human-readable description of a specific value of an option or null if no special meaning
        */
        const char* get_option_value_description(rs2_option option, float val) const
        {
            rs2_error* e = nullptr;
            auto res = rs2_get_option_value_description(_options, option, val, &e);
            error::handle(e);
            return res;
        }

        /**
        * read option's float value
        * \param[in] option   option id to be queried
        * \return value of the option
        */
        float get_option(rs2_option option) const
        {
            rs2_error* e = nullptr;
            auto res = rs2_get_option(_options, option, &e);
            error::handle(e);
            return res;
        }

        /**
        * read option's value
        * \param[in] option_id   option id to be queried
        * \return                option value
        */
        option_value get_option_value( rs2_option option_id ) const
        {
            rs2_error * e = nullptr;
            auto value = rs2_get_option_value( _options, option_id, &e );
            error::handle( e );
            return option_value( value );
        }

        /**
        * retrieve the available range of values of a supported option
        * \return option  range containing minimum and maximum values, step and default value
        */
        option_range get_option_range(rs2_option option) const
        {
            option_range result;
            rs2_error* e = nullptr;
            rs2_get_option_range(_options, option,
                &result.min, &result.max, &result.step, &result.def, &e);
            error::handle(e);
            return result;
        }

        /**
        * write new value to the option
        * \param[in] option     option id to be queried
        * \param[in] value      new value for the option
        */
        void set_option(rs2_option option, float value) const
        {
            rs2_error* e = nullptr;
            rs2_set_option(_options, option, value, &e);
            error::handle(e);
        }

        /**
        * write new value to the option
        * \param[in] option     option id to be queried
        * \param[in] value      option (id,type,is_valid,new value)
        */
        void set_option_value( option_value const & value ) const
        {
            rs2_error * e = nullptr;
            rs2_set_option_value( _options, value, &e );
            error::handle( e );
        }

        /**
        * check if particular option is read-only
        * \param[in] option     option id to be checked
        * \return true if option is read-only
        */
        bool is_option_read_only(rs2_option option) const
        {
            rs2_error* e = nullptr;
            auto res = rs2_is_option_read_only(_options, option, &e);
            error::handle(e);
            return res > 0;
        }

        /**
         * sets a callback in case an option in this options container value is updated
         * \param[in] callback     the callback function
         */
        void on_options_changed( std::function< void( const options_list & ) > callback ) const
        {
            rs2_error * e = nullptr;
            rs2_set_options_changed_callback_cpp( _options, new options_changed_callback( callback ), &e );
            error::handle( e );
        }

        std::vector<rs2_option> get_supported_options()
        {
            std::vector<rs2_option> res;
            rs2_error* e = nullptr;
            std::shared_ptr< rs2_options_list > options_list( rs2_get_options_list(_options, &e), rs2_delete_options_list);
            error::handle( e );

            for (auto opt = 0; opt < rs2_get_options_list_size(options_list.get(), &e);opt++)
            {
                res.push_back(rs2_get_option_from_list(options_list.get(), opt, &e));
            }
            return res;
        };

        options_list get_supported_option_values()
        {
            rs2_error * e = nullptr;
            std::shared_ptr< rs2_options_list > sptr(
                rs2_get_options_list( _options, &e ),
                rs2_delete_options_list );
            error::handle( e );
            return options_list( sptr );
        };

        /**
        * See rs_composite_option.h for the underlying C API and atomicity contract (one UVC
        * transaction per call) - these methods just forward to the corresponding rs2_* function.
        */

        /**
        * write new value to a composite option, atomically, in ONE UVC transaction
        * \param[in] id     composite option id to write
        * \param[in] data   pointer to the caller's struct matching the option's documented wire layout
        * \param[in] size   sizeof(...) of the caller's struct
        */
        void set_composite_option( rs2_composite_option_id id, const void * data, size_t size ) const
        {
            // The C API takes an unsigned int; guard the narrowing cast rather than silently
            // truncating a caller-supplied size_t larger than UINT_MAX into a smaller, wrong value.
            if( size > (std::numeric_limits< unsigned int >::max)() )
                throw std::runtime_error( "composite option payload size (" + std::to_string( size )
                                           + ") exceeds what the C API can represent" );

            rs2_error * e = nullptr;
            rs2_set_composite_option( _options, id, data, static_cast< unsigned int >( size ), &e );
            error::handle( e );
        }

        /**
        * read a composite option's current raw payload, atomically, in ONE UVC transaction
        * \param[in] id   composite option id to read
        * \return         the option's current raw payload bytes - cast to a struct matching the
        *                 option's documented wire layout
        */
        std::vector< uint8_t > get_composite_option( rs2_composite_option_id id ) const
        {
            rs2_error * e = nullptr;
            auto buffer = rs2_get_composite_option( _options, id, &e );
            return unwrap_raw_data_buffer( buffer, e );
        }

        /**
        * read a composite option's supported {min,max,step,def} bounds
        * \param[in] id   composite option id to read
        * \return         the option's range payload bytes - cast to the range struct documented for id
        */
        std::vector< uint8_t > get_composite_option_range( rs2_composite_option_id id ) const
        {
            rs2_error * e = nullptr;
            auto buffer = rs2_get_composite_option_range( _options, id, &e );
            return unwrap_raw_data_buffer( buffer, e );
        }

        /**
        * typed counterpart to get_composite_option() - casts the raw payload directly into T.
        * Validates sizeof(T) matches, and for structs with a composed dpp_header, that its
        * version is DPP_HEADER_CURRENT_VERSION (see rs_dpp_header.h), unpopulated (0) included.
        * \param[in] id   composite option id to read
        * \return         T, populated from the option's current raw payload
        */
        template< typename T >
        T get_composite_option_as( rs2_composite_option_id id ) const
        {
            T value{};
            cast_composite_payload( get_composite_option( id ), value );
            return value;
        }

        /**
        * typed counterpart to get_composite_option_range() - casts the raw {min,max,step,def}
        * payload into TRange. Validates sizeof(TRange); the wrapper itself has no version field,
        * but each of its four bounds does, each checked the same way as check_version_supported().
        * \param[in] id   composite option id to read
        * \return         TRange, populated from the option's raw range payload
        */
        template< typename TRange >
        TRange get_composite_option_range_as( rs2_composite_option_id id ) const
        {
            TRange range{};
            cast_composite_payload( get_composite_option_range( id ), range );
            return range;
        }

        /**
        * typed counterpart to set_composite_option() - sends value itself. For structs carrying a
        * `header.version` field, rejects a value whose version isn't current - most likely a
        * get-modify-set that left the header zero-initialized.
        * \param[in] id      composite option id to write
        * \param[in] value   the caller's struct matching the option's documented wire layout
        */
        template< typename T >
        void set_composite_option_from( rs2_composite_option_id id, const T & value ) const
        {
            check_version_supported( value );
            set_composite_option( id, &value, sizeof( T ) );
        }

        /**
        * check if particular composite option is supported (and currently enabled)
        * \param[in] id   composite option id to be checked
        * \return true if the composite option is supported
        */
        bool supports_composite_option( rs2_composite_option_id id ) const
        {
            rs2_error * e = nullptr;
            auto res = rs2_supports_composite_option( _options, id, &e );
            error::handle( e );
            return res > 0;
        }

        /**
        * check if a composite option is read-only
        * \param[in] id   composite option id to be checked
        * \return true if the composite option is read-only
        */
        bool is_composite_option_read_only( rs2_composite_option_id id ) const
        {
            rs2_error * e = nullptr;
            auto res = rs2_is_composite_option_read_only( _options, id, &e );
            error::handle( e );
            return res > 0;
        }

        /**
        * get a composite option's human-readable description
        * \param[in] id   composite option id to describe
        * \return human-readable composite option description
        */
        const char * get_composite_option_description( rs2_composite_option_id id ) const
        {
            rs2_error * e = nullptr;
            auto res = rs2_get_composite_option_description( _options, id, &e );
            error::handle( e );
            return res;
        }

        /**
        * \return the list of composite option ids this options object (sensor or embedded_filter) supports
        */
        std::vector< rs2_composite_option_id > get_supported_composite_options() const
        {
            std::vector< rs2_composite_option_id > res;
            rs2_error * e = nullptr;
            std::shared_ptr< rs2_composite_options_list > list( rs2_get_composite_options_list( _options, &e ),
                                                                 rs2_delete_composite_options_list );
            error::handle( e );

            auto size = rs2_get_composite_options_list_size( list.get(), &e );
            error::handle( e );
            for( auto i = 0; i < size; i++ )
            {
                res.push_back( rs2_get_composite_option_from_list( list.get(), i, &e ) );
                error::handle( e );
            }
            return res;
        }

        options& operator=(const options& other)
        {
            _options = other._options;
            return *this;
        }
        // if operator= is ok, this should be ok too
        options(const options& other) : _options(other._options) {}

        virtual ~options() = default;

    protected:
        explicit options(rs2_options* o = nullptr) : _options(o)
        {
        }

        template<class T>
        options& operator=(const T& dev)
        {
            _options = (rs2_options*)(dev.get());
            return *this;
        }

    private:
        // Shared unwrap helper for get_composite_option()/get_composite_option_range() - hides the
        // raw rs2_raw_data_buffer/manual-free (mirrors safety_sensor::get_safety_preset's pattern).
        static std::vector< uint8_t > unwrap_raw_data_buffer( const rs2_raw_data_buffer * buffer, rs2_error * e )
        {
            std::shared_ptr< const rs2_raw_data_buffer > guard( buffer, rs2_delete_raw_data );
            error::handle( e );

            rs2_error * e2 = nullptr;
            auto size = rs2_get_raw_data_size( guard.get(), &e2 );
            error::handle( e2 );

            auto start = rs2_get_raw_data( guard.get(), &e2 );
            error::handle( e2 );

            std::vector< uint8_t > result;
            result.insert( result.begin(), start, start + size );
            return result;
        }

        // ---- typed composite-option cast helpers (get_composite_option_as() and friends) -----
        // Detects whether T has a `.header.version` member via a composed dpp_header - value
        // structs like rs2_icr_control.
        template< typename U >
        class has_header_version
        {
            template< typename V > static auto test( int ) -> decltype( std::declval< V >().header.version, std::true_type{} );
            template< typename > static std::false_type test( ... );
        public:
            static const bool value = decltype( test< U >( 0 ) )::value;
        };

        // Detects whether T is a {min,max,step,def} range wrapper whose ELEMENTS each carry their
        // own `.header.version` - e.g. rs2_icr_control_range. The wrapper itself has no version
        // field of its own (see rs_icr_control.h) - each of its four bounds does.
        template< typename U >
        class has_min_header_version
        {
            template< typename V > static auto test( int ) -> decltype( std::declval< V >().min.header.version, std::true_type{} );
            template< typename > static std::false_type test( ... );
        public:
            static const bool value = decltype( test< U >( 0 ) )::value;
        };

        // T has neither shape - no dpp_header anywhere in it - nothing to check. Every composite-
        // option struct currently in the SDK has one or the other, but a future one might not
        // (e.g. a control outside the HKR DPP family entirely).
        template< typename T >
        static typename std::enable_if< ! has_header_version< T >::value && ! has_min_header_version< T >::value >::type
        check_version_supported( const T & )
        {
        }

        // This is what makes the version field worth having: reject anything other than
        // DPP_HEADER_CURRENT_VERSION up front - including 0, the "never populated" sentinel - rather
        // than silently misreading a struct laid out by a wire version this build doesn't know.
        template< typename T >
        static typename std::enable_if< has_header_version< T >::value >::type
        check_version_supported( const T & value )
        {
            if( value.header.version != DPP_HEADER_CURRENT_VERSION )
                throw std::runtime_error( "composite option struct has unsupported wire version "
                                           + std::to_string( (int)value.header.version ) + " (this SDK build supports version "
                                           + std::to_string( (int)DPP_HEADER_CURRENT_VERSION )
                                           + ") - wrong struct for this option id, a zero-initialized header about to "
                                             "be sent, or a newer device/firmware speaking a wire version this build "
                                             "doesn't know yet?" );
        }

        // Range wrapper (e.g. rs2_icr_control_range) - checks each of the four bounds' own
        // header.version the same way, since the wrapper carries none of its own.
        template< typename T >
        static typename std::enable_if< has_min_header_version< T >::value >::type
        check_version_supported( const T & value )
        {
            check_version_supported( value.min );
            check_version_supported( value.max );
            check_version_supported( value.step );
            check_version_supported( value.def );
        }

        template< typename T >
        static void cast_composite_payload( const std::vector< uint8_t > & raw, T & out )
        {
            if( raw.size() != sizeof( T ) )
                throw std::runtime_error( "composite option payload size (" + std::to_string( raw.size() )
                                           + ") does not match sizeof(T) (" + std::to_string( sizeof( T ) )
                                           + ") - wrong struct for this option id?" );
            std::memcpy( &out, raw.data(), sizeof( T ) );
            check_version_supported( out );
        }

        rs2_options* _options;
    };


}
#endif // LIBREALSENSE_RS2_OIPTIONS_HPP
