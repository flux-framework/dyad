import subprocess
import threading
from functools import lru_cache

# Cache responses to avoid redundant computations
@lru_cache(maxsize=128)
def run_ollama_model_cached(prompt, model):
    """
    Cached version of the Ollama model runner.
    """
    try:
        command = ["ollama", "run", model, prompt]
        result = subprocess.run(command, capture_output=True, text=True, timeout=120)
        if result.returncode == 0:
            return result.stdout.strip()
        else:
            return f"ModelError: {result.stderr.strip()}"
    except Exception as e:
        return f"ModelError: {str(e)}"

def run_ollama_model(prompt, model="phi3", timeout=120, use_cache=True):
    """
    Run a prompt through a local Ollama model with error handling and caching.
    :param prompt: The input text for the model.
    :param model: The name of the Ollama model to use.
    :param timeout: Maximum time (seconds) to wait for the model response.
    :param use_cache: Whether to use cached responses.
    :return: The model's response or an error message.
    """
    if use_cache:
        return run_ollama_model_cached(prompt, model)
    
    try:
        command = ["ollama", "run", model, prompt]
        result = subprocess.run(command, capture_output=True, text=True, timeout=timeout)
        if result.returncode == 0:
            return result.stdout.strip()
        else:
            return f"ModelError: {result.stderr.strip()}"
    except subprocess.TimeoutExpired:
        return "ModelError: Model response timed out."
    except FileNotFoundError:
        return "ModelError: Ollama is not installed or not in PATH."
    except Exception as e:
        return f"ModelError: {str(e)}"

def run_model_async(prompt, model, callback):
    """
    Run the model in a separate thread and call the callback with the result.
    """
    def worker():
        response = run_ollama_model(prompt, model)
        callback(response)
    
    thread = threading.Thread(target=worker)
    thread.start()
    return thread