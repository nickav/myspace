---
title: "Simple Markdown Styles"
arbitrary_keys: "With arbitrary values"
hello: 123
draft: true
# i would like to make a comment
array: [1, 2, 3]
---


Kitchen Sink demo.
This is a markdown-like syntax. I've made changes to suit my needs.

Some basic text.

*bold text*
_italic text_
~strike through~
`inline code`

For now, these don't combine.

Escape character is "\\"
For example: \* \_ \~ \`

Horizontal rule:

---

Headers:

# H1 tag

## H2 tag

### H3 tag

#### H4 tag

##### H5 tag

###### H6 tag

Links:

Here is a link: [nickav.co](https://nickav.co).

Lists:

Bulleted List:
- item 1
- item 2
- item 3

Numbered List:
1. hello
2. world
3. test

Code:

```c
// Here is a code block
int main() {
    char ch;
}
```

Quotes:

> Here is a quote
> that spans multiple lines

>>>
Multiline blockquote
>>>


Custom tags:

@posts(5)
@link("← View All Posts", "/posts")
@hr()

@img("/r/journey_01_pixel.png")

@iframe("http://player.vimeo.com/video/152479854")


Inline HTML:

<img width="576" height="576" src="/r/journey_01_pixel.png" alt="" style="image-rendering:pixelated;" />

<hr/>
