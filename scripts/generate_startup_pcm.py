import os
import subprocess
import soundfile as sf
import numpy as np
from pathlib import Path

def main():
    piper_exe = r"C:\piper\piper.exe"
    voice_model = r"C:\piper\voices\en_US-lessac-medium.onnx"
    text = "Hello, I am Zyra."
    
    # Target file
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent
    startup_h_path = project_root / "xiaozhi-esp32" / "main" / "startup.h"
    temp_wav_path = script_dir / "temp_hello.wav"
    
    print(f"Running Piper to generate speech WAV file...")
    
    # Run Piper executable
    args = [
        piper_exe,
        "--model", voice_model,
        "--output_file", str(temp_wav_path)
    ]
    
    try:
        process = subprocess.Popen(
            args,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        stdout, stderr = process.communicate(input=text.encode("utf-8"))
        if process.returncode != 0:
            print(f"Error running Piper: {stderr.decode('utf-8')}")
            return
    except Exception as e:
        print(f"Failed to execute Piper: {e}")
        return
        
    if not temp_wav_path.exists() or temp_wav_path.stat().st_size == 0:
        print("Piper output file is empty or missing.")
        return
        
    print(f"Reading WAV file and converting to 16kHz mono PCM...")
    data, sr = sf.read(str(temp_wav_path))
    
    # Convert to mono if stereo
    if len(data.shape) > 1:
        data = np.mean(data, axis=1)
        
    # Resample to 16000Hz if necessary
    target_sr = 16000
    if sr != target_sr:
        duration = len(data) / sr
        num_target_samples = int(duration * target_sr)
        src_indices = np.linspace(0, len(data) - 1, len(data))
        target_indices = np.linspace(0, len(data) - 1, num_target_samples)
        data = np.interp(target_indices, src_indices, data)
        
    # Peak normalize and scale
    max_peak = np.max(np.abs(data))
    if max_peak > 1e-4:
        data = (data / max_peak) * 0.90
        
    # Convert to 16-bit signed integer PCM
    pcm16 = (data * 32767).astype(np.int16)
    
    print(f"Formatting PCM data as C header: {startup_h_path}")
    
    # Write C header
    with open(startup_h_path, "w", encoding="utf-8") as f:
        f.write("#pragma once\n")
        f.write("#include <cstdint>\n")
        f.write("#include <cstddef>\n\n")
        f.write("const int16_t startup_pcm[] = {\n")
        
        # Write PCM values, 12 per line
        chunks = [str(val) for val in pcm16]
        for i in range(0, len(chunks), 12):
            line_chunk = ", ".join(chunks[i:i+12])
            f.write(f"    {line_chunk},\n")
            
        f.write("};\n\n")
        f.write("const size_t startup_pcm_len = sizeof(startup_pcm) / sizeof(startup_pcm[0]);\n")
        
    print(f"Header successfully generated with {len(pcm16)} samples.")
    
    # Clean up
    if temp_wav_path.exists():
        os.remove(temp_wav_path)
        print("Cleaned up temporary WAV file.")

if __name__ == "__main__":
    main()
