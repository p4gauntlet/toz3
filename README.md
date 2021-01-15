# ToZ3: The P4-to-Z3Py Interpreter
ToZ3 produces intermediate Python code that can be used to generate Z3
expressions from P4 programs. The code does not work without the parent repository
[Gauntlet](https://github.com/p4gauntlet/gauntlet). Please see more details there.

## Requirements
ToZ3 is written as an extension to P4C. It has no other dependencies. First P4C must be installed. Instructions can be found [here](https://github.com/p4lang/p4c#dependencies).

## Flags
| Option | Description |
| :--- | :---: |
| ```--ouput``` | File name of the output Python Z3 formula |
| ```--emit_p4``` | If set, will generate a ```--output.p4``` P4 program |
| ```--prune``` | If set, will randomly prune the input P4 program and output to ```--output``` Z3 formula |
