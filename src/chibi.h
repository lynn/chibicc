#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int total_alloc;
#define calloc(n, a) (total_alloc += (n) * (a), calloc(n, a))
#define malloc(a) (total_alloc += (a), malloc(a))

typedef struct Type Type;
typedef struct Member Member;
typedef struct Initializer Initializer;

//
// tokenize.c
//

// Token
typedef enum {
  TK_RESERVED, // Keywords or punctuators
  TK_IDENT,    // Identifiers
  TK_STR,      // String literals
  TK_NUM,      // Integer literals
  TK_EOF,      // End-of-file markers
} TokenKind;

// Token type
typedef struct Token Token;
struct Token {
  TokenKind kind; // Token kind
  Token *next;    // Next token
  long val;       // If kind is TK_NUM, its value
  Type *ty;       // Used if TK_NUM
  char *str;      // Token string
  int len;        // Token length

  char *contents; // String literal contents including terminating '\0'
  char cont_len;  // string literal length
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
void warn_tok(Token *tok, char *fmt, ...);
Token *peek(char *s);
Token *consume(char *op);
Token *consume_ident(void);
void expect(char *op);
char *expect_ident(void);
bool at_eof(void);
Token *tokenize(void);

extern char *filename;
extern char *user_input;
extern Token *token;

//
// parse.c
//

// Variable
typedef struct Var Var;
struct Var {
  char *name;    // Variable name
  Type *ty;      // Type
  bool is_local; // local or global

  // Local variable
  int offset; // Offset from RBP

  // Global variable
  bool is_static;
  Initializer *initializer;
  bool is_string_literal;
};

typedef struct VarList VarList;
struct VarList {
  VarList *next;
  Var *var;
};

// AST node
typedef enum {
  ND_ADD,        // num + num
  ND_PTR_ADD,    // ptr + num or num + ptr
  ND_SUB,        // num - num
  ND_PTR_SUB,    // ptr - num
  ND_PTR_DIFF,   // ptr - ptr
  ND_MUL,        // *
  ND_DIV,        // /
  ND_MOD,        // %
  ND_BITAND,     // &
  ND_BITOR,      // |
  ND_BITXOR,     // ^
  ND_SHL,        // <<
  ND_SHR,        // >>
  ND_EQ,         // ==
  ND_NE,         // !=
  ND_LT,         // <
  ND_LE,         // <=
  ND_ASSIGN,     // =
  ND_TERNARY,    // ?:
  ND_PRE_INC,    // pre ++
  ND_PRE_DEC,    // pre --
  ND_POST_INC,   // post ++
  ND_POST_DEC,   // post --
  ND_ADD_EQ,     // +=
  ND_PTR_ADD_EQ, // +=
  ND_SUB_EQ,     // -=
  ND_PTR_SUB_EQ, // -=
  ND_MUL_EQ,     // *=
  ND_DIV_EQ,     // /=
  ND_MOD_EQ,     // %=
  ND_SHL_EQ,     // <<=
  ND_SHR_EQ,     // >>=
  ND_BITAND_EQ,  // &=
  ND_BITOR_EQ,   // |=
  ND_BITXOR_EQ,  // ^=
  ND_COMMA,      // ,
  ND_MEMBER,     // . (struct member access)
  ND_ADDR,       // unary &
  ND_DEREF,      // unary *
  ND_NOT,        // !
  ND_BITNOT,     // ~
  ND_LOGAND,     // &&
  ND_LOGOR,      // ||
  ND_RETURN,     // "return"
  ND_IF,         // "if"
  ND_WHILE,      // "while"
  ND_FOR,        // "for"
  ND_DO,         // "do"
  ND_SWITCH,     // "switch"
  ND_CASE,       // "case"
  ND_BLOCK,      // { ... }
  ND_BREAK,      // "break"
  ND_CONTINUE,   // "continue"
  ND_GOTO,       // "goto"
  ND_LABEL,      // Labeled statement
  ND_FUNCALL,    // Function call
  ND_EXPR_STMT,  // Expression statement
  ND_STMT_EXPR,  // Statement expression
  ND_VAR,        // Variable
  ND_NUM,        // Integer
  ND_CAST,       // Type cast
  ND_NULL,       // Empty statement
} NodeKind;

// AST node type
typedef struct Node Node;
struct Node {
  NodeKind kind; // Node kind
  Node *next;    // Next node
  Token *tok;    // Representative token
  Type *ty;      // Type, e.g. int or pointer to int
#ifndef SHRINK_NODE
#define union struct
#endif
  union {
    Node *inc;
    Node *case_next;
  };
  union {
    Node *lhs;
    Node *cond;
  };
  union {
    Node *rhs;
    Node *els;
    Node *init;
    Member *member; // Struct member access
    int case_label;
  };
  union {
    Node *args; // Function call
    Node *default_case;
    int case_end_label;
  };
  union {
    Node *then;
    Node *body; // Block or statement expression
  };
  union {
    char *funcname;   // Function call
    char *label_name; // Goto or labeled statement
    Var *var;
    long val; // Integer literal
  };
#ifndef SHRINK_NODE
#undef union
#endif
};

// Global variable initializer. Global variables can be initialized
// either by a constant expression or a pointer to another global
// variable with an addend.
struct Initializer {
  Initializer *next;

  // Constant expression
  int sz;
  long val;

  // Reference to another global variable
  char *label;
  long addend;
};

typedef struct Function Function;
struct Function {
  Function *next;
  char *name;
  VarList *params;
  bool is_static;
  bool has_varargs;

