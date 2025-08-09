import tkinter as tk
from tkinter import ttk, scrolledtext
from pydyad.ollama_integration import run_ollama_model

class OllamaGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("DYAD - Ollama Model Selector")
        self.root.geometry("600x400")

        # Model Selection
        tk.Label(root, text="Select Model:").pack(pady=5)
        self.model_var = tk.StringVar()
        self.model_dropdown = ttk.Combobox(root, textvariable=self.model_var)
        self.model_dropdown['values'] = ("phi3", "llama3.2", "deepseek-coder", "codellama")
        self.model_dropdown.set("phi3")  # Default selection
        self.model_dropdown.pack(pady=5)

        # Prompt Entry
        tk.Label(root, text="Enter Prompt:").pack(pady=5)
        self.prompt_entry = tk.Entry(root, width=70)
        self.prompt_entry.pack(pady=5)

        # Submit Button
        submit_button = tk.Button(root, text="Run Model", command=self.run_model)
        submit_button.pack(pady=10)

        # Output Text Area
        self.output_text = scrolledtext.ScrolledText(root, width=70, height=15)
        self.output_text.pack(pady=10)

    def run_model(self):
        model = self.model_var.get()
        prompt = self.prompt_entry.get()
        if not prompt:
            self.output_text.insert(tk.END, "Please enter a prompt.\\n")
            return
        
        self.output_text.insert(tk.END, f"Running model '{model}'...\\n")
        self.root.update()  # Refresh GUI to show "Running..." message

        response = run_ollama_model(prompt, model)
        self.output_text.insert(tk.END, f"Response:\\n{response}\\n\\n")

if __name__ == "__main__":
    root = tk.Tk()
    app = OllamaGUI(root)
    root.mainloop()