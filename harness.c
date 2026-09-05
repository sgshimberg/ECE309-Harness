/* harness.c -- ECE 309 Project 1: LLM Mini-Harness in C via Vibe Coding */
#include <stdio.h>                              /* standard input/output: printf, fgets, snprintf, fflush */
#include <string.h>                             /* string ops: strcmp, strncpy, strtok, strlen, memset */

#define LINE_LEN 256                            /* fixed width of every stored/read message line */
#define MAX_STORAGE 10                          /* 10 slots = last 5 turns (1 user + 1 model msg per turn) */
#define USER 0                                  /* tag value meaning "this slot was written by the user" */
#define MODEL 1                                 /* tag value meaning "this slot was written by the model" */

static char Storage[MAX_STORAGE][LINE_LEN];     /* static/global context buffer: no malloc, so no free() needed */
static int Storage_Agent[MAX_STORAGE];          /* parallel array tagging who (USER/MODEL) wrote each slot */
static int Storage_Idx = 0;                     /* next write position; caps at MAX_STORAGE once full */

void strip_newline(char *buffer);               /* forward decl: remove trailing '\n' left by fgets */
int equals_ci(const char *a, const char *b);    /* forward decl: case-insensitive exact string compare */
int contains_ci(const char *haystack, const char *needle); /* forward decl: case-insensitive substring search */
char match_operator(const char *tok);           /* forward decl: map an operator token to +,-,*,/ or 0 */
void Context_Window_Shift(void);                /* forward decl: FIFO-shift Storage left by one when full */
void Storage_Context(const char *buffer, int agent); /* forward decl: write one message into Storage */
void Context_End(void);                         /* forward decl: END state - wipe Storage, reset Storage_Idx */
int Tool_Calculator(const char *input, char *output); /* forward decl: parse+run "calculate NUM OP NUM" */
void Model(const char *input, char *output);    /* forward decl: mock LLM - tool check, hello check, echo */

/* strip_newline: fgets keeps the trailing '\n', so we cut it off in place */
void strip_newline(char *buffer) {
    size_t len = strlen(buffer);                /* length of the string currently in buffer */
    if (len > 0 && buffer[len - 1] == '\n') {   /* only touch it if the last char really is a newline */
        buffer[len - 1] = '\0';                 /* overwrite the newline with a null terminator */
    }
}

/* equals_ci: returns 1 if a and b are equal ignoring letter case, 0 otherwise */
int equals_ci(const char *a, const char *b) {
    size_t i;                                   /* loop index shared across both strings */
    if (strlen(a) != strlen(b)) {                /* different lengths can never be equal */
        return 0;                               /* not equal, bail out early */
    }
    for (i = 0; a[i] != '\0'; i++) {            /* walk both strings one character at a time */
        char ca = a[i];                         /* current character from string a */
        char cb = b[i];                         /* current character from string b */
        if (ca >= 'A' && ca <= 'Z') {            /* manually fold a's uppercase letters to lowercase */
            ca = (char)(ca + 32);               /* 'A'..'Z' -> 'a'..'z' via ASCII offset */
        }
        if (cb >= 'A' && cb <= 'Z') {            /* manually fold b's uppercase letters to lowercase */
            cb = (char)(cb + 32);               /* 'A'..'Z' -> 'a'..'z' via ASCII offset */
        }
        if (ca != cb) {                         /* first mismatch means the strings are not equal */
            return 0;                           /* not equal, bail out early */
        }
    }
    return 1;                                   /* every character matched: strings are equal */
}

