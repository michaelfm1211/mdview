# mdview

mdview is a small and fast markdown-to-html compiler. You can access mdview
through the `mdv` command line interface or through the `libmdview` C library.
You can read more about them below.

mdview only supports a reasonable subset of markdown. Although it is not
fully complete and compatible with every other implementation, most people
should find it suitable for their usage with little to no changes in their
markdown syntax.

*A note on syntax highlighting in code blocks*: mdview does not, and will not in
the future, natively support syntax highlighting. mdview simply wraps code in an
HTML `<pre><code></code></pre>` block. If you need syntax highlighting, then you
should use a third-party tool.

### `mdv`

`mdv` is the command line interface to libmdview. It uses `libmdview` under the
hood. `mdv` takes markdown through standard input and returns the generated HTML
through standard output. Because of this, usage is very simple; in most shells
all you have to do is `mdv < input.md > output.html`.

### `libmdview`

`libmdev` is a markdown-to-html parser implemented through a C library. The core
of the library is the `mdview_ctx` structure. The general flow of parsing some
markdown should be:
- call `mdview_init` to initialize the context
- feed markdown to the parser with `mdview_feed`
- handle the HTML string output returned from `mdview_feed` and check for errors
- go back to step 2 and repeat until you have fed all your markdown into the
  parser
- call `mdview_flush` to get any HTML that may have been generated but still
  awaiting future data
- free the context with `mdview_free`.

For a concrete example of this flow, see the source code in *mdv.c*.

### Development

mdview is developed on GitHub at [https://github.com/michaelfm1211/mdview]().
You can file an issue [here](https://github.com/michaelfm1211/mdview/issues).
