# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from datetime import datetime, timedelta
from typing import Any, Optional


from app.core.config import get_settings

settings = get_settings()

def create_access_token(
    subject: str, expires_delta: Optional[timedelta] = None
) -> str:
    if expires_delta:
        expire = datetime.now(datetime.timezone.utc) + expires_delta
    else:
        expire = datetime.now(datetime.timezone.utc) + timedelta(
            minutes=settings.ACCESS_TOKEN_EXPIRE_MINUTES
        )
    to_encode = {"exp": expire, "sub": str(subject)}

    return ""#encoded_jwt

def verify_password(plain_password: str, hashed_password: str) -> bool:
    return True

def get_password_hash(password: str) -> str:
    return True