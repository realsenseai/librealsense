// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "h264-decoder.h"

#include <rsutils/easylogging/easyloggingpp.h>

#include <vpl/mfxvideo.h>
#include <vpl/mfxdispatcher.h>
#include <va/va.h>
#include <va/va_drm.h>

#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <cstring>

namespace librealsense
{
    namespace
    {
        std::string mfx_string( mfxStatus s ) { return std::to_string( static_cast< int >( s ) ); }

        // oneVPL decoding on an Intel GPU, the counterpart of the Media Foundation backend on Windows. Like that one it is
        // a stateful API - the library parses the SPS/PPS and manages reference frames, we only hand it access units.
        class vpl_h264_decoder : public h264_decoder_backend
        {
        public:
            vpl_h264_decoder( int width, int height )
                : _width( width )
                , _height( height )
            {
                _loader = MFXLoad();
                if( ! _loader ) throw std::runtime_error( "MFXLoad failed" );
                require( MFX_IMPL_TYPE_HARDWARE, "mfxImplDescription.Impl" );
                require( MFX_CODEC_AVC, "mfxImplDescription.mfxDecoderDescription.decoder.CodecID" );

                mfxStatus s = MFXCreateSession( _loader, 0, &_session );
                if( s != MFX_ERR_NONE )
                    throw std::runtime_error( "no hardware oneVPL H.264 decoder, MFXCreateSession=" + mfx_string( s ) );

                // The Linux runtime decodes through VA-API and will not initialize until we give it a display
                _drm_fd = open( "/dev/dri/renderD128", O_RDWR );
                if( _drm_fd < 0 )
                    throw std::runtime_error( "cannot open /dev/dri/renderD128 - is this user in the 'render' group?" );
                _va = vaGetDisplayDRM( _drm_fd );
                int major = 0, minor = 0;
                VAStatus v = _va ? vaInitialize( _va, &major, &minor ) : VA_STATUS_ERROR_INVALID_DISPLAY;
                if( v != VA_STATUS_SUCCESS )
                    throw std::runtime_error( std::string( "vaInitialize failed: " ) + vaErrorStr( v ) );
                if( ( s = MFXVideoCORE_SetHandle( _session, MFX_HANDLE_VA_DISPLAY, (mfxHDL)_va ) ) != MFX_ERR_NONE )
                    throw std::runtime_error( "MFXVideoCORE_SetHandle=" + mfx_string( s ) );
            }

            ~vpl_h264_decoder() override
            {
                if( _initialized ) MFXVideoDECODE_Close( _session );
                if( _session ) MFXClose( _session );
                if( _loader ) MFXUnload( _loader );
                if( _va ) vaTerminate( _va );
                if( _drm_fd >= 0 ) close( _drm_fd );
            }

            bool decode( uint8_t const * data, size_t size, std::vector< uint8_t > & nv12 ) override
            {
                mfxBitstream bs = {};
                bs.Data = const_cast< mfxU8 * >( data );
                bs.DataLength = bs.MaxLength = static_cast< mfxU32 >( size );
                bs.CodecId = MFX_CODEC_AVC;
                // Each call carries exactly one access unit, so the decoder need not look for frame boundaries itself
                bs.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;

                if( ! _initialized && ! start( bs ) )
                    return false;

                mfxFrameSurface1 * work = free_surface();
                if( ! work )
                {
                    LOG_WARNING( "H264 oneVPL surface pool exhausted" );
                    return false;
                }

                mfxFrameSurface1 * out = nullptr;
                mfxSyncPoint sync = nullptr;
                mfxStatus s = MFXVideoDECODE_DecodeFrameAsync( _session, &bs, work, &out, &sync );
                if( s == MFX_ERR_MORE_DATA )
                    return false;  // priming, or waiting for parameter sets
                if( s < MFX_ERR_NONE || ! sync )
                {
                    if( s < MFX_ERR_NONE )
                        LOG_WARNING( "H264 DecodeFrameAsync=" << mfx_string( s ) );
                    return false;
                }
                if( ( s = MFXVideoCORE_SyncOperation( _session, sync, sync_timeout_ms ) ) != MFX_ERR_NONE )
                {
                    LOG_WARNING( "H264 SyncOperation=" << mfx_string( s ) );
                    return false;
                }
                copy_out( *out, nv12 );
                return true;
            }

