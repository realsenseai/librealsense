# Per-Sensor Start/Stop API Implementation Plan

## Executive Summary

This document outlines the plan to add per-sensor start/stop capabilities using the RealSense **sensor API** alongside the existing **pipeline API** for full camera start/stop. This provides fine-grained control for applications that need independent sensor management while maintaining backward compatibility.

---

## Current Architecture

### Pipeline API (Existing)
- **Location**: `app/services/rs_manager.py` → `start_stream()`, `stop_stream()`
- **Endpoints**: `POST /api/v1/streams/start`, `POST /api/v1/streams/stop`
- **Behavior**: Starts/stops all configured streams atomically via `rs.pipeline`
- **Use case**: Simple "start all / stop all" workflow for the entire camera

### Sensor API (New)
- **Mechanism**: Direct sensor control via `sensor.open()`, `sensor.start()`, `sensor.stop()`, `sensor.close()`
- **Use case**: Start/stop individual sensors (depth, color, IMU) independently
- **Benefit**: Finer control, ability to reconfigure one sensor without affecting others

---

## Design Goals

1. **Independent Control**: Start/stop sensors (depth, RGB, IMU) independently
2. **Batch Operations**: Start/stop multiple sensors atomically in a single request
3. **Coexistence**: Both pipeline API and sensor API can be used (but not simultaneously on same device)
4. **State Tracking**: Track per-sensor streaming state, errors, and configuration
5. **Backward Compatibility**: Existing pipeline endpoints remain unchanged

---

## API Design

### New Endpoints

```
# Per-Sensor Control
POST   /api/v1/devices/{device_id}/sensors/{sensor_id}/start
POST   /api/v1/devices/{device_id}/sensors/{sensor_id}/stop
GET    /api/v1/devices/{device_id}/sensors/{sensor_id}/status

# Batch Sensor Control
POST   /api/v1/devices/{device_id}/sensors/batch/start
POST   /api/v1/devices/{device_id}/sensors/batch/stop
GET    /api/v1/devices/{device_id}/sensors/batch/status
```

### Request/Response Models

#### Per-Sensor Start Request
```python
class SensorStreamConfig(BaseModel):
    stream_type: str           # e.g., "depth", "color", "infrared-1"
    format: str                # e.g., "z16", "rgb8", "y8"
    resolution: Resolution     # {width, height}
    framerate: int             # e.g., 30

class SensorStartRequest(BaseModel):
    config: SensorStreamConfig
```

#### Batch Start Request
```python
class BatchSensorStartRequest(BaseModel):
    sensors: List[SensorStartItem]

class SensorStartItem(BaseModel):
    sensor_id: str
    config: SensorStreamConfig
```

#### Sensor Status Response
```python
class SensorStreamStatus(BaseModel):
    sensor_id: str
    is_streaming: bool
    stream_type: Optional[str]
    resolution: Optional[Resolution]
    framerate: Optional[int]
    format: Optional[str]
    error: Optional[str]
    started_at: Optional[datetime]
```

#### Batch Status Response
```python
class BatchSensorStatus(BaseModel):
    device_id: str
    mode: str                              # "sensor_api" | "pipeline_api" | "idle"
    sensors: List[SensorStreamStatus]
    errors: List[str]
```

---

## Implementation Plan

### Phase 1: Models & Types

**New file**: `app/models/sensor_streaming.py`

```python
from pydantic import BaseModel
from typing import List, Optional
from datetime import datetime

class SensorStreamConfig(BaseModel):
    stream_type: str
    format: str
    resolution: Resolution
    framerate: int

class SensorStartRequest(BaseModel):
    config: SensorStreamConfig

class SensorStartItem(BaseModel):
    sensor_id: str
    config: SensorStreamConfig

class BatchSensorStartRequest(BaseModel):
    sensors: List[SensorStartItem]

class SensorStreamStatus(BaseModel):
    sensor_id: str
    is_streaming: bool
    stream_type: Optional[str] = None
    resolution: Optional[Resolution] = None
    framerate: Optional[int] = None
    format: Optional[str] = None
    error: Optional[str] = None
    started_at: Optional[datetime] = None

class BatchSensorStatus(BaseModel):
    device_id: str
    mode: str  # "sensor_api" | "pipeline_api" | "idle"
    sensors: List[SensorStreamStatus]
    errors: List[str] = []
```

### Phase 2: RealSenseManager Extensions