/* contains_ci: returns 1 if needle appears anywhere in haystack, ignoring case */
int contains_ci(const char *haystack, const char *needle) {
    size_t hlen = strlen(haystack);             /* total length of the string being searched */
    size_t nlen = strlen(needle);                /* length of the substring we're looking for */
    size_t i, j;                                 /* i = start offset in haystack, j = offset within needle */
    if (nlen == 0) {                             /* an empty needle trivially "matches" anywhere */
        return 1;                               /* report a match */
    }
    if (nlen > hlen) {                           /* needle longer than haystack can never fit */
        return 0;                               /* no match possible */
    }
    for (i = 0; i + nlen <= hlen; i++) {        /* try every valid starting offset in haystack */
        int matched = 1;                        /* assume this offset matches until proven otherwise */
        for (j = 0; j < nlen; j++) {            /* compare needle against haystack[i..i+nlen) */
            char hc = haystack[i + j];          /* current haystack character under comparison */
            char nc = needle[j];                /* current needle character under comparison */
            if (hc >= 'A' && hc <= 'Z') {        /* fold haystack char to lowercase manually */
                hc = (char)(hc + 32);           /* 'A'..'Z' -> 'a'..'z' via ASCII offset */
            }
            if (nc >= 'A' && nc <= 'Z') {        /* fold needle char to lowercase manually */
                nc = (char)(nc + 32);           /* 'A'..'Z' -> 'a'..'z' via ASCII offset */
            }
            if (hc != nc) {                      /* any mismatch means this offset fails */
                matched = 0;                    /* mark this offset as not a match */
                break;                          /* stop scanning this offset, try the next one */
            }
        }
        if (matched) {                          /* every character in the window matched */
            return 1;                           /* needle found inside haystack */
        }
    }
    return 0;                                   /* scanned every offset, never found needle */
}

/* match_operator: maps an operator token (symbol or word synonym) to +, -, *, / or 0 if unknown */
char match_operator(const char *tok) {
    if (strcmp(tok, "+") == 0 || equals_ci(tok, "add") ||
        equals_ci(tok, "plus") || equals_ci(tok, "sum")) {  /* addition symbol or word synonyms */
        return '+';                             /* report addition */
    }
    if (strcmp(tok, "-") == 0 || equals_ci(tok, "subtract") ||
        equals_ci(tok, "minus") || equals_ci(tok, "less")) { /* subtraction symbol or word synonyms */
        return '-';                             /* report subtraction */
    }
    if (strcmp(tok, "*") == 0 || equals_ci(tok, "multiply") ||
        equals_ci(tok, "times") || equals_ci(tok, "product")) { /* multiplication symbol or word synonyms */
        return '*';                             /* report multiplication */
    }
    if (strcmp(tok, "/") == 0 || equals_ci(tok, "divide") ||
        equals_ci(tok, "over")) {               /* division symbol or single-word synonyms */
        return '/';                             /* report division ("divided by" is handled separately) */
    }
    return 0;                                    /* token did not match any known operator */
}

/* Context_Window_Shift: FIFO shift - drop the oldest entry, open up a fresh last slot */
void Context_Window_Shift(void) {
    int i;                                       /* loop index over the shifting slots */
    for (i = 0; i < MAX_STORAGE - 1; i++) {      /* walk slots 0..8, pulling each one from its neighbor */
        strncpy(Storage[i], Storage[i + 1], LINE_LEN - 1); /* copy slot i+1's text into slot i */
        Storage[i][LINE_LEN - 1] = '\0';        /* guarantee null-termination after the copy */
        Storage_Agent[i] = Storage_Agent[i + 1]; /* carry the author tag along with the text */
    }
    memset(Storage[MAX_STORAGE - 1], 0, LINE_LEN); /* clear the newly-freed last slot with null bytes */
    Storage_Agent[MAX_STORAGE - 1] = 0;         /* reset its author tag too */
}

