import asyncio
import uuid
import weakref
import threading
import time
from typing import Dict, List, Optional, Any, Tuple
import numpy as np
import cv2
from aiortc import RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, RTCConfiguration, RTCIceServer
from aiortc.mediastreams import VideoStreamTrack
from av import VideoFrame
from app.core.errors import RealSenseError
from app.core.config import get_settings
from app.models.webrtc import WebRTCSession, WebRTCStatus

class RealSenseVideoTrack(VideoStreamTrack):
    """Video track that captures frames from RealSense camera."""

    def __init__(self, realsense_manager, device_id, stream_type):
        super().__init__()
        self.realsense_manager = realsense_manager
        self.device_id = device_id
        self.stream_type = stream_type
        self._start = time.time()

    async def recv(self):
        try:
            # Get frame from RealSense
            frame_data = self.realsense_manager.get_latest_frame(self.device_id, self.stream_type)

            # Handle different data types and normalize to uint8
            if frame_data.dtype == np.uint16:
                # For uint16 data (IR Y16), use fast bit-shift normalization
                # This assumes 10-bit or 12-bit data in 16-bit container
                # Right shift by 8 gives good visualization for most IR sensors
                frame_data = (frame_data >> 6).clip(0, 255).astype(np.uint8)
            elif frame_data.dtype != np.uint8:
                # Convert other dtypes to uint8
                frame_data = frame_data.astype(np.uint8)

            # Convert to RGB format if necessary
            if len(frame_data.shape) == 3 and frame_data.shape[2] == 3:
                # Already has 3 channels - could be RGB or BGR
                # RealSense colorizer outputs BGR, convert to RGB
                if self.stream_type.startswith('depth'):
                    img = cv2.cvtColor(frame_data, cv2.COLOR_BGR2RGB)
                else:
                    img = frame_data
            elif len(frame_data.shape) == 3 and frame_data.shape[2] == 4:
                # RGBA/BGRA - convert to RGB
                img = cv2.cvtColor(frame_data, cv2.COLOR_BGRA2RGB)
            elif len(frame_data.shape) == 2:
                # Grayscale (e.g., infrared Y8/Y16) - convert to RGB
                img = cv2.cvtColor(frame_data, cv2.COLOR_GRAY2RGB)
            else:
                # Unknown format, try to use as-is
                img = frame_data
                
            # Create VideoFrame
            video_frame = VideoFrame.from_ndarray(img, format="rgb24")

            # Set frame timestamp
            pts, time_base = await self.next_timestamp()
            video_frame.pts = pts
            video_frame.time_base = time_base

            return video_frame
        except Exception as e:
            # On error, return a black frame
            width, height = 640, 480  # Default size
            img = np.zeros((height, width, 3), dtype=np.uint8)
            video_frame = VideoFrame.from_ndarray(img, format="rgb24")
            pts, time_base = await self.next_timestamp()
            video_frame.pts = pts
            video_frame.time_base = time_base

            import traceback
            error_detail = getattr(e, 'detail', str(e))
            print(f"Error getting frame for {self.stream_type}: {error_detail}")
            traceback.print_exc()
            return video_frame

