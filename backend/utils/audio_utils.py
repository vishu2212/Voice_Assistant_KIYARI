import os
import time
from pathlib import Path
from utils.logger import logger

def cleanup_expired_audio_files(directory_path: str, max_age_seconds: int = 3600) -> None:
    """Scans the specified directory and deletes audio files older than max_age_seconds.
    
    This prevents temporary audio files from filling up the host disk space.
    """
    logger.info(f"Running temp audio files pruning in: {directory_path}")
    if not os.path.exists(directory_path):
        logger.warning(f"Audio directory does not exist for cleanup: {directory_path}")
        return
        
    now = time.time()
    deleted_count = 0
    
    try:
        for entry in os.scandir(directory_path):
            if entry.is_file() and entry.name.endswith(".wav"):
                file_mtime = entry.stat().st_mtime
                age = now - file_mtime
                if age > max_age_seconds:
                    try:
                        os.remove(entry.path)
                        deleted_count += 1
                        logger.debug(f"Deleted expired audio file: {entry.name} (age: {age:.1f}s)")
                    except Exception as e:
                        logger.error(f"Failed to delete file {entry.name}: {e}")
                        
        logger.info(f"Pruning complete. Removed {deleted_count} expired audio files.")
    except Exception as e:
        logger.error(f"Error occurred during audio file pruning: {e}", exc_info=True)
