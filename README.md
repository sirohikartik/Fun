# Fun

Some random fun stuff — small C experiments and utilities.

## Projects


### Http Server 

A minimal http server that serves on a port and responds to a get route request with my portfolio page html.

**Files**
- `server.c` - main source

**Build & Run**
```bash
cd http-server
clang server.c -o server
./server
```

###  FileManager

A simple directory listing tool written in C.

It builds a basic tree-like structure of the current directory and prints the names of all entries (files and folders).

**Files**
- `files.c` — main source

**Build & Run**
```bash
cd FileManager
gcc files.c -o files
./files
```
