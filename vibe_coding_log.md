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

### Entry 2 — State Machine: RUNNING and EXIT States

- **Date:** 2026-09-05
- **Prompt (from student), verbatim:**

  ```
  State Running

  * Prompt user (">")
  * We only want to use the C Standard Library. Use fgets [fgets(buffer,256,stdin)]
  * We always want to strip the newline character.
  * We check for the keyword "exit" FIRST, before we store anything.
     * Use strcmp(buffer, "exit") == 0; We will go to STATE: Exit;
     * Do NOT pass this buffer to Storage_Context(). We don't want "exit" sitting in our history.
  * If it's not "exit":
     * Pass buffer to Storage_Context(buffer, USER);
     * Store user message into Storage[Storage_Idx][256]
     * We will pass the buffer to the function Model().
     * Store model response into Storage via Storage_Context(response, MODEL);
     * Print the model function output and stay in the running state.

  State Exit:

  * Our Storage array is static/global, not malloc'd — so we don't actually free() anything here, we just clear it out.
  * Change context management state to END State.
  * Print "Goodbye."
  * Exit(0)
  ```

- **Derived architectural rules:**
  - `buffer` is `char[256]`, populated via `fgets(buffer, 256, stdin)`.
  - Newline stripping is mandatory immediately after every `fgets` call.
  - The `exit` keyword check (`strcmp(buffer, "exit") == 0`) happens
    *before* any call to `Storage_Context`, so "exit" never enters history.
  - Non-exit turns: `Storage_Context(buffer, USER)` →
    `Model(buffer)` → `Storage_Context(response, MODEL)` → print
    response → remain in RUNNING state.
  - `Storage` is a **static/global** array shaped `Storage[][256]` indexed
    by `Storage_Idx` (no heap allocation — no `malloc`/`free`).
  - EXIT state: clear `Storage` in place (no `free()`), transition context
    state to `END`, print `Goodbye.`, call `exit(0)`.
- **Open items for next SDD installment:** START state definition,
  `Storage` array dimensions / `Storage_Idx` wraparound policy for
  "last 5 turns", `Model()` mock behavior, tool-execution trigger and
  logic.
