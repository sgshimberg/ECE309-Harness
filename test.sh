#!/bin/bash
# test.sh -- automated, non-interactive test driver for the ECE 309 harness.
# Validates: build, core echo/hello behavior, calculator tool (all four
# operations, synonyms, "divided by", divide-by-zero, and malformed input),
# context-window state management across more than 5 turns, safe exit, and
# (when valgrind is available) a basic memory-leak check.

set -u                                      # treat unset variables as errors to catch typos early

BINARY="./harness"                          # path to the compiled harness under test
SOURCE="harness.c"                          # path to the harness source to (re)build if needed
FAIL_COUNT=0                                # running count of failed assertions

# Build the harness fresh so the test always reflects the current source.
gcc -std=c99 -Wall -Wextra "$SOURCE" -o harness
if [ $? -ne 0 ]; then                       # gcc reported a compile error
    echo "BUILD FAILED"                     # tell the user the build itself failed
    exit 1                                  # nothing else can run without a binary
fi

# run_case: pipes $1 (a multi-line here-string of user input) into the
# harness and checks that $2 (a plain substring) appears somewhere in the
# output. $3 is a human-readable label for the assertion.
run_case() {
    local input="$1"                        # the full sequence of lines to feed the harness
    local expect="$2"                       # substring we expect to find in the output
    local label="$3"                        # description printed in the pass/fail line
    local output                            # captured stdout from the harness run

    output=$(printf '%s' "$input" | "$BINARY")
    if echo "$output" | grep -qF -- "$expect"; then
        echo "PASS: $label"                 # substring was found, assertion succeeded
    else
        echo "FAIL: $label (expected to find: '$expect')"  # substring missing, assertion failed
        echo "--- actual output ---"        # show the real output to help debugging
        echo "$output"                      # dump what the harness actually printed
        echo "---------------------"        # closing separator for readability
        FAIL_COUNT=$((FAIL_COUNT + 1))       # record this failure
    fi
}

echo "== Core loop: hello / echo / exit =="
run_case $'hello\nexit\n' "Hello! Nice to hear from you." "hello triggers greeting"
run_case $'well hello there\nexit\n' "Hello! Nice to hear from you." "hello variation (case/substring) triggers greeting"
run_case $'random gibberish\nexit\n' "random gibberish" "non-matching input is echoed back"
run_case $'hello\nexit\n' "Goodbye." "exit prints farewell and terminates"

echo "== Calculator tool =="
run_case $'calculate 2 + 2\nexit\n' "The answer is 4" "addition via symbol"
run_case $'calculate 5 add 5\nexit\n' "The answer is 10" "addition via 'add' synonym"
run_case $'calculate 7 minus 3\nexit\n' "The answer is 4" "subtraction via 'minus' synonym"
run_case $'calculate 6 times 7\nexit\n' "The answer is 42" "multiplication via 'times' synonym"
run_case $'calculate 20 divided by 4\nexit\n' "The answer is 5" "division via two-word 'divided by' synonym"
run_case $'calculate 9 over 3\nexit\n' "The answer is 3" "division via 'over' synonym"
run_case $'calculate 10 divided by 0\nexit\n' "Error: divide by zero" "divide-by-zero is caught, not crashed"
run_case $'calculate 5 % 2\nexit\n' "Error: unknown operator" "unrecognized operator is rejected cleanly"
run_case $'calculate abc + 2\nexit\n' "Error: invalid number" "non-numeric operand is rejected cleanly"

echo "== Context window (last 5 turns) survives past capacity =="
run_case $'one\ntwo\nthree\nfour\nfive\nsix\nseven\ncalculate 3 + 5\nexit\n' "The answer is 8" "harness stays correct after more than 5 turns (FIFO shift)"

echo "== Exit keyword is never echoed or stored =="
run_case $'exit\n' "Goodbye." "typing exit immediately shuts down cleanly"

echo
if [ "$FAIL_COUNT" -eq 0 ]; then
    echo "ALL FUNCTIONAL TESTS PASSED"
else
    echo "$FAIL_COUNT FUNCTIONAL TEST(S) FAILED"
fi

echo
echo "== Memory leak check =="
if command -v valgrind >/dev/null 2>&1; then
    VALGRIND_LOG=$(mktemp)                  # temp file to capture valgrind's report
    printf 'hello\ncalculate 4 * 4\ncalculate 1 divided by 0\nexit\n' | \
        valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all \
            "$BINARY" > /dev/null 2> "$VALGRIND_LOG"
    VALGRIND_STATUS=$?                       # valgrind's exit code (99 means it flagged an error)

    if grep -q "no leaks are possible" "$VALGRIND_LOG" || \
       grep -q "definitely lost: 0 bytes in 0 blocks" "$VALGRIND_LOG"; then
        echo "PASS: valgrind reports no memory leaks"
    else
        echo "FAIL: valgrind reported possible leaks"
        cat "$VALGRIND_LOG"                  # show the full report for debugging
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi

    if [ "$VALGRIND_STATUS" -eq 99 ]; then
        echo "FAIL: valgrind reported a memory error (invalid read/write, etc.)"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi

    rm -f "$VALGRIND_LOG"                    # clean up the temp report file
else
    echo "SKIPPED: valgrind not installed on this system"
fi

echo
if [ "$FAIL_COUNT" -eq 0 ]; then
    echo "TEST SUITE RESULT: PASS"
    exit 0
else
    echo "TEST SUITE RESULT: FAIL ($FAIL_COUNT failure(s))"
    exit 1
fi
