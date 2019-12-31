# budgie

A super-overengineered Brainfuck compiler that only works for x86_64 Linux.

## How to use it

### Building budgie

A simple `make` should suffice.

### Using budgie

Pass Brainfuck code to `./budgie <name_of_output_file>`. If stdout is piped, it
will output the compiled binary to stdout (and ignore the name). If stdout is
not piped, it will output to the name given. If stdout is not piped and no name
is given, it defaults to `a.out`.

## How it works

budgie does a single pass on the input to translate from input Brainfuck to an
intermediate representation (performing a few simple optimizations while doing
so), does a few more simple optimizations, and then translates the intermediate
representation to an executable.

## Why

Mostly this was used to help me understand how a program is loaded. There is not
really a practical use for this.

## Thanks

* Original idea: https://github.com/matslina/awib
* Language reference: https://esolangs.org/wiki/Brainfuck
* Register information: https://wiki.cdot.senecacollege.ca/wiki/X86_64_Register_and_Instruction_Quick_Start
* System call reference: https://syscalls64.paolostivanin.com
* Easy-to-use online assembler and diassembler: https://defuse.ca/online-x86-assembler.htm
