/* License: Apache 2.0. See LICENSE file in root directory. */
/* Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/* C99 composite-option sample - the same device sweep and full per-id exercise (Get, Set
   read-modify-write, Get again + verify, Get range, metadata) as the C++ samples, through the
   raw C API: no exceptions, no templates, no RAII, no pointer-to-member field tables. */

#include <librealsense2/rs.h>
#include <librealsense2/h/rs_decimation_filter_dpp.h>
#include <librealsense2/h/rs_temporal_filter_dpp.h>
#include <librealsense2/h/rs_hdrd_control.h>
#include "example.h"   /* shared check_error()/print_device_info() used by every Examples/C sample */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char * composite_option_name( rs2_composite_option_id id )
{
    switch( id )
    {
    case RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP:  return "DECIMATION_FILTER_DPP";
    case RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP:    return "TEMPORAL_FILTER_DPP";
    case RS2_COMPOSITE_OPTION_HDRD_CONTROL:           return "HDRD_CONTROL";
    default:                                           return "UNKNOWN";
    }
}

/* C99 equivalent of the C++ sample's print_bytes(): the untyped raw payload. Short buffers print
   on one line; long ones wrap into a rectangular grid, picking the fewest rows keeping each
   row's width in [32,64] so the last row isn't a ragged leftover. */
static void print_bytes( const char * label, const unsigned char * data, int size )
{
    const int wrap_threshold = 60;
    const int max_bytes_per_line = 64;
    int i;

    printf( "%s bytes (%d):", label, size );

    if( size <= wrap_threshold )
    {
        for( i = 0; i < size; ++i )
            printf( " %02x", data[i] );
    }
    else
    {
        int num_lines = ( size + max_bytes_per_line - 1 ) / max_bytes_per_line;
        int bytes_per_line = ( size + num_lines - 1 ) / num_lines;
        for( i = 0; i < size; ++i )
            printf( "%s%02x", ( i % bytes_per_line == 0 ) ? "\n        " : " ", data[i] );
    }
    printf( "\n" );
}

/* C99 equivalent of print_struct(decimation_filter_dpp_fields(), ...) - no pointer-to-member
   table to drive it in C, so every field is spelled out here once, in wire order. */
static void print_decimation_filter_dpp_struct( const rs2_decimation_filter_dpp_config * v )
{
    printf( "        %-16s = %d\n", "version", (int)v->header.version );
    printf( "        %-16s = %d\n", "flags", (int)v->header.flags );
    printf( "        %-16s = 0x%x\n", "ctl_id", (unsigned int)v->header.ctl_id );
    printf( "        %-16s = %d\n", "param_count", (int)v->header.param_count );
    printf( "        %-16s = %d\n", "param_type", (int)v->header.param_type );
    printf( "        %-16s = %d\n", "enabled", v->enabled );
    printf( "        %-16s = %d\n", "magnitude", v->magnitude );
}

/* C99 equivalent of the C++ sample's generic print_range() for this struct. */
static void print_decimation_filter_dpp_range( const rs2_decimation_filter_dpp_range * r )
{
    printf( "        %-16s   [ min, max, default, step ]\n", "" );
#define DECIMATION_FILTER_DPP_ROW( label, field ) \
    printf( "        %-16s = [ %d, %d, %d, %d ]\n", label, \
            (int)r->min.field, (int)r->max.field, (int)r->def.field, (int)r->step.field )
    DECIMATION_FILTER_DPP_ROW( "version", header.version );
    DECIMATION_FILTER_DPP_ROW( "flags", header.flags );
    DECIMATION_FILTER_DPP_ROW( "ctl_id", header.ctl_id );
    DECIMATION_FILTER_DPP_ROW( "param_count", header.param_count );
    DECIMATION_FILTER_DPP_ROW( "param_type", header.param_type );
    DECIMATION_FILTER_DPP_ROW( "enabled", enabled );
    DECIMATION_FILTER_DPP_ROW( "magnitude", magnitude );
#undef DECIMATION_FILTER_DPP_ROW
}

