import os
from config import settings
from utils.logger import logger

class WhisperService:
    """Service to handle Speech-to-Text transcription using faster-whisper."""
    
    def __init__(self) -> None:
        self.model = None
        
        logger.info(
            f"Initializing WhisperModel (Model: {settings.WHISPER_MODEL_NAME}, "
            f"Device: {settings.WHISPER_DEVICE}, Compute Type: {settings.WHISPER_COMPUTE_TYPE})"
        )
        
        try:
            from faster_whisper import WhisperModel
            self.model = WhisperModel(
                settings.WHISPER_MODEL_NAME,
                device=settings.WHISPER_DEVICE,
                compute_type=settings.WHISPER_COMPUTE_TYPE,
            )
            logger.info("WhisperModel loaded successfully.")
        except Exception as e:
            logger.critical(f"Failed to load WhisperModel: {e}", exc_info=True)
            raise e

    def transcribe(self, audio_path: str) -> str:
        """Transcribes the given audio file and returns the text."""
        if self.model is None:
            raise RuntimeError("Whisper model is not initialized.")
            
        if not os.path.exists(audio_path):
            raise FileNotFoundError(f"Audio file not found at: {audio_path}")
            
        logger.info(f"Starting transcription for file: {audio_path}")
        try:
            segments, info = self.model.transcribe(
                audio_path,
                language=None,
                beam_size=1,
                vad_filter=True,
                vad_parameters=dict(
                    threshold=0.6,
                    min_speech_duration_ms=250
                )
            )
            logger.info(f"Language: {info.language} (probability: {info.language_probability:.2f})")
            
            # Iterate through generator to retrieve full transcription text
            transcription = []
            for segment in segments:
                transcription.append(segment.text)
                
            text = " ".join(transcription).strip()
            logger.info(f"Recognized text: {text}")
            return text
        except Exception as e:
            logger.error(f"Error occurred during Whisper transcription: {e}", exc_info=True)
            raise e