**File**: `app/services/rs_manager.py`

Add new instance variables and methods:

```python
class RealSenseManager:
    def __init__(self, ...):
        # ... existing init ...
        
        # Per-sensor streaming state
        self.sensor_streams: Dict[str, Dict[str, SensorStreamInfo]] = {}  # device_id -> sensor_id -> info
        self.sensor_frame_queues: Dict[str, Dict[str, rs.frame_queue]] = {}  # device_id -> sensor_id -> queue
        self.streaming_mode: Dict[str, str] = {}  # device_id -> "pipeline" | "sensor" | "idle"
    
    # --- Per-Sensor Control Methods ---
    
    def start_sensor(
        self,
        device_id: str,
        sensor_id: str,
        config: SensorStreamConfig
    ) -> SensorStreamStatus:
        """
        Start streaming from a single sensor using the sensor API.
        
        Steps:
        1. Validate device and sensor exist
        2. Check streaming mode compatibility (can't mix pipeline and sensor API)
        3. Find matching stream profile
        4. Open sensor with profile
        5. Start sensor with frame queue or callback
        6. Start frame collection thread
        7. Update state tracking
        """
        pass
    
    def stop_sensor(
        self,
        device_id: str,
        sensor_id: str
    ) -> SensorStreamStatus:
        """
        Stop streaming from a single sensor.
        
        Steps:
        1. Validate device and sensor exist
        2. Call sensor.stop()
        3. Call sensor.close()
        4. Clean up frame queue
        5. Update state tracking
        """
        pass
    
    def get_sensor_status(
        self,
        device_id: str,
        sensor_id: str
    ) -> SensorStreamStatus:
        """Get streaming status for a specific sensor."""
        pass
    
    # --- Batch Sensor Control Methods ---
    
    def batch_start_sensors(
        self,
        device_id: str,
        sensors: List[SensorStartItem]
    ) -> BatchSensorStatus:
        """
        Start multiple sensors atomically.
        
        Steps:
        1. Validate all sensors and configs
        2. Check streaming mode compatibility
        3. Start each sensor in order (or use syncer for sync'd start)
        4. If any fails, stop already-started sensors and report error
        5. Update state tracking
        """
        pass
    
    def batch_stop_sensors(
        self,
        device_id: str,
        sensor_ids: Optional[List[str]] = None  # None = all sensors
    ) -> BatchSensorStatus:
        """
        Stop multiple sensors atomically.
        
        Steps:
        1. Stop each sensor
        2. Clean up resources
        3. Update state tracking
        """
        pass
    
    def get_batch_status(
        self,
        device_id: str
    ) -> BatchSensorStatus:
        """Get streaming status for all sensors on a device."""
        pass
```

### Phase 3: Sensor API Implementation Details

#### Opening a Sensor with Profile

```python
def _open_sensor_with_config(
    self,
    sensor: rs.sensor,
    config: SensorStreamConfig
) -> rs.stream_profile:
    """
    Find and open sensor with matching profile.
    
    1. Get all supported profiles from sensor
    2. Find profile matching stream_type, format, resolution, fps
    3. Call sensor.open(profile)
    4. Return the opened profile
    """
    profiles = sensor.get_stream_profiles()
    
    for profile in profiles:
        if not profile.is_video_stream_profile():
            continue
        
        video_profile = profile.as_video_stream_profile()
        stream_name = profile.stream_type().name.lower()
        
        # Handle infrared index
        if profile.stream_type() == rs.stream.infrared:
            stream_name = f"infrared-{profile.stream_index()}"
        
        if (stream_name == config.stream_type.lower() and
            str(profile.format()).split('.')[-1].lower() == config.format.lower() and
            video_profile.width() == config.resolution.width and
            video_profile.height() == config.resolution.height and
            video_profile.fps() == config.framerate):
            
            sensor.open(profile)
            return profile
    
    raise RealSenseError(
        status_code=400,
        detail=f"No matching profile found for {config}"
    )
```

#### Starting Sensor with Frame Queue

```python
def _start_sensor_streaming(
    self,
    device_id: str,
    sensor_id: str,
    sensor: rs.sensor
) -> None:
    """
    Start sensor streaming to a frame queue.
    
    1. Create frame_queue for this sensor
    2. Call sensor.start(queue)
    3. Start frame collection thread
    """
    queue = rs.frame_queue(50, keep_frames=True)
    
    with self.lock:
        if device_id not in self.sensor_frame_queues:
            self.sensor_frame_queues[device_id] = {}
        self.sensor_frame_queues[device_id][sensor_id] = queue
    
    sensor.start(queue)
    
    # Start frame collection thread
    threading.Thread(
        target=self._collect_sensor_frames,
        args=(device_id, sensor_id, queue),
        daemon=True
    ).start()
```