/* C99 equivalent of print_struct(temporal_filter_dpp_fields(), ...) - see
   print_decimation_filter_dpp_struct() above. smooth_alpha is [0,1] scaled into [0,1000] (every
   DPP param slot is an int32), so it prints as a plain %d like every other field, not a float. */
static void print_temporal_filter_dpp_struct( const rs2_temporal_filter_dpp_config * v )
{
    printf( "        %-16s = %d\n", "version", (int)v->header.version );
    printf( "        %-16s = %d\n", "flags", (int)v->header.flags );
    printf( "        %-16s = 0x%x\n", "ctl_id", (unsigned int)v->header.ctl_id );
    printf( "        %-16s = %d\n", "param_count", (int)v->header.param_count );
    printf( "        %-16s = %d\n", "param_type", (int)v->header.param_type );
    printf( "        %-16s = %d\n", "enabled", v->enabled );
    printf( "        %-16s = %d\n", "smooth_alpha", v->smooth_alpha );
    printf( "        %-16s = %d\n", "smooth_delta", v->smooth_delta );
    printf( "        %-16s = %d\n", "persistency_index", v->persistency_index );
}

/* C99 equivalent of the C++ sample's generic print_range() for this struct - see
   print_decimation_filter_dpp_range() above. */
static void print_temporal_filter_dpp_range( const rs2_temporal_filter_dpp_range * r )
{
    printf( "        %-16s   [ min, max, default, step ]\n", "" );
#define TEMPORAL_FILTER_DPP_ROW( label, field ) \
    printf( "        %-16s = [ %d, %d, %d, %d ]\n", label, \
            (int)r->min.field, (int)r->max.field, (int)r->def.field, (int)r->step.field )
    TEMPORAL_FILTER_DPP_ROW( "version", header.version );
    TEMPORAL_FILTER_DPP_ROW( "flags", header.flags );
    TEMPORAL_FILTER_DPP_ROW( "ctl_id", header.ctl_id );
    TEMPORAL_FILTER_DPP_ROW( "param_count", header.param_count );
    TEMPORAL_FILTER_DPP_ROW( "param_type", header.param_type );
    TEMPORAL_FILTER_DPP_ROW( "enabled", enabled );
    TEMPORAL_FILTER_DPP_ROW( "smooth_alpha", smooth_alpha );
    TEMPORAL_FILTER_DPP_ROW( "smooth_delta", smooth_delta );
    TEMPORAL_FILTER_DPP_ROW( "persistency_index", persistency_index );
#undef TEMPORAL_FILTER_DPP_ROW
}

/* C99 equivalent of the C++ sample's generic print_struct(hdrd_fields(), ...) - no
   pointer-to-member table to drive it in C, so every field is spelled out here once, in wire
   order, matching rs_hdrd_control.h. */
static void print_hdrd_struct( const rs2_hdrd_control * v )
{
    printf( "        %-16s = %d\n", "version", (int)v->header.version );
    printf( "        %-16s = %d\n", "flags", (int)v->header.flags );
    printf( "        %-16s = 0x%x\n", "ctl_id", (unsigned int)v->header.ctl_id );
    printf( "        %-16s = %d\n", "param_count", (int)v->header.param_count );
    printf( "        %-16s = %d\n", "param_type", (int)v->header.param_type );
    printf( "        %-16s = %d\n", "enable", v->enable );
    printf( "        %-16s = %d\n", "filter_type", v->filter_type );
    printf( "        %-16s = %d\n", "downscale_ratio", v->downscale_ratio );
    printf( "        %-16s = %d\n", "shift_mode", v->shift_mode );
    printf( "        %-16s = %d\n", "shift_pixels", v->shift_pixels );
    printf( "        %-16s = %d\n", "threshold_mode", v->threshold_mode );
    printf( "        %-16s = %d\n", "threshold_mm", v->threshold_mm );
}

