# Lisb

## Abstract
Lisb is a Lisp-like, intepreted language. This project serves as an exercise
to learn C, study compiler implementation and theory, and become familiar with Lisp.

## Usage
a file can be executed through the command line using ```lisb filename.lisb```
The '.lisb' extension is currently not required, but useful for labeling.
If called without a file name, Lisb can be used through a command line REPL.

## Language Specs
TBA

## Under the Hood
The C implementation uses variable sized arrays to represent S-Expressions, as opposed to the
usual linked lists. It also has Q-Expressions, or Quoted-Expressions, in order to
implement un-evaluated lists.

The current parser is built from a grammar using the MPC library (github.com/orangeduck/mpc).
