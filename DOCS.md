# Documentation

This is the official libmdview documentation. This document will overview the
intended behavior of libmdview. If you believe the actual behavior is
inconsistent with what is described here, please submit a bug request describing
the differences. This document will describe both quirks/features that are
relevant to end-users and the inner workings of libmdview that are relevant to
developers.

### Streaming Architecture

libmdview is different from other markdown transpilers because it must work on
**streams** of text. The input can be ended at any time and we must be ready
for it. Every character is handled individually with only itself and a previous
state object being visible to the current handler; we can never look ahead.
While this complicates the task for libmdview developers, it has the potential
to make it much easier for the end user. With few exceptions, mdview can handle
extremely long files by parsing them in sequential blocks of arbitrary, end-user
chosen length. libmdview also has a better chance of working on memory-limited
devides because only the current block, a context object, and a temporary buffer
need to be stored.

### Blocks and Decorations

The core of libmdview is two concepts: blocks and decorations, both of which
represent HTML blocks (or in some cases, nested HTML blocks). As a general rule
of thumb, HTML `display: block; ` elements should be represented as libmdview
blocks and HTML `display: inline;` elements should be represented as libmdview
decorations. Another way of thinking about it is that decorations in libmdview
decorate the text, while blocks in libmdview separate the text into sections.

##### Decorations

When the parser detects a sequence of special characters, it will toggle any
decorations associated with that sequence. Decorations cannot be specially be
turned on or off, they must be independently toggled. Below is a list of
decorations and the HTML tags they transpile to:
- Italics: `<i></i>`. Example: *The New York Times*
- Bold: `<b></b>`. Example: **important**
- Strike: `<s></s>`. Example: Today is ~~yesterday~~ today.
- Subscript: `<sub></sub>`. Example: H~2~O
- Superscript: `<sup></sup>`. Example: 2^10^=1024
- Inline code: `<code></code>`. Example: `printf("Hello, world!\n");`

and below is the list of special character sequences that toggle these
decorations. The longest sequence is always matched:

