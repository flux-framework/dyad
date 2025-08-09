from pydyad.ollama_integration import run_ollama_model

def select_model():
    models = ["phi3", "llama3.2", "deepseek-coder", "codellama"]
    print("Available models:", models)
    choice = input("Select a model: ")
    return choice if choice in models else "phi3"

def main():
    print("Welcome to the DYAD Ollama CLI!")
    model = select_model()
    prompt = input("Enter your prompt: ")
    response = run_ollama_model(prompt, model)
    print("\nResponse:", response)

if __name__ == "__main__":
    main()