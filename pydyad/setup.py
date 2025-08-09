from setuptools import setup, Extension

# Define the C extension module
dyad_ollama_module = Extension(
    'pydyad.dyad_ollama',
    sources=['pydyad/dyad_ollama.c'],
)

setup(
    ext_modules=[dyad_ollama_module],
)