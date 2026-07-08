import os
import numpy as np
import soundfile as sf
from utils.logger import logger

class AudioService:
    """Service to process, validate, and convert audio payloads with noise filtering and gates."""
    
    def __init__(self) -> None:
        logger.info("Initialized AudioService")

    def clean_audio(self, audio_array: np.ndarray) -> np.ndarray:
        """Applies digital signal processing (DSP) to remove DC offset, hum, and noise."""
        if len(audio_array) == 0:
            return audio_array
            
        # 1. Remove DC offset
        data = audio_array.astype(np.float32)
        data = data - np.mean(data)
        
        # 2. Noise Gate: check peak on raw DC-removed signal before filtering
        # Threshold of 250 is very safe (approx 0.7% of full scale)
        peak = np.max(np.abs(data))
        if peak < 250:
            logger.info(f"Noise Gate Triggered: Peak amplitude ({peak:.1f}) is below threshold (250). Silencing audio.")
            return np.zeros_like(audio_array)
            
        # 3. High-pass filter (Pre-emphasis y[n] = x[n] - 0.95 * x[n-1])
        # This filters out low frequency rumble (vents, fans, AC) and highlights vocal frequencies.
        filtered = np.zeros_like(data)
        filtered[0] = data[0]
        filtered[1:] = data[1:] - 0.95 * data[:-1]
            
        # Scale back and clip to standard int16 range
        filtered = np.clip(filtered, -32768, 32767)
        return filtered.astype(np.int16)

    def process_incoming_audio(self, raw_bytes: bytes, target_path: str) -> str:
        """Processes incoming audio payload, cleans it, and saves as WAV file."""
        if not raw_bytes:
            raise ValueError("Audio payload cannot be empty")
            
        is_wav = raw_bytes[:4] == b"RIFF"
        
        try:
            if is_wav:
                logger.info(f"Audio payload detected as standard WAV format. Saving and cleaning.")
                with open(target_path, "wb") as f:
                    f.write(raw_bytes)
                
                # Load, clean, and rewrite the audio data
                audio_array, sr = sf.read(target_path, dtype="int16")
                cleaned_array = self.clean_audio(audio_array)
                sf.write(target_path, cleaned_array, sr, subtype="PCM_16")
                
                info = sf.info(target_path)
                logger.info(
                    f"Validated and cleaned WAV: Format={info.format}, "
                    f"SampleRate={info.samplerate}Hz, Channels={info.channels}, "
                    f"Duration={info.duration:.2f}s"
                )
            else:
                logger.info(
                    f"Audio payload detected as raw PCM data. Converting to 16kHz Mono WAV."
                )
                audio_array = np.frombuffer(raw_bytes, dtype=np.int16)
                
                # Clean the raw PCM audio
                cleaned_array = self.clean_audio(audio_array)
                
                sf.write(target_path, cleaned_array, 16000, subtype="PCM_16")
                duration = len(cleaned_array) / 16000
                logger.info(
                    f"Cleaned raw PCM & wrote WAV: Samples={len(cleaned_array)}, Duration={duration:.2f}s"
                )
                
            return target_path
            
        except Exception as e:
            logger.error(f"Failed to process and validate incoming audio: {e}", exc_info=True)
            if os.path.exists(target_path):
                try:
                    os.remove(target_path)
                except Exception:
                    pass
            raise ValueError(f"Invalid or corrupted audio payload: {e}")
