from typing import List, Dict
from config import settings
from utils.logger import logger
from utils.document_parser import compile_knowledge_documents

class ConversationService:
    """Service to manage conversation memory and history buffers per session."""
    
    def __init__(self) -> None:
        self.histories: Dict[str, List[Dict[str, str]]] = {}
        self.limit = settings.CONVERSATION_HISTORY_LIMIT
        
        # 1. Compile knowledge documents (PDFs, DOCX, TXT, MD) from folder
        compile_knowledge_documents(settings.KNOWLEDGE_DOCS_DIR, settings.COMPILED_KNOWLEDGE_PATH)
        
        # 2. Load compiled knowledge if exists
        self.compiled_knowledge = ""
        if settings.COMPILED_KNOWLEDGE_PATH.exists():
            try:
                with open(settings.COMPILED_KNOWLEDGE_PATH, "r", encoding="utf-8") as f:
                    self.compiled_knowledge = f.read().strip()
                logger.info("Loaded compiled knowledge from files.")
            except Exception as e:
                logger.error(f"Failed to read compiled_knowledge.txt: {e}")

        # 3. Load custom assistant knowledge if exists
        self.assistant_knowledge = ""
        knowledge_file = settings.BASE_DIR / "knowledge.md"
        if knowledge_file.exists():
            try:
                with open(knowledge_file, "r", encoding="utf-8") as f:
                    self.assistant_knowledge = f.read().strip()
                logger.info("Loaded custom assistant knowledge from knowledge.md")
            except Exception as e:
                logger.error(f"Failed to read knowledge.md: {e}")

        # 4. Voice assistant system prompt to keep responses natural and spoken-friendly
        self.base_system_prompt = (
            "You are JARVIS, a helpful offline English voice assistant. "
            "You speak and understand English. "
            "You MUST reply in English only. Never write or reply in Hindi or Hinglish. "
            "Always give natural, informative, and spoken-friendly answers (typically 2 to 3 sentences, around 40-60 words). "
            "Keep it concise, clear, and direct. Avoid long, winding paragraphs. "
            "Avoid list items, bullet points, formatting, and markdown, as your output is spoken."
        )
        
        logger.info(f"Initialized ConversationService with history limit: {self.limit}")

    def retrieve_relevant_knowledge(self, query: str, top_n: int = 5) -> str:
        """Dynamically retrieves only the most relevant lines from the compiled knowledge matching query keywords."""
        if not self.compiled_knowledge:
            return ""
            
        lines = [line.strip() for line in self.compiled_knowledge.split('\n') if line.strip()]
        
        # Simple overlap search using lowercase keywords
        query_words = set(w.lower() for w in query.split() if len(w) > 3)
        if not query_words:
            # Fallback to returning the first few lines of knowledge
            return "\n".join(lines[:10])
            
        scored_lines = []
        for line in lines:
            line_words = set(w.lower() for w in line.split())
            overlap = len(query_words.intersection(line_words))
            if overlap > 0:
                scored_lines.append((overlap, line))
                
        if not scored_lines:
            # Fallback to general lines
            return "\n".join(lines[:8])
            
        # Sort by overlap score descending
        scored_lines.sort(key=lambda x: x[0], reverse=True)
        relevant_lines = [line for score, line in scored_lines[:top_n]]
        return "\n".join(relevant_lines)

    def get_system_prompt(self, query: str) -> str:
        """Constructs a compact system prompt dynamically tailored to the user's query."""
        prompt = self.base_system_prompt
        
        if self.assistant_knowledge:
            prompt += f"\n\nAssistant Knowledge:\n{self.assistant_knowledge}"
            
        relevant_docs = self.retrieve_relevant_knowledge(query)
        if relevant_docs:
            prompt += f"\n\nRelevant knowledge for this question:\n{relevant_docs}"
            
        return prompt

    def get_history(self, session_id: str) -> List[Dict[str, str]]:
        """Returns the conversation history for a given session ID, initializing it if empty."""
        if session_id not in self.histories:
            self.histories[session_id] = []
            logger.info(f"Initialized new conversation session: {session_id}")
            
        return self.histories[session_id]

    def add_message(self, session_id: str, role: str, content: str) -> None:
        """Adds a message to the history and prunes older messages if limit is exceeded."""
        history = self.get_history(session_id)
        history.append({"role": role, "content": content})
        
        # Maintain the history limit (history contains user/assistant messages only)
        if len(history) > self.limit:
            pruned_count = len(history) - self.limit
            self.histories[session_id] = history[pruned_count:]
            logger.info(f"Pruned {pruned_count} messages from session {session_id} history")

    def clear_history(self, session_id: str) -> None:
        """Resets the history for a given session ID."""
        if session_id in self.histories:
            del self.histories[session_id]
            logger.info(f"Cleared conversation history for session: {session_id}")