- `*`: Toggle italics. Example: `*The New York Times*`
- `**`: Toggle bold. Example: `**important**`
- `***`: Toggle italics and bold. 
- `^`: Toggle superscript. Example: `H~2~O`
- `~`: Toggle subscript. Example: `2^10^=1024`
- `~~`: Toggle strike. Exript. Example: `2^10^=1024`
- \`: Toggle inline code: Example: \` `printf("Hello, world!\n");` \`

Note again that decorations are always **toggled**, not set. This can become
important in situations similar to the following:
```
regular text * this is in italics. ***is this bold italics?*** *
```
This will become:

regular text * this is in italics. ***is this bold italics?*** *

This is because the `***` sequence toggles bold and italics. If the text is already
in italics, then toggling it will turn it off. Even more important is that
another `*` will toggle it again to become on. If you don't count, then the rest
of your document could become italics. This is ultimately an effect of libmdview
handling streams: libmdview can't look ahead to see what you're trying to do, so
it'll just trust you even if you're wrong. The only chance libmdview gets to
catch your mistakes is at the end of the document when it knows that nothing
else will follow. At the end, libmdview will close any unclosed decorations.
Don't rely on this though. If you have multiple, then they may be closed out of
order and create invalid HTML (your browser will still be able to handle invalid
HTMl, but you would rather have valid HTML).

##### Blocks

While you can use decorations pretty much as you please, libmdview must abide
by strict rules when using blocks:
- There can only be one active block at a time, and opening another block will
close the current one.
- Each block defines when it should end (in `close_block` in `lib/tags.c`).
There is no unilateral way of breaking out of a block. You can think of a block
as being a kind of subroutine. The only exception to this is that the current block
is always ended when the end of the document is reached.
- Each block may define how characters are handled. A paragraph block is
considered to be "standard". Most blocks work like paragraphs do except they
wrap their text in a different tag like `<h1></h1>` (header blocks) or
`<blockquote></blockquote>` (quote blocks), but they are not required to do so.
For example, only special character sequences consisting of backticks are
handled in code blocks; the rest is escaped text.

Currently, multiple functions must be changed to alter the behavior of a block
or add a new type. However, libmdview's project structure may change in the
future to better reflect the modularity of blocks.

Below is a list of all currently supported blocks and what special character
sequences are used to activate them:
- Paragraph: considered the "default". All decorations and special character
sequences are supported. The document automatically begins in a paragraph block
and a new paragraph block is started when most blocks end.
- Headers: activated when a line begins with one to six hashtags (`#`)
characters followed by a space, and ended on the next newline. All text between
the start sequence and the newline is wrapped in an HTML `<hx></hx>` block,
where `x` is the number of hashtags in the start sequence. A new paragraph block
is begun when this block ends.
- Unordered list: activated when a line begins with a `-` character followed by
a space and ends on two consecutive newlines. All text between the first `- `
and the two consecutive newlines are wrapped in an HTML `<ul></ul>` block. Inside
the block, a `<li></li>` element is created for every line beginning with a `- `
sequence. A new paragraph block begins when this element ends.
- Ordered list: activated when a line begins with a number followed by a `.`
character and a space. This is identical to the unordered list block, except
that the HTML `<ol></ol>` element is used and list items begin as previously
described.
- Code block: activated when a line begins with three or more backtick (\`)
characters. Inside a code block, all characters are escaped and wrapped in a
HTML `<pre><code></code></pre>` block. A code block ends when the same sequence
that begins it is found at the beginning of a line. A new paragraph block begins
when this block ends.
- Quote blocks: activated when a line begins with the `>` character followed by
a space and ends on two consecutive newlines. All text between the start
sequence and the newline is wrapped in an HTML `<blockquote></blockquote>`
block. A new paragraph block is begun when this block ends.

### Special Cases

While almost everything is built around blocks and decorations, not everything
is. Below is a list of special character sequences that don't toggle a
decoration or activate a block:
- `---`: Writes a `<hr>` element (horizontal line).
- `\`: Escapes a character and prevents it from being counted towards a special
character sequence. Escaping is available in all blocks.

##### Links

libmdview accepts two ways of making a link:
1. Provide the link text in brackets, immediately followed by the URL in
parentheses. For example, `[Example text](https://example.com)` will become
`<a href="https://example.com">Example text</a>`.
2. Provide a URL in brackets and leave the parentheses blank. If you use
links like this, the URL and the title will be the same. For example,
`[https://example.com]()` will become
`<a href="https://example.com">https://example.com</a>`

libmdview will not implicitly convert valid URL into links. If you want to
create a link, then you must use one of the syntaxes listed above.

Links are treated differently in libmdview because the regular rules must be
broken to handle them. In HTML, the URL of a link precedes the link's text, but
in markdown, the URL comes after the text. This inherently requires libmdview to
look ahead, but libmdview is restricted to only seeing one character at a time.
To get around this, the library "cheats" by storing in memory anything that
could become a link (more specifically, any text between a `[` character and the
next space or the next `]` character).

##### Images

In libmdview, images work a lot like links, and much of the code between them is
shared (within the source code, images are referred to as "image links").
Prepending a link with an exclamation mark (`!`) will cause libmdview to use a
`<img>` tag instead of `<a>`, with the link text becoming the alt text and the
link URL becoming the image source URL. For example:
```
![Image of a panda.](//img/panda.png)
```
will become
```
<img src="//img/panda.png" alt="Image of a panda." />
```

##### Tables

Markdown table syntax is currently unsupported and it is unlikely that it will
become supported in the future. If you need to create a table, then use raw
HTML.

### Special Character Sequences

Thus far, special character sequences have been used to describe a lot of other
features. This section will describe how libmdview handles them behind the
scenes.

##### Counting special character sequences

Before any character is handled, a check is done to determine whether or not it
can be counted as a special character in the current block. If not (or it is
escaped), then it will end any ongoing special sequences (as they have now been
interrupted by this non-special character) and be written as output. If it is,
then we do some more checks. If the special character (that is, the current
character that has now been determined to be special) is different from the
last special character we counted or we didn't previously count a special
character, then we set the special sequence type to the current character (or a
special code, if this special sequence contains different characters) and set
the current count to 1. If the special character is the same as the last one we
counted, then the count will be incremented. Special characters are not written
as output.

##### Ending special character sequences

As previously mentioned, any ongoing special character sequences are ended and
handled if they are interrupted by a non-special character. They are also ended
and handled if libmdview reaches the end of the document. Once a special
character sequence is ended, libmdview will compare it with all the known
special character sequences. If there is a match, then the action associated
with that sequence is done. If it is not matched, then it is an invalid
sequence and libmdview will write all the special characters that have been
counted as regular characters to the output.
