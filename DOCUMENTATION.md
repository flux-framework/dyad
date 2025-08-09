# DYAD App Documentation

## Overview

DYAD (DYnamic and Asynchronous Data Streamliner) is a system designed to facilitate the sharing of data files between producer and consumer job elements, particularly within an ensemble or between co-scheduled ensembles. It provides a service that transparently synchronizes file I/O between producer and consumer and transfers data from the producer location to the consumer location managed by the service.

Users only need to use file paths that are under the directory managed by the service.

## Components

1. **FLUX Module**: The core component that manages the data streaming service.
2. **I/O Wrapper Set**: Components that wrap file I/O operations to integrate with the DYAD service.
3. **Python Bindings**: Python interface to the C API, allowing Python applications to use DYAD functionality.
4. **AI Integration**: Integration with Ollama for running local AI models, with both CLI and GUI interfaces.

## Core C API

The core functionality of DYAD is exposed through a C API with the following key functions:

### Initialization
- `dyad_init`: Initialize the DYAD context with specific parameters
- `dyad_init_env`: Initialize the DYAD context using environment variables

### Core Operations
- `dyad_produce`: Wrapper function for producer operations
- `dyad_consume`: Wrapper function for consumer operations
- `dyad_consume_w_metadata`: Consumer operation with metadata
- `dyad_get_metadata`: Obtain metadata for a file
- `dyad_free_metadata`: Free metadata resources

### Finalization
- `dyad_finalize`: Finalize the DYAD instance and deallocate the context

## Python Bindings

The Python bindings provide a high-level interface to the DYAD C API through the `Dyad` class:

### Key Methods
- `init`: Initialize the DYAD context with various configuration options
- `init_env`: Initialize the DYAD context using environment variables
- `produce`: Perform producer operations
- `consume`: Perform consumer operations
- `get_metadata`: Retrieve metadata for a file
- `finalize`: Clean up and finalize the DYAD instance

### Usage Example
```python
from pydyad import Dyad

# Create a DYAD instance
dyad = Dyad()

# Initialize with specific parameters
dyad.init(
    debug=True,
    shared_storage=True,
    prod_managed_path="/path/to/producer/data",
    cons_managed_path="/path/to/consumer/data"
)

# Producer operation
dyad.produce("/path/to/producer/data/file.txt")

# Consumer operation
dyad.consume("/path/to/consumer/data/file.txt")

# Finalize when done
dyad.finalize()
```

## AI Integration

The DYAD App includes integration with Ollama for running local AI models:

### CLI Interface
The `ollama_cli.py` script provides a command-line interface for interacting with Ollama models:
- Supports models like `phi3`, `llama3.2`, `deepseek-coder`, and `codellama`
- Allows users to select a model and enter a prompt
- Displays the model's response

### GUI Interface
The `ollama_gui.py` script provides a graphical interface for interacting with Ollama models:
- Tkinter-based GUI with model selection dropdown
- Prompt entry field
- Output text area for model responses
- Asynchronous model execution to keep the UI responsive

## Dependencies

- FLUX framework
- Python 3.x
- Ollama (for AI model integration)
- Various C libraries for the core components

## License

SPDX-License-Identifier: LGPL-3.0

LLNL-CODE-764420

The project also includes code from external projects with their own licenses:
- MURMUR3: Public domain
- LIBB64: Public domain (Creative Commons Public Domain License)