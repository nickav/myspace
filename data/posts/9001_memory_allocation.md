---
title: "On Memory Allocation"
desc:  "The messy reality of dealing with memory in real-time applications"
date:  "2021-11-19 10:13:03"
image: "godrays_04.png"
author: "Nick Aversano"
draft: true
---

When writing high-performance software, organizing your program's memory properly can result in dramatic speed improvements.
While you don't need to do the optimal thing, memory access can _definitely_ be a runtime bound on your program's maximum speed.
It also turns out that in most cases there is a dramatically simpler solution.

As programmers, we often think of memory access as `O(1)` runtime complexity. But the code we write runs on an actual physical CPU in the real world. So while that's true in the theoretical sense, it's not at all true when dealing with modern CPUs.


## Heap Allocators

If you've gone to computer school, then you've probably learned about `malloc` and `free`.
You may have even been taught to write linked lists of `Node`s by `malloc`-ing each node.
This is almost always bad because of how memory access on a CPU works.

The only time this is a good solution is if you need to constantly allocate and free unknown sized buffers in random orders.

When you access memory from the CPU, it turns out there are 3 levels of caching that sits in between you and RAM. This is to help with the fact that memory access is _so slow_.
So while your CPU is taking a stroll down to RAM memory lane, it might as well grab a bunch of stuff while it's already there.

Because memory access on a CPU is incredibly slow, modern CPUs typically have 3 layers of caching to help hide latency.
These layers are called: L1, L2, and L3 and might be sized: 64KB, 256KB, 16MB.
So when you ask for memory, the CPU goes and fetches a whole page and sticks it in the cache.
Similarly for writing values back to memory, but we won't get into that now!

The main problem with a general-purpose heap allocator is that often your allocations aren't in the same cache page.
Allow me to provide a demonstration:

<div class="flex-x center">
<blockquote class="twitter-tweet"><p lang="en" dir="ltr">Trying to explain the data-oriented approach.<a href="https://t.co/qHcVssbwgH">pic.twitter.com/qHcVssbwgH</a></p>&mdash; Keijiro Takahashi (@_kzr)
<a href="https://twitter.com/_kzr/status/1672497446705037312?ref_src=twsrc%5Etfw">June 24, 2023</a></blockquote>
<script async src="https://platform.twitter.com/widgets.js" charset="utf-8"></script>
</div>

The way to make your code as fast as possible is to think "Data-Oriented" about it!
So if heap allocators are too slow, what else can you do?


## Static Allocation

Meaning, just declare the thing to be as big as you ever want it to be:

```c
struct Game_Memory
{
    Entity entities[2048]; // 2048 should be enough for anybody
    i64    count;
};
```

The drawbacks to this approach should be obvious, but there are some good properties worth talking about:

1. You know exactly what your worst possible case scenario is and you can make sure your program runs smoothly at the limit. This has knock-on effects with algorithmic complexity.
2. If your program starts at all, then it definitely has enough memory to run.

Depending on your usage, this memory can be stored in a few different places: the stack, the heap, or the data segment.
In fact, the data segment is not any more special than program memory.
When your program is started, the OS loads it into memory somewhere and marks it as read-only.
The only difference is the data segment has both read-write and read-only areas. Whereas program code is read-only.


## Arena Allocators

Enter one my favorite memory strategies. It's so simple you'll fall out of your chair:

```c
struct Arena
{
    u8 *data;
    u64 offset;
    u64 size;
};
```

I first learned about this approach from watching the Handmade Hero series.
The concept is simple: grab a large block of memory to sub-allocate from. Whenever you need more memory, just bump the `offset` forward and return a pointer into that block.
You can also trivially align the resulting pointers.

The pros of this approach is it's dead simple. And it's extremely fast.
It's also trival to free the entire block. All you do is set `offset` to zero.

One of the tricky things with this approach is there's not really a natural way to "free" anything. Unless you happen to be in the happy case where the thing you were trying to free was the last operation you did in the block.
But in practice every subsystem can just manage it's own memory directly.

Here's what some usage code might look like:

```c
u64 size = kilobytes(1);
u8 *data = arena_push(arena, size);
for (int i = 0; i < size; i += 1)
{
    data[i] = 0x42;
}
arena_pop(size);
```

But, sometimes you need to free things out of order. In that case, this approach doesn't really work so well. Or does it?
Well, one thing you can do is to have multiple arena allocators that serve different purposes.


Another drawback to this approach is because everything is allocated from a single block in one-shot, your application might use a lot more memory than it actually needs.

But, there's another pattern we can lean on here to make this a really compelling general-case memory allocation strategy. Enter OS Virtual Memory:


## OS Virtual Memory

Modern operating systems provide a mechanism for virtual memory.
Virtual memory is like real memory, except that it has no restrictions on what address it can start at.

The other cool thing about virtual memory, is you don't need to "commit" the memory upfront -- because it's virtual!

So, combining arenas