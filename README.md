# OS - Project

The project of the OS class (Operating Systems).

The goal of the project was to create a complex File System without a GUI.
You could interact with the File System via the command line.

The project was written in C and explored concepts like Multi-Threading (mutexes, locks, ...), iNodes and pipelining.

# How to run

You can't interact with this project with a CLI.     

The only way to interact with the project is by passings tests, which are C programs that include the project files.  
There are tests already created in the `fs/tests` folder.

To run the project with a test, run `compileAndRunSpecificTest.sh *test*` in the `fs` folder.  
To run all tests, run `compileAndRunEverySUCCESStest.sh`.
