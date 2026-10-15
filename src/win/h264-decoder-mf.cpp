// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <src/proc/h264-decoder.h>

#include <rsutils/easylogging/easyloggingpp.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <wrl/client.h>  // ComPtr - from the Windows SDK, so this file does not require ATL
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <Mferror.h>
#include <wmcodecdsp.h>
#include <d3d11_4.h>

#include <thread>

#pragma comment( lib, "mfplat.lib" )
#pragma comment( lib, "mfuuid.lib" )
#pragma comment( lib, "wmcodecdspuuid.lib" )
#pragma comment( lib, "d3d11.lib" )

namespace librealsense
{
    namespace
    {
        template< class T >
        using ComPtr = Microsoft::WRL::ComPtr< T >;

        std::string hr_string( HRESULT hr )
        {
            char buf[16];
            snprintf( buf, sizeof( buf ), "0x%08lx", static_cast< unsigned long >( hr ) );
            return buf;
        }

        // The Microsoft H.264 Video Decoder MFT. Handles High profile (CABAC, 8x8 transform), which
        // Constrained-Baseline-only decoders cannot. Driven one access unit in / one picture out,
        // which suits a stream with no B-frames.
        class mf_h264_decoder : public h264_decoder_backend
        {
        public:
            mf_h264_decoder( int width, int height, int fps )
                : _width( width )
                , _height( height )
                , _fps( fps )
                , _frame_duration( 10000000 / ( fps > 0 ? fps : 30 ) )  // 100ns units
            {
                HRESULT hr = CoInitializeEx( nullptr, COINIT_MULTITHREADED );
                _co_owned = ( hr == S_OK || hr == S_FALSE );  // not ours on RPC_E_CHANGED_MODE
                _co_thread = std::this_thread::get_id();

                if( FAILED( hr = MFStartup( MF_VERSION, MFSTARTUP_NOSOCKET ) ) )
                    throw std::runtime_error( "MFStartup failed, hr=" + hr_string( hr ) );
                _mf_started = true;

                init_mft( fps, true );  // prefer DXVA; falls back to software on its own
            }

            ~mf_h264_decoder() override
            {
                if( _mft )
                {
                    _mft->ProcessMessage( MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0 );
                    _mft->ProcessMessage( MFT_MESSAGE_NOTIFY_END_STREAMING, 0 );
                    _mft.Reset();
                }
                _in_sample.Reset();
                _out_sample.Reset();
                _dxgi_manager.Reset();
                if( _mf_started )
                    MFShutdown();
                // CoUninitialize only balances the thread that initialized, and frames may be handled
                // on a different thread than the one we are destroyed on
                if( _co_owned && _co_thread == std::this_thread::get_id() )
                    CoUninitialize();
            }

            bool decode( uint8_t const * data, size_t size, std::vector< uint8_t > & nv12 ) override
            {
                if( ! feed( data, size ) )
                    return false;

                for( ;; )
                {
                    HRESULT hr = pull( nv12 );
                    if( SUCCEEDED( hr ) )
                        return true;
                    if( _readback_failed )
                    {
                        // Some drivers hand back surfaces the CPU cannot map. Decoding on the GPU is
                        // useless if we cannot read the result, so rebuild in software.
                        LOG_WARNING( "H264 DXVA readback failed (hr=" << hr_string( hr ) << "), falling back to software" );
                        _readback_failed = false;
                        init_mft( _fps, false );
                        return false;
                    }
                    if( hr == MF_E_TRANSFORM_NEED_MORE_INPUT )
                        return false;  // priming, or waiting for parameter sets
                    if( hr == MF_E_TRANSFORM_STREAM_CHANGE )
                    {
                        // The decoder learned the real coded size from the SPS
                        if( ! set_output_type() )
                            return false;
                        continue;
                    }
                    LOG_WARNING( "H264 ProcessOutput failed, hr=" << hr_string( hr ) );
                    return false;
                }
            }

