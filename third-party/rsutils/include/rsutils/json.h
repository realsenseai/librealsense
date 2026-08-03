// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.
#pragma once

#include "json-fwd.h"
#include <nlohmann/json.hpp>

// Keep in sync with NLOHMANN_JSON_MIN_VERSION in CMake/external_json.cmake: with
// USE_EXTERNAL_NLOHMANN_JSON=ON the header comes from the system package, which may be older
// than the version we bundle
static_assert( NLOHMANN_JSON_VERSION_MAJOR * 10000 + NLOHMANN_JSON_VERSION_MINOR * 100
                       + NLOHMANN_JSON_VERSION_PATCH
                   >= 31103,
               "nlohmann/json 3.11.3 or newer is required" );


namespace rsutils {


template< typename... Rest >
json_ref _nested_json( json const & j );
template< typename... Rest >
json_ref _nested_json( json const & j, bool ( json::*is_fn )() const );
template< typename... Rest >
json_ref _nested_json( json const & j, json_key const & inner, Rest... rest );
template< typename... Rest >
json_ref _nested_json( json const & j, json::size_type index, Rest... rest );


// A copyable reference to a read-only NON-OWNED json.
// I.e., this is for temporary use and is only valid as long as the original json object is alive!
// 
// As this is a read-only reference, no non-const functionality is exposed here.
// 
// This is used as the result of any 'nested' searches inside another JSON. As such, it is also used to communicate
// 'not-found' via the exists() call, so we're castable to bool.
// 
// We also use this together with json_base to supply json functionality before json is defined.
//
class json_ref
{
    json const & _j;

public:
    constexpr json_ref() : _j( missing_json ) {}
    constexpr json_ref( json const & j ) : _j( j ) {}

    template< typename... Rest >
    json_ref( json const & j, Rest... rest )
        : _j( _nested_json( j, std::forward< Rest >( rest )... ) )
    {}

    // A json value can be null, so we use the internal 'discarded' flag to denote a non-existent one
    bool exists() const noexcept { return ! _j.is_discarded(); }
    operator bool() const noexcept { return exists(); }

    constexpr json const & get_json() const noexcept { return _j; }
    constexpr operator json const &() const noexcept { return get_json(); }

    bool is_null() const noexcept { return _j.is_null(); }
    bool is_array() const noexcept { return _j.is_array(); }
    bool is_object() const noexcept { return _j.is_object(); }
    bool is_string() const noexcept { return _j.is_string(); }
    bool is_boolean() const noexcept { return _j.is_boolean(); }
    bool is_number() const noexcept { return _j.is_number(); }
    bool is_number_float() const noexcept { return _j.is_number_float(); }
    bool is_number_integer() const noexcept { return _j.is_number_integer(); }
    bool is_number_unsigned() const noexcept { return _j.is_number_unsigned(); }
    bool is_primitive() const noexcept { return _j.is_primitive(); }
    bool is_structured() const noexcept { return _j.is_structured(); }

    bool empty() const noexcept { return _j.empty(); }
    json::size_type size() const noexcept { return _j.size(); }
    json::value_t type() const noexcept { return _j.type(); }
    json::const_iterator begin() const noexcept { return _j.begin(); }
    json::const_iterator end() const noexcept { return _j.end(); }

    std::string dump( const int indent = -1 ) const { return _j.dump( indent ); }

    // Allow easy read-only lookup of nested json hierarchies:
    //      j["one"]["two"]["three"]           // undefined; will throw/assert if a hierarchy isn't there
    // But:
    //      j.nested( "one", "two", "three" )  // will not throw; does not copy!
    // The result is either a null JSON object or a valid one. The boolean operator can be used as an easy check:
    //      if( auto inside = j.nested( "one", "two" ) )
    //          { ... }
    //
    template< typename... Rest >
    inline json_ref nested( Rest... rest ) const
    {
        return _nested_json( _j, std::forward< Rest >( rest )... );
    }

    template< class Key > inline json::const_reference at( Key key ) const { return _j.at( std::forward< Key >( key ) ); }
    template< class Key > inline json::const_reference operator[]( Key key ) const { return _j.operator[]( key ); }
    template< class T > inline T get() const { return _j.get< T >(); }
    template< class T > inline void get_to( T & value ) const { _j.get_to( value ); }

    // If there, gets the value at the given key and returns true; otherwise false
    template< class T >
    bool get_ex( T & value ) const
    {
        if( ! exists() )
            return false;
        _j.get_to( value );
        return true;
    }

