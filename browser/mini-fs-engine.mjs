import createCore from './mini-fs-core.mjs';

const INVALID_INPUT = -101;
const MAX_INPUT_BYTES = 50;

export async function createMiniFsEngine(options = {}) {
    const core = await createCore(options);

    function snapshot() {
        const json = core.ccall('mini_fs_snapshot', 'string');
        if (!json) throw new Error('Could not inspect the virtual disk.');
        return JSON.parse(json);
    }

    function result(code, output = null) {
        return { code, output, state: snapshot() };
    }

    return {
        format: () => result(core._mini_fs_format()),
        open: () => result(core._mini_fs_open()),
        write(text) {
            if (typeof text !== 'string' || !text.length || text.length > MAX_INPUT_BYTES || /[^ -~]/.test(text)) {
                return result(INVALID_INPUT);
            }
            return result(core.ccall('mini_fs_write', 'number', ['string'], [text]));
        },
        close: () => result(core._mini_fs_close()),
        seek(offset) {
            if (!Number.isInteger(offset) || offset < -2147483648 || offset > 2147483647) {
                return result(INVALID_INPUT);
            }
            return result(core._mini_fs_seek(offset));
        },
        read() {
            const code = core._mini_fs_read();
            const output = code >= 0 ? core.ccall('mini_fs_read_output', 'string') : null;
            return result(code, output);
        },
        remount: () => result(core._mini_fs_remount()),
        snapshot: () => result(0),
    };
}
