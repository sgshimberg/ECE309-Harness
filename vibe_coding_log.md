# Vibe Coding Log

This file documents the Specification Driven Development (SDD) process used
to build the ECE 309 Project 1 LLM Mini-Harness: the architectural rules
established, the exact prompts given to the AI assistant, and summaries of
the AI's responses/iterations, per the project spec's "Vibe Coding Log"
requirement.

## Process

1. Read and internalized the project spec (`Proj1_spec.pdf`).
2. Set up repository scaffolding (README, this log, chat transcript
   directory).
3. Waiting on the student's (sgshimberg) specific SDD directions for the
   harness's state machine, context management, and tool-execution design
   before generating `harness.c`.

## Architectural Rules Established So Far

- Language: standard C only (`<stdio.h>`, `<string.h>`, etc.) — no external
  libraries.
- Must compile in a POSIX environment with `gcc harness.c -o harness`.
- Core loop: terminal-based, reads user input via `fgets`, passes it to a
  mock model function, prints the simulated response, exits safely on
  `exit`.
- Context management: store the last 5 conversation turns in a fixed-size,
  safely managed buffer.
- Tool execution: the harness must detect and dispatch to at least one tool
  (e.g., a calculator) for operations an LLM mock cannot do itself.
- Testing: a separate `test.sh` script pipes deterministic input into the
  compiled binary to validate behavior non-interactively.

## Prompt / Iteration History

_(To be filled in as SDD directions are provided and the AI generates code.
Each entry should include: the exact prompt given, a summary or full text of
the AI's response, and any follow-up corrections.)_

### Entry 1 — Session Setup

- **Date:** 2026-09-05
- **Prompt (from student):** Provided the Project 1 spec PDF; requested that
  all chat sessions be saved to a chat directory/log file, with SDD
  directions to follow incrementally.
- **Action taken:** Created `README.md`, `vibe_coding_log.md`, and
  `chats/session_2026-09-05.md` to track the SDD process going forward.
