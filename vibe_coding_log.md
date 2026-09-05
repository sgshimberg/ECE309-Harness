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

### Entry 3 — Context Management: Storage, CONTEXT_WINDOW, and END States

- **Date:** 2026-09-05
- **Prompt (from student), verbatim:**

  ```
  Context Management.

  * To keep memory free we must limit ourselves to the last 5 turns.
  * Storage[10][256]; — 10 slots since 5 turns = 10 messages (1 user + 1 model per turn).
  * Storage_Idx starts at 0 and increments upward toward 10 on every single write.
  * Storage_Agent_Idx is no longer the same thing as Storage_Idx — it tags WHO wrote the message (USER or MODEL) so we know whose turn it was.

  Storage_Context(buffer, Storage_Agent_Idx):

  * If Storage_Idx = 10, we run the CONTEXT_WINDOW shift BEFORE we write anything new.
  * Write buffer into Storage[Storage_Idx].
  * Tag it with Storage_Agent_Idx so we remember who said it.
  * If Storage_Idx < 10, Storage_Idx++.

  State CONTEXT_WINDOW:

  * Only triggers when Storage_Idx = 10, right before the new write.
  * Shift all of the strings in Storage up 1 so that context can be loaded at Storage_Idx = 9 and off loaded at Storage_Idx = 0. This follows the FIFO (First In First Out) principle.
  * Clear Storage[9] with null characters before the new message lands there.
  * Storage_Idx just stays at 10 from here on — we shift instead of incrementing.

  State END

  * For every Storage_Idx clear the string to start fresh. So clear with null characters.
  * Set Storage_Idx = 0;
  ```

- **Derived architectural rules:**
  - `Storage` is `char Storage[10][256]` (static/global) — 10 message
    slots = last 5 turns (1 user + 1 model message per turn).
  - A parallel tag store (`Storage_Agent_Idx` per slot, e.g.
    `int Storage_Agent[10]` holding `USER`/`MODEL`) records who authored
    each `Storage[i]` entry — distinct from `Storage_Idx`, which is the
    write-position counter.
  - `Storage_Idx` starts at 0, increments by 1 on every write while
    `Storage_Idx < 10`, and once it reaches 10 it stays at 10 permanently
    (no further increment — the array is full and now operates as a FIFO
    via CONTEXT_WINDOW shifting instead of growing).
  - `Storage_Context(buffer, agent_tag)`:
    1. If `Storage_Idx == 10`, run the `CONTEXT_WINDOW` shift first.
    2. Write `buffer` into the current write slot; tag it with
       `agent_tag`.
    3. If `Storage_Idx < 10`, `Storage_Idx++`.
  - `CONTEXT_WINDOW` (triggers only when `Storage_Idx == 10`, right before
    the new write): shift every entry left by one index (`Storage[i] =
    Storage[i+1]` for `i` in `0..8`, tags shifted the same way), clear
    `Storage[9]` (null bytes) so the incoming message lands in a clean
    slot, and leave `Storage_Idx` at 10 (FIFO from here on — oldest entry
    at index 0 is dropped, newest always lands at index 9).
  - `END` state (final cleanup, distinct from the `EXIT` state's
    "Goodbye."/`exit(0)` behavior): null out every `Storage[i]` string and
    reset `Storage_Idx = 0`.
  - **Implementation note for code-gen (to confirm/flag, not yet asked of
    student):** with `Storage_Idx` capped at 10 but valid array indices
    only `0..9`, "write into `Storage[Storage_Idx]`" after the shift is
    read as "write into `Storage[9]`" (the slot just cleared by
    `CONTEXT_WINDOW`) — i.e. once full, every new message always lands at
    index 9 after the FIFO shift. Will implement it this way unless
    corrected.
- **Open items for next SDD installment:** START state definition,
  `Model()` mock behavior, tool-execution trigger and logic.

### Entry 4 — START State

