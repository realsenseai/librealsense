// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Reference sample - "how do I use the composite-option API", against REAL connected devices.
// Walks every device -> depth sensor -> embedded filter -> each composite id found gets a full
// typed Get/Set/Get/Range/metadata sequence, in its own try/catch so one broken control doesn't abort the rest.

#include <librealsense2/rs.hpp>
#include <librealsense2/h/rs_decimation_filter_dpp.h>
#include <librealsense2/h/rs_temporal_filter_dpp.h>
#include <librealsense2/h/rs_hdrd_control.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    const char * composite_option_name( rs2_composite_option_id id )
    {
        switch( id )
        {
        case RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP:  return "DECIMATION_FILTER_DPP";
        case RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP:    return "TEMPORAL_FILTER_DPP";
        case RS2_COMPOSITE_OPTION_HDRD_CONTROL:           return "HDRD_CONTROL";
        default:                                           return "UNKNOWN";
        }
    }

    // Prints a composite option's raw payload as a byte array. Short buffers print on one line;
    // long ones wrap into a rectangular grid: pick the fewest rows keeping each row's width
    // <=64, then divide the buffer evenly, so the last row isn't a ragged leftover.
    void print_bytes( const char * label, const std::vector< uint8_t > & raw )
    {
        constexpr size_t wrap_threshold = 60;
        constexpr size_t max_bytes_per_line = 64;

        std::cout << label << " bytes (" << raw.size() << "):" << std::hex << std::setfill( '0' );

        if( raw.size() <= wrap_threshold )
        {
            for( size_t i = 0; i < raw.size(); ++i )
                std::cout << ' ' << std::setw( 2 ) << (int)raw[i];
        }
        else
        {
            size_t num_lines = ( raw.size() + max_bytes_per_line - 1 ) / max_bytes_per_line;
            size_t bytes_per_line = ( raw.size() + num_lines - 1 ) / num_lines;
            for( size_t i = 0; i < raw.size(); ++i )
            {
                std::cout << ( i % bytes_per_line == 0 ? "\n        " : " " ) << std::setw( 2 ) << (int)raw[i];
            }
        }

        // std::setfill (unlike std::setw) is sticky - restored here or every later std::setw(...)
        // call on std::cout would silently inherit '0' instead of the default space.
        std::cout << std::dec << std::setfill( ' ' ) << '\n';
    }

    // ---- Generic "print any struct's fields" machinery -------------------------------------
    // A pointer-to-member (Attribute S::*) is captured once, at construction, behind a common
    // NON-TEMPLATED base interface, so a list of these can be iterated without ever knowing S or
    // Attribute. print_struct() has zero field names hardcoded; only the per-type table does.

    // uint8_t/int8_t stream as characters via the default operator<< - not useful for a byte-
    // sized numeric field like dpp_header::version - so route those through an int cast.
    // Every other type (int32_t, float, uint16_t, ...) uses the generic overload as-is.
    inline void stream_value( std::ostream & os, uint8_t v ) { os << (unsigned int)v; }
    inline void stream_value( std::ostream & os, int8_t v ) { os << (int)v; }
    template< class T >
    void stream_value( std::ostream & os, const T & v ) { os << v; }

    class field_printer_base
    {
    public:
        virtual const char * name() const = 0;
        // name_width is the widest field name in this table, computed once up front - every row
        // pads its name to that width (spaces) so "=" lands in the same column on every line.
        virtual void print( std::ostream & os, const void * struct_ptr, size_t name_width ) const = 0;
        // Just this field's value, no name/no newline - used by print_range() to lay several
        // struct instances (min/max/def/step) out on one row instead of one block per struct.
        virtual void print_value( std::ostream & os, const void * struct_ptr ) const = 0;
        virtual ~field_printer_base() = default;
    };

    // Stores a std::function accessor rather than a raw pointer-to-member so the same class
    // covers both a direct field (S::*) and one nested a level down through a composed header
    // (S::*header, Header::*field) - see the two make_field_printer() overloads below.
    template< class S, class Attribute >
    class field_printer : public field_printer_base
    {
    public:
        field_printer( const char * name, std::function< const Attribute &( const S & ) > accessor )
            : _name( name ), _accessor( std::move( accessor ) ) {}

        const char * name() const override { return _name; }

        void print( std::ostream & os, const void * struct_ptr, size_t name_width ) const override
        {
            auto & s = *reinterpret_cast< const S * >( struct_ptr );
            os << "        " << std::left << std::setw( (int)name_width ) << _name << std::right << " = ";
            stream_value( os, _accessor( s ) );
            os << '\n';
        }

        void print_value( std::ostream & os, const void * struct_ptr ) const override
        {
            auto & s = *reinterpret_cast< const S * >( struct_ptr );
            stream_value( os, _accessor( s ) );
        }

    private:
        const char * _name;
        std::function< const Attribute &( const S & ) > _accessor;
    };

    template< class S, class Attribute >
    std::shared_ptr< field_printer_base > make_field_printer( const char * name, Attribute S::* field )
    {
        return std::make_shared< field_printer< S, Attribute > >( name,
            [field]( const S & s ) -> const Attribute & { return s.*field; } );
    }

    // Overload for a field nested one level down through a composed sub-struct, e.g.
    // rs2_hdrd_control::header (a dpp_header) then dpp_header::version.
    template< class S, class H, class Attribute >
    std::shared_ptr< field_printer_base > make_field_printer( const char * name, H S::* header_field, Attribute H::* field )
    {
        return std::make_shared< field_printer< S, Attribute > >( name,
            [header_field, field]( const S & s ) -> const Attribute & { return ( s.*header_field ).*field; } );
    }

    // The one fully generic entry point - takes whichever field table matches S, no per-struct
    // code here at all.
    template< class S >
    void print_struct( std::ostream & os, const std::vector< std::shared_ptr< field_printer_base > > & fields,
                        const S & value )
    {
        size_t name_width = 0;
        for( auto & f : fields )
            name_width = std::max( name_width, std::string( f->name() ).size() );

        for( auto & f : fields )
            f->print( os, &value, name_width );
    }

    // One header line naming the columns, then one row per field: name = [ min, max, default, step ].
    // Same field table as print_struct() - no per-struct code here either.
    template< class S >
    void print_range( std::ostream & os, const std::vector< std::shared_ptr< field_printer_base > > & fields,
                       const S & min_v, const S & max_v, const S & def_v, const S & step_v )
    {
        size_t name_width = 0;
        for( auto & f : fields )
            name_width = std::max( name_width, std::string( f->name() ).size() );

        os << "        " << std::left << std::setw( (int)name_width ) << "" << std::right
           << " [ min, max, default, step ]\n";
        for( auto & f : fields )
        {
            os << "        " << std::left << std::setw( (int)name_width ) << f->name() << std::right << " = [ ";
            f->print_value( os, &min_v );
            os << ", ";
            f->print_value( os, &max_v );
            os << ", ";
            f->print_value( os, &def_v );
            os << ", ";
            f->print_value( os, &step_v );
            os << " ]\n";
        }
    }

    const std::vector< std::shared_ptr< field_printer_base > > & decimation_filter_dpp_fields()
    {
        static const std::vector< std::shared_ptr< field_printer_base > > fields = {
            make_field_printer( "version", &rs2_decimation_filter_dpp_config::header, &dpp_header::version ),
            make_field_printer( "flags", &rs2_decimation_filter_dpp_config::header, &dpp_header::flags ),
            make_field_printer( "ctl_id", &rs2_decimation_filter_dpp_config::header, &dpp_header::ctl_id ),
            make_field_printer( "param_count", &rs2_decimation_filter_dpp_config::header, &dpp_header::param_count ),
            make_field_printer( "param_type", &rs2_decimation_filter_dpp_config::header, &dpp_header::param_type ),
            make_field_printer( "enabled", &rs2_decimation_filter_dpp_config::enabled ),
            make_field_printer( "magnitude", &rs2_decimation_filter_dpp_config::magnitude ),
        };
        return fields;
    }

    const std::vector< std::shared_ptr< field_printer_base > > & temporal_filter_dpp_fields()
    {
        static const std::vector< std::shared_ptr< field_printer_base > > fields = {
            make_field_printer( "version", &rs2_temporal_filter_dpp_config::header, &dpp_header::version ),
            make_field_printer( "flags", &rs2_temporal_filter_dpp_config::header, &dpp_header::flags ),
            make_field_printer( "ctl_id", &rs2_temporal_filter_dpp_config::header, &dpp_header::ctl_id ),
            make_field_printer( "param_count", &rs2_temporal_filter_dpp_config::header, &dpp_header::param_count ),
            make_field_printer( "param_type", &rs2_temporal_filter_dpp_config::header, &dpp_header::param_type ),
            make_field_printer( "enabled", &rs2_temporal_filter_dpp_config::enabled ),
            make_field_printer( "smooth_alpha", &rs2_temporal_filter_dpp_config::smooth_alpha ),
            make_field_printer( "smooth_delta", &rs2_temporal_filter_dpp_config::smooth_delta ),
            make_field_printer( "persistency_index", &rs2_temporal_filter_dpp_config::persistency_index ),
        };
        return fields;
    }

    const std::vector< std::shared_ptr< field_printer_base > > & hdrd_fields()
    {
        static const std::vector< std::shared_ptr< field_printer_base > > fields = {
            make_field_printer( "version", &rs2_hdrd_control::header, &dpp_header::version ),
            make_field_printer( "flags", &rs2_hdrd_control::header, &dpp_header::flags ),
            make_field_printer( "ctl_id", &rs2_hdrd_control::header, &dpp_header::ctl_id ),
            make_field_printer( "param_count", &rs2_hdrd_control::header, &dpp_header::param_count ),
            make_field_printer( "param_type", &rs2_hdrd_control::header, &dpp_header::param_type ),
            make_field_printer( "enable", &rs2_hdrd_control::enable ),
            make_field_printer( "filter_type", &rs2_hdrd_control::filter_type ),
            make_field_printer( "downscale_ratio", &rs2_hdrd_control::downscale_ratio ),
            make_field_printer( "shift_mode", &rs2_hdrd_control::shift_mode ),
            make_field_printer( "shift_pixels", &rs2_hdrd_control::shift_pixels ),
            make_field_printer( "threshold_mode", &rs2_hdrd_control::threshold_mode ),
            make_field_printer( "threshold_mm", &rs2_hdrd_control::threshold_mm ),
        };
        return fields;
    }
    // ---- end generic struct-printing machinery ---------------------------------------------

    // Full read-modify-write + range + metadata sequence for RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP.
    // Sends the FW-reported default rather than a hardcoded literal - the documented range was a
    // fixed [2,2] at design time but real firmware may report a wider one (confirmed: [2,4]).
    void exercise_decimation_filter_dpp( rs2::options & opts, rs2_composite_option_id id )
    {
        print_bytes( "Get (before)", opts.get_composite_option( id ) );
        auto current = opts.get_composite_option_as< rs2_decimation_filter_dpp_config >( id );
        std::cout << "      Get (before):\n";
        print_struct( std::cout, decimation_filter_dpp_fields(), current );

        auto range = opts.get_composite_option_range_as< rs2_decimation_filter_dpp_range >( id );

        rs2_decimation_filter_dpp_config cfg_to_send = current;
        cfg_to_send.enabled = 1;
        cfg_to_send.magnitude = range.def.magnitude;
        opts.set_composite_option_from( id, cfg_to_send );
        std::cout << "      Set (read-modify-write): enabled=1 magnitude=" << cfg_to_send.magnitude << '\n';

        print_bytes( "Get (after)", opts.get_composite_option( id ) );
        auto cfg = opts.get_composite_option_as< rs2_decimation_filter_dpp_config >( id );
        std::cout << "      Get (after):\n";
        print_struct( std::cout, decimation_filter_dpp_fields(), cfg );
        bool matches = cfg.enabled == cfg_to_send.enabled && cfg.magnitude == cfg_to_send.magnitude;
        std::cout << "      All fields (" << ( matches ? "match what was sent" : "differ - FW may quantize/clamp on write" )
                  << ")\n";

        print_bytes( "Get Range", opts.get_composite_option_range( id ) );
        std::cout << "      Range:\n";
        print_range( std::cout, decimation_filter_dpp_fields(), range.min, range.max, range.def, range.step );

        std::cout << "      Read-only: " << ( opts.is_composite_option_read_only( id ) ? "true" : "false" ) << '\n';
        std::cout << "      Description: \"" << opts.get_composite_option_description( id ) << "\"\n";

        // Restore the original value read at the very start - same discipline as the other
        // controls, even though this one also reverts on its own once Depth/IR starts streaming.
        opts.set_composite_option_from( id, current );
        auto restored = opts.get_composite_option_as< rs2_decimation_filter_dpp_config >( id );
        std::cout << "      Restore original value: "
                  << ( restored.enabled == current.enabled && restored.magnitude == current.magnitude
                       ? "ok" : "FAILED to restore - device may be left in the sample's last test state" )
                  << '\n';
    }

    // Full read-modify-write + range + metadata sequence for RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP.
    void exercise_temporal_filter_dpp( rs2::options & opts, rs2_composite_option_id id )
    {
        // Both forms of the read, one after another - raw untyped bytes and the typed cast, two
        // separate real GETs, shown side by side purely to illustrate both APIs.
        print_bytes( "Get (before)", opts.get_composite_option( id ) );
        auto current = opts.get_composite_option_as< rs2_temporal_filter_dpp_config >( id );
        std::cout << "      Get (before):\n";
        print_struct( std::cout, temporal_filter_dpp_fields(), current );

        rs2_temporal_filter_dpp_config cfg_to_send = current;
        cfg_to_send.enabled = 1;
        cfg_to_send.smooth_alpha = 550;  // normalized [0,1] scaled into [0,1000] - i.e. 0.55
        cfg_to_send.smooth_delta = 35;
        cfg_to_send.persistency_index = 5;
        opts.set_composite_option_from( id, cfg_to_send );
        std::cout << "      Set (read-modify-write): enabled=1 smooth_alpha=550 smooth_delta=35 persistency_index=5\n";

        print_bytes( "Get (after)", opts.get_composite_option( id ) );
        auto cfg = opts.get_composite_option_as< rs2_temporal_filter_dpp_config >( id );
        std::cout << "      Get (after):\n";
        print_struct( std::cout, temporal_filter_dpp_fields(), cfg );
        // Real firmware may quantize/clamp on write - unlike an in-memory mock, an exact
        // mismatch here isn't necessarily a bug, so this is reported, not asserted/thrown on.
        bool matches = cfg.enabled == cfg_to_send.enabled && cfg.smooth_alpha == cfg_to_send.smooth_alpha
            && cfg.smooth_delta == cfg_to_send.smooth_delta && cfg.persistency_index == cfg_to_send.persistency_index;
        std::cout << "      All fields (" << ( matches ? "match what was sent" : "differ - FW may quantize/clamp on write" )
                  << ")\n";

        print_bytes( "Get Range", opts.get_composite_option_range( id ) );
        auto range = opts.get_composite_option_range_as< rs2_temporal_filter_dpp_range >( id );
        std::cout << "      Range:\n";
        print_range( std::cout, temporal_filter_dpp_fields(), range.min, range.max, range.def, range.step );

        std::cout << "      Read-only: " << ( opts.is_composite_option_read_only( id ) ? "true" : "false" ) << '\n';
        std::cout << "      Description: \"" << opts.get_composite_option_description( id ) << "\"\n";

        // Restore the original value read at the very start - this sample exercises real,
        // consequential state changes, so leaving no lasting effect on the device matters more
        // here than the ceremony of one more Set/Get pair.
        opts.set_composite_option_from( id, current );
        auto restored = opts.get_composite_option_as< rs2_temporal_filter_dpp_config >( id );
        std::cout << "      Restore original value: "
                  << ( restored.enabled == current.enabled && restored.smooth_alpha == current.smooth_alpha
                           && restored.smooth_delta == current.smooth_delta && restored.persistency_index == current.persistency_index
                       ? "ok" : "FAILED to restore - device may be left in the sample's last test state" )
                  << '\n';
    }

    // Prints one verdict line for a just-completed Set - shared by every step below.
    void verify_hdrd_fields_written( const char * step_label, const rs2_hdrd_control & written, bool matches )
    {
        std::cout << "Set (" << step_label << "): "
                  << ( matches ? "matches what was sent" : "differs - FW may quantize/clamp on write" ) << '\n';
        print_struct( std::cout, hdrd_fields(), written );
    }

    // Full read-modify-write + range + metadata sequence. Exercises both of the struct's
    // conditional axes (filter_type -> downscale_ratio/shift_mode+shift_pixels, threshold_mode ->
    // threshold_mm) instead of a single-field toggle, then restores the original value.
    void exercise_hdrd_control( rs2::options & opts, rs2_composite_option_id id )
    {
        // Raw bytes and the typed cast, shown side by side to illustrate both APIs.
        print_bytes( "Get Raw Data:", opts.get_composite_option( id ) );
        auto original = opts.get_composite_option_as< rs2_hdrd_control >( id );
        std::cout << "Get Structured Data:\n";
        print_struct( std::cout, hdrd_fields(), original );

        // Read-modify-write: wire header carried over untouched from what the device just
        // reported, not zero-initialized, on every Set below.
        rs2_hdrd_control cfg = original;
        cfg.enable = 1;

        // 1) Lookup Shift + Manual shift pixels - exercises filter_type together with the
        // (shift_mode, shift_pixels) pair it selects.
        cfg.filter_type = 1;   // Lookup Shift
        cfg.shift_mode = 2;    // Manual
        cfg.shift_pixels = 100;
        opts.set_composite_option_from( id, cfg );
        auto after_shift = opts.get_composite_option_as< rs2_hdrd_control >( id );
        verify_hdrd_fields_written( "filter_type=Lookup Shift, shift_mode=Manual, shift_pixels=100", after_shift,
            after_shift.filter_type == cfg.filter_type && after_shift.shift_mode == cfg.shift_mode
                && after_shift.shift_pixels == cfg.shift_pixels );

        // 2) Downscale + x4 ratio - exercises the OTHER branch filter_type selects between.
        cfg.filter_type = 0;   // Downscale
        cfg.downscale_ratio = 2;   // x4
        opts.set_composite_option_from( id, cfg );
        auto after_downscale = opts.get_composite_option_as< rs2_hdrd_control >( id );
        verify_hdrd_fields_written( "filter_type=Downscale, downscale_ratio=x4", after_downscale,
            after_downscale.filter_type == cfg.filter_type && after_downscale.downscale_ratio == cfg.downscale_ratio );

        // 3) Cycle threshold_mode through all three rungs - threshold_mm only matters for Manual,
        // exercising that same selects-a-field-pair pattern on the other axis of this struct.
        for( int mode : { 0, 1, 2 } )
        {
            cfg.threshold_mode = mode;
            cfg.threshold_mm = ( mode == 2 ) ? 300 : 0;
            opts.set_composite_option_from( id, cfg );
            auto after_threshold = opts.get_composite_option_as< rs2_hdrd_control >( id );
            std::string step_label = "threshold_mode=" + std::to_string( mode );
            verify_hdrd_fields_written( step_label.c_str(), after_threshold,
                after_threshold.threshold_mode == cfg.threshold_mode );
        }

        // Restore the original value read at the very start - this sample exercises real,
        // consequential state changes (unlike a single-field toggle), so leaving no lasting effect
        // on the device matters more here.
        opts.set_composite_option_from( id, original );
        auto restored = opts.get_composite_option_as< rs2_hdrd_control >( id );
        std::cout << "Restore original value: "
                  << ( restored.filter_type == original.filter_type && restored.downscale_ratio == original.downscale_ratio
                           && restored.shift_mode == original.shift_mode && restored.shift_pixels == original.shift_pixels
                           && restored.threshold_mode == original.threshold_mode && restored.threshold_mm == original.threshold_mm
                       ? "ok" : "FAILED to restore - device may be left in the sample's last test state" )
                  << '\n';

        // range.min/max/step/def are each a FULL rs2_hdrd_control - the same struct
        // as `original`/`cfg` above, header fields included - and those bytes ARE exactly what the
        // device's real get_xu_range() call returned.
        print_bytes( "Get Range", opts.get_composite_option_range( id ) );
        auto range = opts.get_composite_option_range_as< rs2_hdrd_control_range >( id );
        std::cout << "      Range:\n";
        print_range( std::cout, hdrd_fields(), range.min, range.max, range.def, range.step );

        std::cout << "      Read-only: " << ( opts.is_composite_option_read_only( id ) ? "true" : "false" ) << '\n';
        std::cout << "      Description: \"" << opts.get_composite_option_description( id ) << "\"\n";
    }

    // Dispatches to the right typed handler - no generic "any composite option" mechanism by
    // design, so a new id needs a case added here. Returns false if unhandled (not a failure);
    // actual device-transaction failures propagate as exceptions for the caller to catch.
    bool exercise_composite_option( rs2::options & opts, rs2_composite_option_id id )
    {
        switch( id )
        {
        case RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP:  exercise_decimation_filter_dpp( opts, id ); return true;
        case RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP:    exercise_temporal_filter_dpp( opts, id ); return true;
        case RS2_COMPOSITE_OPTION_HDRD_CONTROL:           exercise_hdrd_control( opts, id ); return true;
        default:
            std::cout << "      (no typed handler registered for this composite option id)\n";
            return false;
        }
    }
}

