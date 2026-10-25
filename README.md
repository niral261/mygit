# AOS Assignment 4 — Mini Version Control System

C++17 implementation for roll number **2026201045**, matching the roll number in the supplied Assignment 3 ZIP. All nine Assignment 4 commands are implemented. The project follows the supplied code's class-based design, camelCase identifiers, separate `include`/`src` directories, four-space indentation, header guards, and short purpose comments.

## 1. Build and run

Requirements: Linux, a C++17 compiler (GCC 9 or newer recommended), GNU make, and zlib development headers/library. No OpenSSL, libgit2, Git subprocesses, database library, or external SHA1 implementation is used by the application. The permitted zlib compression library is the only linked external library. Git and Python are used only by optional tests.

On Ubuntu/Debian, if these dependencies are missing:

```sh
sudo apt install g++ make zlib1g-dev
```

From this directory:

```sh
make
./mygit --help
```

The executable is `mygit`. To build with another compiler, use `make CXX=clang++` if available. `make clean` deletes only built executables, never repository data.

**Use a separate demonstration directory**, rather than running the example in the source directory:

```sh
mkdir ../mygit-demo
cp mygit ../mygit-demo/mygit
cd ../mygit-demo
./mygit init
```

The root file named `mygit` is excluded from snapshots so checkout can keep the command executable available. All other ordinary files, including hidden files, are eligible. `.mygit` and `.git` directories are always excluded. There is no `.gitignore` parser.

## 2. Commands and features

| Command | Implemented behavior |
|---|---|
| `./mygit init` | Creates `.mygit`, its object store, format marker, index and HEAD; repeating initialization preserves history |
| `./mygit hash-object [-w] <file>` | Prints the Git-style SHA1 of a blob; `-w` compresses and stores it |
| `./mygit cat-file -p <sha>` | Prints blob bytes without adding a newline, readable tree entries, or commit text |
| `./mygit cat-file -s <sha>` | Prints the uncompressed payload size in bytes, excluding the object header |
| `./mygit cat-file -t <sha>` | Prints `blob`, `tree` or `commit` |
| `./mygit write-tree` | Stores a snapshot of the current repository working directory and prints its tree SHA |
| `./mygit ls-tree [--name-only] <tree_sha>` | Lists immediate tree entries; detailed form prints mode, type, SHA and name |
| `./mygit add .` | Stages current files under the current directory, updates changed entries and stages removals |
| `./mygit add <path> [path ...]` | Stages specific files/directories, including previously staged paths which are now deleted |
| `./mygit commit [-m <message>]` | Commits the index, records parent/time/committer/message and updates HEAD; omitted message becomes `Default commit message` |
| `./mygit log` | Follows HEAD's parent chain from newest to oldest and prints the required metadata |
| `./mygit checkout <commit_sha>` | Restores the tracked snapshot, executable modes and index; updates HEAD after successful restoration |

All SHA arguments must be complete **40-character lowercase hexadecimal** strings. Abbreviated hashes, branches and tags are outside this assignment. Quotes for filenames or commit messages are supplied by the shell:

```sh
./mygit add "notes with spaces.txt"
./mygit commit -m "First version of the project"
```

A successful command exits with code 0. Invalid usage, corrupt data and handled I/O failures print `Error: ...` to standard error and exit with code 1. `./mygit --help` prints usage.

Without `-w`, `hash-object` works even outside a repository. Other commands require an initialized repository, except `init` and help. Commands search the current directory and its ancestors for `.mygit`; path arguments are relative to the directory from which the command was invoked. `write-tree` always snapshots the repository root.

## 3. Short demonstration

After initializing the separate demonstration directory:

```sh
printf 'hello world' > hello.txt
mkdir src
printf 'version one\n' > src/sample.txt
./mygit hash-object -w hello.txt
./mygit add .
./mygit commit -m "Initial snapshot"
```

Copy the first commit SHA printed by `commit`. Then:

```sh
printf 'version two\n' > src/sample.txt
./mygit add src/sample.txt
./mygit commit -m "Update sample"
./mygit log
./mygit write-tree
```

Use the tree SHA from `write-tree` with `ls-tree`, and a blob SHA from its listing with `cat-file`. Replace the placeholder below with the actual first commit SHA:

