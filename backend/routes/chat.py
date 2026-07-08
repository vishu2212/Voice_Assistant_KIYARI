import os
import re
import uuid
import time
import json
import asyncio
import soundfile as sf
import numpy as np
from typing import List, Optional
from fastapi import APIRouter, File, UploadFile, Form, BackgroundTasks, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from pydantic import BaseModel

from config import settings
from utils.logger import logger
from services.whisper_service import WhisperService
from services.llm_service import LLMService
from services.tts_service import TTSService
from services.audio_service import AudioService
from services.conversation_service import ConversationService

router = APIRouter(tags=["Chat & Audio Services"])

# Initialize services (lazy-wrapped or fallback handled)
whisper_service: Optional[WhisperService] = None
try:
    whisper_service = WhisperService()
except Exception as e:
    logger.error(
        f"Whisper Service failed to initialize on startup. "
        f"Transcribe and Chat endpoints will be unavailable. Error: {e}"
    )

llm_service = LLMService()
tts_service = TTSService()
audio_service = AudioService()
conversation_service = ConversationService()

# Global volume multiplier (from 0.1 to 1.5, default 0.8)
volume_multiplier = 0.8

def handle_volume_change(user_prompt: str) -> str:
    global volume_multiplier
    prompt_lower = user_prompt.lower()
    
    # 1. Check for absolute volume changes (e.g., "volume 80", "set volume to 50%")
    match = re.search(r"(?:set\s+)?(?:volume|sound|voice)(?:\s+to|\s+at)?\s*(\d+)\s*%?", prompt_lower)
    if not match:
        match = re.search(r"(\d+)\s*%\s*(?:volume|sound)", prompt_lower)
        
    if match:
        val = int(match.group(1))
        if 1 <= val <= 10:
            val = val * 10
        val = max(10, min(120, val))
        volume_multiplier = val / 100.0
        logger.info(f"Volume adjusted to {val}% (multiplier: {volume_multiplier})")
        return f"[System Message: The user set the volume to {val}%. This volume change has already been applied to the hardware audio output. Please confirm this volume setting to the user in your response.]"
        
    # 2. Check for relative volume increase
    increase_keywords = ["volume up", "increase volume", "increase the volume", "make it louder", "louder", "volume increase"]
    if any(kw in prompt_lower for kw in increase_keywords):
        current_pct = int(round(volume_multiplier * 100))
        new_pct = min(120, current_pct + 20)
        volume_multiplier = new_pct / 100.0
        logger.info(f"Volume increased to {new_pct}% (multiplier: {volume_multiplier})")
        return f"[System Message: The user requested to increase the volume. The volume has been increased from {current_pct}% to {new_pct}% and applied to the output. Please confirm this to the user in your response.]"
        
    # 3. Check for relative volume decrease
    decrease_keywords = ["volume down", "decrease volume", "decrease the volume", "quieter", "softer", "reduce volume", "volume decrease", "reduce the volume"]
    if any(kw in prompt_lower for kw in decrease_keywords):
        current_pct = int(round(volume_multiplier * 100))
        new_pct = max(10, current_pct - 20)
        volume_multiplier = new_pct / 100.0
        logger.info(f"Volume decreased to {new_pct}% (multiplier: {volume_multiplier})")
        return f"[System Message: The user requested to decrease the volume. The volume has been decreased from {current_pct}% to {new_pct}% and applied to the output. Please confirm this to the user in your response.]"
        
    return ""

# Request/Response Schemas
class ModelsResponse(BaseModel):
    models: List[str]

class TranscribeResponse(BaseModel):
    text: str

class SpeakRequest(BaseModel):
    text: str

class ChatVoiceResponse(BaseModel):
    transcript: str
    response: str
    audio_url: str
    latency_ms: int
    latency_profile: Optional[dict] = None

def remove_temp_file(file_path: str) -> None:
    """Utility callback for BackgroundTasks to delete files after response completes."""
    try:
        if os.path.exists(file_path):
            # os.remove(file_path)
            logger.info(f"Preserved temporary file for debugging: {file_path}")
    except Exception as e:
        logger.error(f"Failed to process temp file {file_path}: {e}")