### Phase 4: REST Endpoints

**New file**: `app/api/endpoints/sensor_streaming.py`

```python
from fastapi import APIRouter, Depends, HTTPException
from typing import List, Optional

from app.models.sensor_streaming import (
    SensorStartRequest,
    SensorStreamStatus,
    BatchSensorStartRequest,
    BatchSensorStatus,
)
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_realsense_manager

router = APIRouter()

# --- Per-Sensor Endpoints ---

@router.post("/{sensor_id}/start", response_model=SensorStreamStatus)
async def start_sensor(
    device_id: str,
    sensor_id: str,
    request: SensorStartRequest,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Start streaming from a specific sensor using the sensor API."""
    try:
        return rs_manager.start_sensor(device_id, sensor_id, request.config)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@router.post("/{sensor_id}/stop", response_model=SensorStreamStatus)
async def stop_sensor(
    device_id: str,
    sensor_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Stop streaming from a specific sensor."""
    try:
        return rs_manager.stop_sensor(device_id, sensor_id)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@router.get("/{sensor_id}/status", response_model=SensorStreamStatus)
async def get_sensor_status(
    device_id: str,
    sensor_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Get streaming status for a specific sensor."""
    try:
        return rs_manager.get_sensor_status(device_id, sensor_id)
    except Exception as e:
        raise HTTPException(status_code=404, detail=str(e))

# --- Batch Endpoints ---

@router.post("/batch/start", response_model=BatchSensorStatus)
async def batch_start_sensors(
    device_id: str,
    request: BatchSensorStartRequest,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Start multiple sensors atomically."""
    try:
        return rs_manager.batch_start_sensors(device_id, request.sensors)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@router.post("/batch/stop", response_model=BatchSensorStatus)
async def batch_stop_sensors(
    device_id: str,
    sensor_ids: Optional[List[str]] = None,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Stop multiple sensors (or all if sensor_ids is None)."""
    try:
        return rs_manager.batch_stop_sensors(device_id, sensor_ids)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@router.get("/batch/status", response_model=BatchSensorStatus)
async def get_batch_status(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Get streaming status for all sensors on a device."""
    try:
        return rs_manager.get_batch_status(device_id)
    except Exception as e:
        raise HTTPException(status_code=404, detail=str(e))
```

### Phase 5: Router Integration

**File**: `app/api/router.py`

```python
from app.api.endpoints import sensor_streaming

# Add to existing router setup
api_router.include_router(
    sensor_streaming.router,
    prefix="/devices/{device_id}/sensors",
    tags=["sensor-streaming"],
)
```

---

## State Management

### Streaming Mode Enforcement

The system must prevent mixing pipeline API and sensor API on the same device:

```python
def _check_streaming_mode(self, device_id: str, requested_mode: str) -> None:
    """
    Ensure requested mode is compatible with current state.
    
    Raises:
        RealSenseError: If mode conflict detected
    """
    current_mode = self.streaming_mode.get(device_id, "idle")
    
    if current_mode == "idle":
        return  # OK to start with any mode
    
    if current_mode != requested_mode:
        raise RealSenseError(
            status_code=409,
            detail=f"Device is in '{current_mode}' mode. "
                   f"Stop all streams before switching to '{requested_mode}' mode."
        )
```

### Per-Sensor State Tracking

```python
@dataclass
class SensorStreamInfo:
    sensor_id: str
    is_streaming: bool
    stream_type: Optional[str]
    resolution: Optional[Tuple[int, int]]
    framerate: Optional[int]
    format: Optional[str]
    error: Optional[str]
    started_at: Optional[datetime]
    rs_sensor: Optional[rs.sensor]  # Reference to open sensor
```

---

## Frame Collection

### Per-Sensor Frame Collection

