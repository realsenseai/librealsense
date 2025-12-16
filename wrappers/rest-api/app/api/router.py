from fastapi import APIRouter
from app.api.endpoints import devices, sensors, options, streams, webrtc, point_cloud, firmware

api_router = APIRouter()
# Register firmware routes before devices to avoid conflicts with /{device_id} catch-all
api_router.include_router(firmware.router, prefix="/devices", tags=["firmware"])
api_router.include_router(devices.router, prefix="/devices", tags=["devices"])
api_router.include_router(sensors.router, prefix="/devices/{device_id}/sensors", tags=["sensors"])
api_router.include_router(options.router, prefix="/devices/{device_id}/sensors/{sensor_id}/options", tags=["options"])
api_router.include_router(streams.router, prefix="/devices/{device_id}/stream", tags=["streams"])
api_router.include_router(point_cloud.router, prefix="/devices/{device_id}/point_cloud", tags=["point_cloud"])
api_router.include_router(webrtc.router, prefix="/webrtc", tags=["webrtc"])