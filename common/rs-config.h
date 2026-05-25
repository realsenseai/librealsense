// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.
#pragma once

#include <rsutils/json.h>

#include <map>
#include <string>
#include <sstream>
#include <functional>

namespace rs2
{
    class config_value
    {
    public:
        template<class T>
        operator T()
        {
            std::stringstream ss;
            ss.str(_val);
            T res;
            ss >> res;
            return res;
        }

        // When converting config_value to string, we can't use >> operator since it reads until first whitespace rather than the whole string;
        // Therefore we use a different overload for strings
        operator std::string()
        {
            return _val;
        }

        config_value(std::string val) : _val(std::move(val)) {}

    private:
        std::string _val;
    };

    class config_file
    {
    public:
        config_file();
        config_file( std::string const & filename );

        void set_default(const char* key, const char* calculate);

        template<class T>
        void set_default(const char* key, T val)
        {
            std::stringstream ss;
            ss << val;
            set_default(key, ss.str().c_str());
        }

        bool operator==(const config_file& other) const;

        config_file& operator=(const config_file& other);

        void set(const char* key, const char* value);
        std::string get(const char* key, const char* def) const;

        config_value get(const char* key) const;

        template<class T>
        T get_or_default(const char* key, T def) const
        {
            if (contains(key)) return get(key);
            return def;
        }

        template<class T>
        void set(const char* key, T val)
        {
            std::stringstream ss;
            ss << val;
            set(key, ss.str().c_str());
        }

        bool contains(const char* key) const;

        void save(const char* filename);

        void reset();

        void remove(const char* key);

        bool is_new_file() const { return _j.empty(); }

        static config_file& instance();

        // Retrieves a value from a nested JSON structure using dot notation
        template< typename T >
        T get_nested( const std::string & path, const T & def ) const
        {
            std::istringstream ss( path );
            std::string token;
            const rsutils::json * current = &_j;

            while( std::getline( ss, token, '.' ) )
            {
                if( ! current->contains( token ) )
                {
                    return def;
                }
                current = &( *current )[token];  // getting to the next level in the JSON structure
            }

            T ret_value;
            if (!current->get_ex<T>(ret_value))
            {
                return def;
            }
            return ret_value;
        }

        // Sets a value in a nested JSON structure using dot notation
        template< typename T >
        void set_nested( const std::string & path, const T & val )
        {
            std::vector< std::string > keys;
            std::istringstream ss( path );
            std::string token;

            while( std::getline( ss, token, '.' ) )
            {
                keys.push_back( token );
            }
            rsutils::json * current = &_j;

            for( size_t i = 0; i < keys.size() - 1; ++i )
            {
                if( ! current->contains( keys[i] ) )
                {
                    ( *current )[keys[i]] = rsutils::json::object();
                }
                current = &( *current )[keys[i]];
            }

            ( *current )[keys.back()] = val;
            save();
        }

        // Sets a default value to the config and default map
        template< typename T >
        void set_nested_default( const std::string & path, const T & default_val )
        {
            std::vector< std::string > keys;
            std::istringstream ss( path );
            std::string token;

            while( std::getline( ss, token, '.' ) )
            {
                keys.push_back( token );
            }

            rsutils::json * current = &_j;
            bool exists = true;

            for( const auto & key : keys )
            {
                if( ! current->contains( key ) )
                {
                    exists = false;
                    break;
                }
                current = &( *current )[key];
            }

            // If it doesn't exist, set the default value in JSON 
            if( ! exists )
            {
                current = &_j;
                for( size_t i = 0; i < keys.size() - 1; ++i )
                {
                    if( ! current->contains( keys[i] ) )
                    {
                        ( *current )[keys[i]] = rsutils::json::object();
                    }
                    current = &( *current )[keys[i]];
                }
                ( *current )[keys.back()] = default_val;
                save();
            }
        }

    private:
        std::string get_default(const char* key, const char* def) const;

        void save();

        std::map<std::string, std::string> _defaults;
        std::string _filename;
        rsutils::json _j;
    };
}