        private:
            void require( mfxU32 value, char const * property )
            {
                mfxConfig cfg = MFXCreateConfig( _loader );
                mfxVariant v = {};
                v.Version.Version = MFX_VARIANT_VERSION;
                v.Type = MFX_VARIANT_TYPE_U32;
                v.Data.U32 = value;
                MFXSetConfigFilterProperty( cfg, reinterpret_cast< mfxU8 const * >( property ), v );
            }

            // The parameter sets ride in the first access unit, so the stream itself tells us the geometry to allocate for
            bool start( mfxBitstream & bs )
            {
                mfxVideoParam par = {};
                par.mfx.CodecId = MFX_CODEC_AVC;
                par.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
                mfxStatus s = MFXVideoDECODE_DecodeHeader( _session, &bs, &par );
                if( s != MFX_ERR_NONE )
                    return false;  // not enough of the stream yet; the caller feeds us the next access unit

                if( ( s = MFXVideoDECODE_Init( _session, &par ) ) < MFX_ERR_NONE )
                {
                    LOG_WARNING( "H264 DECODE_Init=" << mfx_string( s ) );
                    return false;
                }

                mfxFrameAllocRequest req = {};
                MFXVideoDECODE_QueryIOSurf( _session, &par, &req );
                _coded_width = ( par.mfx.FrameInfo.Width + 31 ) & ~31;
                _coded_height = ( par.mfx.FrameInfo.Height + 31 ) & ~31;
                size_t const plane = size_t( _coded_width ) * _coded_height;

                _surfaces.resize( req.NumFrameSuggested + extra_surfaces );
                _planes.resize( _surfaces.size() );
                for( size_t i = 0; i < _surfaces.size(); ++i )
                {
                    _planes[i].resize( plane * 3 / 2 );
                    _surfaces[i] = {};
                    _surfaces[i].Info = par.mfx.FrameInfo;
                    _surfaces[i].Data.Y = _planes[i].data();
                    _surfaces[i].Data.UV = _planes[i].data() + plane;
                    _surfaces[i].Data.Pitch = static_cast< mfxU16 >( _coded_width );
                }
                _initialized = true;
                LOG_DEBUG( "H.264 decoding via oneVPL (GPU), coded " << _coded_width << 'x' << _coded_height );
                return true;
            }

            mfxFrameSurface1 * free_surface()
            {
                for( auto & s : _surfaces )
                    if( ! s.Data.Locked )
                        return &s;
                return nullptr;
            }

            // Hand back tightly packed NV12 at the profile's resolution: the surface is padded both for alignment and
            // because H.264 codes in whole macroblocks, so every row has to be copied out of its pitch
            void copy_out( mfxFrameSurface1 const & surf, std::vector< uint8_t > & nv12 ) const
            {
                int const pitch = surf.Data.Pitch;
                nv12.resize( size_t( _width ) * _height * 3 / 2 );
                uint8_t * dst = nv12.data();
                for( int row = 0; row < _height; ++row, dst += _width )
                    memcpy( dst, surf.Data.Y + size_t( row ) * pitch, _width );
                for( int row = 0; row < _height / 2; ++row, dst += _width )
                    memcpy( dst, surf.Data.UV + size_t( row ) * pitch, _width );
            }

            static constexpr mfxU32 sync_timeout_ms = 2000;
            static constexpr size_t extra_surfaces = 8;

            mfxLoader _loader = nullptr;
            mfxSession _session = nullptr;
            VADisplay _va = nullptr;
            int _drm_fd = -1;
            std::vector< mfxFrameSurface1 > _surfaces;
            std::vector< std::vector< uint8_t > > _planes;
            int _width, _height;
            int _coded_width = 0, _coded_height = 0;
            bool _initialized = false;
        };
    }

    std::unique_ptr< h264_decoder_backend > h264_decoder_backend::create( int width, int height, int )
    {
        try
        {
            return std::unique_ptr< h264_decoder_backend >( new vpl_h264_decoder( width, height ) );
        }
        catch( std::exception const & e )
        {
            LOG_WARNING( "no oneVPL H.264 decoder: " << e.what() );
            return nullptr;
        }
    }
}
