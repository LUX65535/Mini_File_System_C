import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createMiniFsEngine } from './mini-fs-engine.mjs';

const wasm = await readFile(new URL('./mini-fs-core.wasm', import.meta.url));
assert.equal(WebAssembly.validate(wasm), true);

const engine = await createMiniFsEngine();
assert.equal(engine.snapshot().state.mounted, false);
assert.equal(engine.open().code, -100);

const formatted = engine.format();
assert.equal(formatted.code, 0);
assert.equal(formatted.state.blockSize, 5);
assert.equal(formatted.state.totalBlocks, 10);
assert.equal(formatted.state.nextFreeBlock, 0);
assert.deepEqual(formatted.state.files, []);
assert.equal(formatted.state.descriptor, null);
assert.equal(formatted.state.blocks.length, 10);
assert.ok(formatted.state.blocks.every(({ bytes }) => bytes.length === 5 && bytes.every(byte => byte === 0)));

assert.equal(engine.open().code, 0);
assert.equal(engine.open().code, -2);
assert.equal(engine.read().output, '');
assert.equal(engine.write('').code, -101);
assert.equal(engine.write('é').code, -101);
assert.equal(engine.write('a'.repeat(51)).code, -101);

const sample = 'hello 0orld test';
const written = engine.write(sample);
assert.equal(written.code, 16);
assert.deepEqual(written.state.files, [{ name: 'report.txt', length: 16, blocks: [0, 1, 2, 3] }]);
assert.equal(written.state.nextFreeBlock, 4);
assert.deepEqual(written.state.descriptor, { fd: 0, cursor: 16, file: 'report.txt' });
assert.deepEqual(written.state.blocks.slice(0, 4).flatMap(({ bytes }) => bytes), [
    ...Buffer.from(sample), 0, 0, 0, 0,
]);

assert.equal(engine.close().state.descriptor, null);
assert.equal(engine.read().code, -3);
assert.equal(engine.open().state.descriptor.cursor, 0);
const positioned = engine.seek(-4);
assert.equal(positioned.code, 0);
assert.equal(positioned.state.descriptor.cursor, 12);
const tail = engine.read();
assert.equal(tail.code, 4);
assert.equal(tail.output, 'test');
assert.equal(tail.state.descriptor.cursor, 16);
const clamped = engine.seek(-99);
assert.equal(clamped.code, -5);
assert.equal(clamped.state.descriptor.cursor, 0);

const remounted = engine.remount();
assert.equal(remounted.code, 0);
assert.equal(remounted.state.descriptor, null);
assert.deepEqual(remounted.state.files, written.state.files);
assert.deepEqual(remounted.state.blocks, written.state.blocks);
assert.equal(engine.open().code, 0);
assert.equal(engine.read().output, sample);

const full = '1234567890'.repeat(5);
assert.equal(engine.seek(0).code, 0);
const filled = engine.write(full);
assert.equal(filled.code, 50);
assert.equal(filled.state.nextFreeBlock, 10);
assert.ok(engine.write('x').code < 0);
assert.deepEqual(engine.snapshot().state, filled.state);
assert.equal(engine.remount().code, 0);
assert.equal(engine.open().code, 0);
assert.equal(engine.read().output, full);

const separateEngine = await createMiniFsEngine();
assert.equal(separateEngine.snapshot().state.mounted, false);
assert.equal(separateEngine.format().code, 0);
assert.deepEqual(separateEngine.snapshot().state.files, []);
assert.deepEqual(engine.snapshot().state.files, filled.state.files);

const reset = engine.format();
assert.equal(reset.code, 0);
assert.deepEqual(reset.state, formatted.state);
console.log('WebAssembly format, physical bytes, seek/read, remount, capacity, and reset passed.');