        private:
            // Creates the decoder MFT and negotiates types. With try_dxva the MFT decodes on the GPU
            // when the driver supports it; any failure along the way just leaves us in software.
            void init_mft( int fps, bool try_dxva )
            {
                _mft.Reset();
                _in_sample.Reset();
                _out_sample.Reset();
                _dxgi_manager.Reset();
                _dxva = false;
                _coded_width = _coded_height = 0;

                HRESULT hr;
                if( FAILED( hr = CoCreateInstance( CLSID_CMSH264DecoderMFT,
                                                   nullptr,
                                                   CLSCTX_INPROC_SERVER,
                                                   IID_PPV_ARGS( &_mft ) ) ) )
                    throw std::runtime_error( "no H.264 decoder MFT, hr=" + hr_string( hr ) );

                ComPtr< IMFAttributes > attrs;
                if( SUCCEEDED( _mft->GetAttributes( &attrs ) ) && attrs )
                    attrs->SetUINT32( MF_LOW_LATENCY, TRUE );  // emit each picture as soon as decoded

                if( try_dxva )
                    _dxva = enable_dxva( attrs.Get() );
                LOG_DEBUG( "H.264 decoding via " << ( _dxva ? "DXVA (GPU)" : "software" ) );

                set_input_type( fps );
                if( ! set_output_type() )
                    throw std::runtime_error( "no NV12 output type on the H.264 decoder" );

                _mft->ProcessMessage( MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0 );
                _mft->ProcessMessage( MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0 );
            }

            // Hands the MFT a D3D11 device so it decodes on the GPU. Must precede type negotiation,
            // which changes once the MFT is in D3D mode.
            bool enable_dxva( IMFAttributes * attrs )
            {
                UINT32 aware = 0;
                if( ! attrs || FAILED( attrs->GetUINT32( MF_SA_D3D11_AWARE, &aware ) ) || ! aware )
                    return false;

                ComPtr< ID3D11Device > device;
                ComPtr< ID3D11DeviceContext > context;
                HRESULT hr = D3D11CreateDevice( nullptr,
                                                D3D_DRIVER_TYPE_HARDWARE,
                                                nullptr,
                                                D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                                                nullptr, 0,
                                                D3D11_SDK_VERSION,
                                                &device, nullptr, &context );
                if( FAILED( hr ) )
                    return false;

                // The MFT calls in on its own threads
                ComPtr< ID3D11Multithread > multithread;
                if( SUCCEEDED( context.As( &multithread ) ) )
                    multithread->SetMultithreadProtected( TRUE );

                UINT token = 0;
                if( FAILED( MFCreateDXGIDeviceManager( &token, &_dxgi_manager ) )
                    || FAILED( _dxgi_manager->ResetDevice( device.Get(), token ) )
                    || FAILED( _mft->ProcessMessage( MFT_MESSAGE_SET_D3D_MANAGER,
                                                     reinterpret_cast< ULONG_PTR >( _dxgi_manager.Get() ) ) ) )
                {
                    _dxgi_manager.Reset();
                    return false;
                }
                return true;
            }

            void set_input_type( int fps )
            {
                ComPtr< IMFMediaType > type;
                HRESULT hr;
                if( FAILED( hr = MFCreateMediaType( &type ) ) )
                    throw std::runtime_error( "MFCreateMediaType failed, hr=" + hr_string( hr ) );
                type->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video );
                type->SetGUID( MF_MT_SUBTYPE, MFVideoFormat_H264 );
                type->SetUINT32( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
                MFSetAttributeSize( type.Get(), MF_MT_FRAME_SIZE, _width, _height );
                MFSetAttributeRatio( type.Get(), MF_MT_FRAME_RATE, fps > 0 ? fps : 30, 1 );
                if( FAILED( hr = _mft->SetInputType( 0, type.Get(), 0 ) ) )
                    throw std::runtime_error( "SetInputType(H264) failed, hr=" + hr_string( hr ) );
            }