class WebRTCManager:
    def __init__(self, realsense_manager):
        self.realsense_manager = realsense_manager
        self.sessions: Dict[str, Dict[str, Any]] = {}
        self.lock = asyncio.Lock()
        self.settings = get_settings()

        # Set up ICE servers for WebRTC
        self.ice_servers = []

        if self.settings.STUN_SERVER:
            self.ice_servers.append(RTCIceServer(urls=self.settings.STUN_SERVER))

        if self.settings.TURN_SERVER:
            self.ice_servers.append(
                RTCIceServer(
                    urls=self.settings.TURN_SERVER,
                    username=self.settings.TURN_USERNAME,
                    credential=self.settings.TURN_PASSWORD
                )
            )

    async def create_offer(self, device_id: str, stream_types: List[str]) -> Tuple[str, dict]:
        """Create a WebRTC offer for device streams."""
        # Verify device exists and is streaming
        stream_status = self.realsense_manager.get_stream_status(device_id)
        if not stream_status.is_streaming:
            raise RealSenseError(status_code=400, detail=f"Device {device_id} is not streaming")

        # Verify requested stream types are available
        for stream_type in stream_types:
            if stream_type not in stream_status.active_streams:
                raise RealSenseError(status_code=400, detail=f"Stream type {stream_type} is not active")

        # Create peer connection
        pc = RTCPeerConnection(RTCConfiguration(iceServers=self.ice_servers))

        # Create session ID
        session_id = str(uuid.uuid4())

        # Add video tracks for each stream type
        for stream_type in stream_types:
            video_track = RealSenseVideoTrack(self.realsense_manager, device_id, stream_type)
            pc.addTrack(video_track)

        # Create offer
        offer = await pc.createOffer()
        await pc.setLocalDescription(offer)

        # Store session
        async with self.lock:
            self.sessions[session_id] = {
                "device_id": device_id,
                "stream_types": stream_types,
                "pc": pc,
                "connected": False,
                "created_at": time.time()
            }

        # Schedule cleanup of unused sessions
        asyncio.create_task(self._cleanup_sessions())

        # Return session ID and offer
        return session_id, {
            "sdp": pc.localDescription.sdp,
            "type": pc.localDescription.type
        }

    async def process_answer(self, session_id: str, sdp: str, type_: str) -> bool:
        """Process a WebRTC answer."""
        async with self.lock:
            if session_id not in self.sessions:
                raise RealSenseError(status_code=404, detail=f"Session {session_id} not found")

            pc = self.sessions[session_id]["pc"]

        # Set remote description
        try:
            await pc.setRemoteDescription(RTCSessionDescription(sdp=sdp, type=type_))

            # Mark as connected
            async with self.lock:
                self.sessions[session_id]["connected"] = True

            return True
        except Exception as e:
            raise RealSenseError(status_code=400, detail=f"Error processing answer: {str(e)}")

    async def add_ice_candidate(self, session_id: str, candidate: str, sdp_mid: str, sdp_mline_index: int) -> bool:
        """Add an ICE candidate to a session."""
        async with self.lock:
            if session_id not in self.sessions:
                raise RealSenseError(status_code=404, detail=f"Session {session_id} not found")

            pc = self.sessions[session_id]["pc"]

        # Add ICE candidate
        try:
            candidate_obj = RTCIceCandidate(
                component=1,
                foundation="0",
                ip="0.0.0.0",
                port=0,
                priority=0,
                protocol="udp",
                type="host",
                sdpMid=sdp_mid,
                sdpMLineIndex=sdp_mline_index
            )
            candidate_obj.candidate = candidate

            await pc.addIceCandidate(candidate_obj)
            return True
        except Exception as e:
            raise RealSenseError(status_code=400, detail=f"Error adding ICE candidate: {str(e)}")

    async def get_ice_candidates(self, session_id: str) -> List[dict]:
        """Get ICE candidates for a session."""
        async with self.lock:
            if session_id not in self.sessions:
                raise RealSenseError(status_code=404, detail=f"Session {session_id} not found")

            pc = self.sessions[session_id]["pc"]

        # ICE candidates would be sent via events in a real application
        # This is a placeholder for the API
        return []

    async def get_session(self, session_id: str) -> WebRTCStatus:
        """Get session status."""
        async with self.lock:
            if session_id not in self.sessions:
                raise RealSenseError(status_code=404, detail=f"Session {session_id} not found")

            session = self.sessions[session_id]
            pc = session["pc"]

        # Get WebRTC stats (if available)
        stats = None
        try:
            stats_dict = await pc.getStats()
            stats = {k: v.__dict__ for k, v in stats_dict.items()}
        except Exception:
            stats = None

        # Return session status
        return WebRTCStatus(
            session_id=session_id,
            device_id=session["device_id"],
            connected=session["connected"],
            streaming=session["connected"],
            stream_types=session["stream_types"],
            stats=stats
        )

    async def close_session(self, session_id: str) -> bool:
        """Close a WebRTC session."""
        async with self.lock:
            if session_id not in self.sessions:
                return False

            # Close peer connection
            try:
                await self.sessions[session_id]["pc"].close()
            except Exception:
                pass

            # Remove session
            del self.sessions[session_id]
            return True

    async def _cleanup_sessions(self):
        """Clean up old or disconnected sessions."""
        async with self.lock:
            now = time.time()
            session_ids = list(self.sessions.keys())

            for session_id in session_ids:
                session = self.sessions[session_id]

                # Remove sessions older than 1 hour
                if now - session["created_at"] > 3600:
                    try:
                        await session["pc"].close()
                    except Exception:
                        pass

                    del self.sessions[session_id]