import os
from pathlib import Path
from utils.logger import logger

def compile_knowledge_documents(docs_dir: Path, target_compiled_path: Path) -> str:
    """Scans docs_dir, parses PDF, DOCX, TXT, MD, and writes unified content to target_compiled_path.
    
    Returns:
        The combined extracted text.
    """
    os.makedirs(docs_dir, exist_ok=True)
    
    supported_exts = {".pdf", ".docx", ".txt", ".md"}
    combined_texts = []
    processed_files = []
    
    logger.info(f"Scanning directory for knowledge documents: {docs_dir}")
    
    for file_entry in os.scandir(docs_dir):
        if not file_entry.is_file():
            continue
            
        file_path = Path(file_entry.path)
        ext = file_path.suffix.lower()
        
        if ext not in supported_exts:
            continue
            
        logger.info(f"Parsing knowledge document: {file_path.name}")
        extracted_text = ""
        
        try:
            if ext == ".pdf":
                from pypdf import PdfReader
                reader = PdfReader(str(file_path))
                text_list = []
                for page in reader.pages:
                    t = page.extract_text()
                    if t:
                        text_list.append(t)
                extracted_text = "\n".join(text_list)
                
            elif ext == ".docx":
                import docx
                doc = docx.Document(str(file_path))
                extracted_text = "\n".join([p.text for p in doc.paragraphs])
                
            elif ext in {".txt", ".md"}:
                with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                    extracted_text = f.read()
                    
            if extracted_text.strip():
                combined_texts.append(f"--- START FILE: {file_path.name} ---\n{extracted_text.strip()}\n--- END FILE: {file_path.name} ---")
                processed_files.append(file_path.name)
                
        except Exception as e:
            logger.error(f"Error parsing file {file_path.name}: {e}", exc_info=True)
            
    if combined_texts:
        full_content = "\n\n".join(combined_texts)
        try:
            os.makedirs(target_compiled_path.parent, exist_ok=True)
            with open(target_compiled_path, "w", encoding="utf-8") as f:
                f.write(full_content)
            logger.info(f"Successfully compiled knowledge from {len(processed_files)} files to {target_compiled_path}")
        except Exception as e:
            logger.error(f"Failed to write compiled knowledge file: {e}")
        return full_content
    else:
        if target_compiled_path.exists():
            try: os.remove(target_compiled_path)
            except: pass
        return ""