/* C99 equivalent of the C++ sample's generic print_range(hdrd_fields(), ...): one header
   line naming the columns, then one row per field, name = [ min, max, default, step ]. */
static void print_hdrd_range( const rs2_hdrd_control_range * r )
{
    printf( "        %-16s   [ min, max, default, step ]\n", "" );
#define HDRD_ROW( label, field ) \
    printf( "        %-16s = [ %d, %d, %d, %d ]\n", label, \
            (int)r->min.field, (int)r->max.field, (int)r->def.field, (int)r->step.field )
    HDRD_ROW( "version", header.version );
    HDRD_ROW( "flags", header.flags );
    HDRD_ROW( "ctl_id", header.ctl_id );
    HDRD_ROW( "param_count", header.param_count );
    HDRD_ROW( "param_type", header.param_type );
    HDRD_ROW( "enable", enable );
    HDRD_ROW( "filter_type", filter_type );
    HDRD_ROW( "downscale_ratio", downscale_ratio );
    HDRD_ROW( "shift_mode", shift_mode );
    HDRD_ROW( "shift_pixels", shift_pixels );
    HDRD_ROW( "threshold_mode", threshold_mode );
    HDRD_ROW( "threshold_mm", threshold_mm );
#undef HDRD_ROW
}

/* Sets *cfg and reads it back into *after - the C99 equivalent for rs2_decimation_filter_dpp_config.
   Returns 1 on success, 0 (SKIPPED message printed) on failure. */
static int decimation_filter_dpp_set_and_readback( const rs2_options * opts, rs2_composite_option_id id,
                                                     const rs2_decimation_filter_dpp_config * cfg,
                                                     rs2_decimation_filter_dpp_config * after )
{
    rs2_error * e = NULL;
    const rs2_raw_data_buffer * raw;
    const unsigned char * bytes;

    rs2_set_composite_option( opts, id, cfg, sizeof( *cfg ), &e );
    if( e )
        goto failed;

    raw = rs2_get_composite_option( opts, id, &e );
    if( e )
        goto failed;
    bytes = rs2_get_raw_data( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto failed;
    }
    memcpy( after, bytes, sizeof( *after ) );
    rs2_delete_raw_data( raw );
    return 1;

failed:
    printf( "      SKIPPED (registered but not functional on this device/FW): %s\n", rs2_get_error_message( e ) );
    rs2_free_error( e );
    return 0;
}

/* Full read-modify-write + range + metadata sequence - the C99 equivalent of
   exercise_decimation_filter_dpp() in rs-composite-option.cpp. Magnitude is currently a fixed
   FW range ([2,2]) on some builds, wider on others - the Set below still exercises the full
   atomic write path using the FW-reported default rather than a hardcoded literal. */
