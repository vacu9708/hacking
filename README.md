# What winAPIs are needed to handle other processes' memory?
- ReadProcessMemory() to read memory
- WriteProcessMemory() to write to memory
- VirtualQueryEx() to retrieve the target process's memory information

# Alternative
Other processes' memory can be read by injecting a DLL into the target process.<br>
This way, the target process is considered the same process as the manipulating DLL, which allows direct handling of other processes' memory.

# Why this is not enough to hack a game?
Anti-anti-debugging is required because anti-debugging techniques are applied in all commercial games which prevent the games from being debugged.
