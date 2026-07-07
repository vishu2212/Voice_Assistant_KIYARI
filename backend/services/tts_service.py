import asyncio
import os
import uuid
from config import settings
from utils.logger import logger

class TTSService:
    """Service to handle Text-to-Speech synthesis using local Piper executable."""
    
    def __init__(self) -> None:
        self.executable = settings.PIPER_EXECUTABLE
        self.model = settings.PIPER_VOICE_MODEL
        logger.info(f"Initialized TTSService (Executable: {self.executable}, Model: {self.model})")

    async def speak(self, text: str) -> str:
        """Synthesizes text to a WAV file using Piper.
        
        Returns:
            The absolute path of the generated WAV file.
        """
        # Validate that the Piper executable exists
        if not os.path.exists(self.executable):
            err_msg = f"Piper executable not found at: {self.executable}"
            logger.error(err_msg)
            raise FileNotFoundError(err_msg)
            
        # Detect Hindi Devanagari script and dynamically select the voice model
        is_hindi = any('\u0900' <= char <= '\u097f' for char in text)
        voice_model = "C:\\piper\\voices\\hi_IN-pratham-medium.onnx" if is_hindi else self.model
        
        if not os.path.exists(voice_model):
            logger.warning(f"Voice model {voice_model} not found, falling back to default English voice.")
            voice_model = self.model
            
        if not os.path.exists(voice_model):
            err_msg = f"Piper voice model not found at: {voice_model}"
            logger.error(err_msg)
            raise FileNotFoundError(err_msg)
            
        # Ensure text is not empty
        text = text.strip()
        if not text:
            raise ValueError("Input text for TTS cannot be empty")
            
        output_filename = f"tts_{uuid.uuid4().hex}.wav"
        output_path = str(settings.TEMP_AUDIO_DIR / output_filename)
        
        logger.info(f"Synthesizing text (Hindi={is_hindi}): '{text[:50]}...' -> {output_path}")
        
        # Arguments to invoke Piper
        args = [
            "--model", voice_model,
            "--output_file", output_path
        ]
        
        try:
            # Start Piper process asynchronously
            process = await asyncio.create_subprocess_exec(
                self.executable,
                *args,
                stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            
            # Pipe text to standard input and wait for completion
            stdout, stderr = await process.communicate(input=text.encode("utf-8"))
            
            if process.returncode != 0:
                err_content = stderr.decode("utf-8", errors="ignore").strip()
                logger.error(f"Piper exited with code {process.returncode}: {err_content}")
                raise RuntimeError(f"Piper execution failed: {err_content}")
                
            if not os.path.exists(output_path) or os.path.getsize(output_path) == 0:
                logger.error(f"Piper finished but output file is missing or empty: {output_path}")
                raise RuntimeError("Piper generated an empty or invalid audio file.")
                
            logger.info(f"Speech WAV file successfully synthesized: {output_path}")
            return output_path
            
        except Exception as e:
            logger.error(f"Failed to generate TTS audio: {e}", exc_info=True)
            raise e
