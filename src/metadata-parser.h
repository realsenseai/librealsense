// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

// Metadata attributes provided by RS4xx Depth Cameras

#include "frame.h"
#include "metadata.h"
#include <src/firmware-version.h>
#include <cmath>
#include <rsutils/number/crc32.h>
#include <rsutils/string/from.h>
#include <rsutils/easylogging/easyloggingpp.h>


namespace librealsense
{
    /**\brief Base class that establishes the interface for retrieving metadata attributes*/
    class md_attribute_parser_base
    {
    public:
        virtual bool find( const frame & frm, rs2_metadata_type * p_value ) const = 0;

        virtual ~md_attribute_parser_base() = default;
    };

    /**\brief Tries two parsers in order, returning the first that finds a valid attribute. Used when one
     *  sensor delivers frames with different metadata layouts (e.g. depth/IR and color on the same sensor):
     *  each sub-parser self-validates by md_type, so only the one matching the frame's layout succeeds. */
    class md_layout_selecting_parser : public md_attribute_parser_base
    {
        std::shared_ptr< md_attribute_parser_base > _first, _second;
    public:
        md_layout_selecting_parser( std::shared_ptr< md_attribute_parser_base > first,
                                    std::shared_ptr< md_attribute_parser_base > second )
            : _first( std::move( first ) ), _second( std::move( second ) ) {}

        bool find( const frame & frm, rs2_metadata_type * p_value ) const override
        {
            return ( _first && _first->find( frm, p_value ) ) || ( _second && _second->find( frm, p_value ) );
        }
    };

    /**\brief metadata parser class - support metadata in format: rs2_frame_metadata_value, rs2_metadata_type */
    class md_constant_parser : public md_attribute_parser_base
    {
    public:
        md_constant_parser(rs2_frame_metadata_value type) : _type(type) {}

        bool find( const frame & frm, rs2_metadata_type * p_value ) const override
        {
            const uint8_t * pos = frm.additional_data.metadata_blob.data();
            while( pos <= frm.additional_data.metadata_blob.data() + frm.additional_data.metadata_blob.size() )
            {
                const rs2_frame_metadata_value * type = reinterpret_cast<const rs2_frame_metadata_value *>(pos);
                pos += sizeof( rs2_frame_metadata_value );
                if( _type == *type )
                {
                    const rs2_metadata_type * value = reinterpret_cast<const rs2_metadata_type *>(pos);
                    if( p_value )
                        memcpy( (void *) p_value, (const void *) value, sizeof( *value ) );
                    return true;
                }
                pos += sizeof( rs2_metadata_type );
            }
            return false;
        }

        static std::shared_ptr<metadata_parser_map> create_metadata_parser_map()
        {
            auto md_parser_map = std::make_shared<metadata_parser_map>();
            for (int i = 0; i < static_cast<int>(rs2_frame_metadata_value::RS2_FRAME_METADATA_COUNT); ++i)
            {
                auto frame_md_type = static_cast<rs2_frame_metadata_value>(i);
                md_parser_map->insert(std::make_pair(frame_md_type, std::make_shared<md_constant_parser>(frame_md_type)));
            }
            return md_parser_map;
        }

    private:
        rs2_frame_metadata_value _type;
    };


    /**\brief metadata parser class - support metadata as array of (bool,rs2_metadata_type) */
    class md_array_parser : public md_attribute_parser_base
    {
        rs2_frame_metadata_value _key;
        std::shared_ptr< md_attribute_parser_base > _fallback;

    public:
        md_array_parser( rs2_frame_metadata_value key, std::shared_ptr< md_attribute_parser_base > fallback = {} )
            : _key( key )
            , _fallback( std::move( fallback ) )
        {
        }

        bool find( const frame & frm, rs2_metadata_type * p_value ) const override
        {
            auto pmd = reinterpret_cast< metadata_array_value const * >( frm.additional_data.metadata_blob.data() );
            metadata_array_value const & value = pmd[_key];
            if( ! value.is_valid )
                return _fallback ? _fallback->find( frm, p_value ) : false;
            if( p_value )
                *p_value = value.value;
            return true;
        }
    };


    /**\brief Post-processing adjustment of the metadata attribute
     *  e.g change auto_exposure enum to boolean, change units from nano->ms,etc'*/
    typedef std::function<rs2_metadata_type(const rs2_metadata_type& param)> attrib_modifyer;

