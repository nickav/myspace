---
title: "On Memory Allocation"
desc:  "The messy reality of dealing with memory in real-time applications"
date:  "2021-11-19 10:13:03"
image: "godrays_04.png"
author: "Nick Aversano"
draft: true
---

When writing high-performance software, organizing your program's memory properly can result in dramatic speed improvements.
While you don't always need to do the most optimal thing, memory access can _definitely_ be a runtime bound on your program's maximum speed.
For that reason, it's often important to carefully consider where your memory comes from.

As programmers, we often think of memory access as `O(1)` runtime complexity. But the code we write runs on an actual physical CPU in the real world. So while that is true in the theoretical sense, it's not at all true when dealing with modern CPUs.

## Heap Allocators

Of course there are the classic `malloc` and `free` functions.

Anyone who's taken a CS course has been taught to write code like this:

```c
struct Node
{
    Node *next;
    Node *last;

    u64 data;
};

void insert_node(Node *first, u64 data)
{
    Node *next = (Node *)malloc(sizeof(Node));
    next->data = data;

    first->last = next;
}

int main()
{
    for (int i = 0; i < 100; i += 1)
    {
        insert_node(first)
    }
}
```

While it's not true that linked-lists are _always_ bad, you have no idea where the memory for each node is going to be stored.

What you _should_ have done in this case, is the following:

The way to make your code as fast as possible is to think "Data-Oriented" about it!

When you access memory from the CPU, it turns out there are 3 levels of caching that sits in between you and RAM. This is to help with the fact that memory access is _so slow_.
So while your CPU is taking a stroll down to RAM memory lane, it might as well grab a bunch of stuff while it's already there.


In non-trivial programs this actually starts to really slow down your code.


So if heap allocators are too slow, what else can you do?


## Static Allocation

Well, for starters, there's the classic static allocation. Meaning, just declare the thing to be as big as you possibly could ever want it to be:

```c
struct Game_Memory
{
    Entity entities[2048]; // 2048 should be enough for anybody
    i64    count;
};
```

The drawbacks to this approach should be obvious, but there are some good properties worth talking about:

1) You know exactly what your worst possible case scenario is and you can make sure your program runs smoothly at the limit. This has knock-on effects with algorithmic complexity.

2) Another benefit is that you know if your program starts at all, then it definitely has enough memory to run.

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

The concept is simple: grab a large block of memory to sub-allocate from. Whenever you need more memory, just bump the `offset` forward and return a pointer into that block.
You can also trivially align the resulting pointers.

I first learned about this approach from watching Handmade Hero.

The pros of this approach is it's dead simple. And it's extremely fast.

It's also trival to free the entire block. All you do is set `offset` to zero.

One of the tricky things with this approach is there's not really a natural way to "free" anything. Unless you happen to be in the happy case where the thing you were trying to free was the last operation you did in the block.

For this reaon, it's often useful to allow the `Arena` to provide `push` and `pop` methods.

This allows you to do some nice scratch memory kind of stuff. For example:

```c
u64 size = kilobytes(1);
void *data = arena_push(arena, size);
if (data)
{
    // do something that requires an unknown amount of memory ahead of time...
    u64 used_size = 512;
    arena_pop((i64)(size - used_size));
}
```

But, sometimes you need to free things out of order. In that case, this approach doesn't really work so well. Or does it?
Well, one thing you can do is to have multiple arena allocators that serve different purposes.



Another drawback to this approach is because everything is allocated from a single block in one-shot, your application might use more memory than it actually needs.

But, there's another pattern we can lean on here to make this a really compelling general-case memory allocation strategy. Enter OS Virtual Memory:


## OS Virtual Memory

NOTE(nick): Talk about virtual memory on operating systems.

Only commit what you use!