  Node *node;
  VarList *locals;
  int stack_size;
  Type *ty;
};

typedef struct {
  VarList *globals;
  Function *fns;
} Program;

Program *program(void);

//
// typing.c
//

typedef enum {
  TY_VOID,
  TY_BOOL,
  TY_CHAR,
  // TY_SHORT,
  TY_INT,
  // TY_LONG,
  TY_ENUM,
  TY_PTR,
  TY_ARRAY,
  TY_STRUCT,
  TY_FUNC,
} TypeKind;

struct Type {
  TypeKind kind;
  int size;           // sizeof() value
  int align;          // alignment
  bool is_unsigned;   // unsignedness (integers only)
  bool is_incomplete; // incomplete type

  Type *base;      // pointer or array
  int array_len;   // array
  Member *members; // struct
  Type *return_ty; // function
};

// Struct member
struct Member {
  Member *next;
  Type *ty;
  Token *tok; // for error message
  char *name;
  int offset;
};

extern Type *void_type;
extern Type *bool_type;
extern Type *char_type;
extern Type *uchar_type;
// extern Type *short_type;
// extern Type *ushort_type;
extern Type *int_type;
extern Type *uint_type;
// extern Type *long_type;
// extern Type *ulong_type;

bool is_integer(Type *ty);
int align_to(int n, int align);
Type *pointer_to(Type *base);
Type *array_of(Type *base, int size);
Type *func_type(Type *return_ty);
Type *enum_type(void);
Type *struct_type(void);
Type *promote(Type *type);
Type *uac(Type *type1, Type *type2);
void add_type(Node *node);

//
// codegen.c
//

typedef struct {
  char *name;
  unsigned char port;
} Device;

void varvara_argc_argv_hook(void);

void codegen(Program *prog, bool do_opt, int devices_size, Device* devices, Device* console, void (*argc_argv_hook)(void));

//
// optimize.c
//

typedef enum {
  // 0x00 is used to indicate "no opcode" rather than BRK (0x100).
  INC = 0x01,
  POP = 0x02,
  NIP = 0x03,
  SWP = 0x04,
  ROT = 0x05,
  DUP = 0x06,
  OVR = 0x07,
  EQU = 0x08,
  NEQ = 0x09,
  GTH = 0x0a,
  LTH = 0x0b,
  // JMP = 0x0c,
  // JCN = 0x0d,
  // JSR = 0x0e,
  STH = 0x0f,
  // LDZ = 0x10,
  // STZ = 0x11,
  // LDR = 0x12,
  // STR = 0x13,
  LDA = 0x14,
  STA = 0x15,
  DEI = 0x16,
  DEO = 0x17,
  ADD = 0x18,
  SUB = 0x19,
  MUL = 0x1a,
  DIV = 0x1b,
  AND = 0x1c,
  ORA = 0x1d,
  EOR = 0x1e,
  SFT = 0x1f,

  flag_2 = 0x20,
  flag_r = 0x40,
  flag_k = 0x80,

  INC2 = INC | flag_2,
  POP2 = POP | flag_2,
  NIP2 = NIP | flag_2,
  SWP2 = SWP | flag_2,
  ROT2 = ROT | flag_2,
  DUP2 = DUP | flag_2,
  OVR2 = OVR | flag_2,
  EQU2 = EQU | flag_2,
  NEQ2 = NEQ | flag_2,
  GTH2 = GTH | flag_2,
  LTH2 = LTH | flag_2,
  STH2 = STH | flag_2,
  LDA2 = LDA | flag_2,
  STA2 = STA | flag_2,
  DEI2 = DEI | flag_2,
  DEO2 = DEO | flag_2,
  ADD2 = ADD | flag_2,
  SUB2 = SUB | flag_2,
  MUL2 = MUL | flag_2,
  DIV2 = DIV | flag_2,
  AND2 = AND | flag_2,
  ORA2 = ORA | flag_2,
  EOR2 = EOR | flag_2,
  SFT2 = SFT | flag_2,

  ORAk = ORA | flag_k,
  STA2k = STA2 | flag_k,
  STH2kr = STH2 | flag_k | flag_r,
  POP2r = POP2 | flag_r,
  JMP2r = 0x6c,

  JCI = 0x20, // ?label or ?&label
  JMI = 0x40, // !label or !&label
  JSI = 0x60, // label

  LIT = 0x80,
  LIT2 = 0xa0,
  LIT2r = 0xe0,

  BRK = 0x100,
  AT = 0x101,   // @label or &label
  SEMI = 0x102, // ;label or ;&label
  BAR = 0x103,  // |0000

  ASM = 0x1ff, // asm() code
} Opcode;

typedef struct Instruction Instruction;
struct Instruction {
  Opcode opcode;
  unsigned short literal; // appended if opcode is "LIT"
  char *label;            // appended if opcode is "JSI", "JMI", "JCI"
  Instruction *next;
};

extern Instruction *emit_head;
void emit(Opcode opcode, int literal, char *label);
void op(Opcode o);
void jci(char *fmt, ...);
void jmi(char *fmt, ...);
void jsi(char *fmt, ...);
void lit(unsigned char n);
void lit2(unsigned short n);
void lit2r(unsigned short n);
void at(char *fmt, ...);
void semi(char *fmt, ...);
void bar(unsigned short n);

void optimize(Instruction *prog);
void output(Instruction *prog);
void proof_of_concept(void);