/* Storage_Context: write one message (from USER or MODEL) into the context history */
void Storage_Context(const char *buffer, int agent) {
    int write_idx;                               /* the slot this message will actually be written to */
    if (Storage_Idx == MAX_STORAGE) {            /* history is full: make room before writing */
        Context_Window_Shift();                  /* shift everything left, freeing the last slot */
    }
    write_idx = (Storage_Idx < MAX_STORAGE) ? Storage_Idx : (MAX_STORAGE - 1); /* pick target slot */
    strncpy(Storage[write_idx], buffer, LINE_LEN - 1); /* copy the message text into that slot */
    Storage[write_idx][LINE_LEN - 1] = '\0';    /* guarantee null-termination after the copy */
    Storage_Agent[write_idx] = agent;            /* record who (USER/MODEL) said this */
    if (Storage_Idx < MAX_STORAGE) {             /* only grow the counter while there's still room */
        Storage_Idx++;                           /* advance to the next free slot for next time */
    }
}

/* Context_End: END state - wipe every slot back to empty and reset the write position */
void Context_End(void) {
    int i;                                        /* loop index over every storage slot */
    for (i = 0; i < MAX_STORAGE; i++) {          /* visit every slot in the history */
        memset(Storage[i], 0, LINE_LEN);         /* clear its text with null characters */
        Storage_Agent[i] = 0;                    /* reset its author tag to a neutral default */
    }
    Storage_Idx = 0;                              /* history is empty again: next write goes to slot 0 */
}

/* Tool_Calculator: parses "calculate NUM OP NUM" and writes the result (or an error) into output */
int Tool_Calculator(const char *input, char *output) {
    char work[LINE_LEN];                          /* mutable copy of input, since strtok rewrites it */
    char *tokens[6];                              /* up to 6 whitespace-separated tokens from the line */
    int ntok = 0;                                 /* how many tokens we actually collected */
    char *tok;                                    /* cursor used while walking strtok's results */
    int num1, num2;                               /* the two integer operands once parsed */
    char opchar;                                  /* the resolved operator: '+', '-', '*', or '/' */
    int num2_idx;                                 /* which token holds the second number */
    int result;                                   /* the computed result for +, -, * cases */

    strncpy(work, input, LINE_LEN - 1);           /* copy input so the original buffer stays untouched */
    work[LINE_LEN - 1] = '\0';                    /* guarantee null-termination after the copy */

    tok = strtok(work, " ");                      /* grab the first whitespace-delimited token */
    while (tok != NULL && ntok < 6) {              /* keep collecting tokens until none remain or array fills */
        tokens[ntok] = tok;                        /* store this token pointer */
        ntok++;                                    /* count it */
        tok = strtok(NULL, " ");                   /* advance to the next token in the same string */
    }

    if (ntok < 4) {                                /* strict format needs at least keyword, num, op, num */
        snprintf(output, LINE_LEN, "Error: invalid calculation format"); /* report the format problem */
        return 0;                                  /* signal parse failure to the caller */
    }

    if (sscanf(tokens[1], "%d", &num1) != 1) {     /* try to read the first number from token[1] */
        snprintf(output, LINE_LEN, "Error: invalid number"); /* token[1] wasn't a valid integer */
        return 0;                                  /* signal parse failure to the caller */
    }

    if (equals_ci(tokens[2], "divided") && ntok >= 5 && equals_ci(tokens[3], "by")) { /* "divided by" case */
        opchar = '/';                              /* two-word synonym always means division */
        num2_idx = 4;                              /* the number comes after "divided by", so token[4] */
    } else {                                        /* every other operator is exactly one token */
        opchar = match_operator(tokens[2]);        /* resolve token[2] against the operator synonym list */
        num2_idx = 3;                               /* the second number is the very next token */
    }

    if (opchar == 0) {                              /* match_operator found nothing usable */
        snprintf(output, LINE_LEN, "Error: unknown operator"); /* report the unrecognized operator */
        return 0;                                   /* signal parse failure to the caller */
    }

    if (num2_idx >= ntok || sscanf(tokens[num2_idx], "%d", &num2) != 1) { /* fetch the second number */
        snprintf(output, LINE_LEN, "Error: invalid number"); /* missing token or not a valid integer */
        return 0;                                   /* signal parse failure to the caller */
    }

    if (opchar == '+') {                            /* addition branch */
        result = num1 + num2;                       /* compute the sum */
        snprintf(output, LINE_LEN, "%d", result);   /* write the numeric result as a string */
    } else if (opchar == '-') {                     /* subtraction branch */
        result = num1 - num2;                        /* compute the difference */
        snprintf(output, LINE_LEN, "%d", result);    /* write the numeric result as a string */
    } else if (opchar == '*') {                     /* multiplication branch */
        result = num1 * num2;                         /* compute the product */
        snprintf(output, LINE_LEN, "%d", result);     /* write the numeric result as a string */
    } else {                                          /* division branch (only '/' remains) */
        if (num2 == 0) {                              /* guard against dividing by zero */
            snprintf(output, LINE_LEN, "Error: divide by zero"); /* refuse the division, report the error */
            return 1;                                 /* this is a handled outcome, not a parse failure */
        }
        result = num1 / num2;                          /* integer division since inputs are parsed as %d */
        snprintf(output, LINE_LEN, "%d", result);       /* write the numeric result as a string */
    }
    return 1;                                          /* calculation completed successfully */
}

