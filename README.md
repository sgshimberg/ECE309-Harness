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

## Requirements

- Standard C, compiles in a POSIX environment (no external libraries beyond
  the standard C library).
- Build: `gcc harness.c -o harness`
- Run: `./harness`
- Test: `bash test.sh`

## Status

Project scaffolding in progress. See `vibe_coding_log.md` for the SDD process
and `chats/` for the full conversation history.
