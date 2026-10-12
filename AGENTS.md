# libeditingcore

## Rule: keep the consumers' pins in sync

lite-xl and thither fetch this library with CMake FetchContent, pinned to an
exact libeditingcore commit. Every time you commit to this repo, update both
pins to the new commit in the same piece of work:

- `../lite-xl/CMakeLists.txt`, in `FetchContent_Declare(editingcore ...)`
- `../thither/CMakeLists.txt`, in `FetchContent_Declare(editingcore ...)`

How:

1. Commit here, then get the full hash with `git rev-parse HEAD`. Use the full
   40-character hash, not a short one.
2. Set `GIT_TAG` in both files to that hash, or to a release tag if the commit
   was tagged.
3. FetchContent clones from GitHub, so the commit only resolves once it has
   been pushed to `origin`. If it hasn't been pushed yet, say so, and don't push
   without asking.
4. Check that both consumers build against the new pin. A quick way is a build
   with `-DFETCHCONTENT_SOURCE_DIR_EDITINGCORE=../libeditingcore` from the
   committed tree. thither is POSIX-only, so build it under WSL on Windows.
5. Commit each pin bump in that repo, on its current branch. Mention the
   libeditingcore commit in the message, e.g.
   `bump libeditingcore to <short-hash>: <subject>`.

If an API change here breaks lite-xl or thither, fix the consumer in the same
pass instead of leaving its pin behind.