/* Model: mock LLM - checks for the calculator trigger first, then "hello", else echoes the input */
void Model(const char *input, char *output) {
    char tool_output[LINE_LEN];                       /* holds Tool_Calculator's raw result/error string */
    if (contains_ci(input, "calculate")) {             /* calculator trigger check happens before "hello" */
        Tool_Calculator(input, tool_output);           /* run the calculator on the raw input line */
        if (strncmp(tool_output, "Error", 5) == 0) {   /* an error string should not get "The answer is" */
            snprintf(output, LINE_LEN, "%s", tool_output); /* pass the error through unframed */
        } else {                                        /* a genuine numeric result was produced */
            snprintf(output, LINE_LEN, "The answer is %s", tool_output); /* frame it as a model response */
        }
        return;                                          /* calculator path is done, skip hello/echo checks */
    }
    if (contains_ci(input, "hello")) {                  /* check for any variation of "hello" */
        snprintf(output, LINE_LEN, "Hello! Nice to hear from you."); /* hardcoded greeting response */
        return;                                          /* greeting path is done, skip the echo fallback */
    }
    snprintf(output, LINE_LEN, "%s", input);             /* default: echo the user's input back verbatim */
}

/* main: the START/RUNNING/EXIT state machine that drives the whole harness */
int main(void) {
    char buffer[LINE_LEN];                                /* holds each raw line typed by the user */
    char response[LINE_LEN];                              /* holds Model()'s response for the current turn */

    for (;;) {                                             /* START state folds into this loop's first pass */
        printf("> ");                                      /* show the empty-field prompt for the user */
        fflush(stdout);                                    /* force the prompt to appear before fgets blocks */
        if (fgets(buffer, LINE_LEN, stdin) == NULL) {       /* block waiting for input; NULL means EOF/error */
            break;                                          /* no more input is coming, so shut down safely */
        }
        strip_newline(buffer);                              /* remove the trailing '\n' fgets left behind */

        if (strcmp(buffer, "exit") == 0) {                  /* exit check happens before any storage write */
            Context_End();                                   /* END state: wipe Storage, reset Storage_Idx */
            printf("Goodbye.\n");                            /* EXIT state: print the farewell message */
            break;                                            /* leave the loop, falling through to return 0 */
        }

        Storage_Context(buffer, USER);                       /* record the user's message before calling Model */
        Model(buffer, response);                             /* run the mock model on the user's input */
        Storage_Context(response, MODEL);                    /* record the model's response in the same history */
        printf("%s\n", response);                            /* show the model's response back to the user */
    }

    return 0;                                                 /* equivalent to Exit(0) without needing stdlib.h */
}
