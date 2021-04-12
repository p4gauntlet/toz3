# ToZ3:
ToZ3 is a tool that produces Z3 expressions from [P4-16](https://p4.org/p4-spec/docs/P4-16-v1.2.0) programs.

## Requirements
ToZ3 is written as an extension to [P4C](https://github.com/p4lang/p4c). It has no other dependencies. First P4C must be installed. Instructions can be found [here](https://github.com/p4lang/p4c#dependencies).

## Install
Once P4C has been installed, ToZ3 can be installed as an extension. The following commands provide an example:

    cd ~/p4c/
    mkdir extensions
    cd extensions
    git clone https://github.com/p4gauntlet/toz3_v2
    cd ~/p4c/build
    make
Afterwards, the ToZ3 executables can be found in the `p4c/build`folder. ToZ3 can be installed globally by running `sudo make install`.

## Usage
Generating Z3 from P4-16 with ToZ3 is straightforward. The tool currently supports three different modes.

[TODO]