- **Date:** 2026-09-05
- **Prompt (from student), verbatim:**

  ```
  In the start state, it should wait to be prompted. Once someone types into
  the chat it should then execute. It should be an empty field for the user
  to type in then it stays in running until exited.
  ```

- **Derived architectural rules:**
  - `START` state: print the prompt (`> `) with an empty input field and
    block on `fgets` — no processing happens until the user actually types
    something and presses enter.
  - The first line of input received is what triggers execution: it flows
    straight into the RUNNING-state logic already specified (exit check →
    `Storage_Context`/`Model` dispatch, per Entry 2). There is no separate
    "waiting" busy-loop — the blocking `fgets` call itself is the wait.
  - After that first input is processed, the program remains in the
    RUNNING state, re-prompting and re-reading on each iteration, until
    the `exit` keyword is entered (→ EXIT state, per Entry 2).
  - In effect, `START` is just the first pass through the same
    prompt/read/process loop as `RUNNING` — no distinct banner or extra
    initialization beyond `Storage_Idx = 0` and printing the first `> `
    prompt.
- **Open items for next SDD installment:** `Model()` mock behavior,
  tool-execution trigger and logic.

### Entry 5 — Header Constraint Confirmed: `<stdio.h>` and `<string.h>` Only

- **Date:** 2026-09-05
- **Prompt (from student), verbatim:** "For reference you are only using
  Stdio.h and String.h?" → followed by "keep it." in response to being
  offered a choice between (1) also allowing `<stdlib.h>` for `exit()`/
  number parsing, or (2) strictly limiting to `<stdio.h>` and
  `<string.h>` only.
- **Decision:** Strict — **only** `<stdio.h>` and `<string.h>` may be
  `#include`d. No `<stdlib.h>`, `<ctype.h>`, etc.
- **Consequences for implementation:**
  - The EXIT state's `Exit(0)` (Entry 1) will be implemented as falling
    out of the main loop and `return 0;` from `main()` (equivalent
    process-exit behavior) rather than calling the `<stdlib.h>` `exit()`
    function, since `exit()` is not declared by `<stdio.h>`/`<string.h>`.
  - The math tool-execution spec (once provided) will need to parse
    numeric text manually via direct character comparisons
    (`c >= '0' && c <= '9'`, etc.) rather than `atoi`/`atof`/`strtol`
    (`<stdlib.h>`) or `isdigit` (`<ctype.h>`).
- **Open items for next SDD installment:** `Model()` mock behavior,
  tool-execution trigger and logic.

### Entry 6 — `Model()` Mock Behavior & Global Commenting Requirement

- **Date:** 2026-09-05
- **Prompt (from student), verbatim:** "for the model() of the input
  includes andy [any] variation of hello then respond back with a
  greetings. other than that echo back what the user sends back." —
  (message interrupted by student) — followed by: "Make sure every line
  has a comment of what it is doing."