@router.get("/models", response_model=ModelsResponse)
async def get_models():
    """Returns the list of loaded/available models from local LM Studio."""
    try:
        models = await llm_service.get_available_models()
        return ModelsResponse(models=models)
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/chat", response_model=ChatVoiceResponse)
@router.post("/voice_chat", response_model=ChatVoiceResponse)
async def post_chat(
    background_tasks: BackgroundTasks,
    audio: UploadFile = File(...),
    session_id: str = Form(default="default")
):
    """Processes uploaded audio (Speech-to-Text) -> LLM -> TTS -> returns JSON response metadata."""
    if whisper_service is None:
        raise HTTPException(
            status_code=503,
            detail="Speech-to-Text engine (Whisper) is not initialized or available."
        )

    # Start total transaction timer
    start_total = time.perf_counter()
    
    # 1. Define temporary file names
    session_uuid = uuid.uuid4().hex
    input_wav_path = str(settings.TEMP_AUDIO_DIR / f"in_{session_uuid}.wav")
    output_wav_path = None
    
    try:
        # 2. Save and process the uploaded audio payload (handles raw PCM or standard WAV)
        logger.info(f"Received audio chat request for session: {session_id}")
        raw_bytes = await audio.read()
        audio_service.process_incoming_audio(raw_bytes, input_wav_path)
        background_tasks.add_task(remove_temp_file, input_wav_path)

        # 3. Transcribe audio to text (measure STT latency)
        start_stt = time.perf_counter()
        user_prompt = whisper_service.transcribe(input_wav_path)
        stt_ms = int((time.perf_counter() - start_stt) * 1000)
        logger.info(f"User Transcribed: '{user_prompt}'")
        
        if not user_prompt:
            # Return a default small tone or brief response if transcription was empty
            user_prompt = "[Silence]"
            
        # 4. Fetch context and add user message
        conversation_service.add_message(session_id, "user", user_prompt)
        history = conversation_service.get_history(session_id)
        system_prompt = conversation_service.get_system_prompt(user_prompt)
        messages_to_send = [{"role": "system", "content": system_prompt}] + history
        
        # 5. Fetch response from LM Studio LLM (measure LLM latency)
        start_llm = time.perf_counter()
        assistant_reply = await llm_service.get_response(messages_to_send)
        if not assistant_reply:
            assistant_reply = "I'm sorry, I couldn't generate a reply. Please try again."
        llm_ms = int((time.perf_counter() - start_llm) * 1000)
        logger.info(f"Assistant Reply: '{assistant_reply}'")
        conversation_service.add_message(session_id, "assistant", assistant_reply)
        
        # Parse potential bilingual format: [HINDI] ... [HINGLISH] ...
        speech_text = assistant_reply
        display_text = assistant_reply
        if "[HINDI]" in assistant_reply and "[HINGLISH]" in assistant_reply:
            try:
                parts = assistant_reply.split("[HINGLISH]")
                hindi_part = parts[0].replace("[HINDI]", "").strip()
                hinglish_part = parts[1].strip()
                speech_text = hindi_part
                display_text = hinglish_part
                logger.info(f"Parsed bilingual reply - Speech (Hindi): '{speech_text}', Display (Hinglish): '{display_text}'")
            except Exception as pe:
                logger.error(f"Failed to parse bilingual reply: {pe}")
        
        # 6. Synthesize assistant response text to Speech WAV file (measure TTS latency)
        start_tts = time.perf_counter()
        output_wav_path = await tts_service.speak(speech_text)
        tts_ms = int((time.perf_counter() - start_tts) * 1000)
        
        # Calculate relative static URL path (served statically via /temp/audio/...)
        audio_filename = os.path.basename(output_wav_path)
        audio_url = f"/temp/audio/{audio_filename}"
        
        # Calculate total latency
        total_ms = int((time.perf_counter() - start_total) * 1000)
        
        # Log latency profile to the server console in clean JSON
        latency_profile = {
            "stt_ms": stt_ms,
            "llm_ms": llm_ms,
            "tts_ms": tts_ms,
            "total_ms": total_ms
        }
        logger.info(f"Voice Latency Profile:\n{json.dumps(latency_profile, indent=2)}")
        
        # 7. Return JSON response containing transcription, text response, audio URL, and latency metrics
        return ChatVoiceResponse(
            transcript=user_prompt,
            response=display_text,
            audio_url=audio_url,
            latency_ms=total_ms,
            latency_profile=latency_profile
        )
        
    except Exception as e:
        logger.error(f"Error in chat processing loop: {e}", exc_info=True)
        # Ensure cleanup in case of error
        if os.path.exists(input_wav_path):
            remove_temp_file(input_wav_path)
        if output_wav_path and os.path.exists(output_wav_path):
            remove_temp_file(output_wav_path)
        raise HTTPException(status_code=500, detail=f"Failed to process chat: {str(e)}")


