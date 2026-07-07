from fastapi import APIRouter
from pydantic import BaseModel

router = APIRouter(tags=["Health"])

class HealthResponse(BaseModel):
    status: str

@router.get("/health", response_model=HealthResponse)
async def get_health():
    """Simple health check endpoint to verify backend server status."""
    return HealthResponse(status="ok")