    /**\brief The metadata parser class directly access the metadata attribute in the blob received from HW.
    *   Given the metadata-nested construct, and the c++ lack of pointers
    *   to the inner struct, we pre-calculate and store the attribute offset internally
    *   http://stackoverflow.com/questions/1929887/is-pointer-to-inner-struct-member-forbidden
    */

    /**\brief always enabled param parser class*/
    template<class S, class Attribute>
    class md_always_enabled_param_parser : public md_attribute_parser_base
    {
    public:
        md_always_enabled_param_parser(Attribute S::* attribute_name, unsigned long long offset, attrib_modifyer mod)
            : _md_attribute(attribute_name)
            , _offset(offset)
            , _modifyer(mod) {};

        bool find(const frame& frm, rs2_metadata_type* p_value) const override
        {
            auto s = reinterpret_cast<const S*>(((const uint8_t*)frm.additional_data.metadata_blob.data())
                + _offset);

            if (!is_attribute_valid(s))
                return false;

            if (p_value)
            {
                auto attrib = static_cast<rs2_metadata_type>((*s).*_md_attribute);
                if (_modifyer)
                    attrib = _modifyer(attrib);
                *p_value = attrib;
            }
            return true;
        }

    protected:
        virtual bool is_attribute_valid(const S* s) const
        {
            // verify that the struct is of the correct type
            // Check that the header id and the struct size corresponds.
            // Note that this heurisic is not deterministic and may validate false frames! TODO - requires review
            md_type expected_type = md_type_trait< S >::type;

            if (s->header.md_type_id != expected_type)
            {
                // Some mismatched metadata types are expected, e.g dual-RGB color metadata on a depth sensor.
                return false;
            }

            if (s->header.md_size < sizeof(*s))
            {
                LOG_DEBUG("Metadata size mismatch - actual: " << (uint32_t)s->header.md_size << ", expected: " << sizeof(*s) << std::dec << " ("
                    << md_type_desc.at(expected_type) << ")");
                return false;
            }
            return true;
        }

    private:
        md_always_enabled_param_parser() = delete;
        md_always_enabled_param_parser(const md_always_enabled_param_parser&) = delete;

        Attribute S::*      _md_attribute;  // Pointer to the attribute within uvc header that provides the relevant data
        unsigned long long  _offset;        // Inner struct offset with regard to the most outer one
        attrib_modifyer     _modifyer;      // Post-processing on received attribute
    };

    /**\brief A utility function to create UVC metadata header parser*/
    template<class S, class Attribute>
    std::shared_ptr<md_attribute_parser_base> make_always_enabled_param_parser(Attribute S::* attribute, unsigned long long offset, attrib_modifyer mod = nullptr)
    {
        std::shared_ptr<md_always_enabled_param_parser<S, Attribute>> parser(new md_always_enabled_param_parser<S, Attribute>(attribute, offset, mod));
        return parser;
    }

    template<class S, class Attribute, typename Flag>
    class md_attribute_parser : public md_always_enabled_param_parser<S, Attribute>
    {
    public:
        md_attribute_parser( Attribute S::*attribute_name, Flag flag, unsigned long long offset, attrib_modifyer mod )
            : md_always_enabled_param_parser<S, Attribute>(attribute_name, offset, mod)
            , _md_flag( flag )
        {
        }

    protected:
        virtual bool is_attribute_valid( const S * s ) const
        {
            if (!md_always_enabled_param_parser<S, Attribute>::is_attribute_valid(s))
                return false;

            // Check if the attribute's flag is set
            auto attribute_enabled = (0 != (s->flags & static_cast<uint32_t>(_md_flag)));
            return attribute_enabled;
        }

        Flag  _md_flag;       // Bit that indicates whether the particular attribute is active

    private:
        md_attribute_parser() = delete;
        md_attribute_parser(const md_attribute_parser&) = delete;
    };

    /**\brief A helper function to create a specialized attribute parser.
     *  Return it as a pointer to a base-class*/
    template<class S, class Attribute, typename Flag>
    std::shared_ptr<md_attribute_parser_base> make_attribute_parser(Attribute S::* attribute, Flag flag, unsigned long long offset, attrib_modifyer mod = nullptr)
    {
        std::shared_ptr<md_attribute_parser<S, Attribute, Flag>> parser(new md_attribute_parser<S, Attribute, Flag>(attribute, flag, offset, mod));
        return parser;
    }

