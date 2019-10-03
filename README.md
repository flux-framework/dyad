# flux-dyad
DYAD: DYnamic and Asynchronous Data Streamliner

DYAD is a tool to facilitate sharing data files between producer and consumer job elements, especially within an ensemble or between co-scheduled ensembles. DYAD is implemented using two components: a Flux service module and an I/O wrapper that is preloaded into the application's address space and intercepts their `open()` and `close ()` calls (and their variants). DYAD transparently synchronizes file I/O between the producers and consumers, and automatically transfers data from the producer's node to the consumer's node. User can use DYAD  without having to modify the application's source code: they only need to ensure the applications use the directory managed by DYAD. 
