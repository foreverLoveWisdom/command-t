<p align="center">
<img src="https://raw.githubusercontent.com/wincent/command-t/media/command-t-small.jpg" />
<img src="https://raw.githubusercontent.com/wincent/command-t/media/command-t-v6.gif" />
</p>

# Command-T

<a href="https://dotfyle.com/plugins/wincent/command-t">
  <img src="https://dotfyle.com/plugins/wincent/command-t/shield" />
</a>

Command-T is a Neovim and Vim plug-in that provides an extremely fast "fuzzy" mechanism for:

- Opening files and buffers
- Jumping to tags and help
- Running commands, or previous searches and commands

with a minimum of keystrokes.

Files are selected by typing characters that appear in their paths, and are ranked by an algorithm which knows that characters that appear in certain locations (for example, immediately after a path separator) should be given more weight.

Files can be opened in the current window, or in splits or tabs. Many configuration options are provided.

Speed is the primary design goal, along with providing high-quality, intuitive match ordering. The hand-crafted matching algorithm, implemented in low-level C and combined with parallelized search, input debouncing, integration with Watchman and many other optimizations, mean that Command-T is the fastest fuzzy file finder bar none.

---

For more information, see [the documentation](https://github.com/wincent/command-t/blob/main/doc/command-t.txt).