- **Derived architectural rules:**
  - `Model(buffer)` behavior:
    - If `buffer` contains **any variation of "hello"** (case-insensitive
      substring match — e.g. "Hello", "HELLO", "well hello there",
      "hellooo") → return a hardcoded greeting string.
    - Otherwise → echo the input back verbatim as the "response".
  - Since case-insensitive matching is needed and `<ctype.h>`
    (`tolower`) is off-limits under the Entry 5 header constraint, the
    substring search will be implemented manually (hand-rolled
    case-insensitive substring match comparing characters with manual
    upper/lower-case offset logic, using only `<string.h>`/`<stdio.h>`).
  - **Global code style requirement:** every line of the generated
    `harness.c` must carry a comment explaining what that line does
    (matches the spec's Phase 2 "Vibe" prompt template item 5: "clear,
    line-by-line comments").
- **Open items for next SDD installment:** tool-execution trigger and
  logic (math calculation tool).

### Entry 7 — Tool Integration: Calculator

- **Date:** 2026-09-05
- **Prompt (from student), verbatim:**

  ```
  Tool Integration: Calculator
  State Tool_Calculator

  * We will support simple calculations: Addition, Subtraction, Multiplication, and Division.
  * To keep parsing simple, we require a strict input format: "calculate NUM OP NUM" (e.g. "calculate 2 + 2"). We are NOT supporting multi-step expressions like "2 + 2 * 3" — only two numbers and one operator.
  * We look for a trigger keyword like "calculate" (and synonyms) inside the user's buffer to decide if the tool should run instead of the normal Model() echo/hello logic. This check must happen BEFORE the "hello" check in Model(), otherwise a calculation request could get swallowed by the echo/hello logic.
  * We also look for synonyms of each operation so the user doesn't have to type exact symbols:
     * Addition → "add", "plus", "sum" → maps to '+'
     * Subtraction → "subtract", "minus", "less" → maps to '-'
     * Multiplication → "multiply", "times", "product" → maps to '*'
     * Division → "divide", "divided by", "over" → maps to '/'
  * Parse the buffer:
     * Use strtok or sscanf to split the buffer into tokens (numbers and operator words/symbols), since we're enforcing the strict format above.
     * Convert number tokens using sscanf directly (e.g. sscanf(token, "%d", &num)) — NOT atoi/atof, since those require <stdlib.h>.
     * Match operator tokens against our synonym list to figure out which operator to actually perform.
  * Perform the calculation based on the matched operator.
     * Divide by zero check: if the operator is '/' and the second number is 0, do NOT perform the division. Return an error string like "Error: divide by zero" instead.
  * Store the calculated value (or the divide-by-zero error string) as a string (snprintf into a buffer) so it can go through the same Storage/Model pipeline as everything else.
  * Function signature: int Tool_Calculator(char *input, char *output); — takes the input buffer, writes the result into the output buffer, matching the same "pass buffer in, write buffer out" pattern as Model().

  Flow (Tool → Model → User → Storage):

  1. Tool_Calculator(buffer, output) runs, writes result string into output (this includes the divide-by-zero error case).
  2. Output string gets passed into Model() so it can be framed as part of the AI's response (e.g., "The answer is 4").
  3. Model's output (containing the tool result) prints to the user like normal.
  4. Model's output is stored into Storage via Storage_Context(response, MODEL) — same as any other model response.
  ```

- **Derived architectural rules / implementation decisions:**
  - `Model(char *input, char *output)` signature: matches the "buffer in,
    buffer out" pattern established by `Tool_Calculator`, since the
    student's spec repeatedly treats `Model()` as writing a response
    that then flows to `Storage_Context`.
  - Trigger detection happens **inside** `Model()`, as the very first
    check, before the "hello" check: a case-insensitive substring search
    for the literal word `"calculate"` in `input`. No explicit synonym
    list was given for the trigger word itself (only for the four
    operations) — implemented as substring match on "calculate" only;
    flagged as an assumption since the spec says "and synonyms" without
    naming them.
  - Reconciling the two "call order" descriptions in the prompt (trigger
    check "before hello check in `Model()`" vs. the Tool→Model flow
    diagram listing `Tool_Calculator` as step 1 "before" `Model`): read
    as `Model()` internally calling `Tool_Calculator` first (once it
    detects the trigger keyword), then framing the tool's output — so
    both descriptions describe the same code path, just at different
    levels of detail.
  - Framing rule (own addition, not explicit in the prompt): a
    successful/numeric result is framed as `"The answer is <value>"`;
    a `"Error: ..."` string from `Tool_Calculator` is passed straight
    through unframed, since prefixing an error with "The answer is"
    would read oddly. Flagged for confirmation.
  - Operator/token parsing implemented with `strtok(buffer, " ")` into
    an array of tokens, then `sscanf(token, "%d", &num)` for numbers, per
    the prompt. Tokens are matched against synonym lists
    (add/plus/sum→'+', subtract/minus/less→'-',
    multiply/times/product→'*', divide/over→'/') case-insensitively via
    a hand-rolled `equals_ci` (no `<ctype.h>`).
  - **Special case for "divided by":** since it's a two-word synonym but
    the strict format assumes one operator token, `Tool_Calculator`
    specifically checks whether token[2] == "divided" and token[3] ==
    "by", and if so treats the operator as `/` with the second number
    read from token[4] instead of token[3]. Flagged as an own addition
    to reconcile the two-word synonym with the "single operator token"
    format.
  - Divide-by-zero: detected before performing `/`, writes
    `"Error: divide by zero"` into `output` via `snprintf`, does not
    divide.
  - `Tool_Calculator` return value: `1` on a successfully parsed and
    computed (or gracefully divide-by-zero-handled) request, `0` if the
    strict format couldn't be parsed at all (own addition — not
    specified by the prompt, but needed so the caller can distinguish a
    malformed "calculate ..." message from a real result).