    // Get the JSON as a value, or a default if not there (throws if wrong type)
    template< class T >
    T default_value( T const & default_value ) const
    {
        return exists() ? _j.get< T >() : default_value;
    }

    // Get the object, with a default being an empty one; does not throw
    inline json const & default_object() const noexcept { return is_object() ? _j : empty_json_object; }

    // Get the string object, with a default being an empty one; does not throw
    inline json const & default_string() const noexcept { return is_string() ? _j : empty_json_string; }

    // Get a JSON string by reference (zero copy); it must be a string or it'll throw
    inline std::string const & string_ref() const { return _j.get_ref< const json::string_t & >(); }

    // Get a JSON string by reference (zero copy); does not throw
    inline std::string const & string_ref_or_empty() const { return default_string().get_ref< const json::string_t & >(); }
};


inline std::ostream & operator<<( std::ostream & os, json_ref const & j )
{
    return operator<<( os, static_cast< json const & >( j ) );
}


// When you do:
//     json j = j2.nested( "subkey" );
// The nesting returns a json_ref, correctly.
// operator=() get called. But the implementation is a copy-and-swap one, meaning it attempts to construct a json value
// from the argument. Since the argument isn't a json object (doesn't matter that it has a conversion operator), it does
// this by attempting to deserialize it with to_json()! We need to avoid this, for this usage and others...
inline void to_json( json & j, json_ref const & jr )
{
    j = jr.get_json();
}


template< typename... Rest >
json_ref _nested_json( json const & j )
{
    // j.nested()
    return j;
}
template< typename... Rest >
json_ref _nested_json( json const & j, bool ( json::*is_fn )() const )
{
    // j.nested( &json::is_string )
    if( ! ( j.*is_fn )() )
        return missing_json;
    return j;
}
template< typename... Rest >
json_ref _nested_json( json const & j, json_key const & inner, Rest... rest )
{
    // j.nested( "key", ... )
    auto it = j.find( inner );
    if( it == j.end() )
        return missing_json;
    return _nested_json( *it, std::forward< Rest >( rest )... );
}
template< typename... Rest >
json_ref _nested_json( json const & j, json::size_type index, Rest... rest )
{
    // j.nested( index, ... )
    if( ! j.is_array() )
        return missing_json;
    if( index >= j.size() )
        return missing_json;
    return _nested_json( j[index], std::forward< Rest >( rest )... );
}


// Since we know how we're derived from, we can get the json, or the reference to it, from this:
//     json <- nlohmann::basic_json<...> <- json_base
inline json_ref json_base::_ref() const
{
    return static_cast< json const & >( *this );
}


// Returns false if the object wasn't found
inline bool json_base::exists() const
{
    return _ref().exists();
}


// WARNING: this "overrides" the implicit operator<T> that the *derived* class defines that does implicit casting!
// I.e., if
//      bool b = j;
// used the implicit conversion ('bool b = j.get< bool >();') before, now it will do:
//      bool b = j.exists();
// In general, it is preferable to NEVER use implicit value conversions and instead stick to the .get<T>() syntax.
inline json_base::operator bool() const
{
    return exists();
}


// Get the JSON as a value, or a default if not there (throws if wrong type)
template< class T >
inline T json_base::default_value( T const & default_value ) const
{
    return _ref().default_value( default_value );
}

// If there, gets the value at the given key and returns true; otherwise false
template< class T >
inline bool json_base::get_ex( T & value ) const
{
    return _ref().get_ex( value );
}

// Get the object, with a default being an empty one; does not throw
inline json_ref json_base::default_object() const
{
    return _ref().default_object();
}

// Get the string object, with a default being an empty one; does not throw
inline json_ref json_base::default_string() const
{
    return _ref().default_string();
}

// Get a JSON string by reference (zero copy); it must be a string or it'll throw
inline std::string const & json_base::string_ref() const
{
    return _ref().string_ref();
}

// Get a JSON string by reference (zero copy); does not throw
inline std::string const & json_base::string_ref_or_empty() const
{
    return default_string().string_ref();
}

// Allow easy read-only lookup of nested json hierarchies:
//      j["one"]["two"]["three"]           // undefined; will throw/assert if a hierarchy isn't there
// But:
//      j.nested( "one", "two", "three" )  // will not throw; does not copy!
// The result is either a null JSON object or a valid one. The boolean operator can be used as an easy check:
//      if( auto inside = j.nested( "one", "two" ) )
//          { ... }
//
template< typename... Rest >
inline json_ref json_base::nested( Rest... rest ) const
{
    return _ref().nested( std::forward< Rest >( rest )... );
}


}  // namespace rsutils
