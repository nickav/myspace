---
title: "WASM from Scratch"
desc:  "Web Assembly"
date:  "2023-08-23 23:23:23"
image: "3.jpg"
author: "Nick Aversano"
draft: true
---

I recently was trying to figure out how to compile my C code to WASM.
If you search the internet for this, you'll find a _ton_ of resources that say to use `emscripten`. Even [MDN](https://developer.mozilla.org/en-US/docs/WebAssembly/C_to_Wasm) says this.
While emscripten is not a bad choice, it does a _lot_ for you.
Not to mention that installing it can be a bit of a pain.
If you're like me and prefer to take a more [handmade](https://handmade.network) approach to building software, it can be difficult to figure out everything you have to do to just compile some C code to WASM.

Here is a comprehensive guide to what I learned after searching the internet and talking with some friends:


## Basics

Writing for Web Assembly is similar to writing for an embedded system.
So if you've already taken care to keep your [dependencies minimal](https://github.com/nickav/na/blob/master/na.h), this should be pretty straightforward.
If you're starting a new project I *strongly* recommend this approach.

Let's say you have the world's most basic C program:
```c
// File: hello.c
int add(int a, int b) {
    return a + b;
}
```

If we run `clang -print-targets` you can see the available targets for your version of the compiler. And indeed, we can see both `wasm32` and `wasm64` are present.

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

This is clang telling us that here is no `_start` function (the function that eventually calls `int main` in a typical C program). OK, well it looks like there's a suggestion there too so let's just using that. The thing to notice is that it's actually `wasm-ld` here that's giving us the error. So to pass a `wasm-ld` linker flag you have to prefix it with `-Wl,`:

```bash
> clang --target=wasm32 --no-standard-libraries -Wl,--no-entry -o hello.wasm hello.c
```

And that works! There is one more thing: we need to actually tell clang to export our `add` function as a symbol so our WASM module can call it. To do that we can just add the `--export-all` linker flag. So build the code again with this:

```bash
> clang --target=wasm32 --no-standard-libraries -Wl,--export-all -Wl,--no-entry -o hello.wasm hello.c
```

Now let's write a simple HTML file to load our WASM code:

```js
// File: hello.html
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

        console.log(wasm.instance.exports.add(42, 42));
    };

    run();
</script>
</body>
</html>
```

Unfortunately, because of "security reasons" you can't just open this HTML file in your browser. You actually need to run a local web server to dynamically fetch content.

Any of the following commands will work:

```bash
> python -m http.server 8000

> npx serve -p 8000
```

Now open [http://localhost:8000/hello.html](http://localhost:8000/hello.html) and open the developer console. You should see:

```js
84
```


Talk about clang `__builtin`s


Limitations

This doesn't do stuff like: OpenGL, Threads, ...

Because the WASM client is JavaScript you can shoot yourself in the foot if you pass in the wrong types for things.


To build the C code:


```bash
clang --target=wasm32 -O2 --no-standard-libraries -nostartfiles -mbulk-memory -Wl,--export-all -Wl,--no-entry -Wl,--import-memory -o hello.wasm hello.c
```



To import declarations from JS:

```c
__attribute__((import_module("env"), import_name("os_output"))) void os_output(char *str, i32 size);
```

Define your own math functions:

```c
#ifdef __wasm__
    f64 sqrt(f64 x) {
        return __builtin_sqrt(x);
    }

    // NOTE(nick): these functions must be provided by JS
    __attribute__((import_module("env"), import_name("math_cos"))) f64 math_cos(f64 x);
    __attribute__((import_module("env"), import_name("math_tan"))) f64 math_tan(f64 x);
    __attribute__((import_module("env"), import_name("math_atan2"))) f64 math_atan2(f64 x, f64 y);
    __attribute__((import_module("env"), import_name("math_exp"))) f64 math_exp(f64 x);

    __attribute__((import_module("env"), import_name("math_sin"))) f64 math_sin(f64 x);
    f64 sin(f64 x) {
        return math_sin(x);
    }

    f64 cos(f64 x) {
        return math_cos(x);
    }

    f64 tan(f64 x) {
        return math_tan(x);
    }

    f64 atan2(f64 x, f64 y) {
        return math_atan2(x, y);
    }

    f64 exp(f64 x) {
        return math_exp(x);
    }

    #define INFINITY __builtin_huge_valf()
#endif
```


Get the heap size and base pointer:

```c
import u32 __heap_base;

export u64 wasm__heapsize()
{
    return __builtin_wasm_memory_size(0) << 16;
}

export void *wasm__heapbase()
{
    return (void *)&__heap_base;
}
```

The JS client:

```js
// Encode string into memory starting at address base.
const encode = (memory, base, string) => {
    for (let i = 0; i < string.length; i++) {
        memory[base + i] = string.charCodeAt(i);
    }

    memory[base + string.length] = 0;
};

const UTF8Decoder = new TextDecoder("utf8");

const UTF8ToStringLength = (u8Array, idx, length) => {
    if (!idx) return "";

    const endPtr = idx + length;
    if (u8Array.subarray)
    {
        return UTF8Decoder.decode(u8Array.subarray(idx, endPtr));
    }
    return UTF8Decoder.decode(new Uint8Array(u8Array.slice(idx, endPtr)));
}

// return dataView.getInt32(ptr, true);
// return dataView.getUint32(ptr, true);
// return data.getBigInt64(ptr, true);
// return data.getBigUint64(ptr, true);

const Kilobytes = (x) => 1024 * x;
const Megabytes = (x) => 1024 * 1024 * x;

export const load = async (wasmPath) => {
    const response = await fetch(wasmPath);
    const bytes = await response.arrayBuffer();

    const numPages = Megabytes(64) / Kilobytes(64);

    const memory = new WebAssembly.Memory({ initial: numPages });
    const heap = new Uint8Array(memory.buffer);

    const obj = await WebAssembly.instantiate(bytes, {
        env: {
            memory,
            print: (str, size) => {
                const result = UTF8ToStringLength(heap, str, size);
                console.log("[braxc]", result);
            },
            math_sin: Math.sin,
            math_cos: Math.cos,
            math_tan: Math.tan,
            math_atan2: Math.atan2,
            math_exp: Math.exp,
        },
    });

    const { instance } = obj;

    return {
        add: (x, y) => {
            instance.exports.add();
        },

    };
};

```