    /**\brief MIPI color metadata parser class that handles firmware version-dependent type validation*/
    template<class S, class Attribute, typename Flag>
    class md_attribute_parser_mipi_color : public md_attribute_parser<S, Attribute, Flag>
    {
    public:
        md_attribute_parser_mipi_color(Attribute S::* attribute_name, Flag flag, unsigned long long offset,
            librealsense::firmware_version fw_version, attrib_modifyer mod = nullptr)
            : md_attribute_parser<S, Attribute, Flag>(attribute_name, flag, offset, mod)
            , _fw_version(fw_version)
        {
        }

        static const librealsense::firmware_version _min_fw_for_type;
        static const md_type _old_type;

    protected:
        virtual bool is_attribute_valid(const S* s) const override
        {
            // Firmware version-dependent type validation
            md_type expected_type;
            if (_fw_version < _min_fw_for_type)
            {
                expected_type = _old_type;
            }
            else
            {
                expected_type = md_type_trait<S>::type;
            }

            // checking type and size, with updated expected type
            if (s->header.md_type_id != expected_type)
            {
                std::string type
                    = (md_type_desc.count(s->header.md_type_id) > 0)
                    ? md_type_desc.at(s->header.md_type_id)
                    : (rsutils::string::from()
                        << "0x" << std::hex << static_cast<uint32_t>(s->header.md_type_id) << std::dec);
                LOG_DEBUG("Metadata type mismatch - actual: " << type << ", expected: 0x" << std::hex
                    << (uint32_t)expected_type << std::dec << " ("
                    << md_type_desc.at(expected_type) << ")");
                return false;
            }

            if (s->header.md_size < sizeof(*s))
            {
                LOG_DEBUG("Metadata size mismatch - actual: " << (uint32_t)s->header.md_size << ", expected: " << sizeof(*s) << std::dec << " ("
                    << md_type_desc.at(expected_type) << ")");
                return false;
            }

            // Check if the attribute's flag is set
            auto attribute_enabled = (0 != (s->flags & static_cast<uint32_t>(this->_md_flag)));
            if (!attribute_enabled)
                return false;

            return true;
        }

    private:
        md_attribute_parser_mipi_color() = delete;
        md_attribute_parser_mipi_color(const md_attribute_parser_mipi_color&) = delete;

        librealsense::firmware_version _fw_version;
    };

    template<class S, class Attribute, typename Flag>
    const librealsense::firmware_version md_attribute_parser_mipi_color<S, Attribute, Flag>::_min_fw_for_type{ "5.17.0.12" };

    template<class S, class Attribute, typename Flag>
    const md_type md_attribute_parser_mipi_color<S, Attribute, Flag>::_old_type = md_type::META_DATA_INTEL_RGB_CONTROL_ID;

    /**\brief A helper function to create a specialized MIPI color attribute parser*/
    template<class S, class Attribute, typename Flag>
    std::shared_ptr<md_attribute_parser_base> make_mipi_color_attribute_parser(Attribute S::* attribute, Flag flag,
        unsigned long long offset,
        librealsense::firmware_version fw_version,
        attrib_modifyer mod = nullptr)
    {
        std::shared_ptr<md_attribute_parser_mipi_color<S, Attribute, Flag>> parser(
            new md_attribute_parser_mipi_color<S, Attribute, Flag>(attribute, flag, offset, fw_version, mod));
        return parser;
    }

    /**\brief A UVC-Header parser class*/
    template<class St, class Attribute>
    class md_uvc_header_parser : public md_attribute_parser_base
    {
    public:
        md_uvc_header_parser(Attribute St::* attribute_name, attrib_modifyer mod) :
            _md_attribute(attribute_name), _modifyer(mod){};

        bool find( const frame & frm, rs2_metadata_type * p_value ) const override
        {
            if( frm.additional_data.metadata_size < platform::uvc_header_size )
                return false;

            if( p_value )
            {
                auto attrib = static_cast< rs2_metadata_type >(
                    ( *reinterpret_cast< const St * >( (const uint8_t *)frm.additional_data.metadata_blob.data() ) )
                    .*_md_attribute );
                if( _modifyer )
                    attrib = _modifyer( attrib );
                *p_value = attrib;
            }
            return true;
        }

    private:
        md_uvc_header_parser() = delete;
        md_uvc_header_parser(const md_uvc_header_parser&) = delete;

        Attribute St::*     _md_attribute;  // Pointer to the attribute within uvc header that provides the relevant data
        attrib_modifyer     _modifyer;      // Post-processing on received attribute
    };