            // Selects the NV12 output type and caches the coded geometry it reports
            bool set_output_type()
            {
                for( DWORD i = 0;; ++i )
                {
                    ComPtr< IMFMediaType > type;
                    if( FAILED( _mft->GetOutputAvailableType( 0, i, &type ) ) )
                        break;
                    GUID subtype;
                    if( FAILED( type->GetGUID( MF_MT_SUBTYPE, &subtype ) ) || subtype != MFVideoFormat_NV12 )
                        continue;
                    if( FAILED( _mft->SetOutputType( 0, type.Get(), 0 ) ) )
                        continue;

                    UINT32 w = _width, h = _height;
                    MFGetAttributeSize( type.Get(), MF_MT_FRAME_SIZE, &w, &h );
                    _coded_width = static_cast< int >( w );
                    _coded_height = static_cast< int >( h );
                    _out_sample.Reset();  // geometry changed, so the cached sample no longer fits
                    return true;
                }
                return false;
            }

            bool feed( uint8_t const * data, size_t size )
            {
                ComPtr< IMFMediaBuffer > buffer;
                if( _in_sample && SUCCEEDED( _in_sample->GetBufferByIndex( 0, &buffer ) ) )
                {
                    DWORD capacity = 0;
                    buffer->GetMaxLength( &capacity );
                    if( capacity < size )
                    {
                        _in_sample.Reset();  // access units vary widely in size
                        buffer.Reset();
                    }
                }
                if( ! _in_sample )
                {
                    if( ! make_sample( _in_sample, size ) )
                        return false;
                    if( FAILED( _in_sample->GetBufferByIndex( 0, &buffer ) ) )
                        return false;
                }

                BYTE * dst = nullptr;
                if( FAILED( buffer->Lock( &dst, nullptr, nullptr ) ) )
                    return false;
                memcpy( dst, data, size );
                buffer->Unlock();
                buffer->SetCurrentLength( static_cast< DWORD >( size ) );

                _in_sample->SetSampleTime( _sample_time );
                _in_sample->SetSampleDuration( _frame_duration );
                _sample_time += _frame_duration;

                HRESULT hr = _mft->ProcessInput( 0, _in_sample.Get(), 0 );
                if( FAILED( hr ) )
                {
                    LOG_WARNING( "H264 ProcessInput failed, hr=" << hr_string( hr ) );
                    return false;
                }
                return true;
            }

            HRESULT pull( std::vector< uint8_t > & nv12 )
            {
                MFT_OUTPUT_STREAM_INFO info = {};
                _mft->GetOutputStreamInfo( 0, &info );
                bool const mft_allocates
                    = ( info.dwFlags
                        & ( MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES ) ) != 0;

                if( ! mft_allocates )
                {
                    if( ! _out_sample && ! make_sample( _out_sample, info.cbSize ) )
                        return E_OUTOFMEMORY;
                    // The MFT refuses to write into a buffer that still holds the previous picture
                    ComPtr< IMFMediaBuffer > reused;
                    if( SUCCEEDED( _out_sample->GetBufferByIndex( 0, &reused ) ) )
                        reused->SetCurrentLength( 0 );
                }

                MFT_OUTPUT_DATA_BUFFER out = {};
                out.pSample = mft_allocates ? nullptr : _out_sample.Get();
                DWORD status = 0;
                HRESULT hr = _mft->ProcessOutput( 0, 1, &out, &status );
                ComPtr< IMFSample > produced;
                if( mft_allocates )
                    produced.Attach( out.pSample );  // we own what the MFT allocated
                if( out.pEvents )
                    out.pEvents->Release();
                if( FAILED( hr ) )
                    return hr;

                IMFSample * sample = mft_allocates ? produced.Get() : _out_sample.Get();
                if( ! sample )
                    return E_UNEXPECTED;

                ComPtr< IMFMediaBuffer > buffer;
                if( FAILED( hr = sample->ConvertToContiguousBuffer( &buffer ) ) )
                    return hr;
                return copy_out( buffer.Get(), nv12 );
            }

