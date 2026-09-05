# ECE 309 Project 1: LLM Mini-Harness in C via Vibe Coding

A minimal LLM agent harness written in standard C, built using Specification
Driven Development (SDD) / "vibe coding" per the ECE 309 Project 1 spec.

## Project Layout

- `harness.c` — single-file C implementation of the mini-harness (core loop,
  context management, tool execution).
- `test.sh` — Bash script that drives the compiled harness deterministically
  for automated testing.
- `vibe_coding_log.md` — the exact SDD prompts, AI iterations, and responses
  used to generate the C code (required deliverable per spec).
- `chats/` — full running transcript of the chat sessions used to build this
  project, saved as we go.
- `docs/sdd_notes.pdf` — the original SDD design notes (state machine,
  context management, and calculator tool spec) written up ahead of time.

## Requirements

- Standard C, compiles in a POSIX environment (no external libraries beyond
  the standard C library).
- Build: `gcc harness.c -o harness`
- Run: `./harness`
- Test: `bash test.sh`

## Features

- **Core loop:** prompts with `> `, reads a line via `fgets`, strips the
  newline, and checks for `exit` before anything else is stored.
- **Context management:** a static `Storage[10][256]` array (no heap
  allocation) holds the last 5 turns (user + model message each). Once
  full, a FIFO `CONTEXT_WINDOW` shift drops the oldest entry to make room.
- **Mock model:** any variation of "hello" gets a hardcoded greeting;
  everything else is echoed back — unless the calculator tool triggers.
- **Calculator tool:** typing `calculate NUM OP NUM` (e.g. `calculate 2 + 2`)
  runs addition, subtraction, multiplication, or division. Operators accept
  symbols (`+ - * /`) or word synonyms (`add`/`plus`/`sum`,
  `subtract`/`minus`/`less`, `multiply`/`times`/`product`,
  `divide`/`divided by`/`over`). Division by zero is caught and reported as
  an error instead of crashing.
- **Safe shutdown:** typing `exit` clears the context history and prints
  `Goodbye.` before terminating.

## Testing

`test.sh` rebuilds the harness, pipes deterministic input through it, and
asserts on the output for the core loop, every calculator operation and
synonym, divide-by-zero, malformed input, and context-window behavior past
5 turns. If `valgrind` is installed, it also runs a full leak-check pass.

```
bash test.sh
```

## Status

Functionally complete per the current SDD spec. See `vibe_coding_log.md` for
the full SDD process and prompt history, and `chats/` for the full
conversation transcript.
