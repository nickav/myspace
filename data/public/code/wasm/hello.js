const decoder = new TextDecoder("utf8");

const DecodeString = (u8Array, idx, length) => {
    if (!idx) return "";
    const endPtr = idx + length;
    return decoder.decode(u8Array.subarray(idx, endPtr));
}

const StringLength = (u8Array, ptr) => {
    let endPtr = ptr;
    while (u8Array[endPtr]) ++endPtr;
    return endPtr - ptr;
}

const encoder = new TextEncoder("utf8");

// NOTE(nick): the output space needed is >= s.length and <= s.length * 3
const EncodeString = (u8Array, base, string) => {
    if (!string.length) return 0;
    return encoder.encodeInto(string, u8Array.subarray(base)).written;
}

const Kilobytes = (x) => 1024 * x;
const Megabytes = (x) => 1024 * 1024 * x;

const load = async (wasmPath) => {
    const response = await fetch(wasmPath);
    const bytes = await response.arrayBuffer();

    // NOTE(nick): WASM pages are 64K each
    const numPages = Megabytes(64) / Kilobytes(64);

    const memory = new WebAssembly.Memory({ initial: numPages });
    const heap = new Uint8Array(memory.buffer);

    const wasm = await WebAssembly.instantiate(bytes, {
        env: {
            memory,
            js__output: (str, size) => {
                const result = DecodeString(heap, str, size);
                console.log("[hello]", result);
            },
            js__sin: Math.sin,
        },
    });

    const { instance } = wasm;

    return {
        add: (x, y) => {
            return instance.exports.add(x, y);
        },

        secret_message: () => {
            const ptr = instance.exports.secret_message();
            return DecodeString(heap, ptr, StringLength(heap, ptr));
        },

        ping: (message) => {
            const ptr = instance.exports.wasm__push(3 * message.length);
            const count = EncodeString(heap, ptr, message);
            instance.exports.ping(ptr, count);
            instance.exports.wasm__pop(3 * message.length);
        },

        compute_square_roots: (array) => {
            const cArrayPtr = instance.exports.wasm__push(8 * array.length);
            const cArray = new Float64Array(memory.buffer, cArrayPtr, array.length);
            cArray.set(array);

            instance.exports.compute_square_roots(cArrayPtr, array.length);

            instance.exports.wasm__pop(8 * array.length);

            return Array.from(cArray);
        },

        push_f64_array: (length) => {
            const ptr = instance.exports.push_f64_array(length);

            // NOTE(nick): read the memory directly
            const dataView = new DataView(memory.buffer);
            let at = ptr;
            const count = dataView.getUint32(at, true);
            at += 4;
            // NOTE(nick): pointers are u32
            const data = dataView.getUint32(at, true);
            at += 4;

            const result = new Float64Array(memory.buffer, data, count);
            return result;
        },
    };
};

const main = async () => {
    const hello = await load('/hello.wasm');

    console.log("hello.add(42, 42):", hello.add(42, 42));
    console.log("secret_message():", hello.secret_message());
    console.log(`ping("hello, good sir"):`, hello.ping("hello, good sir"));
    console.log(`compute_square_roots([1, 2, 3, 4, 5]):`, hello.compute_square_roots([1, 2, 3, 4, 5]));
    console.log(`push_f64_array(10):`, hello.push_f64_array(10));
};

main();
