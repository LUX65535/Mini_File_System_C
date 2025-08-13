# File_System_C

A compact, well-structured **file system + shell I/O layer in C**.  

---

## Overview

- Architected and developed a foundational file system module as an extension to a fully operational custom kernel and memory manager, enhancing the system's capabilities for file management and disk operations.
- Engineered a disk driver in C, encapsulating low-level I/O operations to interface with the storage hardware, including initialization routines, partitioning logic, file system mounting, and block data management. Implemented essential on-disk data structures such as the partition table and file allocation table (FAT).
- Designed and implemented a high-level API enabling user interaction with the file system, facilitating operations such as mounting partitions, writing to files, and reading files into script variables.

---

## Getting Started

### Build

```bash
./recompile.sh
```


---

## Quick Demo

```text
mount partition4 10 5
open_EXAM 0 file1
write_EXAM 0 [hello world test]
close_EXAM 0

open_EXAM 0 file1
read_EXAM 0 | set x
seek_EXAM 0 -4
read_EXAM 0 var
print var            # -> test
close_EXAM 0
print x              # -> hello world test
quit
```

**Expected output**
```
test
hello world test
```

This session demonstrates:
- file creation and writing,
- redirecting the full content into a variable (`x`),
- seeking backward and reading the tail into another variable (`var`),
- buffer ownership tied to the FD (variables hold copies and remain valid after `close`).

---
