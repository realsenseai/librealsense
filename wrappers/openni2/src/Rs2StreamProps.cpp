#include "Rs2Driver.h"
#include "PS1080.h"

static const unsigned long long GAIN_VAL = 42;
static const unsigned long long CONST_SHIFT_VAL = 200;
static const unsigned long long MAX_SHIFT_VAL = 2047;
static const unsigned long long PARAM_COEFF_VAL = 4;
static const unsigned long long SHIFT_SCALE_VAL = 10;
static const unsigned long long ZERO_PLANE_DISTANCE_VAL = 120;
static const double ZERO_PLANE_PIXEL_SIZE_VAL = 0.10520000010728836;
static const double EMITTER_DCMOS_DISTANCE_VAL = 7.5; 

namespace oni { namespace driver {

OniStatus Rs2Stream::setProperty(int propertyId, const void* data, int dataSize)
{
	SCOPED_PROFILER;

	rsTraceFunc("propertyId=%d dataSize=%d", propertyId, dataSize);

	switch (propertyId)
	{
		case ONI_STREAM_PROPERTY_VIDEO_MODE:
		{
			if (data && (dataSize == sizeof(OniVideoMode)))
			{
				OniVideoMode* mode = (OniVideoMode*)data;
				rsLogDebug("set video mode: %dx%d @%d format=%d", 
					(int)mode->resolutionX, (int)mode->resolutionY, (int)mode->fps, (int)mode->pixelFormat);

				if (isVideoModeSupported(mode))
				{
					m_videoMode = *mode;
					return ONI_STATUS_OK;
				}
			}
			break;
		}

		case ONI_STREAM_PROPERTY_AUTO_WHITE_BALANCE:
		{
			if (data && dataSize == sizeof(OniBool) && m_oniType == ONI_SENSOR_COLOR)
			{
				Rs2Error e;
				float value = (float)*((OniBool*)data);
				rs2_set_option((const rs2_options*)m_sensor, RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE, value, &e);
				if (e.success()) return ONI_STATUS_OK;
			}
			break;
		}

		case ONI_STREAM_PROPERTY_AUTO_EXPOSURE:
		{
			if (data && dataSize == sizeof(OniBool) && m_oniType == ONI_SENSOR_COLOR)
			{
				Rs2Error e;
				float value = (float)*((OniBool*)data);
				rs2_set_option((const rs2_options*)m_sensor, RS2_OPTION_ENABLE_AUTO_EXPOSURE, value, &e);
				if (e.success()) return ONI_STATUS_OK;
			}
			break;
		}

		case ONI_STREAM_PROPERTY_EXPOSURE:
		{
			if (data && dataSize == sizeof(int) && m_oniType == ONI_SENSOR_COLOR)
			{
				Rs2Error e;
				float value = (float)*((int*)data);
				rs2_set_option((const rs2_options*)m_sensor, RS2_OPTION_EXPOSURE, value, &e);
				if (e.success()) return ONI_STATUS_OK;
			}
			break;
		}

		case ONI_STREAM_PROPERTY_GAIN:
		{
			if (data && dataSize == sizeof(int) && m_oniType == ONI_SENSOR_COLOR)
			{
				Rs2Error e;
				float value = (float)*((int*)data);
				rs2_set_option((const rs2_options*)m_sensor, RS2_OPTION_GAIN, value, &e);
				if (e.success()) return ONI_STATUS_OK;
			}
			break;
		}

		case XN_STREAM_PROPERTY_S2D_TABLE:
		{
			if (data && m_oniType == ONI_SENSOR_DEPTH)
			{
				if (setTable(data, dataSize, m_s2d))
				{
					return ONI_STATUS_OK;
				}
			}
			break;
		}

		case XN_STREAM_PROPERTY_D2S_TABLE:
		{
			if (data && m_oniType == ONI_SENSOR_DEPTH)
			{
				if (setTable(data, dataSize, m_d2s))
				{
					return ONI_STATUS_OK;
				}
			}
			break;
		}
		
		case ONI_STREAM_PROPERTY_MIRRORING:
		{
			// Doesn't do anything, but prevents certain 3D scanning applications from crashing.
			return ONI_STATUS_OK;
			break;
		}

		default:
		{
			#if defined(RS2_TRACE_NOT_SUPPORTED_PROPS)
				rsTraceError("Not supported: propertyId=%d", propertyId);
			#endif
			return ONI_STATUS_NOT_SUPPORTED;
		}
	}

	rsTraceError("propertyId=%d dataSize=%d", propertyId, dataSize);
	return ONI_STATUS_ERROR;
}

OniStatus Rs2Stream::getProperty(int propertyId, void* data, int* dataSize)
{
	SCOPED_PROFILER;

	switch (propertyId)
	{
		case ONI_STREAM_PROPERTY_CROPPING:
		{
			if (data && dataSize && *dataSize == sizeof(OniCropping))
			{
				OniCropping value;
				value.enabled = false;
				value.originX = 0;
				value.originY = 0;
				value.width = m_videoMode.resolutionX;
				value.height = m_videoMode.resolutionY;
				*((OniCropping*)data) = value;
				return ONI_STATUS_OK;
			}
			break;
		}

		case ONI_STREAM_PROPERTY_HORIZONTAL_FOV:
		{
			if (data && dataSize && *dataSize == sizeof(float))
			{
				*((float*)data) = m_fovX * 0.01745329251994329576923690768489f;
				return ONI_STATUS_OK;
			}
			break;
		}

		case ONI_STREAM_PROPERTY_VERTICAL_FOV:
		{
			if (data && dataSize && *dataSize == sizeof(float))
			{
				*((float*)data) = m_fovY * 0.01745329251994329576923690768489f;
				return ONI_STATUS_OK;
			}
			break;
		}

		case ONI_STREAM_PROPERTY_VIDEO_MODE:
		{
			if (data && dataSize && *dataSize == sizeof(OniVideoMode))
			{
				*((OniVideoMode*)data) = m_videoMode;
				return ONI_STATUS_OK;
			}
			break;
		}

		case ONI_STREAM_PROPERTY_MAX_VALUE:
		{
			if (data && dataSize && *dataSize == sizeof(int) && m_oniType == ONI_SENSOR_DEPTH)
			{
				*((int*)data) = ONI_MAX_DEPTH;
				return ONI_STATUS_OK;
			}
			break;
		}

		case ONI_STREAM_PROPERTY_MIN_VALUE:
		{
			if (data && dataSize && *dataSize == sizeof(int) && m_oniType == ONI_SENSOR_DEPTH)
			{
				*((int*)data) = 0;
				return ONI_STATUS_OK;
			}
			break;
		}

		case ONI_STREAM_PROPERTY_STRIDE:
		{
			if (data && dataSize && *dataSize == sizeof(int))
			{
				*((int*)data) = m_videoMode.resolutionX * getPixelFormatBytes(convertPixelFormat(m_videoMode.pixelFormat));
				return ONI_STATUS_OK;
			}
			break;
		}

		case ONI_STREAM_PROPERTY_MIRRORING:
		{
			if (data && dataSize && *dataSize == sizeof(OniBool))
			{
				*((OniBool*)data) = false;
				return ONI_STATUS_OK;
			}
			break;
		}

		case ONI_STREAM_PROPERTY_AUTO_WHITE_BALANCE:
		{
			if (data && dataSize && *dataSize == sizeof(OniBool) && m_oniType == ONI_SENSOR_COLOR)
			{
				Rs2Error e;
				float value = rs2_get_option((const rs2_options*)m_sensor, RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE, &e);
				if (e.success())
				{
					*((OniBool*)data) = (int)value ? true : false;
					return ONI_STATUS_OK;
				}
			}
			break;
		}

		case ONI_STREAM_PROPERTY_AUTO_EXPOSURE:
		{
			if (data && dataSize && *dataSize == sizeof(OniBool) && m_oniType == ONI_SENSOR_COLOR)
			{
				Rs2Error e;
				float value = rs2_get_option((const rs2_options*)m_sensor, RS2_OPTION_ENABLE_AUTO_EXPOSURE, &e);
				if (e.success())
				{
					*((OniBool*)data) = (int)value ? true : false;
					return ONI_STATUS_OK;
				}
			}
			break;
		}

		case ONI_STREAM_PROPERTY_EXPOSURE:
		{
			if (data && dataSize && *dataSize == sizeof(int) && m_oniType == ONI_SENSOR_COLOR)
			{
				Rs2Error e;
				float value = rs2_get_option((const rs2_options*)m_sensor, RS2_OPTION_EXPOSURE, &e);
				if (e.success())
				{
					*((int*)data) = (int)value;
					return ONI_STATUS_OK;
				}
			}
			break;
		}

		case ONI_STREAM_PROPERTY_GAIN:
		{
			if (data && dataSize && *dataSize == sizeof(int) && m_oniType == ONI_SENSOR_COLOR)
			{
				Rs2Error e;
				float value = rs2_get_option((const rs2_options*)m_sensor, RS2_OPTION_GAIN, &e);
				if (e.success())
				{
					*((int*)data) = (int)value;
					return ONI_STATUS_OK;
				}
			}
			break;
		}

		case XN_STREAM_PROPERTY_GAIN:
		{
			if (data && dataSize && *dataSize == sizeof(unsigned long long) && m_oniType == ONI_SENSOR_DEPTH)
			{
				*((unsigned long long*)data) = GAIN_VAL;
				return ONI_STATUS_OK;
			}
			break;
		}

        case XN_STREAM_PROPERTY_CONST_SHIFT:
		{
			if (data && dataSize && *dataSize == sizeof(unsigned long long) && m_oniType == ONI_SENSOR_DEPTH)
			{
				*((unsigned long long*)data) = CONST_SHIFT_VAL;
				return ONI_STATUS_OK;
			}
			break;
		}

        case XN_STREAM_PROPERTY_MAX_SHIFT:
		{
			if (data && dataSize && *dataSize == sizeof(unsigned long long) && m_oniType == ONI_SENSOR_DEPTH)
			{
				*((unsigned long long*)data) = MAX_SHIFT_VAL;
				return ONI_STATUS_OK;
			}
			break;
		}

        case XN_STREAM_PROPERTY_PARAM_COEFF:
		{
			if (data && dataSize && *dataSize == sizeof(unsigned long long) && m_oniType == ONI_SENSOR_DEPTH)
			{
				*((unsigned long long*)data) = PARAM_COEFF_VAL;
				return ONI_STATUS_OK;
			}
			break;
		}

        case XN_STREAM_PROPERTY_SHIFT_SCALE:
		{
			if (data && dataSize && *dataSize == sizeof(unsigned long long) && m_oniType == ONI_SENSOR_DEPTH)
			{
				*((unsigned long long*)data) = SHIFT_SCALE_VAL;
				return ONI_STATUS_OK;
			}
			break;
		}

        case XN_STREAM_PROPERTY_ZERO_PLANE_DISTANCE:
		{
			if (data && dataSize && *dataSize == sizeof(unsigned long long) && m_oniType == ONI_SENSOR_DEPTH)
			{
				*((unsigned long long*)data) = ZERO_PLANE_DISTANCE_VAL;
				return ONI_STATUS_OK;
			}
			break;
		}

        case XN_STREAM_PROPERTY_ZERO_PLANE_PIXEL_SIZE:
		{
			if (data && dataSize && *dataSize == sizeof(double) && m_oniType == ONI_SENSOR_DEPTH)
			{
				*((double*)data) = ZERO_PLANE_PIXEL_SIZE_VAL;
				return ONI_STATUS_OK;
			}
			break;
		}

        case XN_STREAM_PROPERTY_EMITTER_DCMOS_DISTANCE:
		{
			if (data && dataSize && *dataSize == sizeof(double) && m_oniType == ONI_SENSOR_DEPTH)
			{
				*((double*)data) = EMITTER_DCMOS_DISTANCE_VAL;
				return ONI_STATUS_OK;
			}
			break;
		}

		case XN_STREAM_PROPERTY_S2D_TABLE:
		{
			if (data && dataSize && m_oniType == ONI_SENSOR_DEPTH)
			{
				if (getTable(data, dataSize, m_s2d))
				{
					return ONI_STATUS_OK;
				}
			}
			break;
		}

		case XN_STREAM_PROPERTY_D2S_TABLE:
		{
			if (data && dataSize && m_oniType == ONI_SENSOR_DEPTH)
			{
				if (getTable(data, dataSize, m_d2s))
				{
					return ONI_STATUS_OK;
				}
			}
			break;
		}

		default:
		{
			#if defined(RS2_TRACE_NOT_SUPPORTED_PROPS)
				rsTraceError("Not supported: propertyId=%d", propertyId);
			#endif
			return ONI_STATUS_NOT_SUPPORTED;
		}
	}

	rsTraceError("propertyId=%d dataSize=%d", propertyId, *dataSize);
	return ONI_STATUS_ERROR;
}

OniBool Rs2Stream::isPropertySupported(int propertyId)
{
	switch (propertyId)
	{
		case ONI_STREAM_PROPERTY_CROPPING:				// OniCropping*
		case ONI_STREAM_PROPERTY_HORIZONTAL_FOV:		// float: radians
		case ONI_STREAM_PROPERTY_VERTICAL_FOV:			// float: radians
		case ONI_STREAM_PROPERTY_VIDEO_MODE:			// OniVideoMode*
		case ONI_STREAM_PROPERTY_MAX_VALUE:				// int
		case ONI_STREAM_PROPERTY_MIN_VALUE:				// int
		case ONI_STREAM_PROPERTY_STRIDE:				// int
			return true;

		
		case ONI_STREAM_PROPERTY_NUMBER_OF_FRAMES:		// int
		case ONI_STREAM_PROPERTY_MIRRORING:				// OniBool
			return false;

		case ONI_STREAM_PROPERTY_AUTO_WHITE_BALANCE:	// OniBool
		case ONI_STREAM_PROPERTY_AUTO_EXPOSURE:			// OniBool
		case ONI_STREAM_PROPERTY_EXPOSURE:				// int
		case ONI_STREAM_PROPERTY_GAIN:					// int
			return true;

		case XN_STREAM_PROPERTY_GAIN:
        case XN_STREAM_PROPERTY_CONST_SHIFT:
        case XN_STREAM_PROPERTY_MAX_SHIFT:
        case XN_STREAM_PROPERTY_PARAM_COEFF:
        case XN_STREAM_PROPERTY_SHIFT_SCALE:
        case XN_STREAM_PROPERTY_ZERO_PLANE_DISTANCE:
        case XN_STREAM_PROPERTY_ZERO_PLANE_PIXEL_SIZE:
        case XN_STREAM_PROPERTY_EMITTER_DCMOS_DISTANCE:
        case XN_STREAM_PROPERTY_S2D_TABLE:
        case XN_STREAM_PROPERTY_D2S_TABLE:
			return true;

		default:
			return false;
	}
}

}} // namespace
