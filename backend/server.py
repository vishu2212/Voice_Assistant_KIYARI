import asyncio
from contextlib import asynccontextmanager
import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from config import settings
from routes import health, chat
from utils.logger import logger
from utils.audio_utils import cleanup_expired_audio_files

from fastapi.staticfiles import StaticFiles

async def periodic_audio_cleanup():
    """Runs a background loop to prune old audio files from cache every 5 minutes."""
    logger.info("Background temp audio cleanup loop started.")
    while True:
        try:
            # Check for files to cleanup every 5 minutes (300 seconds)
            await asyncio.sleep(300)
            cleanup_expired_audio_files(str(settings.TEMP_AUDIO_DIR), max_age_seconds=300)
        except asyncio.CancelledError:
            logger.info("Background temp audio cleanup loop cancelled.")
            break
        except Exception as e:
            logger.error(f"Error in periodic cleanup task: {e}", exc_info=True)

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup actions
    logger.info("Initializing KIYARI Backend Application Lifespan...")
    logger.info(f"Target Settings: HOST={settings.HOST}, PORT={settings.PORT}")
    logger.info(f"LM Studio URL: {settings.LM_STUDIO_URL}")
    
    # Run an initial clean to sweep any legacy temp files left from a previous crash
    cleanup_expired_audio_files(str(settings.TEMP_AUDIO_DIR), max_age_seconds=0)
    
    # Start background loop task
    cleanup_task = asyncio.create_task(periodic_audio_cleanup())
    
    yield
    
    # Shutdown actions
    logger.info("Shutting down KIYARI Backend Application Lifespan...")
    cleanup_task.cancel()
    # Wait until cleanup task exits cleanly
    try:
        await cleanup_task
    except asyncio.CancelledError:
        pass

# Initialize FastAPI with lifespan context
app = FastAPI(
    title="KIYARI Voice Assistant Backend",
    description="Offline voice assistant service pipeline for ESP32-S3",
    version="1.0.0",
    lifespan=lifespan
)

app.mount("/temp", StaticFiles(directory=str(settings.TEMP_DIR)), name="temp")

# CORS configurations for developer dashboards/testing
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include endpoint routes
app.include_router(health.router)
app.include_router(chat.router)

if __name__ == "__main__":
    # Start uvicorn server programmatically if file executed directly
    uvicorn.run("server:app", host=settings.HOST, port=settings.PORT, reload=False)
