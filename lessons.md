# C Study Guide

An original, chapter-by-chapter study guide that covers the same ground, in the same order, as the classic C reference text (*The C Programming Language* by Kernighan & Ritchie). All notes, examples and exercises here are written from scratch. Use it alongside the book, not as a replacement for it.

**How to use this guide:** read a chapter, type out every example, compile with `gcc -Wall -Wextra -std=c11`, then do the exercises before moving on.

## Contents

1. [A Quick Tour](#chapter-1-a-quick-tour)
2. [Types, Operators and Expressions](#chapter-2-types-operators-and-expressions)
3. [Control Flow](#chapter-3-control-flow)
4. [Functions and Program Structure](#chapter-4-functions-and-program-structure)
5. [Pointers and Arrays](#chapter-5-pointers-and-arrays)
6. [Structures](#chapter-6-structures)
7. [Input and Output](#chapter-7-input-and-output)
8. [The UNIX System Interface](#chapter-8-the-unix-system-interface)
9. [Appendix: Standard Library Quick Reference](#appendix-standard-library-quick-reference)

---

## Chapter 1: A Quick Tour

**Goal:** get a feel for the whole language before studying the details.

### Program shape

```c
#include <stdio.h>

int main(void) {
    printf("C is small but sharp.\n");
    return 0;
}
```

- `main` is where execution begins.
- Statements end with `;`, blocks are wrapped in `{ }`.
- Comments: `/* block */` and `// line` (C99 and later).

### Variables and arithmetic

All variables must be declared before use. Integer division truncates:

```c
int apples = 7, people = 2;
printf("%d\n", apples / people);        // 3
printf("%.1f\n", (double)apples / people); // 3.5
```

### Loops and symbolic constants

```c
#define LIMIT 5

for (int i = 0; i < LIMIT; i++) {
    printf("%d squared = %d\n", i, i * i);
}
```

`#define` replaces text before compilation. It has no type and no semicolon.

### Character input and output

`getchar()` reads one character and returns an `int`; `putchar(c)` writes one. The end of input is signalled by the constant `EOF`, which is why the variable must be an `int` and not a `char`.

```c
// count how many lines are typed in
int c, lines = 0;
while ((c = getchar()) != EOF) {
    if (c == '\n') lines++;
}
printf("%d lines\n", lines);
```

Run it with a file as input: `./prog < notes.txt`. Press Ctrl+D to send EOF from the keyboard on Linux/macOS.

### Arrays

```c
int counts[10] = {0};         // ten zeros
counts[3]++;
```

### Functions

A function has a return type, a name, parameters and a body. Arguments are passed **by value**: the function works on copies.

```c
int cube(int n) { return n * n * n; }
```

### Character arrays

A string is a `char` array ended by `'\0'`. Many string routines work by scanning until they hit it.

### Exercises

1. Print a table of `n` and `n*n*n` for `n` from 1 to 10.
2. Count the digits, spaces and other characters read from standard input.
3. Write a program that prints each input word on its own line.
4. Make a histogram: count how many typed words have length 1, 2, 3, ... and print bars of `#`.
5. Write `int power(int base, int exp)` and test it in a loop.

---

## Chapter 2: Types, Operators and Expressions

### Names

- Letters, digits and `_`; cannot start with a digit.
- Case-sensitive.
- Cannot be a keyword (`int`, `for`, `return`, ...).
- Convention: lowercase for variables and functions, UPPERCASE for macros.

### Basic types

| Type | Typical size | Notes |
|------|--------------|-------|
| `char` | 1 byte | holds a character or a small integer |
| `short` | 2 bytes | |
| `int` | 4 bytes | natural machine word |
| `long` | 4 or 8 bytes | at least 32 bits |
| `long long` | 8 bytes | C99 |
| `float` | 4 bytes | ~7 decimal digits |
| `double` | 8 bytes | ~15 decimal digits |

Each integer type can be `signed` or `unsigned`. Check your platform's limits in `<limits.h>` and `<float.h>`.

```c
#include <limits.h>
printf("int max: %d\n", INT_MAX);
printf("uchar max: %d\n", UCHAR_MAX);
```

### Constants

```c
42         // int
42L        // long
42U        // unsigned
3.14       // double
3.14f      // float
'A'        // char constant (an int in C)
"text"     // string constant
0x1F       // hexadecimal
017        // octal (starts with 0!)
```

Escape sequences: `\n` newline, `\t` tab, `\\` backslash, `\'`, `\"`, `\0` null.

Enumerations give names to integer constants:

```c
enum Level { LOW, MEDIUM = 5, HIGH };   // 0, 5, 6
```

### Declarations

```c
const double G = 9.81;   // read-only
int a = 1, b = 2;
```

`volatile` tells the compiler a value can change behind its back (hardware registers, signal handlers).

### Arithmetic operators

`+ - * / %`. The `%` operator works only on integers. Its sign for negative operands follows the dividend in C99 and later.

### Relational and logical operators

- Relational: `< <= > >=`; equality: `== !=`.
- `&&` and `||` evaluate left to right and **stop early** (short-circuit):

```c
if (p != NULL && p->value > 0) { ... }   // safe: p->value only runs if p is valid
```

- Results of comparisons are `1` or `0`.

### Type conversions

When types mix, the smaller is promoted to the larger:

```c
int i = 3;
double d = 2.5;
double r = i * d;     // i becomes double
```

Beware of signed/unsigned mixing:

```c
unsigned u = 1;
int s = -2;
if (s < u) { }   // false! s is converted to a huge unsigned number
```

Explicit conversion uses a **cast**: `(double)total / count`.

### Increment and decrement

```c
int i = 5;
int a = i++;   // a = 5, i = 6  (post: use then add)
int b = ++i;   // i = 7, b = 7   (pre: add then use)
```

Never modify the same variable twice in one expression, e.g. `i = i++ + ++i;` is undefined.

### Bitwise operators

| Op | Meaning |
|----|---------|
| `&` | AND |
| `\|` | OR |
| `^` | XOR |
| `~` | complement |
| `<<` `>>` | shift left / right |

Common patterns:

```c
flags |= (1u << 3);      // set bit 3
flags &= ~(1u << 3);     // clear bit 3
flags ^= (1u << 3);      // toggle bit 3
if (flags & (1u << 3)) { }   // test bit 3
```

### Assignment operators and the conditional expression

```c
x += 4;  x <<= 1;  x &= mask;
int smaller = (a < b) ? a : b;
```

### Precedence reminders

When in doubt, add parentheses. Classic trap:

```c
if (x & 1 == 0)     // parsed as x & (1 == 0)  -- probably a bug
if ((x & 1) == 0)   // what you meant
```

### Exercises

1. Print the size and range of every integer type on your machine.
2. Write `unsigned set_bits(unsigned x, int pos, int n)` that sets `n` bits starting at `pos`.
3. Write a function that counts the 1-bits in an unsigned int.
4. Convert a hexadecimal string such as `"1F"` to an integer without using library helpers.
5. Predict the output of `printf("%d\n", -7 / 2);` and `printf("%d\n", -7 % 2);`, then check.

---

## Chapter 3: Control Flow

### Statements and blocks

A statement ends with `;`. Braces group statements into a block, which can declare its own local variables.

### `if` / `else`

```c
if (temp > 30) {
    puts("hot");
} else if (temp > 15) {
    puts("mild");
} else {
    puts("cold");
}
```

A dangling `else` always attaches to the nearest `if`; braces remove doubt.

### `switch`

```c
switch (cmd) {
    case 'a':
    case 'A':
        add();
        break;
    case 'q':
        quit();
        break;
    default:
        puts("unknown");
}
```

- Cases fall through unless you `break`.
- Case labels must be integer constants.
- Stacking labels (like `'a'` and `'A'` above) is a deliberate use of fall-through.

### Loops

```c
while (cond) { ... }
for (init; cond; step) { ... }
do { ... } while (cond);
```

- Any part of `for` may be empty: `for (;;)` loops forever.
- The comma operator allows two updates: `for (i = 0, j = n - 1; i < j; i++, j--)`.

### `break` and `continue`

- `break` leaves the nearest loop or `switch`.
- `continue` skips to the next iteration.

### `goto` and labels

Rarely needed. One accepted use is leaving deeply nested loops or jumping to cleanup code:

```c
FILE *f = fopen(path, "r");
if (!f) goto fail;
char *buf = malloc(1024);
if (!buf) goto close_file;
/* ... work ... */
free(buf);
close_file:
fclose(f);
fail:
return;
```

### Worked example: binary search

```c
int find(const int a[], int n, int target) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] == target) return mid;
        if (a[mid] < target) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}
```

Note `lo + (hi - lo) / 2`: it avoids overflow that `(lo + hi) / 2` can cause with big arrays.

### Exercises

1. Write a program that classifies a character as digit, letter, whitespace or other using `switch`.
2. Implement insertion sort for an int array.
3. Write `void reverse(char s[])` using two indexes moving toward each other.
4. Convert an integer to a string in any base from 2 to 16.
5. Rewrite the `goto` example without `goto`. Which version do you find clearer?

---

## Chapter 4: Functions and Program Structure

### Function basics

```c
double average(const double v[], int n) {
    double sum = 0;
    for (int i = 0; i < n; i++) sum += v[i];
    return n ? sum / n : 0.0;
}
```

- A function with no result is `void`.
- `return expr;` converts `expr` to the declared type.
- Always write `(void)` for an empty parameter list in C; `()` means "unspecified".

### Prototypes

Declare a function before using it so the compiler can check arguments:

```c
double average(const double v[], int n);
```

### External (global) variables

Variables defined outside any function are visible to the functions after them.

```c
int counter = 0;               // definition (allocates storage)
extern int counter;            // declaration (refers to it elsewhere)
```

Use globals sparingly; they make code harder to reason about.

### Scope rules

- **Block scope:** variables inside `{ }`.
- **File scope:** variables and functions outside all functions.
- Inner declarations shadow outer ones with the same name. Avoid doing this on purpose.

### Header files

Put shared declarations in `.h` files, definitions in `.c` files, and guard against double inclusion:

```c
/* stack.h */
#ifndef STACK_H
#define STACK_H

void stack_push(int v);
int  stack_pop(void);
int  stack_empty(void);

#endif
```

```c
/* stack.c */
#include "stack.h"

#define CAP 128
static int data[CAP];
static int top = 0;

void stack_push(int v) { if (top < CAP) data[top++] = v; }
int  stack_pop(void)   { return top > 0 ? data[--top] : 0; }
int  stack_empty(void) { return top == 0; }
```

### `static`

- On a global or function: limits visibility to that file ("private").
- On a local variable: keeps its value between calls.

```c
int next_id(void) {
    static int id = 0;
    return ++id;
}
```

### `register`

A hint to keep a variable in a CPU register. Modern compilers ignore it; you can leave it out.

### Initialization

- Globals and statics start at zero if not initialized.
- Locals contain garbage unless you set them.

### The C preprocessor

```c
#include <stdio.h>        // system header
#include "mine.h"         // your header

#define SQUARE(x) ((x) * (x))
#define DEBUG 1

#if DEBUG
    #define LOG(msg) fprintf(stderr, "log: %s\n", msg)
#else
    #define LOG(msg) ((void)0)
#endif
```

Macro hygiene:

- Wrap parameters and the whole body in parentheses.
- Avoid passing expressions with side effects: `SQUARE(i++)` expands to `((i++) * (i++))`, which is undefined.
- Prefer `static inline` functions when you can.

`#ifdef`, `#ifndef`, `#else`, `#elif`, `#endif`, `#undef` control conditional compilation.

### Recursion

```c
void print_binary(unsigned n) {
    if (n > 1) print_binary(n / 2);
    putchar('0' + (n % 2));
}
```

Every recursive function needs a base case and progress toward it. Deep recursion can exhaust the stack.

### Worked example: a tiny RPN calculator

Reads tokens like `3 4 + 2 *` using the stack module above:

```c
#include <stdio.h>
#include <stdlib.h>
#include "stack.h"

int main(void) {
    char tok[64];
    while (scanf("%63s", tok) == 1) {
        if (tok[0] >= '0' && tok[0] <= '9') {
            stack_push(atoi(tok));
        } else {
            int b = stack_pop(), a = stack_pop();
            switch (tok[0]) {
                case '+': stack_push(a + b); break;
                case '-': stack_push(a - b); break;
                case '*': stack_push(a * b); break;
                case '/': stack_push(b ? a / b : 0); break;
            }
        }
    }
    printf("= %d\n", stack_pop());
    return 0;
}
```

Build: `gcc -Wall -o rpn rpn.c stack.c`.

### Exercises

1. Add `stack_peek` and `stack_size` to the stack module.
2. Write a macro `MIN(a, b)` and show a case where it misbehaves with side effects.
3. Write a recursive function to compute `gcd(a, b)`.
4. Make a file `util.c`/`util.h` with a `static` helper only visible inside `util.c`.
5. Extend the RPN calculator to support negative numbers and report an error for stack underflow.

---

## Chapter 5: Pointers and Arrays

### Pointers and addresses

```c
int n = 10;
int *p = &n;     // p stores the address of n
*p = 20;         // n is now 20
```

- `&` takes an address, `*` follows one (dereference).
- A pointer's type tells the compiler how to interpret the memory it points to.
- `void *` is a generic pointer; cast it before dereferencing.
- `NULL` is the "points to nothing" value.

### Pointers and function arguments

To let a function change the caller's variable, pass its address:

```c
void swap_ints(int *a, int *b) {
    int t = *a; *a = *b; *b = t;
}
```

### Pointers and arrays

`a[i]` is defined as `*(a + i)`. An array name converts to a pointer to its first element in most expressions.

```c
int a[4] = {1, 2, 3, 4};
int *p = a;
printf("%d %d\n", *p, *(p + 3));   // 1 4
```

Differences to remember:

- `sizeof(a)` gives the whole array size; `sizeof(p)` gives the pointer size.
- An array name cannot be reassigned; a pointer can.
- As a function parameter, `int a[]` really means `int *a`.

### Address arithmetic

- Adding `n` to a pointer moves `n` **elements**, not bytes.
- Subtracting two pointers into the same array gives the element distance.
- Comparison of pointers is meaningful only within one array.

```c
size_t my_strlen(const char *s) {
    const char *start = s;
    while (*s) s++;
    return s - start;
}
```

### Character pointers and strings

```c
char buf[] = "mutable";   // array: a modifiable copy
char *lit  = "literal";   // pointer to read-only data; do not modify
```

Copying a string by pointer:

```c
void copy(char *dst, const char *src) {
    while ((*dst++ = *src++) != '\0')
        ;
}
```

### Arrays of pointers; pointers to pointers

```c
const char *days[] = { "Mon", "Tue", "Wed" };
for (int i = 0; i < 3; i++) puts(days[i]);
```

`char **argv` is a pointer to the first element of an array of `char *`.

### Multi-dimensional arrays

```c
int grid[3][4];
```

Stored row by row. When passing to a function, all dimensions except the first are required:

```c
void clear(int g[][4], int rows) {
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < 4; c++) g[r][c] = 0;
}
```

An array of pointers (`int *rows[3]`) is different: each row can have its own length.

### Command-line arguments

```c
int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++)
        printf("%s\n", argv[i]);
    return 0;
}
```

`argv[0]` is the program name; `argv[argc]` is `NULL`.

### Pointers to functions

```c
int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

int nums[] = {5, 2, 9, 1};
qsort(nums, 4, sizeof nums[0], cmp_int);
```

Declaring one: `int (*op)(int, int);`, then `op = add; op(2, 3);`.

### Reading complicated declarations

Read from the name outward, remembering that `[]` and `()` bind tighter than `*`:

| Declaration | Meaning |
|-------------|---------|
| `int *p[5]` | array of 5 pointers to int |
| `int (*p)[5]` | pointer to an array of 5 ints |
| `int *f(void)` | function returning pointer to int |
| `int (*f)(void)` | pointer to function returning int |

### Worked example: sort lines alphabetically

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int by_text(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

int main(void) {
    char *lines[100];
    char buf[256];
    int n = 0;

    while (n < 100 && fgets(buf, sizeof buf, stdin)) {
        lines[n] = malloc(strlen(buf) + 1);
        if (!lines[n]) return 1;
        strcpy(lines[n++], buf);
    }
    qsort(lines, n, sizeof lines[0], by_text);
    for (int i = 0; i < n; i++) {
        fputs(lines[i], stdout);
        free(lines[i]);
    }
    return 0;
}
```

### Common pitfalls

- Dereferencing uninitialized or `NULL` pointers.
- Reading past the end of an array.
- Returning the address of a local variable.
- Modifying a string literal.
- Forgetting that `sizeof` on a parameter array gives pointer size.

### Exercises

1. Write your own `strcmp`, `strcat` and `strchr` using pointers only.
2. Write `char *str_dup(const char *s)` using `malloc`.
3. Reverse a string in place using two pointers.
4. Write a function that takes a function pointer and applies it to every element of an int array.
5. Print `argv` in reverse order.
6. Implement a 2D matrix transpose for a fixed 3x3 array.

---

## Chapter 6: Structures

### Basics

```c
struct Point {
    double x;
    double y;
};

struct Point a = {1.5, 2.0};
a.x += 1;
```

- Members are accessed with `.`.
- Structs can be assigned and passed by value (copied member by member).
- You cannot compare two structs with `==`.

### Structures and functions

```c
struct Point midpoint(struct Point a, struct Point b) {
    struct Point m = { (a.x + b.x) / 2, (a.y + b.y) / 2 };
    return m;
}
```

For large structs pass a pointer instead:

```c
void scale(struct Point *p, double k) {
    p->x *= k;       // same as (*p).x *= k
    p->y *= k;
}
```

### Arrays of structures

```c
struct Item { const char *name; int qty; };

struct Item stock[] = {
    {"bolt", 100},
    {"nut", 250},
    {"gear", 12},
};
int count = sizeof stock / sizeof stock[0];
```

### Pointers to structures

Pointer arithmetic steps by whole structs. The size of a struct may be larger than the sum of its members because of **padding** for alignment. Always use `sizeof`.

### Self-referential structures

A struct can contain a pointer to its own type. This builds lists and trees:

```c
struct Node {
    int value;
    struct Node *next;
};

struct Node *push_front(struct Node *head, int v) {
    struct Node *n = malloc(sizeof *n);
    if (!n) return head;
    n->value = v;
    n->next = head;
    return n;
}

void free_list(struct Node *head) {
    while (head) {
        struct Node *next = head->next;
        free(head);
        head = next;
    }
}
```

### Binary search tree

```c
struct Tree {
    int key;
    struct Tree *left, *right;
};

struct Tree *insert(struct Tree *t, int key) {
    if (!t) {
        t = calloc(1, sizeof *t);
        if (t) t->key = key;
        return t;
    }
    if (key < t->key)       t->left  = insert(t->left, key);
    else if (key > t->key)  t->right = insert(t->right, key);
    return t;
}

void print_in_order(const struct Tree *t) {
    if (!t) return;
    print_in_order(t->left);
    printf("%d ", t->key);
    print_in_order(t->right);
}
```

### Table lookup with a hash table

A hash function maps a key to a bucket index. Collisions are handled by chaining nodes in a list:

```c
#define BUCKETS 64

struct Entry {
    char *key;
    int value;
    struct Entry *next;
};

static struct Entry *table[BUCKETS];

static unsigned hash(const char *s) {
    unsigned h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h % BUCKETS;
}
```

### `typedef`

```c
typedef struct Point Point;
typedef int (*Comparator)(const void *, const void *);
```

### Unions

A union stores one of several members in the same memory.

```c
union Number {
    int i;
    float f;
};
```

Only read the member you most recently wrote. Pair a union with a tag to track which is active:

```c
struct Value {
    enum { IS_INT, IS_FLOAT } kind;
    union { int i; float f; } as;
};
```

### Bit-fields

```c
struct Flags {
    unsigned visible : 1;
    unsigned locked  : 1;
    unsigned level   : 4;
};
```

Layout is implementation-defined, so avoid bit-fields for data that must be portable between machines.

### Exercises

1. Define a `Rect` struct built from two `Point`s. Write `area` and `contains(Rect, Point)`.
2. Write a linked-list `reverse` function.
3. Add `count_nodes` and `find` to the list code.
4. Implement tree `height` and a function that frees the whole tree.
5. Build a word-frequency counter using the hash table sketch above.
6. Print `sizeof` for a struct with members `char`, `int`, `char` in different orders. What do you notice?

---

## Chapter 7: Input and Output

### Standard streams

- `stdin` standard input
- `stdout` standard output
- `stderr` standard error (unbuffered; use it for error messages)

Redirect in the shell: `./prog < in.txt > out.txt 2> err.txt`, or chain programs with a pipe: `./a | ./b`.

### Formatted output: `printf`

```c
printf("%5d|%-5d|%05d\n", 42, 42, 42);
printf("%8.3f\n", 3.14159);
printf("%s has %zu chars\n", s, strlen(s));
printf("%x %o %e %c %%\n", 255, 8, 12345.678, 'z');
```

Flag/width/precision pattern: `%[flags][width][.precision][length]type`.

| Type | Meaning |
|------|---------|
| `d` `i` | signed int |
| `u` | unsigned int |
| `x` `X` | hex |
| `o` | octal |
| `f` `e` `g` | floating point |
| `c` | character |
| `s` | string |
| `p` | pointer |
| `zu` | `size_t` |

Variants: `sprintf`/`snprintf` write into a buffer (prefer `snprintf`), `fprintf` writes to a stream.

### Variable-length argument lists

Use `<stdarg.h>` to write your own `printf`-like functions:

```c
#include <stdarg.h>

int sum_ints(int count, ...) {
    va_list ap;
    va_start(ap, count);
    int total = 0;
    for (int i = 0; i < count; i++) total += va_arg(ap, int);
    va_end(ap);
    return total;
}
```

### Formatted input: `scanf`

```c
int day; char month[16]; int year;
if (scanf("%d %15s %d", &day, month, &year) == 3) {
    /* success */
}
```

- Returns the number of items successfully read; **always check it**.
- Whitespace in the format skips any whitespace in the input.
- Limit `%s` with a width to avoid overflowing the buffer.
- `sscanf` reads from a string and is often the safest way to parse a line obtained with `fgets`.

### File access

```c
FILE *f = fopen("data.txt", "r");
if (!f) { perror("data.txt"); return 1; }

int value;
while (fscanf(f, "%d", &value) == 1) {
    printf("%d\n", value);
}
fclose(f);
```

Modes: `r`, `w`, `a`, plus `+` for update, `b` for binary.

### Error handling

- `perror("context")` prints your text plus the system's description of `errno`.
- `ferror(f)` and `feof(f)` tell you why reading stopped.
- `exit(status)` ends the program; `EXIT_SUCCESS` and `EXIT_FAILURE` are the portable codes.

### Line-oriented I/O

```c
char line[512];
while (fgets(line, sizeof line, stdin)) {
    line[strcspn(line, "\n")] = '\0';   // strip newline
    printf("[%s]\n", line);
}
```

Avoid `gets`; it was removed from the language because it cannot be used safely.

### Character helpers (`<ctype.h>`)

`isalpha`, `isdigit`, `isalnum`, `isspace`, `isupper`, `islower`, `toupper`, `tolower`. Cast the argument to `unsigned char` when it comes from a `char` that could be negative.

### Binary I/O

```c
struct Record { int id; double score; };
struct Record r = {7, 91.5};

FILE *out = fopen("rec.bin", "wb");
fwrite(&r, sizeof r, 1, out);
fclose(out);

FILE *in = fopen("rec.bin", "rb");
fread(&r, sizeof r, 1, in);
fclose(in);
```

Binary files written this way are not portable across different architectures (padding, endianness).

### Random access

`fseek(f, offset, SEEK_SET | SEEK_CUR | SEEK_END)`, `ftell(f)`, `rewind(f)`.

### Worked example: file statistics

```c
#include <stdio.h>
#include <ctype.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s file\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 1; }

    long chars = 0, words = 0, lines = 0;
    int c, in_word = 0;
    while ((c = fgetc(f)) != EOF) {
        chars++;
        if (c == '\n') lines++;
        if (isspace(c)) in_word = 0;
        else if (!in_word) { in_word = 1; words++; }
    }
    fclose(f);
    printf("%ld lines, %ld words, %ld chars\n", lines, words, chars);
    return 0;
}
```

### Exercises

1. Write a program that prints the longest line in a file.
2. Print two files side by side.
3. Write `my_printf` that supports only `%d`, `%s` and `%c`, using `<stdarg.h>`.
4. Read lines of the form `name score`, then print them sorted by score.
5. Write a program that converts all text in a file to uppercase and saves the result to a new file.
6. Save an array of structs to a binary file and load it back; compare the file size with `sizeof`.

---

## Chapter 8: The UNIX System Interface

This chapter assumes Linux or macOS. These are system calls: direct requests to the kernel, below the standard library.

### File descriptors

Each open file is a small integer. By default:

| fd | Stream |
|----|--------|
| 0 | standard input |
| 1 | standard output |
| 2 | standard error |

### Low-level I/O

```c
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    int fd = open("data.txt", O_RDONLY);
    if (fd < 0) return 1;

    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof buf)) > 0) {
        write(STDOUT_FILENO, buf, n);
    }
    close(fd);
    return 0;
}
```

- `read` and `write` return the number of bytes transferred, `0` on end of file, `-1` on error.
- Use larger buffers for speed: each call is a trip into the kernel.
- `open` flags: `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_TRUNC`, `O_APPEND`. With `O_CREAT`, a third argument gives the permissions (for example `0644`).
- `lseek(fd, offset, whence)` moves the position.

### Creating and removing files

`creat`, `unlink` (delete), `rename` (from `<stdio.h>`), `mkdir`, `rmdir` (from `<sys/stat.h>`/`<unistd.h>`).

### Error handling

System calls report failure with `-1` and set `errno`. Print the cause with `perror` or `strerror(errno)`.

### Inspecting files: `stat`

```c
#include <sys/stat.h>

struct stat st;
if (stat("data.txt", &st) == 0) {
    printf("size: %lld bytes\n", (long long)st.st_size);
    if (S_ISDIR(st.st_mode)) puts("directory");
}
```

### Listing a directory

```c
#include <dirent.h>

DIR *d = opendir(".");
struct dirent *e;
while (d && (e = readdir(d)) != NULL) {
    puts(e->d_name);
}
if (d) closedir(d);
```

### Processes

```c
#include <sys/wait.h>
#include <unistd.h>

pid_t pid = fork();           // duplicates the process
if (pid == 0) {
    execlp("ls", "ls", "-l", (char *)NULL);   // child becomes `ls`
    _exit(127);                                // only reached if exec failed
} else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);                 // parent waits
}
```

### Pipes

```c
int fds[2];
pipe(fds);                    // fds[0] read end, fds[1] write end
if (fork() == 0) {
    close(fds[0]);
    write(fds[1], "ping", 4);
    _exit(0);
}
close(fds[1]);
char buf[8] = {0};
read(fds[0], buf, 7);
wait(NULL);
printf("child said: %s\n", buf);
```

### Memory allocation: how `malloc` can work

Under the hood an allocator asks the OS for large blocks (via `sbrk` or `mmap`) and carves them into pieces, tracking sizes in headers and keeping a **free list** of returned blocks. Coalescing neighbours and alignment are the main design problems. You do not need to write one for daily work, but doing so once is an excellent exercise.

### Exercises

1. Write a `cat` clone using `open`, `read`, `write`.
2. Write a `cp` clone and compare its speed using 1-byte, 512-byte and 64 KB buffers.
3. Write a tiny `ls -l`-style tool using `opendir` and `stat`.
4. Write a mini-shell: read a command, `fork`, `exec`, `wait`, repeat.
5. Use `pipe` and `fork` to send a message from parent to child and back.
6. (Challenge) Implement a simple first-fit allocator on top of a static 1 MB array.

---

## Appendix: Standard Library Quick Reference

| Header | Highlights |
|--------|-----------|
| `<stdio.h>` | `printf`, `scanf`, `fopen`, `fgets`, `fread`, `fwrite`, `perror` |
| `<stdlib.h>` | `malloc`, `calloc`, `realloc`, `free`, `exit`, `atoi`, `strtol`, `qsort`, `bsearch`, `rand` |
| `<string.h>` | `strlen`, `strcpy`, `strncpy`, `strcat`, `strcmp`, `strchr`, `strstr`, `memcpy`, `memmove`, `memset` |
| `<ctype.h>` | `isalpha`, `isdigit`, `isspace`, `toupper`, `tolower` |
| `<math.h>` | `sqrt`, `pow`, `sin`, `cos`, `fabs`, `floor`, `ceil` (link with `-lm`) |
| `<limits.h>` | `INT_MAX`, `CHAR_BIT`, `LONG_MAX`, ... |
| `<stddef.h>` | `size_t`, `NULL`, `offsetof` |
| `<stdbool.h>` | `bool`, `true`, `false` |
| `<stdarg.h>` | `va_list`, `va_start`, `va_arg`, `va_end` |
| `<time.h>` | `time`, `clock`, `strftime`, `localtime` |
| `<assert.h>` | `assert` |
| `<errno.h>` | `errno` |

### Study habits that help

- Compile with warnings on; treat every warning as a bug.
- Use `-fsanitize=address,undefined` while learning.
- Draw memory diagrams when you work with pointers.
- Re-implement library functions yourself, then compare with the real ones.
- Read other people's small C programs, such as classic Unix tools.

### Suggested pace

| Week | Chapters |
|------|----------|
| 1 | 1 and 2 |
| 2 | 3 and 4 |
| 3 | 5 |
| 4 | 6 |
| 5 | 7 |
| 6 | 8 and review projects |