### Entry 8 — Code Generation: `harness.c`, `test.sh`

- **Date:** 2026-09-05
- **Action:** With every SDD item from Entries 1-7 covering the full state
  machine (START/RUNNING/EXIT), context management (`Storage`,
  `Storage_Context`, `CONTEXT_WINDOW`, `END`), `Model()`, and
  `Tool_Calculator`, generated a single-file `harness.c` implementing all
  of it, with a comment on every line per the global commenting
  requirement (Entry 6).
- **Two implementation choices made during code-gen, not explicit in any
  prior prompt:**
  1. `main()`'s `for (;;)` loop treats `fgets` returning `NULL` (stdin
     EOF/closed, e.g. when a test script's piped input runs out) as a
     silent shutdown — `break`s out of the loop without printing
     "Goodbye." (since the user never typed `exit`), then falls through
     to `return 0;`. This wasn't specified, but is required so the
     harness terminates safely instead of spinning on `NULL` input when
     driven non-interactively.
  2. Reconciled the EXIT state's informal "clear it out" (Entry 2) with
     the formally-specified END state (Entry 3) as the *same* single
     action: `main()`'s exit path calls one `Context_End()` function
     (implementing the END state's clear-and-reset), then prints
     "Goodbye." — rather than clearing twice.
- **Verification performed:**
  - `gcc -std=c99 -Wall -Wextra -pedantic harness.c -o harness` — compiles
    with only one benign `-Wformat-truncation` note on the `snprintf`
    framing line (not a real bug: results are always short numbers/error
    strings, well within the 256-byte buffer).
  - Manually piped test sequences covering: hello, echo, all four
    calculator operations (symbols and every listed synonym, including
    the two-word "divided by" case), divide-by-zero, an unrecognized
    operator (`%`), a non-numeric operand, more than 5 turns in a row (to
    exercise the `CONTEXT_WINDOW` FIFO shift), and `exit` — all produced
    the expected output with no crashes.
  - Wrote `test.sh` (AI-generated per spec's "AI-Generated Testing"
    requirement): rebuilds the harness, asserts on piped-input output for
    every case above, and — since `valgrind` is available in this
    environment — runs `valgrind --leak-check=full --show-leak-kinds=all`
    over a representative session. Result: all functional assertions
    passed, and valgrind reports zero leaks (expected, since `Storage` is
    static/global with no heap allocation anywhere in the program).
  - Updated `README.md` with a features summary and testing instructions.

### Entry 9 — Authorship Correction and Design Notes

- **Date:** 2026-09-05
- **Prompt (from student):** Asked to be made the lead contributor on the
  GitHub repo (commits so far were authored as "Claude
  <noreply@anthropic.com>"), and to add their own SDD design-notes
  document (`docs/sdd_notes.pdf`) to the repo for their professor.
- **Action taken:**
  - Set the local git identity to the student (`sgshimberg
    <Distorex1@gmail.com>`) and rewrote every existing commit's
    author/committer to match, preserving each commit's message
    (including the `Co-Authored-By: Claude` trailer) and force-pushing
    the rewritten history.
  - Added `docs/sdd_notes.pdf` — the student's original written SDD
    notes (state machine, context management, and calculator tool spec)
    that were fed to the AI assistant piece-by-piece across Entries 2-7.
    Referenced it from `README.md`.