static int exercise_decimation_filter_dpp( const rs2_options * opts, rs2_composite_option_id id )
{
    rs2_error * e = NULL;
    const rs2_raw_data_buffer * raw;
    const unsigned char * bytes;
    int size;
    rs2_decimation_filter_dpp_config current, cfg, after;
    rs2_decimation_filter_dpp_range range;

    raw = rs2_get_composite_option( opts, id, &e );
    if( e )
        goto skipped;
    size = rs2_get_raw_data_size( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    if( (size_t)size != sizeof( current ) )
    {
        printf( "      unexpected payload size: %d (expected %zu)\n", size, sizeof( current ) );
        rs2_delete_raw_data( raw );
        return 0;
    }
    bytes = rs2_get_raw_data( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    print_bytes( "      Get Raw Data:", bytes, size );
    memcpy( &current, bytes, sizeof( current ) );
    rs2_delete_raw_data( raw );
    printf( "      Get Structured Data:\n" );
    print_decimation_filter_dpp_struct( &current );

    /* Get range first - magnitude's Set below uses range.def rather than a hardcoded literal,
       since the FW-reported valid range is currently a single fixed value. */
    raw = rs2_get_composite_option_range( opts, id, &e );
    if( e )
        goto skipped;
    size = rs2_get_raw_data_size( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    if( (size_t)size != sizeof( range ) )
    {
        printf( "      unexpected range payload size: %d (expected %zu)\n", size, sizeof( range ) );
        rs2_delete_raw_data( raw );
        return 0;
    }
    bytes = rs2_get_raw_data( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    print_bytes( "      Get Range", bytes, size );
    memcpy( &range, bytes, sizeof( range ) );
    rs2_delete_raw_data( raw );
    printf( "      Range:\n" );
    print_decimation_filter_dpp_range( &range );

    cfg = current;
    cfg.enabled = 1;
    cfg.magnitude = range.def.magnitude;
    if( ! decimation_filter_dpp_set_and_readback( opts, id, &cfg, &after ) )
        return 0;  /* helper already printed and freed its own error */
    printf( "      Set (enabled=1 magnitude=%d): %s\n", cfg.magnitude,
            ( after.enabled == cfg.enabled && after.magnitude == cfg.magnitude )
                ? "matches what was sent" : "differs - FW may quantize/clamp on write" );
    print_decimation_filter_dpp_struct( &after );

    /* Restore the original value read in step 1 - same discipline as the other controls, even
       though this one also reverts on its own once Depth/IR starts streaming. */
    if( ! decimation_filter_dpp_set_and_readback( opts, id, &current, &after ) )
        return 0;  /* helper already printed and freed its own error */
    printf( "      Restore original value: %s\n",
            ( after.enabled == current.enabled && after.magnitude == current.magnitude )
                ? "ok" : "FAILED to restore - device may be left in the sample's last test state" );

    printf( "      Read-only: %s\n", rs2_is_composite_option_read_only( opts, id, &e ) ? "true" : "false" );
    if( e )
        goto skipped;
    {
        const char * description = rs2_get_composite_option_description( opts, id, &e );
        if( e )
            goto skipped;
        printf( "      Description: \"%s\"\n", description );
    }

    return 1;

skipped:
    printf( "      SKIPPED (registered but not functional on this device/FW): %s\n", rs2_get_error_message( e ) );
    rs2_free_error( e );
    return 0;
}

/* Sets *cfg and reads it back into *after - the C99 equivalent for rs2_temporal_filter_dpp_config.
   Returns 1 on success, 0 (SKIPPED message printed) on failure. */
static int temporal_filter_dpp_set_and_readback( const rs2_options * opts, rs2_composite_option_id id,
                                                   const rs2_temporal_filter_dpp_config * cfg,
                                                   rs2_temporal_filter_dpp_config * after )
{
    rs2_error * e = NULL;
    const rs2_raw_data_buffer * raw;
    const unsigned char * bytes;

    rs2_set_composite_option( opts, id, cfg, sizeof( *cfg ), &e );
    if( e )
        goto failed;

    raw = rs2_get_composite_option( opts, id, &e );
    if( e )
        goto failed;
    bytes = rs2_get_raw_data( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto failed;
    }
    memcpy( after, bytes, sizeof( *after ) );
    rs2_delete_raw_data( raw );
    return 1;

failed:
    printf( "      SKIPPED (registered but not functional on this device/FW): %s\n", rs2_get_error_message( e ) );
    rs2_free_error( e );
    return 0;
}

/* Full read-modify-write + range + metadata sequence - the C99 equivalent of
   exercise_temporal_filter_dpp() in rs-composite-option.cpp. A non-functional control is
   reported as SKIPPED via `goto skipped` rather than aborting the whole program. */
static int exercise_temporal_filter_dpp( const rs2_options * opts, rs2_composite_option_id id )
{
    rs2_error * e = NULL;
    const rs2_raw_data_buffer * raw;
    const unsigned char * bytes;
    int size;
    rs2_temporal_filter_dpp_config current, cfg, after;
    rs2_temporal_filter_dpp_range range;

    /* 1) Get (before) - both forms of the read, same as the C++ sample: the raw untyped
       bytes get_composite_option() returns, and (here, by hand) the typed cast. */
    raw = rs2_get_composite_option( opts, id, &e );
    if( e )
        goto skipped;
    size = rs2_get_raw_data_size( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    if( (size_t)size != sizeof( current ) )
    {
        printf( "      unexpected payload size: %d (expected %zu)\n", size, sizeof( current ) );
        rs2_delete_raw_data( raw );
        return 0;
    }
    bytes = rs2_get_raw_data( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    print_bytes( "      Get Raw Data:", bytes, size );
    memcpy( &current, bytes, sizeof( current ) );
    rs2_delete_raw_data( raw );
    printf( "      Get Structured Data:\n" );
    print_temporal_filter_dpp_struct( &current );

    /* 2) Set + Get read-modify-write - wire fields carried over untouched from `current` on
       every step below, same discipline as exercise_hdrd_control() below. */
    cfg = current;
    cfg.enabled = 1;
    cfg.smooth_alpha = 550;  /* normalized [0,1] scaled into [0,1000] - i.e. 0.55 */
    cfg.smooth_delta = 35;
    cfg.persistency_index = 5;
    if( ! temporal_filter_dpp_set_and_readback( opts, id, &cfg, &after ) )
        return 0;  /* helper already printed and freed its own error */
    printf( "      Set (enabled=1 smooth_alpha=550 smooth_delta=35 persistency_index=5): %s\n",
            ( after.enabled == cfg.enabled && after.smooth_alpha == cfg.smooth_alpha
              && after.smooth_delta == cfg.smooth_delta && after.persistency_index == cfg.persistency_index )
                ? "matches what was sent" : "differs - FW may quantize/clamp on write" );
    print_temporal_filter_dpp_struct( &after );

    /* Restore the original value read in step 1 - leaving no lasting effect on the device
       matters more here than the ceremony of one more Set/Get pair. */
    if( ! temporal_filter_dpp_set_and_readback( opts, id, &current, &after ) )
        return 0;  /* helper already printed and freed its own error */
    printf( "      Restore original value: %s\n",
            ( after.enabled == current.enabled && after.smooth_alpha == current.smooth_alpha
              && after.smooth_delta == current.smooth_delta && after.persistency_index == current.persistency_index )
                ? "ok" : "FAILED to restore - device may be left in the sample's last test state" );

    /* 3) Get range - raw bytes, then the manual cast to rs2_temporal_filter_dpp_range (4 full
       copies of the struct - min/max/step/def). */
    raw = rs2_get_composite_option_range( opts, id, &e );
    if( e )
        goto skipped;
    size = rs2_get_raw_data_size( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    if( (size_t)size != sizeof( range ) )
    {
        printf( "      unexpected range payload size: %d (expected %zu)\n", size, sizeof( range ) );
        rs2_delete_raw_data( raw );
        return 0;
    }
    bytes = rs2_get_raw_data( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    print_bytes( "      Get Range", bytes, size );
    memcpy( &range, bytes, sizeof( range ) );
    rs2_delete_raw_data( raw );
    printf( "      Range:\n" );
    print_temporal_filter_dpp_range( &range );

    /* 4) Query info. */
    printf( "      Read-only: %s\n", rs2_is_composite_option_read_only( opts, id, &e ) ? "true" : "false" );
    if( e )
        goto skipped;
    {
        const char * description = rs2_get_composite_option_description( opts, id, &e );
        if( e )
            goto skipped;
        printf( "      Description: \"%s\"\n", description );
    }

    return 1;

skipped:
    printf( "      SKIPPED (registered but not functional on this device/FW): %s\n", rs2_get_error_message( e ) );
    rs2_free_error( e );
    return 0;
}

/* Sets *cfg and reads it back into *after. Returns 1 on success, 0 (SKIPPED message printed) on
   failure. */
static int hdrd_set_and_readback( const rs2_options * opts, rs2_composite_option_id id,
                                                    const rs2_hdrd_control * cfg,
                                                    rs2_hdrd_control * after )
{
    rs2_error * e = NULL;
    const rs2_raw_data_buffer * raw;
    const unsigned char * bytes;

    rs2_set_composite_option( opts, id, cfg, sizeof( *cfg ), &e );
    if( e )
        goto failed;

    raw = rs2_get_composite_option( opts, id, &e );
    if( e )
        goto failed;
    bytes = rs2_get_raw_data( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto failed;
    }
    memcpy( after, bytes, sizeof( *after ) );
    rs2_delete_raw_data( raw );
    return 1;

failed:
    printf( "      SKIPPED (registered but not functional on this device/FW): %s\n", rs2_get_error_message( e ) );
    rs2_free_error( e );
    return 0;
}

/* Full read-modify-write + range + metadata sequence - the C99 equivalent of
   exercise_hdrd_control() in rs-composite-option.cpp. Exercises both conditional axes then
   restores the original value; a non-functional control is reported via `goto skipped`. */
static int exercise_hdrd_control( const rs2_options * opts, rs2_composite_option_id id )
{
    rs2_error * e = NULL;
    const rs2_raw_data_buffer * raw;
    const unsigned char * bytes;
    int size;
    rs2_hdrd_control current, cfg, after;
    rs2_hdrd_control_range range;
    int mode;

    /* 1) Get (before) - both forms of the read, same as the C++ sample: the raw untyped
       bytes get_composite_option() returns, and (here, by hand) the typed cast. */
    raw = rs2_get_composite_option( opts, id, &e );
    if( e )
        goto skipped;
    size = rs2_get_raw_data_size( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    if( (size_t)size != sizeof( current ) )
    {
        printf( "      unexpected payload size: %d (expected %zu)\n", size, sizeof( current ) );
        rs2_delete_raw_data( raw );
        return 0;
    }
    bytes = rs2_get_raw_data( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    print_bytes( "      Get Raw Data:", bytes, size );
    memcpy( &current, bytes, sizeof( current ) );
    rs2_delete_raw_data( raw );
    printf( "      Get Structured Data:\n" );
    print_hdrd_struct( &current );

    /* 2) Set + Get, wire header and every other field carried over untouched from `current`, not
       zero-initialized, on every step below. */
    cfg = current;
    cfg.enable = 1;

    /* Lookup Shift + Manual shift pixels - exercises filter_type together with the
       (shift_mode, shift_pixels) pair it selects. */
    cfg.filter_type = 1;
    cfg.shift_mode = 2;
    cfg.shift_pixels = 100;
    if( ! hdrd_set_and_readback( opts, id, &cfg, &after ) )
        return 0;  /* helper already printed and freed its own error */
    printf( "      Set (filter_type=Lookup Shift, shift_mode=Manual, shift_pixels=100): %s\n",
            ( after.filter_type == cfg.filter_type && after.shift_mode == cfg.shift_mode
              && after.shift_pixels == cfg.shift_pixels )
                ? "matches what was sent" : "differs - FW may quantize/clamp on write" );
    print_hdrd_struct( &after );

    /* Downscale + x4 ratio - exercises the OTHER branch filter_type selects between. */
    cfg.filter_type = 0;
    cfg.downscale_ratio = 2;
    if( ! hdrd_set_and_readback( opts, id, &cfg, &after ) )
        return 0;  /* helper already printed and freed its own error */
    printf( "      Set (filter_type=Downscale, downscale_ratio=x4): %s\n",
            ( after.filter_type == cfg.filter_type && after.downscale_ratio == cfg.downscale_ratio )
                ? "matches what was sent" : "differs - FW may quantize/clamp on write" );
    print_hdrd_struct( &after );

    /* Cycle threshold_mode through all three rungs - threshold_mm only matters for Manual. */
    for( mode = 0; mode <= 2; ++mode )
    {
        cfg.threshold_mode = mode;
        cfg.threshold_mm = ( mode == 2 ) ? 300 : 0;
        if( ! hdrd_set_and_readback( opts, id, &cfg, &after ) )
            return 0;  /* helper already printed and freed its own error */
        printf( "      Set (threshold_mode=%d): %s\n", mode,
                ( after.threshold_mode == cfg.threshold_mode )
                    ? "matches what was sent" : "differs - FW may quantize/clamp on write" );
        print_hdrd_struct( &after );
    }

    /* Restore the original value read in step 1 - this sample exercises real, consequential
       state changes (unlike a single-field toggle), so leaving no lasting effect on the device
       matters more here. */
    if( ! hdrd_set_and_readback( opts, id, &current, &after ) )
        return 0;  /* helper already printed and freed its own error */
    printf( "      Restore original value: %s\n",
            ( after.filter_type == current.filter_type && after.downscale_ratio == current.downscale_ratio
              && after.shift_mode == current.shift_mode && after.shift_pixels == current.shift_pixels
              && after.threshold_mode == current.threshold_mode && after.threshold_mm == current.threshold_mm )
                ? "ok" : "FAILED to restore - device may be left in the sample's last test state" );

    /* 3) Get range - raw bytes, then the manual cast to rs2_hdrd_control_range
       (4 full copies of the struct - min/max/step/def, header fields included). */
    raw = rs2_get_composite_option_range( opts, id, &e );
    if( e )
        goto skipped;
    size = rs2_get_raw_data_size( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    if( (size_t)size != sizeof( range ) )
    {
        printf( "      unexpected range payload size: %d (expected %zu)\n", size, sizeof( range ) );
        rs2_delete_raw_data( raw );
        return 0;
    }
    bytes = rs2_get_raw_data( raw, &e );
    if( e )
    {
        rs2_delete_raw_data( raw );
        goto skipped;
    }
    print_bytes( "      Get Range", bytes, size );
    memcpy( &range, bytes, sizeof( range ) );
    rs2_delete_raw_data( raw );
    printf( "      Range:\n" );
    print_hdrd_range( &range );

    /* 4) Query info. */
    printf( "      Read-only: %s\n", rs2_is_composite_option_read_only( opts, id, &e ) ? "true" : "false" );
    if( e )
        goto skipped;
    {
        const char * description = rs2_get_composite_option_description( opts, id, &e );
        if( e )
            goto skipped;
        printf( "      Description: \"%s\"\n", description );
    }

    return 1;

skipped:
    printf( "      SKIPPED (registered but not functional on this device/FW): %s\n", rs2_get_error_message( e ) );
    rs2_free_error( e );
    return 0;
}

/* Dispatches to the right typed handler - no generic "any composite option" mechanism by design,
   so a new id needs a case here too. Returns 1 (succeeded), 0 (skipped), or -1 (no typed
   handler registered - not counted as either). */
static int exercise_composite_option( const rs2_options * opts, rs2_composite_option_id id )
{
    switch( id )
    {
    case RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP:
        return exercise_decimation_filter_dpp( opts, id );
    case RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP:
        return exercise_temporal_filter_dpp( opts, id );
    case RS2_COMPOSITE_OPTION_HDRD_CONTROL:
        return exercise_hdrd_control( opts, id );
    default:
        printf( "      (no typed handler registered for this composite option id)\n" );
        return -1;
    }
}

int main( void )
{
    rs2_error * e = NULL;
    int attempted = 0, succeeded = 0, skipped = 0;
    int d, s, f, c;

    rs2_context * ctx = rs2_create_context( RS2_API_VERSION, &e );
    check_error( e );

    rs2_device_list * devices = rs2_query_devices( ctx, &e );
    check_error( e );
    int device_count = rs2_get_device_count( devices, &e );
    check_error( e );
    printf( "Found %d device(s)\n", device_count );

    for( d = 0; d < device_count; ++d )
    {
        rs2_device * dev = rs2_create_device( devices, d, &e );
        check_error( e );

        const char * dev_name = rs2_supports_device_info( dev, RS2_CAMERA_INFO_NAME, &e )
            ? rs2_get_device_info( dev, RS2_CAMERA_INFO_NAME, &e ) : "Unknown device";
        check_error( e );
        const char * dev_sn = rs2_supports_device_info( dev, RS2_CAMERA_INFO_SERIAL_NUMBER, &e )
            ? rs2_get_device_info( dev, RS2_CAMERA_INFO_SERIAL_NUMBER, &e ) : "N/A";
        check_error( e );
        printf( "\nDevice: %s (S/N %s)\n", dev_name, dev_sn );

        rs2_sensor_list * sensors = rs2_query_sensors( dev, &e );
        check_error( e );
        int sensor_count = rs2_get_sensors_count( sensors, &e );
        check_error( e );

        for( s = 0; s < sensor_count; ++s )
        {
            rs2_sensor * sensor = rs2_create_sensor( sensors, s, &e );
            check_error( e );

            int is_depth = rs2_is_sensor_extendable_to( sensor, RS2_EXTENSION_DEPTH_SENSOR, &e );
            check_error( e );
            if( ! is_depth )
            {
                rs2_delete_sensor( sensor );
                continue;
            }

            const char * sensor_name = rs2_supports_sensor_info( sensor, RS2_CAMERA_INFO_NAME, &e )
                ? rs2_get_sensor_info( sensor, RS2_CAMERA_INFO_NAME, &e ) : "Depth Sensor";
            check_error( e );
            printf( "  Depth sensor: %s\n", sensor_name );

            /* Composite options live on a sensor's EMBEDDED FILTERs, each its own independent
               options registry - not on the depth sensor's own registry directly - so every
               filter must be walked, not just the first one. */
            rs2_embedded_filter_list * filters = rs2_query_embedded_filters( sensor, &e );
            check_error( e );
            int filter_count = rs2_get_embedded_filters_count( filters, &e );
            check_error( e );

            for( f = 0; f < filter_count; ++f )
            {
                rs2_embedded_filter * filter = rs2_create_embedded_filter( filters, f, &e );
                check_error( e );

                rs2_embedded_filter_type filter_type = rs2_get_embedded_filter_type( filter, &e );
                check_error( e );
                printf( "    Embedded filter: %s\n", rs2_embedded_filter_type_to_string( filter_type ) );

                /* rs2_embedded_filter is-a rs2_options in the underlying C++ implementation (see
                   struct rs2_embedded_filter : public rs2_options in src/rs.cpp) - every
                   composite-option C function takes an rs2_options*, so this cast is valid. */
                const rs2_options * opts = (const rs2_options *)filter;

                rs2_composite_options_list * composite_ids = rs2_get_composite_options_list( opts, &e );
                check_error( e );
                int composite_count = rs2_get_composite_options_list_size( composite_ids, &e );
                check_error( e );

                if( composite_count == 0 )
                    printf( "      Supports composite options: no (scalar rs2_option only)\n" );
                else
                    printf( "      Supports composite options: yes (%d)\n", composite_count );

                for( c = 0; c < composite_count; ++c )
                {
                    rs2_composite_option_id id = rs2_get_composite_option_from_list( composite_ids, c, &e );
                    check_error( e );

                    printf( "      - %s:\n", composite_option_name( id ) );
                    ++attempted;
                    int result = exercise_composite_option( opts, id );
                    if( result == 1 )
                        ++succeeded;
                    else if( result == 0 )
                        ++skipped;
                }

                rs2_delete_composite_options_list( composite_ids );
                rs2_delete_embedded_filter( filter );
            }

            rs2_delete_embedded_filter_list( filters );
            rs2_delete_sensor( sensor );
        }

        rs2_delete_sensor_list( sensors );
        rs2_delete_device( dev );
    }

    printf( "\n=== Summary: %d composite option(s) found, %d walked successfully end-to-end, "
            "%d skipped (registered but non-functional on this device/FW) ===\n",
            attempted, succeeded, skipped );

    rs2_delete_device_list( devices );
    rs2_delete_context( ctx );
    return 0;
}
