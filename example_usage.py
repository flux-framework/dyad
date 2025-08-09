#!/usr/bin/env python3
"""
Example script demonstrating how to use the DYAD Python bindings.

This script shows the basic usage pattern for the DYAD library,
even though it may not run fully without the proper C libraries
and FLUX framework installed.
"""

try:
    # Import the DYAD module
    from pydyad import Dyad
    from pydyad.bindings import DTLMode, DTLCommMode
    
    print("Successfully imported DYAD modules")
    
    # Create a DYAD instance
    dyad = Dyad()
    print("Created DYAD instance")
    
    # Show available attributes
    print(f"DYAD initialized: {dyad.initialized}")
    print(f"Producer path: {dyad.prod_path}")
    print(f"Consumer path: {dyad.cons_path}")
    
    # Example initialization (this would normally be done with actual paths)
    # Note: This will fail without the required C libraries and FLUX framework
    try:
        dyad.init(
            debug=True,
            shared_storage=True,
            key_depth=3,
            key_bins=1024,
            dtl_mode=DTLMode.DYAD_DTL_UCX,
            dtl_comm_mode=DTLCommMode.DYAD_COMM_RECV
        )
        print("DYAD initialized successfully")
    except Exception as e:
        print(f"Initialization failed (expected without proper setup): {e}")
        
    # Example produce operation
    try:
        # This would normally be a path to a file in the managed directory
        dyad.produce("example_file.txt")
        print("Produce operation completed")
    except Exception as e:
        print(f"Produce operation failed (expected without proper setup): {e}")
        
    # Example consume operation
    try:
        # This would normally be a path to a file in the managed directory
        dyad.consume("example_file.txt")
        print("Consume operation completed")
    except Exception as e:
        print(f"Consume operation failed (expected without proper setup): {e}")
        
    # Finalize
    try:
        dyad.finalize()
        print("DYAD finalized successfully")
    except Exception as e:
        print(f"Finalization failed (expected without proper setup): {e}")
        
except ImportError as e:
    print(f"Failed to import DYAD modules: {e}")
    print("This is expected if the DYAD Python package is not properly installed.")
    
except Exception as e:
    print(f"Unexpected error: {e}")
    import traceback
    traceback.print_exc()