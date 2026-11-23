# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging

from fastapi import FastAPI, Request, status
from fastapi.responses import JSONResponse

def setup_exception_handlers(app: FastAPI):
    # A name, ID or index the path gives that is not there: a device, a sensor, an option, a filter
    @app.exception_handler(LookupError)
    async def not_found_handler(request: Request, exc: LookupError):
        logging.warning("%s %s: not found: %s", request.method, request.url.path, exc)
        return JSONResponse(
            status_code=status.HTTP_404_NOT_FOUND,
            content={"detail": f"Not found: {exc.args[0]}"},
        )

    # pyrealsense2 raises SDK errors as RuntimeError: they are refused requests, with the SDK's own message
    @app.exception_handler(RuntimeError)
    async def sdk_exception_handler(request: Request, exc: RuntimeError):
        logging.warning("%s %s: %s", request.method, request.url.path, exc)
        return JSONResponse(
            status_code=status.HTTP_400_BAD_REQUEST,
            content={"detail": str(exc)},
        )

    @app.exception_handler(Exception)
    async def generic_exception_handler(request: Request, exc: Exception):
        return JSONResponse(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            content={"detail": f"An unexpected error occurred: {str(exc)}"},
        )