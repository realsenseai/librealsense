# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import asyncio
import logging
import uuid
import weakref
import threading
import time
from typing import Dict, List, Optional, Any, Tuple
import numpy as np
import cv2
from aiortc import RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, RTCConfiguration, RTCIceServer
from aiortc.mediastreams import MediaStreamError, VideoStreamTrack, VIDEO_CLOCK_RATE, VIDEO_TIME_BASE
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
        # Not named _start: the VideoStreamTrack base class owns that attribute.
        self._t0 = time.monotonic()
        self._seq = 0
        self._last_pts = -1
        self._last_img = None
        # Set by WebRTCManager; called once when the stream is gone so the
        # session gets closed and the client can observe the disconnect.
        self.on_gone = None

    def _stream_alive(self) -> bool:
        """True while this track's stream is still active on the device."""
        try:
            status = self.realsense_manager.get_stream_status(self.device_id)
        except Exception:
            return False
        return status.is_streaming and any(
            s.lower() == self.stream_type.lower() for s in status.active_streams
        )

    def _notify_gone(self) -> None:
        callback, self.on_gone = self.on_gone, None
        if callback:
            try:
                callback()
            except Exception:
                logging.exception("on_gone callback failed for %s", self.stream_type)

    def _next_pts(self) -> int:
        """Timestamp from the frame's real arrival time, so the receiver's
        jitter buffer stays small and any camera frame rate is supported."""
        pts = int((time.monotonic() - self._t0) * VIDEO_CLOCK_RATE)
        if pts <= self._last_pts:
            pts = self._last_pts + 1
        self._last_pts = pts
        return pts

    def _to_video_frame(self, img) -> VideoFrame:
        video_frame = VideoFrame.from_ndarray(img, format="rgb24")
        video_frame.pts = self._next_pts()
        video_frame.time_base = VIDEO_TIME_BASE
        return video_frame

    async def recv(self):
        if self.readyState != "live":
            raise MediaStreamError
        try:
            # Await frame arrival on the loop (event-based, no executor thread).
            # A slow consumer wakes to the newest frame, so backlog costs fps,
            # never latency.
            frame_data, self._seq = await self.realsense_manager.wait_for_frame_after(
                self.device_id,
                self.stream_type,
                self._seq,
            )

            if frame_data is None:
                # Gap vs gone: repeat the last frame through transient gaps
                # (USB hiccup, hardware reset) for as long as the stream is
                # active; end the track only when it is genuinely stopped.
                if not self._stream_alive():
                    raise MediaStreamError
                if self._last_img is None:
                    # Stream up but first frame not here yet — black keepalive.
                    return self._to_video_frame(np.zeros((480, 640, 3), dtype=np.uint8))
                return self._to_video_frame(self._last_img)

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
                # Already has 3 channels - RealSense colorizer outputs RGB format
                # Use directly without conversion to preserve color scheme (blue=near, red=far)
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

            self._last_img = img
            return self._to_video_frame(img)
        except MediaStreamError:
            # End-of-track: aiortc's sender loop stops on this exception, but
            # the peer connection would stay "connected" — close the session
            # too so the client can observe the disconnect.
            self._notify_gone()
            raise
        except Exception as e:
            status_code = getattr(e, 'status_code', None)
            if status_code == 400:
                # Stream stopped (get_latest_frame: "not streaming") — end the
                # track instead of freezing on a stale image.
                self._notify_gone()
                raise MediaStreamError from e
            # On error, return a black frame. Pace it: without a new frame to
            # wait on, an un-delayed error path would spin the event loop.
            await asyncio.sleep(1 / 30)
            width, height = 640, 480  # Default size
            img = np.zeros((height, width, 3), dtype=np.uint8)
            video_frame = self._to_video_frame(img)

            # Only log non-503 errors (503 = frames not yet available, which is normal briefly)
            error_detail = getattr(e, 'detail', str(e))
            if status_code != 503:
                logging.exception("Error getting frame for %s: %s", self.stream_type, error_detail)
            return video_frame

class WebRTCManager:
    def __init__(self, realsense_manager):
        self.realsense_manager = realsense_manager
        self.sessions: Dict[str, Dict[str, Any]] = {}
        self.lock = asyncio.Lock()
        self._reaper: Optional[asyncio.Task] = None
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
            # When the stream is gone, close the whole session so the client
            # sees the peer connection drop instead of a frozen tile.
            video_track.on_gone = lambda sid=session_id: asyncio.create_task(self.close_session(sid))
            pc.addTrack(video_track)

        # A browser that navigated away, reloaded or crashed never closes its session, and
        # every one left behind keeps encoding frames on the event loop. Drop it as soon as
        # the connection reports it is gone.
        @pc.on("connectionstatechange")
        async def _on_connection_state(sid=session_id, peer=pc):
            logging.info("WebRTC session %s is %s", sid[:8], peer.connectionState)
            if peer.connectionState in ("failed", "closed", "disconnected"):
                await self.close_session(sid)

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

        # Sweep now, and keep sweeping while this process lives
        asyncio.create_task(self._cleanup_sessions())
        if self._reaper is None or self._reaper.done():
            self._reaper = asyncio.create_task(self._reap_forever())

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
        """Close a WebRTC session. The peer is closed outside the lock: closing it fires the
        state-change handler, which calls back in here and would deadlock on a held lock."""
        async with self.lock:
            session = self.sessions.pop(session_id, None)
        if session is None:
            return False
        try:
            await session["pc"].close()
        except Exception:
            pass
        return True

    # An offer nobody answers (the page was closed mid-negotiation) is dead this long after
    UNANSWERED_SESSION_S = 30

    async def _cleanup_sessions(self):
        """Close sessions whose peer is gone, or that were never answered, and very old ones."""
        doomed = []
        async with self.lock:
            now = time.time()
            for session_id, session in list(self.sessions.items()):
                state = getattr(session["pc"], "connectionState", None)
                age = now - session["created_at"]
                dead = state in ("failed", "closed")
                never_answered = not session["connected"] and age > self.UNANSWERED_SESSION_S
                if dead or never_answered or age > 3600:
                    logging.info("Dropping WebRTC session %s (state=%s, connected=%s, age=%.0fs)",
                                 session_id[:8], state, session["connected"], age)
                    doomed.append(self.sessions.pop(session_id))
        for session in doomed:
            try:
                await session["pc"].close()
            except Exception:
                pass

    async def _reap_forever(self, every_s: float = 15.0):
        """A server that runs for hours never creates a session at the moment an old one dies,
        so the sweep cannot wait for the next offer."""
        while True:
            await asyncio.sleep(every_s)
            try:
                await self._cleanup_sessions()
            except Exception as exc:
                logging.warning("WebRTC session sweep failed: %s", exc)