```sh
./mygit checkout <first_commit_sha>
cat src/sample.txt
```

The file now contains `version one`. To stage a deletion:

```sh
rm hello.txt
./mygit add hello.txt
./mygit commit -m "Remove hello"
```

Do not type the angle-bracket placeholders literally; the shell interprets them as redirection.

## 4. Working procedure

1. **Initialize:** construct the metadata in `.mygit.init`, then rename the complete directory to `.mygit`.
2. **Store objects:** create `type + space + decimal size + NUL + payload`, compute SHA1, compress with zlib, and save under `.mygit/objects/<first two hex digits>/<remaining digits>`.
3. **Stage:** read the current index, apply every requested path to a temporary in-memory map, store its blobs, then atomically replace the index. A failed multi-path add leaves the old index intact; harmless unreferenced objects may remain.
4. **Commit:** build recursive trees from the staged map, store a commit referencing the root tree and optional parent, then atomically update HEAD. The index remains the complete staged snapshot after a commit.
5. **Read history:** load each commit, print its metadata and follow its parent until the first commit.
6. **Checkout:** validate the target and current trees, reject dirty/staged/colliding paths, prepare all new files under `.mygit/checkoutTxn/new`, move old tracked files into its `old` directory, install the target, update index and HEAD, then remove the transaction. Handled failures attempt rollback. Backups remain available if rollback fails.

**Staging is a snapshot.** Editing a file after `add` does not alter its staged bytes. A later `commit` includes the bytes captured by `add`. To include newer edits, add the file again.

`write-tree` intentionally follows the assignment's working-directory wording. Unlike the original Git command, it does not use the index. `commit` does use the index. Calling `write-tree` alone does not stage files.

## 5. Classes and source files

`main` is the required C++ entry function; application operations live in classes. There is no unnecessary inheritance hierarchy.

| Header/source pair | Responsibility and assignment mapping |
|---|---|
| `include/hash.h`, `src/hash.cpp` | Incremental SHA1; reused from the supplied ZIP; object identity |
| `include/fileUtil.h`, `src/fileUtil.cpp` | Bounded binary file reads, exclusive writes, atomic metadata replacement, permissions, safe names and RAII writer lock |
| `include/objectStore.h`, `src/objectStore.cpp` | Blob/tree/commit envelopes, hashing, zlib storage, bounded decompression and integrity checks; commands 2 and 3 |
| `include/snapshot.h`, `src/snapshot.cpp` | `Entry`, `TreeEntry`, `Index` and `Snapshot`; index validation, recursive binary trees and tree listings; commands 4, 5 and staging storage |
| `include/commit.h`, `src/commit.cpp` | Commit serialization, parent, timestamp and committer metadata; commands 7 and 8 |
| `include/repository.h`, `src/repository.cpp` | Repository discovery/init, staging, snapshot creation, commit/log and checkout; commands 1 and 4–9 |
| `include/command.h`, `src/command.cpp` | CLI argument validation and command dispatch |
| `src/main.cpp` | Constructs the command class, catches exceptions and sets the exit code |
| `makefile` | Builds all source files and links `-lz`; test and sanitizer targets |
| `tests/integration.py` | Optional black-box command and failure tests; no Python application code |
| `TECHNICAL_REPORT.md` | Architecture, file formats, algorithms and viva notes |
| `TEST_RESULTS.md` | Actual verification results and untested conditions |

## 6. On-disk structure

```text
.mygit/
    format                MYGIT_1 format marker
    HEAD                  current commit SHA plus newline; newline alone before first commit
    index                 complete staged path -> mode/SHA map
    objects/
        ab/cdef...        zlib-compressed object with SHA abcdef...
    LOCK                  exists only while a repository operation runs normally
    checkoutTxn/          temporary checkout data; retained on interruption/failed rollback
```

Tree and blob object encodings are compatible with Git loose objects. Commit objects use Git-style headers and a UTC Unix timestamp. The index and HEAD management are deliberately simpler and are not a Git repository format; do not point Git write commands at `.mygit`.

Committer defaults to `Student <student@localhost>`. Set identity before committing if desired:

```sh
export MYGIT_AUTHOR_NAME="Your name"
export MYGIT_AUTHOR_EMAIL="you@example.com"
```

No personal name is embedded in the source or required to build.

