---
title: "WASM from Scratch"
desc:  "A practical guide for working with Web Assembly"
date:  "2023-08-23 23:23:23"
image: "3.jpg"
author: "Nick Aversano"
draft: false
---

I recently was trying to figure out how to compile my C code to WASM.
If you search the internet for this, you'll find a _ton_ of resources that say to use `emscripten`. Even [MDN](https://developer.mozilla.org/en-US/docs/WebAssembly/C_to_Wasm) says this.
While emscripten is not a bad choice, it does _a lot_ for you.
Not to mention that installing it can be a bit of a pain.
If you're like me and prefer to take a more [handmade](https://handmade.network) approach to building software, it can be difficult to figure out everything you have to do to just compile some C code to WASM and run it.

Here is a comprehensive guide to what I learned after searching the internet and talking with some friends:


## Basics

Writing for Web Assembly is similar to writing for an embedded system.
So if you've already taken care to keep your [dependencies minimal](https://github.com/nickav/na/blob/master/na.h), this should be pretty straightforward.
If you're starting a new project I *strongly* recommend this.

Let's say you have the world's most basic C program:
```c
// File: hello.c
typedef signed int i32;

// NOTE(nick): tell clang to export the function and call it "add"
__attribute__((export_name("add")))
i32 add(i32 a, i32 b) {
    return a + b;
}
```

`clang` can compile our C code directly to WASM with a few additional command-line arguments.
If you run `clang -print-targets` you can see the available compile targets. And indeed, we can see both `wasm32` and `wasm64` are present.
Great! Let's try compiling with `clang` and specify we want to use the `wasm32` build target:

```bash
> clang --target=wasm32 -o hello.wasm hello.c
```


But if you just try to run this, you'll get the following error:
```bash
C:\dev\myspace> clang --target=wasm32 -o hello.wasm hello.c
wasm-ld: error: cannot open crt1.o: no such file or directory
wasm-ld: error: unable to find library -lc
wasm-ld: error: cannot open C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\Llvm\lib\clang\12.0.0\lib\libclang_rt.builtins-wasm32.a: no such file or directory
clang: error: linker command failed with exit code 1 (use -v to see invocation)
```

This is because, much like embeded software, there is no C standard library!
OK, no problem, we've got the `--no-standard-libraries` flag. So let's try that and get another error:

```bash
C:\dev\myspace> clang --target=wasm32 --no-standard-libraries -o hello.wasm hello.c
wasm-ld: error: entry symbol not defined (pass --no-entry to suppress): _start
clang: error: linker command failed with exit code 1 (use -v to see invocation)
```

This is clang telling us that here is no `_start` function (the function that eventually calls `int main` in a typical C program). OK, well it looks like there's a suggestion there too so let's just using that. The thing is `wasm-ld` is giving us the error. To pass a `wasm-ld` linker flag you have to prefix it with `-Wl,`, so the actual flag looks like: `-Wl,--no-entry`.

I will now take the liberty to add a few more compiler flags that will matter later:

`-Wl,--no-entry` - no entry point (we're just building a library)

`-Wl,--import-memory` - import our memory from the JS environment

`-mbulk-memory` - tell the compiler not to be too clever and replace our loops with `memset` (because remember, no C standard library)

I also added the `-O1` optimization flag to make the output a little bit more concise.

Note that we explicitly told `clang` to export our function with the special `__attribute__((export_name("add")))` decorator.
If you prefer to export all the symbols in your program, you can do that with the `-Wl,--export-all` linker flag.
If you're curious, here's a list of [LLVM linker flags](https://lld.llvm.org/WebAssembly.html) you can use.

So now we can compile our program again:

```bash
> clang -O1 --target=wasm32 --no-standard-libraries -mbulk-memory -Wl,--no-entry -Wl,--import-memory -o hello.wasm hello.c
```

And that works!

<div class="message">
💡 Web Assembly is a binary format, and similar to machine assembly there is a text format called WAT (Web Assembly Text).
</div>

You can view the WAT of the WASM binary directly in the browser. Go to the Network tab, then click Preview:

```c
// File: hello.wat
(module
  (memory $env.memory (;0;) (import "env" "memory") 2)
  (global $__stack_pointer (;0;) (mut i32) (i32.const 66560))
  (func $add (;0;) (export "add") (param $var0 i32) (param $var1 i32) (result i32)
    local.get $var1
    local.get $var0
    i32.add
  )
)
```


You can also use a CLI tool like [wasm2wat](https://webassembly.github.io/wabt/demo/wasm2wat/) to disassemble the binary.


## Using the WASM binary

To use the WASM binary, we need a runtime environment.
We can use the browser for this, but there are [other](https://github.com/wasmerio/wasmer) [WASM](https://github.com/bytecodealliance/wasmtime) [runtimes](https://github.com/wasm3/wasm3).

Here is a simple HTML file to load our WASM:

```js
// File: hello.html
<!doctype html>
<html>
<body>
<script>
    const run = async () => {
        const response = await fetch('/hello.wasm');
        const bytes = await response.arrayBuffer();

        const numPages = 4;
        const memory = new WebAssembly.Memory({ initial: numPages });

        const wasm = await WebAssembly.instantiate(bytes, {
            env: {
                memory,
            },
        });

        const { instance } = wasm;

        console.log(instance.exports.add(42, 42));
    };

    run();
</script>
</body>
</html>
```

Unfortunately, because of "security reasons" you can't just open this HTML file in your browser. You need to run a local web server to dynamically fetch content.

Any of the following commands will work:

```bash
> python -m http.server 8000

> npx serve -p 8000
```

Now navigate to [http://localhost:8000/hello.html](http://localhost:8000/hello.html) and open the developer console. You should see:

```js
84
```


## Built-Ins

Clang has built-in functions for a few things such as:

```c
f64 sqrt(f64 x) {
    return __builtin_sqrt(x);
}
```

This will _just work_ in WASM because there is are `f32.sqrt` and `f64.sqrt` instructions!

There are a few more notable built-ins:

```c
__builtin_sqrt(x); // compiles to WASM f32.sqrt or f64.sqrt instruction
__builtin_wasm_memory_size(0); // the number of 64Kb pages we have
__builtin_wasm_memory_grow(0, blocks); // increases amount of pages
__builtin_huge_valf(); // similar to Infinity in JS
```

There are no functions for `sin` and `cos` for example. For these you can: copy their source from [MUSL](https://www.musl-libc.org/), write them yourself, or expose them from JavaScript.

When in doubt, you can actually _look_ at what how [emscripten](https://github.com/emscripten-core/emscripten) implemented a feature you want.
Emscripten does a lot of weird and clever things to emulate certain native features.
For example, threads don't exist in WASM. Everything is single-threaded!


## Imports

Because our WASM code is running in it's own sandbox, it will need to talk to the outside world.
In order to expose some JavaScript callbacks, you use the following syntax:

```c
__attribute__((import_module("env"), import_name("js__sin")))
f64 js__sin(f64 x);
```

This tells `clang` that there is some external function that will be provided when the WASM module starts up.
Note that you _must_ provide the function now otherwise the WASM module will fail to initialize.

And then add the corresponding `js__sin` function to the `env` property of the WASM module:
```c
const wasm = await WebAssembly.instantiate(bytes, {
    env: {
        memory,
        js__sin: Math.sin, // added
    },
});
```

Now you can call `js__sin` from C.

This is also, for example, how you would do OpenGL bindings in WASM.


## Memory

When we booted up our WASM code, we told it exactly how many memory pages it has to run in. Each memory page is 64KB.

Our whole program must run with the memory that it's given.
While there is a builtin to request more memory from the WASM runtime, I prefer to just declare all the memory I will ever use upfront.

<div class="message">
⚠️ If you just try to write directly into your WASM program's memory you will be writing over the stack from `[0, &__heap_base)`.
</div>

You can get the heap size and base pointer with:

```c
//
// NOTE(nick): clang will set the address of this variable to be in the
// memory location of the heap base
//
extern u32 __heap_base;

u64 wasm__heap_size_in_bytes()
{
    return __builtin_wasm_memory_size(0) << 16;
}

void *wasm__heap_pointer()
{
    // IMPORTANT! get the _address_ of the __heap_base symbol
    return (void *)&__heap_base;
}
```

Now we can define our own global allocator with a simple bump allocator:

```c
static u32 offset = 0;

u8 *wasm__push(u32 size)
{
    u8 *result = NULL;

    // NOTE(nick): align up to the nearest 16 bytes
    size = (size + 15) & ~15;

    if (offset + size < wasm__heap_size_in_bytes())
    {
        result = (u8 *)(wasm__heap_pointer()) + offset;
        offset += size;
    }

    return result;
}

void wasm__pop(u32 size)
{
    // NOTE(nick): align up to the nearest 16 bytes
    size = (size + 15) & ~15;

    if ((i64)offset - size > 0) {
        offset -= size;
    } else {
        offset = 0;
    }
}
```

<div class="message">
⚠️ Note that we always align our allocations because some data structures like `Float64Array` require that the memory is 8-byte aligned!
</div>

Pointers will be `u32`-sized because we are using `wasm32`.
As far as I can tell, the most you can return from a native function via the WASM FFI is a `u64` and this gets returned to JavaScript as a `BigInt`.
If you need to read or write more than that, then you can use the `wasm__push` and `wasm__pop` functions.
Another strategy would be to just agree on a reserved address space for JS-WASM communication, sort of like a global scratch buffer.

There is no `malloc` or `free` (again, no standard library).
If you absolutely can't live without them, you can define your own or find an implementation online.
In practice, I literally just use my `Arena` allocator on top of the `wasm__push` and `wasm__pop` functions.


## Strings

Strings in WASM are a special case of dealing with exchanging large amounts of information back and forth (I mean, larger than a `u64`).
You might require special handling to get the UTF8 part correctly, but otherwise we are literally just writing the bytes to some shared location on the heap and passing the pointer to them.

The browser has two built-in functions for dealing with UTF8 strings: `TextDecoder` and `TextEncoder`.

Let's define a couple of functions for reading and writing UTF8 strings:

```js
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
```

Then we can use this code to read strings from WASM like this:
```js
// NOTE(nick): given some function that returns a NULL-terminated string
const ptr = instance.exports.secret_message();

const result = DecodeString(heap, ptr, StringLength(heap, ptr));
console.log("The secret message is:", result);
```

If we want to pass a JS string to WASM we can do this:
```js
// NOTE(nick): we need to put the string into WASM's memory
const ptr = instance.exports.wasm__push(3 * message.length);
const count = EncodeString(heap, ptr, message);
// NOTE(nick): then we pass a pointer to where we put the bytes
instance.exports.ping(ptr, count);
// NOTE(nick): now we're done with the memory
instance.exports.wasm__pop(3 * message.length);
```

## Structs

If you want to return a struct from a function you can either pack it into a `u32` or a `u64` and re-interpret the bytes, or you can return a pointer to the struct and decode the struct fields. I think returning a pointer is a bit easier to work with so that's what I normally do.

For example:

```js
// File: hello.c
typedef unsigned int u32;
struct Foo {
    u32 x;
    u32 y;
};

// File: hello.js
// NOTE(nick): given that this function returns a pointer to Foo:
const ptr = instance.exports.new__foo();

// NOTE(nick): `DataView` helps us read values out of the struct
const dataView = new DataView(memory.buffer);
let at = ptr;
const x = dataView.getUint32(at, true);
at += 4;
const y = dataView.getUint32(at, true);
at += 4;

// Now here is our reconstructed foo struct:
const foo = { x, y };
```

`DataView` has a bunch of [methods](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/DataView) for interpreting different kinds of data.

The second argument should be `true` because according to [Rasmus](https://rsms.me/wasm-intro) the bytes are always little endian independent of the host-machine's endianess.

This strategy also works for pointer fields.


## Arrays

We already saw a couple of examples for reading and writing strings. Arrays are similar.

To send an `f64` array to WASM:

```js
const jsArray = [1, 2, 3, 4, 5];
// NOTE(nick): sizeof(f64) == 8
const cArrayPtr = instance.exports.wasm__push(8 * array.length);
const cArray = new Float64Array(memory.buffer, cArrayPtr, jsArray.length);
cArray.set(jsArray);

instance.exports.compute_square_roots(cArrayPtr, jsArray.length);

// NOTE(nick): if the memory was modified in place, we can read it now
const result = Array.from(cArray);

instance.exports.wasm__pop(8 * array.length);
```

To read an `f64` array from WASM:

```js
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
```

Note that because we just returned a `Float64Array` into WASM's memory, these values will actually change if WASM overwrites that memory. So you should take care to copy the array if you need to, or make sure the memory stays at a fixed address.

For small arrays you can use `Array.from(float64Array)` to convert the `TypedArray` into a plain JavaScript array.
You can also copy the typed array itself with `float64Array.slice()`.

<div class="message">
⚠️ One caveat with `TypedArray`s is that `Array.isArray` will actually return `false` for them!
You should use `ArrayBuffer.isView()` instead.
</div>


## Source code

Here is the full source code of our program, also available on [Github](https://gist.github.com/nickav/b371ba3769160cd321063eac5503c9c7):

```c
// File: hello.c
// Types
#define NULL 0
typedef unsigned char      u8;
typedef unsigned int       u32;
typedef signed int         i32;
typedef signed long long   i64;
typedef double             f64;


// Imports
__attribute__((import_module("env"), import_name("js__output")))
void js__output(char *str, i32 size);

__attribute__((import_module("env"), import_name("js__sin")))
f64 js__sin(f64 x);

// Math
f64 sqrt(f64 x)
{
    return __builtin_sqrt(x);
}

f64 sin(f64 x)
{
    return js__sin(x);
}

// Memory
u32 wasm__heap_size_in_bytes()
{
    return __builtin_wasm_memory_size(0) << 16;
}

extern u32 __heap_base;
void *wasm__heap_pointer()
{
    return (void *)&__heap_base;
}

static u32 offset = 0;

__attribute__((export_name("wasm__push")))
u8 *wasm__push(u32 size)
{
    u8 *result = NULL;

    // NOTE(nick): align up to the nearest 16 bytes
    size = (size + 15) & ~15;

    if (offset + size < wasm__heap_size_in_bytes())
    {
        result = (u8 *)(wasm__heap_pointer()) + offset;
        offset += size;
    }

    return result;
}

__attribute__((export_name("wasm__pop")))
void wasm__pop(u32 size)
{
    // NOTE(nick): align up to the nearest 16 bytes
    size = (size + 15) & ~15;

    if ((i64)offset - size > 0) {
        offset -= size;
    } else {
        offset = 0;
    }
}

//
// Exported API
//

typedef struct f64_Array f64_Array;
struct f64_Array
{
    u32 count;
    f64 *data;
};

__attribute__((export_name("add")))
i32 add(i32 x, i32 y)
{
    return x + y;
}

__attribute__((export_name("secret_message")))
char *secret_message()
{
    return (char *)"Hello, Sailor!";
}

__attribute__((export_name("ping")))
void ping(char *message, i32 count)
{
    js__output(message, count);
}

__attribute__((export_name("compute_square_roots")))
void compute_square_roots(f64 *array, i32 count)
{
    for (i32 i = 0; i < count; i += 1)
    {
        array[i] = sqrt(array[i]);
    }
}

__attribute__((export_name("push_f64_array")))
f64_Array *push_f64_array(u32 count)
{
    f64_Array *result = (f64_Array *)wasm__push(sizeof(f64_Array));
    result->count = count;
    result->data = (f64 *)wasm__push(sizeof(f64) * count);

    for (i32 i = 0; i < count; i += 1)
    {
        result->data[i] = 0;
    }

    return result;
}
```

```c
// File: hello.html
<!doctype html>
<html>
<script src="./hello.js">
</script>
</html>
```

```js
// File: hello.js
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

    console.log(`add(42, 42):`, hello.add(42, 42));
    console.log(`secret_message():`, hello.secret_message());
    console.log(`ping("hello, good sir"):`, hello.ping("hello, good sir"));
    console.log(`compute_square_roots([1, 2, 3, 4, 5]):`, hello.compute_square_roots([1, 2, 3, 4, 5]));
    console.log(`push_f64_array(10):`, hello.push_f64_array(10));
};

main();
```

---

## Debugging with Sourcemaps

To step through your C code in the browser with sourcemaps you need to do the following:

1) Install the Chrome extension: [C/C++ DevTools Support for DWARF](https://chrome.google.com/webstore/detail/cc%20%20-devtools-support-dwa/pdcpmagijalfljmkmjngeonclgbbannb)

2) Add `-gdwarf-5` to the CLI arguments (and probably disable optimizations)

3) Set up a Path Substitution in the extension's Options. For compiling on Windows, I needed to add a substitution from `/home/nick/` to `C:/` and then it just works!

## Wrap Up

The code I've shared should cover just about everything you could ever want to do in WASM.
While it would be a lot of work to emulate _everything_ `emscripten` is doing, I hope this has shown you a viable alternative.

Overall I have enjoyed working with WASM despite some of it's limitations.

## Links

https://rsms.me/wasm-intro

https://depth-first.com/articles/2019/10/16/compiling-c-to-webassembly-and-running-it-without-emscripten/

https://dassur.ma/things/c-to-webassembly/

https://github.com/llvm-mirror/clang/blob/master/test/CodeGen/builtins-wasm.c

https://github.com/emscripten-core/emscripten/tree/main/system/lib/libc/musl

