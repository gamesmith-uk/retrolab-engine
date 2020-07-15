# retrolab

![Run tests](https://github.com/gamesmith-uk/retrolab-engine/workflows/Run%20tests/badge.svg)

This is the repository for the free and open source standalone emulator and compiler for
retrolab.

To get the **full web experience, complete with a high quality IDE, a debugger, and full
documentation** visit https://retrolab.gamesmith.uk/.

[Download here](https://github.com/gamesmith-uk/retrolab-engine/releases/latest/) the
standalone emulator and compiler to be used offline. Currently, it is available only is
source form.

#### Usage:

The full documentation of the emulator and compiler architecture is [here](https://retrolab.gamesmith.uk/learn).
Below is the documentation for the standalone application.

Compiling and running from source assembly file:

```bash
$ retrolab -s SOURCE_FILE.s
```

Compiling a file into ROM:

```bash
$ retrolab -c SOURCE_FILE.s > ROM_FILE.bin
```

Running a ROM file:

```bash
$ retorlab -r ROM_FILE.bin
```