## 7. Error cases and assumptions

- Binary files, empty files, spaces in names, ordinary hidden files, nested directories and executable files are supported.
- Only regular files and real directories are tracked. Symbolic links, FIFOs, devices and sockets are rejected; FIFO reads do not block.
- Paths cannot escape the repository or enter `.mygit`/`.git`. Names containing control bytes, backslashes or path separators are rejected. Backslash is intentionally unsupported even though Linux permits it in filenames.
- Like normal Git staging, empty directories are not represented as tracked entries. Trees contain directories needed by tracked files. Checkout restores the tracked snapshot and preserves unrelated untracked files; it does not erase the entire directory.
- Checkout refuses any unstaged tracked change, missing tracked file, executable-mode change, staged change, or untracked file collision. There is no force option; preserve your work or commit it first. This conservative policy also refuses dirty tracked files which a particular checkout would leave unchanged.
- File/directory replacement is supported when it would not remove an untracked file. An empty untracked directory at a required file path may be replaced.
- Commits with no staged difference are rejected. An initial empty commit is rejected; a later commit deleting all tracked files is valid.
- Files are read into bounded memory. File reads, raw object envelopes and compressed object files have a **64 MiB** bound; object headers and compression overhead can make the largest storable payload slightly smaller. A maximum of 100,000 tracked files, 100,000 tree visits per read and 128 directory levels is enforced. This is an assignment-scale implementation, not a large-repository tool.
- Every loaded object is decompressed with a bound and checked against its SHA, type and declared size. Trees and commits undergo structural checks before restoration. SHA1 detects ordinary corruption but is not suitable for modern adversarial authentication.
- A lock prevents concurrent mygit repository commands. Hashing without `-w` does not need a lock. The working tree must not be concurrently edited by another program during add/checkout; no protection against hostile external filesystem races is claimed.
- Full POSIX permission sets, ownership, modification times, ACLs and hard-link relationships are not versioned. Files restore as `0644` or `0755`.
- HEAD directly names a commit. Checkout followed by commit starts a new parent chain from the checked-out commit. Old objects remain addressable by saved SHA; there are no branches, reflog, merge, push, reset or garbage collection commands.
- File contents are flushed, and metadata is replaced using rename. Full power-loss durability and atomic restoration of a multi-file working directory across a process crash are not guaranteed.

## 8. Interrupted-operation recovery

Normal exceptions trigger rollback; a forcibly killed process can leave `LOCK`, temporary files or `checkoutTxn`. The application refuses to use a repository with a remaining checkout transaction.

Before removing a stale lock, ensure **no mygit process is running**. Back up the entire repository and working directory before manual recovery. Do not blindly remove a checkout transaction; it can contain the only copy of old tracked files.

For an interrupted checkout:

1. `.mygit/checkoutTxn/HEAD` and `index` hold the original metadata. `targetIndex` lists proposed target paths; `old/` contains tracked files moved out of the original working directory, with their original paths and modes. `new/` contains target files not yet installed.
2. If `READY` is absent, no original working file has been moved: preserve your backup, then remove the incomplete transaction directory.
3. If `READY` is present, choose to recover the old snapshot: inspect `targetIndex`, remove only target files already installed, restore every file from `old/` to its original path, and restore the saved `HEAD` and `index`. Original files not present in `old/` may not have been moved yet; compare with the saved index and object data before removing them. Do not remove unrelated untracked files.
4. Verify the restored files and metadata against your backup and saved index. Only then remove the transaction, stale `.tmp` files and `LOCK` and resume commands.

This is a manual recovery procedure, not a claim of automatic crash recovery. A stale `.mygit.init` should likewise be inspected and backed up before removal.

## 9. Tests

```sh
make test
make sanitize
```

Python 3 is required only for these test drivers. Tests use temporary directories; they do not alter a repository you created for a demonstration. If Git is available, three interoperability assertions also compare blob hashes, tree listings and commit bytes with real Git. The driver never invokes Git from the VCS application.

The sanitizer target checks AddressSanitizer and UndefinedBehaviorSanitizer. LeakSanitizer is disabled because the managed test environment restricts `/proc` access needed by its process scanner. See `TEST_RESULTS.md` for actual results and limitations.

The ZIP includes source and tests; run `make` to produce the executable for your own Linux environment.
