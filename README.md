# Lisb

Lisb is a Lisp-like language. It currently has a REPL, with plans for a full compiler. This project serves as an exercise to learn C, study compiler implementation and theory, and become familiar with Lisp.

Lisb uses variable sized arrays to represent S-Expressions, as opposed to the usual linked lists. It also has Q-Expressions, or Quoted-Expressions, in order to implement un-evaluated lists.

The current parser is built from a grammar using the MPC library (github.com/orangeduck/mpc).