@router.post("/transcribe", response_model=TranscribeResponse)
async def post_transcribe(
    background_tasks: BackgroundTasks,
    audio: UploadFile = File(...)
):
    """Processes uploaded audio file and returns transcription text only."""
    if whisper_service is None:
        raise HTTPException(
            status_code=503,
            detail="Speech-to-Text engine (Whisper) is not initialized or available."
        )
        
    input_wav_path = str(settings.TEMP_AUDIO_DIR / f"transcribe_{uuid.uuid4().hex}.wav")
    
    try:
        raw_bytes = await audio.read()
        audio_service.process_incoming_audio(raw_bytes, input_wav_path)
        background_tasks.add_task(remove_temp_file, input_wav_path)
        
        text = whisper_service.transcribe(input_wav_path)
        return TranscribeResponse(text=text)
    except Exception as e:
        logger.error(f"Error in /transcribe: {e}", exc_info=True)
        if os.path.exists(input_wav_path):
            remove_temp_file(input_wav_path)
        raise HTTPException(status_code=500, detail=str(e))


class ChatTextRequest(BaseModel):
    text: str
    session_id: str = "default"

class ChatTextResponse(BaseModel):
    response: str
    latency: Optional[dict] = None


@router.post("/chat/text", response_model=ChatTextResponse)
async def post_chat_text(request: ChatTextRequest):
    """Processes text chat input -> queries LM Studio -> returns text response."""
    start_total = time.perf_counter()
    try:
        conversation_service.add_message(request.session_id, "user", request.text)
        history = conversation_service.get_history(request.session_id)
        system_prompt = conversation_service.get_system_prompt(request.text)
        messages_to_send = [{"role": "system", "content": system_prompt}] + history
        
        start_llm = time.perf_counter()
        reply = await llm_service.get_response(messages_to_send)
        if not reply:
            reply = "I'm sorry, I couldn't generate a reply. Please try again."
        llm_ms = int((time.perf_counter() - start_llm) * 1000)
        
        conversation_service.add_message(request.session_id, "assistant", reply)
        total_ms = int((time.perf_counter() - start_total) * 1000)
        
        latency = {
            "llm_ms": llm_ms,
            "total_ms": total_ms
        }
        logger.info(f"Text Latency Profile:\n{json.dumps(latency, indent=2)}")
        
        return ChatTextResponse(response=reply, latency=latency)
    except Exception as e:
        logger.error(f"Error in /chat/text: {e}", exc_info=True)
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/speak", response_class=FileResponse)
async def post_speak(
    request: SpeakRequest,
    background_tasks: BackgroundTasks
):
    """Accepts JSON text payload, synthesizes speech, and returns a WAV audio file."""
    output_wav_path = None
    try:
        output_wav_path = await tts_service.speak(request.text)
        background_tasks.add_task(remove_temp_file, output_wav_path)
        
        return FileResponse(
            path=output_wav_path,
            media_type="audio/wav",
            filename="speech.wav"
        )
    except Exception as e:
        logger.error(f"Error in /speak: {e}", exc_info=True)
        if output_wav_path and os.path.exists(output_wav_path):
            remove_temp_file(output_wav_path)
        raise HTTPException(status_code=500, detail=str(e))


def get_resampled_pcm16_bytes(wav_path, target_sr=16000, volume_multiplier=1.0) -> bytes:
    data, sr = sf.read(wav_path)
    if len(data.shape) > 1:
        data = np.mean(data, axis=1)
    
    if sr != target_sr:
        duration = len(data) / sr
        num_target_samples = int(duration * target_sr)
        src_indices = np.linspace(0, len(data) - 1, len(data))
        target_indices = np.linspace(0, len(data) - 1, num_target_samples)
        data = np.interp(target_indices, src_indices, data)
        
    # --- Peak Normalization to ensure consistent reference loudness ---
    max_peak = np.max(np.abs(data))
    if max_peak > 1e-4:
        data = (data / max_peak) * 0.90
        
    # Scale volume digitally based on user preference
    data = data * volume_multiplier
    data = np.clip(data, -1.0, 1.0)
        
    pcm16_data = (data * 32767).astype(np.int16)
    return pcm16_data.tobytes()


