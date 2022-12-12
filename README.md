# myspace

My personal website written from scratch in C.

The website is generated via a CLI tool that statically generates the website from source assets and public files.

We wrote our own: CSS minifier, basic markdown compiler, YAML parser, and a syntax-highlighter for C-like languages.

## Why?

I wanted a project that would push me to develop my own [single-file header library](https://github.com/nickav/na/blob/master/na.h) to make working in C easier.

What I like about C is it provides so little that you have to build up your understanding of every problem from first-principles. While this is short-term slower, it is long-term faster because you improve your own understanding of the problem and it gets easier to solve these problems over time.

Rather than just reach for an off-the-shelf library, I developed my own alongside writing this project. I even wrote [my own networking library](https://github.com/nickav/na/blob/master/na_net.h) with support for HTTP 1.0 that I use as the dev server.

I had to confront a lot of stuff I didn't understand about the fundamentals of computing in ways that something like JavaScript doesn't make you do. I wrote my own: Arena allocator, String (with UTF-8 support), Array, and more.


Finally, I think a lot of modern web software is simply too complicated. These projects pull in thousands of dependencies that no one understands. As a result, these tools are bloated, slow, and brittle. And users pay for these supposed "developer conveniences" them.

The nice thing about writing this project from scratch is I could fully control the speed of it the whole time. I made sure it only took a few milliseconds to output the entire website the whole time.

You don't get this kind of granular control in the traditional web ecosystem, and the ability to go full-bore if you want to.

