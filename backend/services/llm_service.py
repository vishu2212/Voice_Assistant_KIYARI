from typing import List, Dict
import httpx
from config import settings
from utils.logger import logger

class LLMService:
    """Service to handle interactions with the local LM Studio instance."""
    
    def __init__(self) -> None:
        self.base_url = settings.LM_STUDIO_URL.rstrip("/")
        self.completions_url = f"{self.base_url}/chat/completions"
        self.models_url = f"{self.base_url}/models"
        self.model = settings.LM_STUDIO_MODEL
        self.timeout = settings.TIMEOUT_LLM
        logger.info(f"Initialized LLMService (Endpoint: {self.completions_url}, Model: {self.model})")

    async def get_response(self, messages: List[Dict[str, str]]) -> str:
        """Sends the message history to LM Studio and returns the LLM response text."""
        payload = {
            "model": self.model,
            "messages": messages,
            "temperature": 0.7,
            "max_tokens": 200
        }
        
        logger.info(f"Sending LLM chat completion request to LM Studio")
        try:
            async with httpx.AsyncClient() as client:
                response = await client.post(
                    self.completions_url,
                    json=payload,
                    headers={"Content-Type": "application/json"},
                    timeout=self.timeout
                )
                
                if response.status_code != 200:
                    logger.error(f"LM Studio returned error {response.status_code}: {response.text}")
                    raise httpx.HTTPStatusError(
                        f"LM Studio API returned status code {response.status_code}",
                        request=response.request,
                        response=response
                    )
                
                data = response.json()
                reply = data["choices"][0]["message"]["content"].strip()
                logger.info("Successfully received chat completion from LM Studio.")
                return reply
        except httpx.RequestError as e:
            logger.error(f"Failed to communicate with LM Studio: {e}", exc_info=True)
            raise RuntimeError(f"Connection to LM Studio failed: {e}")
        except KeyError as e:
            logger.error(f"Invalid JSON response structure from LM Studio: {e}", exc_info=True)
            raise RuntimeError(f"Invalid format returned from LM Studio: {e}")
        except Exception as e:
            logger.error(f"Unexpected error in LLMService: {e}", exc_info=True)
            raise e

    async def get_available_models(self) -> List[str]:
        """Queries the LM Studio API and returns a list of available models."""
        logger.info(f"Querying available models from {self.models_url}")
        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(self.models_url, timeout=10.0)
                
                if response.status_code != 200:
                    logger.error(f"LM Studio returned status {response.status_code} for models endpoint")
                    raise httpx.HTTPStatusError(
                        f"Models API returned status code {response.status_code}",
                        request=response.request,
                        response=response
                    )
                
                data = response.json()
                # Extract model IDs
                models = [m["id"] for m in data.get("data", [])]
                logger.info(f"Models retrieved: {models}")
                return models
        except Exception as e:
            logger.error(f"Failed to fetch models from LM Studio: {e}", exc_info=True)
            raise RuntimeError(f"Failed to retrieve models: {e}")
