##### **# A.S.T.R.A. (Abstract Syntax Tree Routing \& Analysis)**



ASTRA is a lightweight, high-performance polyglot daemon written in C++ that builds an in-memory directed graph of your codebase architecture. By leveraging `tree-sitter` for native parsing and Windows Named Pipes for Inter-Process Communication (IPC), ASTRA allows external tools (CLI clients, AI agents, or text editors) to query code dependencies with $O(1)$ time complexity.



###### **Core Architecture**



Unlike traditional static analysis tools that parse files on every request, ASTRA operates as a stateful OS-level daemon:

1\. Ingestion: A client pumps source code through the IPC pipe.

2\. Parsing: ASTRA routes the code to the correct `tree-sitter` grammar based on the file extension.

3\. Graph Building: It extracts function signatures and local dependencies (call expressions), mapping them into an Adjacency List in the RAM.

4\. Querying: Subsequent queries hit the RAM directly, returning dependency chains in milliseconds.



###### **Supported Languages**

\* C / C++

\* TypeScript / JavaScript

\* Python

\* Java

\* C#

\* Go



###### **Getting Started**



**### Prerequisites:**

\* GCC/G++ Compiler (MinGW on Windows)

\* Git



**### Clone the repository and grammars:**

```bash

git clone \[https://github.com/YOUR\_USERNAME/ASTRA.git](https://github.com/YOUR\_USERNAME/ASTRA.git)

cd ASTRA



Fetch required tree-sitter grammars

git clone \[https://github.com/tree-sitter/tree-sitter-c-sharp.git](https://github.com/tree-sitter/tree-sitter-c-sharp.git)

git clone \[https://github.com/tree-sitter/tree-sitter-cpp.git](https://github.com/tree-sitter/tree-sitter-cpp.git)

git clone \[https://github.com/tree-sitter/tree-sitter-go.git](https://github.com/tree-sitter/tree-sitter-go.git)

git clone \[https://github.com/tree-sitter/tree-sitter-java.git](https://github.com/tree-sitter/tree-sitter-java.git)

git clone \[https://github.com/tree-sitter/tree-sitter-python.git](https://github.com/tree-sitter/tree-sitter-python.git)

git clone \[https://github.com/tree-sitter/tree-sitter-typescript.git](https://github.com/tree-sitter/tree-sitter-typescript.git)



**### Build the Object Files:**

Compile the tree-sitter parsers into machine code (C to .o)



gcc -c tree-sitter-cpp/src/parser.c -o parser.o -I tree-sitter-cpp/src

gcc -c tree-sitter-cpp/src/scanner.c -o scanner.o -I tree-sitter-cpp/src

gcc -c tree-sitter-typescript/typescript/src/parser.c -o ts\_parser.o

gcc -c tree-sitter-typescript/typescript/src/scanner.c -o ts\_scanner.o

gcc -c tree-sitter-python/src/parser.c -o py\_parser.o

gcc -c tree-sitter-python/src/scanner.c -o py\_scanner.o

gcc -c tree-sitter-java/src/parser.c -o java\_parser.o

gcc -c tree-sitter-go/src/parser.c -o go\_parser.o

gcc -c tree-sitter-c-sharp/src/parser.c -o cs\_parser.o

gcc -c tree-sitter-c-sharp/src/scanner.c -o cs\_scanner.o



**### Build the Daemon:**

Link everything together into the final executable



```bash

g++ ipc\_server\_windows.cpp parser.o scanner.o ts\_parser.o ts\_scanner.o py\_parser.o py\_scanner.o java\_parser.o go\_parser.o cs\_parser.o cs\_scanner.o -o astra\_daemon.exe



###### **Usage**



Start the daemon in your terminal. It will open a Named Pipe (\\\\.\\pipe\\astra\_engine\_pipe) and listen for connections:



```bash

./astra\_daemon.exe



In a separate terminal, run the provided PowerShell client to ingest a codebase (e.g., a React/Node backend, or a local project) and start querying:



```bash

./test\_agent.ps1



ASTRA CLI Client

\----------------

Enter project path to scan: C:\\Users\\Alex\\Projects\\VitaLife

Found 42 source files. Starting ingestion...

Index build complete.



astra> validateUserData

Found: function validateUserData(payload: UserDTO)

&#x20; └─ Calls: sanitizeInput(), checkDbConstraint(), generateHash(),



###### **IPC Protocol Specification**



If you want to build your own client (e.g., in Python, Rust, or a VSCode extension), simply connect to the named pipe and send strings formatted as follows:



Ingest Code: ANALYZE:.extension|raw\_source\_code\_here



Query Graph: QUERY:function\_name



###### **Contributing**



Contributions are highly encouraged. Areas where the project could currently use help:



Linux/macOS Support: Abstracting the Windows Named Pipes to Unix Domain Sockets.



New Clients: Building a Python or Node.js client to interface with the daemon.



Advanced AST Traversals: Extracting class/struct definitions and variable scopes.



Please open an issue to discuss architectural changes before submitting large Pull Requests.



###### **License**



MIT LICENSE