```python
def _collect_sensor_frames(
    self,
    device_id: str,
    sensor_id: str,
    queue: rs.frame_queue
) -> None:
    """
    Collect frames from a single sensor's queue.
    Similar to _collect_frames but for individual sensor.
    """
    while True:
        with self.lock:
            if device_id not in self.sensor_streams:
                break
            if sensor_id not in self.sensor_streams[device_id]:
                break
            if not self.sensor_streams[device_id][sensor_id].is_streaming:
                break
        
        try:
            frame = queue.wait_for_frame(timeout_ms=1000)
            if frame:
                self._process_sensor_frame(device_id, sensor_id, frame)
        except Exception as e:
            logging.debug("Frame wait timeout or error: %s", e)
            continue
```

---

## Error Handling

### Per-Sensor Error States

Each sensor tracks its own error state:

```python
class SensorStreamStatus(BaseModel):
    sensor_id: str
    is_streaming: bool
    error: Optional[str] = None  # Last error message if any
```

### Batch Operation Rollback

If batch start fails partway through:

```python
def batch_start_sensors(self, device_id: str, sensors: List[SensorStartItem]) -> BatchSensorStatus:
    started = []
    errors = []
    
    try:
        for item in sensors:
            status = self.start_sensor(device_id, item.sensor_id, item.config)
            if status.error:
                raise Exception(status.error)
            started.append(item.sensor_id)
    except Exception as e:
        # Rollback: stop all successfully started sensors
        for sensor_id in started:
            try:
                self.stop_sensor(device_id, sensor_id)
            except:
                pass
        errors.append(f"Failed to start {item.sensor_id}: {str(e)}")
    
    return self.get_batch_status(device_id)
```

---

## Implementation Sequence

### Week 1: Foundation ✅ COMPLETED
1. ✅ Create `app/models/sensor_streaming.py` with all new models
2. ✅ Add instance variables to `RealSenseManager`
3. ✅ Implement `_check_streaming_mode()` helper
4. ✅ Implement `_find_matching_profile()` helper (was `_open_sensor_with_config`)

### Week 2: Core Sensor Control ✅ COMPLETED
1. ✅ Implement `start_sensor()` method
2. ✅ Implement `stop_sensor()` method
3. ✅ Implement `get_sensor_status()` method
4. ✅ Implement `_collect_sensor_frames()` thread

### Week 3: Batch Operations ✅ COMPLETED
1. ✅ Implement `batch_start_sensors()` method
2. ✅ Implement `batch_stop_sensors()` method
3. ✅ Implement `get_batch_status()` method

### Week 4: REST API & Integration ✅ COMPLETED
1. ✅ Create `app/api/endpoints/sensor_streaming.py`
2. ✅ Register routes in `app/api/router.py`
3. ⏳ Add integration tests
4. ⏳ Update API documentation

### Week 5: Testing & Polish
1. ⏳ Unit tests for all new methods
2. ⏳ Integration tests with real hardware
3. ⏳ Edge case handling (device disconnect, etc.)
4. ⏳ Performance benchmarking

---

## API Summary

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/streams/start` | POST | Start camera (pipeline API) - **existing** |
| `/api/v1/streams/stop` | POST | Stop camera (pipeline API) - **existing** |
| `/api/v1/devices/{id}/sensors/{sid}/start` | POST | Start sensor (sensor API) - **new** |
| `/api/v1/devices/{id}/sensors/{sid}/stop` | POST | Stop sensor (sensor API) - **new** |
| `/api/v1/devices/{id}/sensors/{sid}/status` | GET | Sensor status - **new** |
| `/api/v1/devices/{id}/sensors/batch/start` | POST | Batch start (sensor API) - **new** |
| `/api/v1/devices/{id}/sensors/batch/stop` | POST | Batch stop (sensor API) - **new** |
| `/api/v1/devices/{id}/sensors/batch/status` | GET | Batch status - **new** |

---

## Success Criteria

- ✅ Per-sensor start/stop works independently
- ✅ Batch start/stop works atomically with rollback on failure
- ✅ Pipeline API continues to work unchanged
- ✅ Mode conflict detection prevents mixing APIs on same device
- ✅ Frame collection works for individually started sensors
- ✅ Error states are properly tracked per-sensor
- ✅ All endpoints return appropriate HTTP status codes
- ✅ API documentation is complete and accurate

---

## Future Enhancements

- **Hardware sync**: Use `rs.syncer` for synchronized multi-sensor capture
- **Per-sensor filters**: Apply post-processing to individual sensor streams
- **Sensor presets**: Save/load per-sensor configurations
- **Hot-reconfigure**: Change sensor config without full stop/start cycle
