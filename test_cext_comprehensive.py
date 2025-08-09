#!/usr/bin/env python3

try:
    import pydyad
    from pydyad.bindings import Dyad
    print("Successfully imported pydyad and Dyad from bindings")
    
    # Try to create a Dyad object
    dyad_instance = Dyad()
    print("Successfully created Dyad instance")
    
    # Try to access some attributes
    print(f"Dyad instance initialized: {dyad_instance.initialized}")
    print(f"Dyad client lib: {dyad_instance.dyad_client_lib}")
    print(f"Dyad ctx lib: {dyad_instance.dyad_ctx_lib}")
    
    print("C extension test passed!")
    
except ImportError as e:
    print(f"Failed to import pydyad or Dyad: {e}")
    exit(1)
except FileNotFoundError as e:
    print(f"Failed to find required library: {e}")
    exit(1)
except Exception as e:
    print(f"Error during test: {e}")
    import traceback
    traceback.print_exc()
    exit(1)