    /**\brief A utility function to create UVC metadata header parser*/
    template<class St, class Attribute>
    std::shared_ptr<md_attribute_parser_base> make_uvc_header_parser(Attribute St::* attribute, attrib_modifyer mod = nullptr)
    {
        std::shared_ptr<md_uvc_header_parser<St, Attribute>> parser(new md_uvc_header_parser<St, Attribute>(attribute, mod));
        return parser;
    }

    /**\brief A HID-Header metadata parser class*/
    template<class St, class Attribute>
    class md_hid_header_parser : public md_attribute_parser_base
    {
    public:
        md_hid_header_parser(Attribute St::* attribute_name, attrib_modifyer mod) :
            _md_attribute(attribute_name), _modifyer(mod) {};

        bool find( const frame & frm, rs2_metadata_type * p_value ) const override
        {
            if( frm.additional_data.metadata_size < hid_header_size )
                return false;

            if( p_value )
            {
                auto attrib = static_cast< rs2_metadata_type >(
                    ( *reinterpret_cast< const St * >( (const uint8_t *)frm.additional_data.metadata_blob.data() ) )
                    .*_md_attribute );
                attrib &= 0x00000000ffffffff;
                if( _modifyer )
                    attrib = _modifyer( attrib );
                *p_value = attrib;
            }
            return true;
        }

    private:
        md_hid_header_parser() = delete;
        md_hid_header_parser(const md_hid_header_parser&) = delete;

        Attribute St::*     _md_attribute;  // Pointer to the attribute within uvc header that provides the relevant data
        attrib_modifyer     _modifyer;      // Post-processing on received attribute
    };

    /**\brief A utility function to create HID metadata header parser*/
    template<class St, class Attribute>
    std::shared_ptr<md_attribute_parser_base> make_hid_header_parser(Attribute St::* attribute, attrib_modifyer mod = nullptr)
    {
        std::shared_ptr<md_hid_header_parser<St, Attribute>> parser(new md_hid_header_parser<St, Attribute>(attribute, mod));
        return parser;
    }

    /**\brief provide attributes generated and stored internally by the library*/
    template<class St, class Attribute>
    class md_additional_parser : public md_attribute_parser_base
    {
    public:
        md_additional_parser(Attribute St::* attribute_name) :
            _md_attribute(attribute_name) {}

        bool find( const frame & frm, rs2_metadata_type * p_value ) const override
        {
            if( p_value )
                *p_value = static_cast< rs2_metadata_type >( frm.additional_data.*_md_attribute );
            return true;
        }

    private:
        md_additional_parser() = delete;
        md_additional_parser(const md_additional_parser&) = delete;

        Attribute St::*          _md_attribute;      // Pointer to the attribute within uvc header that provides the relevant data
    };

    /**\brief A utility function to create additional_data parser*/
    template<class St, class Attribute>
    std::shared_ptr<md_attribute_parser_base> make_additional_data_parser(Attribute St::* attribute)
    {
        std::shared_ptr<md_additional_parser<St, Attribute>> parser(new md_additional_parser<St, Attribute>(attribute));
        return parser;
    }

    /**
     * provide attributes generated and stored internally by the library, unless set to a specific (default?)
     * value
     */
    template< class St, class Attribute >
    class md_additional_parser_unless : public md_additional_parser< St, Attribute >
    {
        using super = md_additional_parser< St, Attribute >;

        Attribute _unless;  // value for which we return "not found"

    public:
        md_additional_parser_unless( Attribute St::*attribute_name, Attribute unless )
            : super( attribute_name )
            , _unless( unless )
        {
        }

        bool find( const frame & frm, rs2_metadata_type * p_value ) const override
        {
            rs2_metadata_type value;
            if( ! super::find( frm, &value ) )
                return false;
            if( value == _unless )
                return false;
            if( p_value )
                *p_value = value;
            return true;
        }
    };

    /**\brief A utility function to create additional_data parser*/
    template< class St, class Attribute >
    std::shared_ptr< md_attribute_parser_base > make_additional_data_parser_unless( Attribute St::*attribute,
                                                                                    Attribute unless )
    {
        return std::make_shared< md_additional_parser_unless< St, Attribute > >( attribute, unless );
    }

    /**\brief Optical timestamp for RS4xx devices is calculated internally*/
    class md_rs400_sensor_timestamp : public md_attribute_parser_base
    {
        std::shared_ptr<md_attribute_parser_base> _sensor_ts_parser = nullptr;
        std::shared_ptr<md_attribute_parser_base> _frame_ts_parser = nullptr;

