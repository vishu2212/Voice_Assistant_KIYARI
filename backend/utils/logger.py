import logging
import os
import sys
from logging.handlers import RotatingFileHandler
from pathlib import Path

# Setup logs directory
LOG_DIR = Path(__file__).resolve().parent.parent / "temp"
os.makedirs(LOG_DIR, exist_ok=True)
LOG_FILE = LOG_DIR / "zyra.log"

def setup_logger(name: str = "zyra") -> logging.Logger:
    """Configures and returns a rotating logger that prints to console and file."""
    # Reconfigure stdout/stderr for UTF-8 on Windows
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except AttributeError:
        pass
        
    logger = logging.getLogger(name)
    
    # If logger is already configured, return it
    if logger.handlers:
        return logger

    logger.setLevel(logging.DEBUG)
    
    # Format pattern
    log_format = logging.Formatter(
        "[%(asctime)s] [%(levelname)s] [%(name)s:%(filename)s:%(lineno)d]: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S"
    )
    
    # Console Handler
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(logging.INFO)
    console_handler.setFormatter(log_format)
    
    # File Handler (rotating logs, max 5MB per file, keep 3 backups)
    file_handler = RotatingFileHandler(
        LOG_FILE,
        maxBytes=5 * 1024 * 1024,
        backupCount=3,
        encoding="utf-8"
    )
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(log_format)
    
    # Add handlers
    logger.addHandler(console_handler)
    logger.addHandler(file_handler)
    
    return logger

# Expose global logger
logger = setup_logger()
