#!/bin/sh
# Serve the anim lab on this machine only: http://localhost:8791
exec python3 "$(dirname "$0")/serve.py" 8791