    public:
        explicit md_rs400_sensor_timestamp(std::shared_ptr<md_attribute_parser_base> sensor_ts_parser,
            std::shared_ptr<md_attribute_parser_base> frame_ts_parser) :
            _sensor_ts_parser(sensor_ts_parser), _frame_ts_parser(frame_ts_parser) {};

        virtual ~md_rs400_sensor_timestamp() { _sensor_ts_parser = nullptr; _frame_ts_parser = nullptr; };

        // The sensor's timestamp is defined as the middle of exposure time. Sensor_ts= Frame_ts - (Actual_Exposure/2)
        // For RS4xx the metadata payload holds only the (Actual_Exposure/2) offset, and the actual value needs to be calculated
        bool find( const librealsense::frame & frm, rs2_metadata_type * p_value ) const override
        {
            rs2_metadata_type frame_value, sensor_value;
            if( ! _sensor_ts_parser->find( frm, &sensor_value ) || ! _frame_ts_parser->find( frm, &frame_value ) )
                return false;
            if( p_value )
                *p_value = frame_value - sensor_value;
            return true;
        }
    };

    template<class S, class Attribute, typename Flag>
    class md_attribute_parser_with_crc : public md_attribute_parser<S, Attribute, Flag>
    {
    public: 
        md_attribute_parser_with_crc(Attribute S::* attribute_name, Flag flag, unsigned long long offset, attrib_modifyer mod, unsigned long long start_offset, unsigned long long crc_offset)
            : md_attribute_parser<S, Attribute, Flag>(attribute_name, flag, offset, mod), _start_offset(start_offset), _crc_offset(crc_offset) {}

    protected:
        unsigned long long _start_offset; // offset of the first field that is relevant for CRC calculation
        unsigned long long _crc_offset; // offset of the crc inside the S class
        bool is_attribute_valid(const S* s) const override
        {
            if (!md_attribute_parser<S, Attribute, Flag>::is_attribute_valid(s))
                return false;

            if (!is_crc_valid(s))
            {
                LOG_DEBUG("Metadata CRC mismatch");
                return false;
            }

            return true;
        }

    private:
        md_attribute_parser_with_crc() = delete;
        md_attribute_parser_with_crc(const md_attribute_parser_with_crc&) = delete;

        bool is_crc_valid(const S* md_info) const
        {
            // calculation of CRC32 for the MD payload, starting from "start_offset" till the end of struct, not including the CRC itself.
            // assuming the CRC field is the last field of the struct
            Attribute crc = *(Attribute*)(reinterpret_cast<const uint8_t*>(md_info) + _crc_offset);   //Attribute = CRC's type (ex. uint32_t)
            auto computed_crc32 = rsutils::number::calc_crc32(reinterpret_cast<const uint8_t*>(md_info) + _start_offset,
                sizeof(S) - _start_offset - sizeof(crc));
            return (crc == computed_crc32);
        }
    };

    /**\brief A helper function to create a specialized attribute parser.
     *  **Note that this class is assuming that the CRC is the last variable in the struct**
     *  Return it as a pointer to a base-class*/
    template<class S, class Attribute, typename Flag>
    std::shared_ptr<md_attribute_parser_base> make_attribute_parser_with_crc(Attribute S::* attribute, Flag flag, unsigned long long offset, unsigned long long start_offset, unsigned long long crc_offset, attrib_modifyer mod = nullptr)
    {
        std::shared_ptr<md_attribute_parser<S, Attribute, Flag>> parser(new md_attribute_parser_with_crc<S, Attribute, Flag>(attribute, flag, offset, mod, start_offset, crc_offset));
        return parser;
    }



    class ds_md_attribute_actual_fps : public md_attribute_parser_base
    {
    public:
        bool find( const librealsense::frame & frm, rs2_metadata_type * p_value ) const override
        {
            auto fps = rs2_metadata_type( frm.calc_actual_fps() * 1000 );
            if( fps <= 0. )
                // In case of frame counter reset fallback to fps from the stream configuration
                return false;
            if( p_value )
                *p_value = fps;
            return true;
        }
    };

    /**\brief A helper function to create a specialized parser for RS4xx sensor timestamp*/
    inline std::shared_ptr<md_attribute_parser_base> make_rs400_sensor_ts_parser(std::shared_ptr<md_attribute_parser_base> frame_ts_parser,
        std::shared_ptr<md_attribute_parser_base> sensor_ts_parser)
    {
        std::shared_ptr<md_rs400_sensor_timestamp> parser(new md_rs400_sensor_timestamp(sensor_ts_parser, frame_ts_parser));
        return parser;
    }
}
