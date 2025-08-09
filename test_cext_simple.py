#!/usr/bin/env python3

try:
    import pydyad
    print("Successfully imported pydyad")
    
    # Try to access a basic attribute or function
    # This will depend on what's exposed by the pydyad module
    # Let's assume there's a version or init function
    if hasattr(pydyad, '__version__'):
        print(f"pydyad version: {pydyad.__version__}")
    else:
        print("pydyad module loaded, but no version attribute found")
        
    print("C extension test passed!")
    
except ImportError as e:
    print(f"Failed to import pydyad: {e}")
    exit(1)
except Exception as e:
    print(f"Error during test: {e}")
    exit(1)