# Mini File System in the browser

This bridge compiles the C partition and file drivers to WebAssembly. It exposes one demo file and a 50-byte virtual disk so every operation can be inspected. Each engine instance owns an in-memory disk image. Remount reads the same image with the descriptor closed. A new instance starts with no disk.

Activate [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) version `4.0.23`, then run these commands from the repository root. Node 22 runs the same WebAssembly module for the check.

```sh
bash browser/build.sh
node browser/check.mjs
```

Serve `mini-fs-engine.mjs`, `mini-fs-core.mjs`, and `mini-fs-core.wasm` from the same directory. Serve the binary with `Content-Type: application/wasm`. `createMiniFsEngine()` can be imported in a module worker or a page module. The factory is asynchronous and the returned operations are synchronous.

Call `format()`, `open()`, `write(text)`, `close()`, `seek(offset)`, `read()`, `remount()`, and `snapshot()`. Operations return `{ code, output, state }`. A nonnegative code indicates success. Successful reads and writes return a byte count, and read results include their text in `output`. A negative code identifies a driver error or an input error. A negative seek offset is measured from the end of the file. A nonnegative offset is measured from the start. A clamped seek still returns the driver error code and the updated cursor.

The snapshot carries the actual partition dimensions, next free block, file allocation table, physical bytes, and descriptor. Blank disk bytes are `0`. A written ASCII digit `0` is byte `48`.
