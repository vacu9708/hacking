# Why winAPIs are needed to handle other processes' memory?
- ReadProcessMemory() to read memory
- WriteProcessMemory() to write to memory
- VirtualQueryEx() to retrieve the target process's memory information

# Alternative
Other processes' memory can be handled by injecting a DLL into the target process.<br>
This way, the target process is considered the same process as the manipulating DLL, which allows direct access to other processes' memory through pointers.<br>

# Why this is not enough to hack a game?
To constantly read specific memory addresses such as enemies' coordinates, a breakpoint has to be set so that it is triggered on reading the specific memory addresses. 
Anti-anti-debugging is required because anti-debugging techniques are applied in all commercial games which prevent the games from being debugged.<br>
Kernel-level anti-anti-debugging techniques are necessary to hack commercial games.

![image](https://github.com/vacu9708/hacking/assets/67142421/5023b4c2-99ea-4bdb-b6ae-4c88037aa49c)<br>
Entered the process ID<br>
![image](https://github.com/vacu9708/hacking/assets/67142421/533b03da-d119-41f2-9ca3-0428159550e6)<br>
The default data type is 1 byte(char). Entered "detect even" to search memory addresses in even numbers <br>
![image](https://github.com/vacu9708/hacking/assets/67142421/9468114f-c349-4b1f-bd1f-8937b1c5f7da)<br>
Entered 2 1 to find data that has a value that is 1.<br>
Change the found value in the app you are cheating in and find the changed value to narrow down candidate memory addresses.<br>
Repeat this to find the target value to cheat on.