            // Copies the picture out as tightly packed NV12, cropping the coded size down to the
            // profile's resolution - H.264 pads the height up to a multiple of 16
            HRESULT copy_out( IMFMediaBuffer * buffer, std::vector< uint8_t > & nv12 )
            {
                int const cw = _coded_width > 0 ? _coded_width : _width;
                int const ch = _coded_height > 0 ? _coded_height : _height;
                size_t const coded_size = static_cast< size_t >( cw ) * ch * 3 / 2;
                _scratch.resize( coded_size );

                HRESULT hr;
                ComPtr< IMF2DBuffer > b2d;
                if( SUCCEEDED( buffer->QueryInterface( IID_PPV_ARGS( &b2d ) ) ) )
                {
                    // Packs the planes at the coded stride whatever the surface layout is
                    if( FAILED( hr = b2d->ContiguousCopyTo( _scratch.data(),
                                                            static_cast< DWORD >( _scratch.size() ) ) ) )
                    {
                        _readback_failed = _dxva;
                        return hr;
                    }
                }
                else
                {
                    BYTE * locked = nullptr;
                    DWORD length = 0;
                    if( FAILED( hr = buffer->Lock( &locked, nullptr, &length ) ) )
                    {
                        _readback_failed = _dxva;
                        return hr;
                    }
                    hr = ( length < coded_size ) ? E_UNEXPECTED : S_OK;
                    if( SUCCEEDED( hr ) )
                        memcpy( _scratch.data(), locked, coded_size );
                    buffer->Unlock();
                    if( FAILED( hr ) )
                    {
                        _readback_failed = _dxva;
                        return hr;
                    }
                }

                uint8_t const * const src = _scratch.data();
                nv12.resize( static_cast< size_t >( _width ) * _height * 3 / 2 );
                if( cw == _width && ch == _height )
                {
                    memcpy( nv12.data(), src, nv12.size() );
                    return S_OK;
                }

                uint8_t * dst = nv12.data();
                for( int row = 0; row < _height; ++row, dst += _width )
                    memcpy( dst, src + static_cast< size_t >( row ) * cw, _width );
                uint8_t const * const uv = src + static_cast< size_t >( cw ) * ch;
                for( int row = 0; row < _height / 2; ++row, dst += _width )
                    memcpy( dst, uv + static_cast< size_t >( row ) * cw, _width );
                return S_OK;
            }

            static bool make_sample( ComPtr< IMFSample > & sample, size_t size )
            {
                ComPtr< IMFMediaBuffer > buffer;
                if( FAILED( MFCreateMemoryBuffer( static_cast< DWORD >( size ), &buffer ) ) )
                    return false;
                ComPtr< IMFSample > s;
                if( FAILED( MFCreateSample( &s ) ) || FAILED( s->AddBuffer( buffer.Get() ) ) )
                    return false;
                sample = s;
                return true;
            }

            ComPtr< IMFTransform > _mft;
            ComPtr< IMFDXGIDeviceManager > _dxgi_manager;
            ComPtr< IMFSample > _in_sample;
            ComPtr< IMFSample > _out_sample;
            std::vector< uint8_t > _scratch;
            std::thread::id _co_thread;
            LONGLONG _sample_time = 0;
            int _width, _height, _fps;
            LONGLONG _frame_duration;
            int _coded_width = 0, _coded_height = 0;
            bool _dxva = false;
            bool _readback_failed = false;
            bool _mf_started = false;
            bool _co_owned = false;
        };
    }

    std::unique_ptr< h264_decoder_backend > h264_decoder_backend::create( int width, int height, int fps )
    {
        try
        {
            return std::unique_ptr< h264_decoder_backend >( new mf_h264_decoder( width, height, fps ) );
        }
        catch( std::exception const & e )
        {
            LOG_WARNING( "no Media Foundation H.264 decoder: " << e.what() );
            return nullptr;
        }
    }
}