async def process_and_respond(audio_chunks, websocket: WebSocket, session_id: str):
    await websocket.send_json({"event": "thinking"})
    
    raw_pcm = b"".join(audio_chunks)
    if len(raw_pcm) == 0:
        logger.warning("WS: Empty audio buffer.")
        await websocket.send_json({"event": "done"})
        return
        
    session_uuid = uuid.uuid4().hex
    input_wav_path = str(settings.TEMP_AUDIO_DIR / f"ws_in_{session_uuid}.wav")
    output_wav_path = None
    
    try:
        audio_service.process_incoming_audio(raw_pcm, input_wav_path)
        
        if whisper_service is None:
            await websocket.send_json({"event": "error", "message": "STT not available"})
            return
            
        user_prompt = whisper_service.transcribe(input_wav_path)
        logger.info(f"WS Transcribed: '{user_prompt}'")
        
        if not user_prompt:
            logger.info("WS: Empty transcription detected, skipping LLM query.")
            error_reply = "I didn't catch that. Please try again."
            await websocket.send_json({
                "event": "speaking",
                "transcript": "",
                "response": error_reply
            })
            output_wav_path = await tts_service.speak(error_reply)
            pcm_bytes = get_resampled_pcm16_bytes(output_wav_path, target_sr=16000, volume_multiplier=volume_multiplier)
            chunk_size = 2048
            for i in range(0, len(pcm_bytes), chunk_size):
                chunk = pcm_bytes[i:i+chunk_size]
                await websocket.send_bytes(chunk)
                await asyncio.sleep(0.05)
            await websocket.send_json({"event": "done"})
            return
            
        system_vol_msg = handle_volume_change(user_prompt)
        conversation_service.add_message(session_id, "user", user_prompt)
        history = conversation_service.get_history(session_id)
        system_prompt = conversation_service.get_system_prompt(user_prompt)
        messages_to_send = [{"role": "system", "content": system_prompt}] + history
        
        if system_vol_msg:
            messages_to_send.append({"role": "system", "content": system_vol_msg})
            
        assistant_reply = await llm_service.get_response(messages_to_send)
        if not assistant_reply:
            assistant_reply = "I'm sorry, I couldn't generate a reply. Please try again."
        logger.info(f"WS LLM Reply: '{assistant_reply}'")
        conversation_service.add_message(session_id, "assistant", assistant_reply)
        
        # Parse potential bilingual format: [HINDI] ... [HINGLISH] ...
        speech_text = assistant_reply
        display_text = assistant_reply
        if "[HINDI]" in assistant_reply and "[HINGLISH]" in assistant_reply:
            try:
                parts = assistant_reply.split("[HINGLISH]")
                hindi_part = parts[0].replace("[HINDI]", "").strip()
                hinglish_part = parts[1].strip()
                speech_text = hindi_part
                display_text = hinglish_part
                logger.info(f"Parsed bilingual reply - Speech (Hindi): '{speech_text}', Display (Hinglish): '{display_text}'")
            except Exception as pe:
                logger.error(f"Failed to parse bilingual reply: {pe}")
        
        output_wav_path = await tts_service.speak(speech_text)
        pcm_bytes = get_resampled_pcm16_bytes(output_wav_path, target_sr=16000, volume_multiplier=volume_multiplier)
        
        await websocket.send_json({
            "event": "speaking",
            "transcript": user_prompt,
            "response": display_text
        })
        
        chunk_size = 2048
        for i in range(0, len(pcm_bytes), chunk_size):
            chunk = pcm_bytes[i:i+chunk_size]
            await websocket.send_bytes(chunk)
            await asyncio.sleep(0.05)
            
        await websocket.send_json({"event": "done"})
        
    except Exception as pipeline_ex:
        logger.error(f"WS Pipeline Error: {pipeline_ex}", exc_info=True)
        await websocket.send_json({"event": "error", "message": str(pipeline_ex)})
    finally:
        if os.path.exists(input_wav_path):
            try: os.remove(input_wav_path)
            except: pass
        if output_wav_path and os.path.exists(output_wav_path):
            try: os.remove(output_wav_path)
            except: pass


@router.websocket("/ws/chat")
async def websocket_chat_endpoint(websocket: WebSocket):
    await websocket.accept()
    logger.info("ESP32 WebSocket connection established.")
    
    session_id = "default"
    audio_chunks = []
    is_recording = False
    
    await websocket.send_json({"event": "connected"})
    
    try:
        while True:
            message = await websocket.receive()
            
            if "text" in message:
                try:
                    data = json.loads(message["text"])
                except Exception as je:
                    logger.error(f"Failed to parse JSON text from WebSocket: {je}")
                    continue
                
                event = data.get("event")
                if event == "start":
                    is_recording = True
                    audio_chunks = []
                    logger.info("WS: Recording started by client.")
                    await websocket.send_json({"event": "listening"})
                    
                elif event == "stop":
                    if not is_recording:
                        logger.warning("WS: Got stop event but not recording.")
                        continue
                    
                    is_recording = False
                    logger.info(f"WS: Recording stopped. Collected {len(audio_chunks)} audio chunks.")
                    await process_and_respond(audio_chunks, websocket, session_id)
                    audio_chunks = []
                            
            elif "bytes" in message:
                if is_recording:
                    audio_chunks.append(message["bytes"])
                    
    except WebSocketDisconnect:
        logger.info("WS: ESP32 WebSocket client disconnected.")
    except Exception as e:
        if isinstance(e, RuntimeError) and "disconnect" in str(e):
            logger.info("WS: ESP32 WebSocket client disconnected (RuntimeError).")
        else:
            logger.error(f"WS Error: {e}", exc_info=True)

