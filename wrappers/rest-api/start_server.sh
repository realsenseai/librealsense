#!/bin/bash

# Start the FastAPI server
# No per-message deflate: compressing every raw frame costs a core and caps the fps (as main.py)
uvicorn main:app --host 0.0.0.0 --port 8000 --ws-per-message-deflate false