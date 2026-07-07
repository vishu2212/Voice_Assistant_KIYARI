import os
import sys
from pathlib import Path
from pydantic import BaseModel, Field

# Dynamically inject Nvidia CUDA runtime DLL folders to the search path for Windows platforms.
# This prevents ctranslate2/faster-whisper from failing due to missing system CUDA DLLs.
if sys.platform == "win32":
    _base_dir = Path(__file__).resolve().parent
    _site_packages = _base_dir / "venv" / "Lib" / "site-packages"
    
    _cublas_bin = _site_packages / "nvidia" / "cublas" / "bin"
    _cudnn_bin = _site_packages / "nvidia" / "cudnn" / "bin"
    
    _dll_paths = []
    if _cublas_bin.exists():
        _dll_paths.append(str(_cublas_bin))
    if _cudnn_bin.exists():
        _dll_paths.append(str(_cudnn_bin))
        
    for _path in _dll_paths:
        try:
            os.add_dll_directory(_path)
        except Exception:
            pass
        os.environ["PATH"] = _path + ";" + os.environ.get("PATH", "")

class Settings(BaseModel):
    # Server Settings
    HOST: str = Field(default="0.0.0.0", description="Host to bind the FastAPI server to")
    PORT: int = Field(default=8001, description="Port to run the FastAPI server on")
    
    # Paths
    BASE_DIR: Path = Path(__file__).resolve().parent
    TEMP_DIR: Path = BASE_DIR / "temp"
    TEMP_AUDIO_DIR: Path = TEMP_DIR / "audio"
    KNOWLEDGE_DOCS_DIR: Path = BASE_DIR / "knowledge_documents"
    COMPILED_KNOWLEDGE_PATH: Path = TEMP_DIR / "compiled_knowledge.txt"
    
    # LM Studio Settings
    LM_STUDIO_URL: str = Field(
        default="http://127.0.0.1:1234/v1", 
        description="Base URL for LM Studio API"
    )
    LM_STUDIO_MODEL: str = Field(
        default="qwen2.5-7b-instruct", 
        description="Model identifier in LM Studio"
    )
    TIMEOUT_LLM: float = Field(
        default=30.0, 
        description="Timeout in seconds for LLM generation"
    )
    
    # Speech-to-Text (Whisper) Settings
    WHISPER_MODEL_NAME: str = Field(
        default=str(Path(__file__).resolve().parent.parent / "models" / "whisper" / "small"), 
        description="Model name/size or absolute path for faster-whisper"
    )
    WHISPER_DEVICE: str = Field(
        default="cuda", 
        description="Device to run Whisper on ('cuda', 'cpu')"
    )
    WHISPER_COMPUTE_TYPE: str = Field(
        default="float16", 
        description="Compute type for Whisper (e.g., float16, int8)"
    )

    # Text-to-Speech (Piper) Settings
    # Users will need to set these paths to their local Piper installation
    PIPER_EXECUTABLE: str = Field(
        default="C:\\piper\\piper.exe",
        description="Absolute path to the local Piper executable"
    )
    PIPER_VOICE_MODEL: str = Field(
        default="C:\\piper\\voices\\en_US-lessac-medium.onnx",
        description="Absolute path to the Piper ONNX voice model file"
    )
    
    # Conversation Settings
    CONVERSATION_HISTORY_LIMIT: int = Field(
        default=10, 
        description="Maximum number of messages to keep in history per session"
    )
    
    class Config:
        arbitrary_types_allowed = True

# Instantiate global settings
settings = Settings()

# Ensure directories exist
os.makedirs(settings.TEMP_AUDIO_DIR, exist_ok=True)
os.makedirs(settings.KNOWLEDGE_DOCS_DIR, exist_ok=True)