int main()
try
{
    rs2::context ctx;
    auto devices = ctx.query_devices();
    std::cout << "=== Composite-option API sample ===\n";
    std::cout << "Found " << devices.size() << " device(s)\n";

    int attempted = 0, succeeded = 0, skipped = 0;

    for( auto && dev : devices )
    {
        std::string dev_name = dev.supports( RS2_CAMERA_INFO_NAME ) ? dev.get_info( RS2_CAMERA_INFO_NAME ) : "Unknown device";
        std::string dev_sn = dev.supports( RS2_CAMERA_INFO_SERIAL_NUMBER ) ? dev.get_info( RS2_CAMERA_INFO_SERIAL_NUMBER ) : "N/A";
        std::cout << "\nDevice: " << dev_name << " (S/N " << dev_sn << ")\n";

        for( auto && sensor : dev.query_sensors() )
        {
            if( ! sensor.is< rs2::depth_sensor >() )
                continue;

            std::string sensor_name = sensor.supports( RS2_CAMERA_INFO_NAME ) ? sensor.get_info( RS2_CAMERA_INFO_NAME ) : "Depth Sensor";
            std::cout << "  Depth sensor: " << sensor_name << '\n';

            // Composite options live on the sensor's EMBEDDED FILTERs, not the sensor's own
            // registry - every filter must be explored, not just the first one.
            for( auto && ef : sensor.query_embedded_filters() )
            {
                std::cout << "    Embedded filter: " << rs2_embedded_filter_type_to_string(ef.get_type()) << '\n';
                auto composite_ids = ef.get_supported_composite_options();
                if( composite_ids.empty() )
                    continue;

                for( auto id : composite_ids )
                {
                    std::cout << "    - " << composite_option_name( id ) << ":\n";
                    ++attempted;
                    try
                    {
                        if(exercise_composite_option( ef, id ) )
                            ++succeeded;
                    }
                    catch( const rs2::error & e )
                    {
                        // Registered but not actually functional on this device/FW - skip it and
                        // keep going rather than treating this as a failure.
                        ++skipped;
                        std::cout << "      SKIPPED (registered but not functional on this device/FW): " << e.what() << '\n';
                    }
                    catch( const std::exception & e )
                    {
                        ++skipped;
                        std::cout << "      SKIPPED: " << e.what() << '\n';
                    }
                }
            }
        }
    }

    std::cout << "\n=== Summary: " << attempted << " composite option(s) found, " << succeeded << " walked "
                 "successfully end-to-end, " << skipped << " skipped (registered but non-functional on this "
                 "device/FW) ===\n";
    return 0;
}
catch( const rs2::error & e )
{
    std::cerr << "FAIL: librealsense error: " << e.what() << std::endl;
    return 1;
}
catch( const std::exception & e )
{
    std::cerr << "FAIL: unexpected exception: " << e.what() << std::endl;
    return 1;
}
