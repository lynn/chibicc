#include "chibi.h"

static int labelseq = 1;
static int brkseq;
static int contseq;
static char *funcname;
static bool need_sext_helper;
static bool need_ashr_helper;
static bool need_sdiv_helper;

static void gen(Node *node, int depth);

static void emit_add(int n) {
  if (n != 0) {
    lit2(n);
    op(ADD2);
  }
}

// Pushes the given node's address to the stack.
static void gen_addr(Node *node, int depth) {
  switch (node->kind) {
  case ND_VAR: {
    if (node->init)
      gen(node->init, depth);

    Var *var = node->var;
    if (var->is_local) {
      op(STH2kr);
      emit_add(var->offset);
    } else {
      semi("%s_", var->name);
    }
    return;
  }
  case ND_DEREF:
    gen(node->lhs, depth);
    return;
  case ND_MEMBER:
    gen_addr(node->lhs, depth);
    emit_add(node->member->offset);
    return;
  case ND_ASSIGN:
    if (node->ty->kind == TY_STRUCT) {
      gen_addr(node->rhs, depth);
      return;
    }
    /* fallthrough */
  default:
    error_tok(node->tok, "not an lvalue");
    return;
  }
}

static void gen_lval(Node *node, int depth) {
  if (node->ty->kind == TY_ARRAY)
    error_tok(node->tok, "not an lvalue");
  gen_addr(node, depth);
}

static void load(Type *ty) {
  if (ty->size == 1) {
    op(LDA);
    if (ty->is_unsigned) {
      lit(0x00);
      op(SWP);
    } else {
      need_sext_helper = 1;
      jsi("sext");
    }
  } else {
    op(LDA2);
  }
}

// Expects "newval addr" on the stack and leaves "newval".
static void store(Type *ty) {
  if (ty->size == 1) {
    op(STA | flag_k);
    op(POP2);
  } else {
    op(STA2 | flag_k);
    op(POP2);
  }
}

static void fix_bool(Type *ty) {
  if (ty->kind == TY_BOOL) {
    lit2(0);
    op(NEQ2);
    lit(0);
    op(SWP);
  }
}

static void truncate(Type *ty) {
  if (ty->kind == TY_BOOL) {
    fix_bool(ty);
  } else if (ty->size == 1 && ty->kind != TY_VOID) {
    op(NIP);
    if (ty->is_unsigned) {
      lit(0x00);
      op(SWP);
    } else {
      need_sext_helper = 1;
      jsi("sext");
    }
  }
}

static void inc(Type *ty) {
  int n = ty->base ? ty->base->size : 1;
  emit_add(n);
}

static void dec(Type *ty) {
  lit2(ty->base ? ty->base->size : 1);
  op(SUB2);
}

static void gen_binary(Node *node) {
  switch (node->kind) {
  case ND_ADD:
  case ND_ADD_EQ:
    op(ADD2);
    break;
  case ND_PTR_ADD:
  case ND_PTR_ADD_EQ:
    lit2(node->ty->base->size);
    op(MUL2);
    op(ADD2);
    break;
  case ND_SUB:
  case ND_SUB_EQ:
    op(SUB2);
    break;
  case ND_PTR_SUB:
  case ND_PTR_SUB_EQ:
    lit2(node->ty->base->size);
    op(MUL2);
    op(SUB2);
    break;
  case ND_PTR_DIFF:
    op(SUB2);
    lit2(node->lhs->ty->base->size);
    need_sdiv_helper = true;
    jsi("sdiv/b_pos"); // size should never be negative
    break;
  case ND_MUL:
  case ND_MUL_EQ:
    op(MUL2);
    break;
  case ND_DIV:
  case ND_DIV_EQ:
    if (uac(node->lhs->ty, node->rhs->ty)->is_unsigned) {
      op(DIV2);
    } else {
      need_sdiv_helper = true;
      jsi("sdiv");
    }
    break;
  case ND_MOD:
  case ND_MOD_EQ:
    op(OVR2);
    op(OVR2);
    if (uac(node->lhs->ty, node->rhs->ty)->is_unsigned) {
      op(DIV2);
    } else {
      need_sdiv_helper = true;
      jsi("sdiv");
    }
    op(MUL2);
    op(SUB2);
    break;
  case ND_BITAND:
  case ND_BITAND_EQ:
    op(AND2);
    break;
  case ND_BITOR:
  case ND_BITOR_EQ:
    op(ORA2);
    break;
  case ND_BITXOR:
  case ND_BITXOR_EQ:
    op(EOR2);
    break;
  case ND_SHL:
  case ND_SHL_EQ:
    op(NIP);
    lit(0x40);
    op(SFT);
    op(SFT2);
    break;
  case ND_SHR:
  case ND_SHR_EQ:
    if (promote(node->lhs->ty)->is_unsigned) {
      op(NIP);
      lit(0x0f);
      op(AND);
      op(SFT2);
    } else {
      need_ashr_helper = 1;
      jsi("ashr");
    }
    break;
  case ND_EQ:
    op(EQU2);
    lit(0);
    op(SWP);
    break;
  case ND_NE:
    op(NEQ2);
    lit(0);
    op(SWP);
    break;
  default:
    error_tok(node->tok, "not a binary operation");
  }
}

int args_backwards(Node *node, int depth) {
  if (node->next) {
    depth = args_backwards(node->next, depth);
  }
  gen(node, depth);
  return depth + 2;
}

// Generate code for a given node.
static void gen(Node *node, int depth) {
  switch (node->kind) {
  case ND_NULL:
    return;
  case ND_NUM:
    lit2(node->val);
    return;
  case ND_EXPR_STMT:
    gen(node->lhs, depth);
    op(POP2);
    return;
  case ND_VAR:
    if (node->init)
      gen(node->init, depth);
    gen_addr(node, depth);
    if (node->ty->kind != TY_ARRAY)
      load(node->ty);
    return;
  case ND_MEMBER:
    gen_addr(node, depth);
    if (node->ty->kind != TY_ARRAY)
      load(node->ty);
    return;
  case ND_ASSIGN:
    if (node->rhs->ty->kind == TY_STRUCT) {
      gen(node->rhs, depth);
      op(POP2);
      int i = node->ty->size;
      if (i % 2 == 1) {
        i--;
        gen_lval(node->rhs, depth);
        emit_add(i);
        op(LDA);
        gen_lval(node->lhs, depth);
        emit_add(i);
        op(STA);
      }
      while (i > 0) {
        i -= 2;
        gen_lval(node->rhs, depth);
        emit_add(i);
        op(LDA2);
        gen_lval(node->lhs, depth);
        emit_add(i);
        op(STA2);
      }
      lit2(0);
    } else {
      gen(node->rhs, depth);
      fix_bool(node->ty);
      gen_lval(node->lhs, depth + 2);
      store(node->ty);
    }
    return;
  case ND_TERNARY: {
    int seq = labelseq++;
    gen(node->cond, depth);
    lit2(0);
    op(NEQ2);
    jci("&then.%d", seq);
    gen(node->els, depth);
    jmi("&end.%d", seq);
    at("&then.%d", seq);
    gen(node->then, depth);
    at("&end.%d", seq);
    return;
  }
  case ND_PRE_INC:
    gen_lval(node->lhs, depth);
    op(DUP2);
    load(node->ty);
    inc(node->ty);
    fix_bool(node->ty);
    op(SWP2);
    store(node->ty);
    return;
  case ND_PRE_DEC:
    gen_lval(node->lhs, depth);
    op(DUP2);
    load(node->ty);
    dec(node->ty);
    fix_bool(node->ty);
    op(SWP2);
    store(node->ty);
    return;
  case ND_POST_INC:
    gen_lval(node->lhs, depth);
    op(DUP2);
    load(node->ty);
    op(DUP2);
    inc(node->ty);
    fix_bool(node->ty);
    op(ROT2);
    store(node->ty);
    op(POP2);
    return;
  case ND_POST_DEC:
    gen_lval(node->lhs, depth);
    op(DUP2);
    load(node->ty);
    op(DUP2);
    dec(node->ty);
    fix_bool(node->ty);
    op(ROT2);
    store(node->ty);
    op(POP2);
    return;
  case ND_ADD_EQ:
  case ND_PTR_ADD_EQ:
  case ND_SUB_EQ:
  case ND_PTR_SUB_EQ:
  case ND_MUL_EQ:
  case ND_DIV_EQ:
  case ND_SHL_EQ:
  case ND_SHR_EQ:
  case ND_BITAND_EQ:
  case ND_BITOR_EQ:
  case ND_BITXOR_EQ:
    gen_lval(node->lhs, depth);
    op(DUP2);
    load(node->lhs->ty);
    gen(node->rhs, depth + 4);
    gen_binary(node);
    fix_bool(node->ty);
    op(SWP2);
    store(node->ty);
    return;
  case ND_COMMA:
    gen(node->lhs, depth);
    gen(node->rhs, depth);
    return;
  case ND_ADDR:
    gen_addr(node->lhs, depth);
    return;
  case ND_DEREF:
    gen(node->lhs, depth);
    if (node->ty->kind != TY_ARRAY)
      load(node->ty);
    return;
  case ND_NOT:
    gen(node->lhs, depth);
    lit2(0);
    op(EQU2);
    lit(0);
    op(SWP);
    return;
  case ND_BITNOT:
    gen(node->lhs, depth);
    lit2(0xffff);
    op(EOR2);
    return;
  case ND_LOGAND: {
    int seq = labelseq++;
    gen(node->lhs, depth);
    lit2(0);
    op(EQU2);
    jci("&false.%d", seq);
    gen(node->rhs, depth);
    lit2(0);
    op(EQU2);
    jci("&false.%d", seq);
    lit2(1);
    jmi("&end.%d", seq);
    at("&false.%d", seq);
    lit2(0);
    at("&end.%d", seq);
    return;
  }
  case ND_LOGOR: {
    int seq = labelseq++;
    gen(node->lhs, depth);
    op(ORA);
    jci("&true.%d", seq);
    gen(node->rhs, depth);
    op(ORA);
    jci("&true.%d", seq);
    lit2(0);
    jmi("&end.%d", seq);
    at("&true.%d", seq);
    lit2(1);
    at("&end.%d", seq);
    return;
  }
  case ND_IF: {
    int seq = labelseq++;
    if (node->els) {
      gen(node->cond, depth);
      lit2(0);
      op(NEQ2);
      jci("&then.%d", seq);
      gen(node->els, depth);
      jmi("&end.%d", seq);
      at("&then.%d", seq);
      gen(node->then, depth);
      at("&end.%d", seq);
    } else {
      gen(node->cond, depth);
      lit2(0);
      op(EQU2);
      jci("&end.%d", seq);
      gen(node->then, depth);
      at("&end.%d", seq);
    }
    return;
  }
  case ND_WHILE: {
    int seq = labelseq++;
    int brk = brkseq;
    int cont = contseq;
    brkseq = contseq = seq;

    at("&continue.%d", seq);

    gen(node->cond, depth);
    lit2(0);
    op(EQU2);
    jci("&break.%d", seq);

    gen(node->then, depth);
    jmi("&continue.%d", seq);
    at("&break.%d", seq);

    brkseq = brk;
    contseq = cont;
    return;
  }
  case ND_FOR: {
    int seq = labelseq++;
    int brk = brkseq;
    int cont = contseq;
    brkseq = contseq = seq;

    if (node->init)
      gen(node->init, depth);
    at("&begin.%d", seq);
    if (node->cond) {
      gen(node->cond, depth);
      lit2(0);
      op(EQU2);
      jci("&break.%d", seq);
    }
    gen(node->then, depth);
    at("&continue.%d", seq);
    if (node->inc)
      gen(node->inc, depth);
    jmi("&begin.%d", seq);
    at("&break.%d", seq);

    brkseq = brk;
    contseq = cont;
    return;
  }
  case ND_DO: {
    int seq = labelseq++;
    int brk = brkseq;
    int cont = contseq;
    brkseq = contseq = seq;

    at("&begin.%d", seq);
    gen(node->then, depth);
    at("&continue.%d", seq);
    gen(node->cond, depth);
    lit2(0);
    op(NEQ2);
    jci("&begin.%d", seq);
    at("&break.%d", seq);

    brkseq = brk;
    contseq = cont;
    return;
  }
  case ND_SWITCH: {
    int seq = labelseq++;
    int brk = brkseq;
    brkseq = seq;
    node->case_label = seq;

    gen(node->cond, depth);

    for (Node *n = node->case_next; n; n = n->case_next) {
      n->case_label = labelseq++;
      n->case_end_label = seq;
      op(DUP2);
      lit2(n->val);
      op(EQU2);
      jci("&case.%d", n->case_label);
    }

    if (node->default_case) {
      int i = labelseq++;
      node->default_case->case_end_label = seq;
      node->default_case->case_label = i;
      jmi("&case.%d", i);
    }

    jmi("&break.%d", seq);
    gen(node->then, depth + 2);
    at("&break.%d", seq);
    op(POP2);

    brkseq = brk;
    return;
  }
  case ND_CASE:
    at("&case.%d", node->case_label);
    gen(node->lhs, depth);
    return;
  case ND_BLOCK:
  case ND_STMT_EXPR:
    for (Node *n = node->body; n; n = n->next)
      gen(n, depth);
    return;
  case ND_BREAK:
    if (brkseq == 0)
      error_tok(node->tok, "stray break");
    jmi("&break.%d", brkseq);
    return;
  case ND_CONTINUE:
    if (contseq == 0)
      error_tok(node->tok, "stray continue");
    jmi("&continue.%d", contseq);
    return;
  case ND_GOTO:
    jmi("&label.%s.%s", funcname, node->label_name);
    return;
  case ND_LABEL:
    at("&label.%s.%s", funcname, node->label_name);
    gen(node->lhs, depth);
    return;
  case ND_FUNCALL: {
    if (!strcmp(node->funcname, "asm")) {
      Node *arg = node->args;
      while (arg->next) {
        gen(arg, depth);
        arg = arg->next;
      }
      // Last arg is asm code:
      if (arg->tok->kind != TK_STR)
        error_tok(arg->tok, "not a string");
      emit(ASM, 0, arg->tok->contents);
      return;
    }
    if (!strcmp(node->funcname, "deo")) {
      gen(node->args, depth);
      op(NIP);
      gen(node->args->next, depth + 1);
      op(NIP);
      op(DEO);
      lit2(0); // Will be followed by POP2
      return;
    }
    if (!strcmp(node->funcname, "deo2")) {
      gen(node->args, depth);
      gen(node->args->next, depth + 2);
      op(NIP);
      op(DEO2);
      lit2(0); // Will be followed by POP2
      return;
    }
    if (!strcmp(node->funcname, "dei")) {
      gen(node->args, depth);
      op(NIP);
      op(DEI);
      lit(0);
      op(SWP);
      return;
    }
    if (!strcmp(node->funcname, "dei2")) {
      gen(node->args, depth);
      op(NIP);
      op(DEI2);
      return;
    }
    if (!strcmp(node->funcname, "exit")) {
      gen(node->args, depth);
      op(NIP);
      lit(0x80);
      op(ORA);
      lit(0x0f);
      op(DEO);
      op(BRK);
      return;
    }
    if (!strcmp(node->funcname, "__builtin_va_arg")) {
      // fprintf(stderr, "depth=%d\n", depth);
      if (depth % 2) {
        error_tok(node->tok, "bad va_arg depth");
        return;
      } else if (depth == 0) {
        // nothing
      } else if (depth == 2) {
        op(SWP2);
      } else {
        int i;
        for (i = 4; i < depth; i += 2)
          op(STH2);
        op(ROT2);
        for (; i > 4; i -= 2)
          op(STH2 | flag_r), op(SWP2);
      }
      return;
    }

    // Function arguments are passed via the uxn working stack.
    if (node->args)
      args_backwards(node->args, depth);

    jsi("%s_", node->funcname);
    return;
  }
  case ND_RETURN:
    if (node->lhs) {
      gen(node->lhs, depth);
    } else {
      lit2(0); // dummy return value
    }
    jmi("&return");
    return;
  case ND_CAST:
    gen(node->lhs, depth);
    truncate(node->ty);
    return;
  case ND_LT:
    if (uac(node->lhs->ty, node->rhs->ty)->is_unsigned) {
      gen(node->lhs, depth);
      gen(node->rhs, depth + 2);
    } else {
      gen(node->lhs, depth);
      lit2(0x8000);
      op(EOR2);
      gen(node->rhs, depth + 2);
      lit2(0x8000);
      op(EOR2);
    }
    op(LTH2);
    lit(0);
    op(SWP);
    return;
  case ND_LE:
    if (uac(node->lhs->ty, node->rhs->ty)->is_unsigned) {
      gen(node->lhs, depth);
      gen(node->rhs, depth + 2);
    } else {
      gen(node->lhs, depth);
      lit2(0x8000);
      op(EOR2);
      gen(node->rhs, depth + 2);
      lit2(0x8000);
      op(EOR2);
    }
    op(GTH2);
    lit(0);
    op(SWP);
    lit(1);
    op(EOR);
    return;

  // These are commutative, so we'll move literals to the right for the
  // optimizer.
  case ND_ADD:
  case ND_MUL:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR:
  case ND_EQ:
  case ND_NE:
    if (node->lhs->kind == ND_NUM) {
      gen(node->rhs, depth);
      gen(node->lhs, depth + 2);
      gen_binary(node);
      return;
    }
    // Else, fall through

  default: // Binary operations
    gen(node->lhs, depth);
    gen(node->rhs, depth + 2);
    gen_binary(node);
  }
}

static void emit_string_literal(Initializer *init) {
  putchar(' ');
  int n = 0;
  for (; init; init = init->next) {
    char c = init->val;
    if (c >= '!' && c <= '~') {
      if (n++ == 0)
        printf(" \"");
      putchar(c);
      // Avoid running into uxnasm's token length limit.
      if (n >= 60)
        n = 0;
    } else {
      printf(" %02x", c);
      n = 0;
    }
  }
  putchar('\n');
}

static void emit_data(Program *prog) {
  printf("( bss )\n");

  for (VarList *vl = prog->globals; vl; vl = vl->next) {
    Var *var = vl->var;
    if (var->initializer)
      continue;

    // Name is suffixed with _ so that uxnasm won't complain if it happens to be
    // hexadecimal.
    printf("@%s_ $%x\n", var->name, var->ty->size);
  }

  printf("( data )\n");

  for (VarList *vl = prog->globals; vl; vl = vl->next) {
    Var *var = vl->var;
    if (!var->initializer)
      continue;

    // printf(".align %d\n", var->ty->align);
    printf("@%s_\n", var->name);

    if (var->is_string_literal) {
      emit_string_literal(var->initializer);
      continue;
    }

    int column = 0;
    for (Initializer *init = var->initializer; init; init = init->next) {
      if (column == 0)
        putchar(' ');
      if (init->label) {
        if (init->addend) {
          // uxntal sadly has no way to express an offset to an addressing rune
          error("unsupported initializer for var \"%s\": can't handle non-zero "
                "addend (%+ld) with label (\"%s\")",
                var->name, init->addend, init->label);
        }
        printf(" =%s_", init->label);
        column += 2;
      } else if (init->sz == 1) {
        printf(" %02x", (unsigned char)init->val);
        column += 1;
      } else if (init->sz == 2) {
        printf(" %04x", (unsigned short)init->val);
        column += 2;
      } else {
        error("unsupported initializer size");
      }
      if (column >= 16) {
        putchar('\n');
        column = 0;
      }
    }
    putchar('\n');
  }
}

static void load_arg(Var *var) {
  op(STH2kr);
  emit_add(var->offset);
  if (var->ty->size == 1) {
    op(STA);
    op(POP);
  } else {
    op(STA2);
  }
}

char signature[256];
char *sigptr;
static void walk_signature(VarList *param) {
  char *end = signature + sizeof(signature);
  if (!param) {
    *sigptr = '\0';
    return;
  }
  walk_signature(param->next);
  if (sigptr < end) {
    sigptr += snprintf(sigptr, end - sigptr, " %s*", param->var->name);
  }
}

static void emit_text(Program *prog) {
  printf("( text )\n");

  for (Function *fn = prog->fns; fn; fn = fn->next) {
    // Name is suffixed with _ so that uxnasm won't complain if it happens to be
    // hexadecimal.
    sigptr = signature;
    walk_signature(fn->params);
    at("%s_ (%s -- result* )", fn->name, signature);
    funcname = fn->name;
    labelseq = 1;
    brkseq = 0;
    contseq = 0;

    // Prologue
    // Copy the frame pointer from the "frame" below.
    op(OVR2 | flag_r);
    if (fn->stack_size != 0) {
      lit2r(fn->stack_size);
      op(SUB2 | flag_r);
    }

    // Save arg registers if function is variadic
    if (fn->has_varargs) {
      // error("unsupported varargs");
    }

    // Push arguments to the in-memory stack from the uxn working stack
    for (VarList *vl = fn->params; vl; vl = vl->next)
      load_arg(vl->var);

    // Emit code
    for (Node *node = fn->node; node; node = node->next)
      gen(node, 0);

    // Epilogue
    lit2(0); // dummy return value
    at("&return");

    // Pop the frame pointer, then return.
    truncate(fn->ty);
    op(POP2r);
    op(JMP2r);
  }

  if (need_sext_helper) {
    // 8-bit to 16-bit sign extension
    printf("@sext\n");
    printf("  #80 ANDk EQU #ff MUL SWP JMP2r\n");
  }
  if (need_ashr_helper) {
    // 16-bit arithmetic right shift (uxn's SFT does logical right shift)
    // If the sign bit was unset, this is ((uint16_t)a >> b)
    // If the sign bit was set, this is ~((uint16_t)(~a) >> b)
    // The double NOT has the effect of making the zeroes shifted in become ones
    // to match the sign bit
    printf("@ashr\n");
    // Swap value to be shifted with shift amount
    printf("  SWP2\n");
    // Check sign bit and convert it into an XOR mask
    printf("  #8000 AND2k EQU2 #ff MUL DUP\n");
    // XOR before shifting
    printf("  DUP2 ROT2 EOR2 ROT2\n");
    // XOR, shift, XOR again
    // Convert shift amount to SFT format and do shift
    printf("  NIP #0f AND SFT2\n");
    // XOR again
    printf("  EOR2\n");
    // Return
    printf("  JMP2r\n");
  }
  if (need_sdiv_helper) {
    // Signed division (uxn's DIV is unsigned)
    // Normally we jump to sdiv, but as an optimization, we can jump to
    // sdiv/b_pos or sdiv/b_neg directly if we know the divisor's sign!
    printf("@sdiv\n");
    printf("    OVR #80 AND ?&b_neg");
    printf("  &b_pos");
    printf("    OVR2 POP #80 AND ?&a_neg_b_pos");
    // (a / b)
    printf("    DIV2 JMP2r");
    printf("  &a_neg_b_pos");
    // -(-a / b)
    printf("    SWP2 #0000 SWP2 SUB2 SWP2 DIV2 #0000 SWP2 SUB2 JMP2r\n");
    printf("  &b_neg");
    printf("    #0000 SWP2 SUB2 OVR2 POP #80 AND ?&a_neg_b_neg");
    // -(a / -b)
    printf("    DIV2 #0000 SWP2 SUB2 JMP2r\n");
    printf("  &a_neg_b_neg");
    // (-a / -b)
    printf("    #0000 ROT2 SUB2 SWP2 DIV2 JMP2r\n");
  }
}

void varvara_argc_argv_hook(void) {
  // This is routines/argc_argv.tal but without comments or indentation.
  // To update this function after editing that file, use update_argc_argv.sh
  printf("  LIT2r fffe #0001 STH2kr STA2 LIT2r 0001 SUB2r #17 DEI ?&set_arg_hook !prep_argc_argv_and_call_main\n");
  printf("  &set_arg_hook ;L.console.hook #10 DEO2 BRK\n");
  printf("@L.console.hook #17 DEI #02 SUB DUP #02 GTH ?&not_an_arg LIT2r 0001 SUB2r DUP ?&arg_space_or_args_end POP #12 DEI STH2kr STA\n");
  printf("  &done\n");
  printf("  &not_an_arg BRK\n");
  printf("  &arg_space_or_args_end #00 STH2kr STA #fffe LDA2k INC2 SWP2 STA2 #01 EQU ?&done\n");
  printf("@prep_argc_argv_and_call_main #fffd DUP2r\n");
  printf("  &stack_reverse_loop LDAk STH2kr LDA SWP STH2kr STA ROT ROT STAk ROT POP #0001 SUB2 INC2r STH2kr OVR2 LTH2 ?&stack_reverse_loop POP2 POP2r STH2kr LIT2r fffe LDA2r INCr LITr 10 SFT2r SUB2r STH2kr SWP2\n");
  printf("  &argv_loop SWP2 STA2k #0002 ADD2 SWP2\n");
  printf("  &strlen_loop INC2k SWP2 LDA ?&strlen_loop DUP2 #fffe LTH2 ?&argv_loop POP2 POP2 STH2kr #fffe LDA2 main_ BRK\n");
}

void codegen(Program *prog, bool do_opt, int devices_size, Device* devices, Device* console, void (*argc_argv_hook)(void)) {
  int i;
  bool need_device_hook[devices_size];
  for(i = 0; i < devices_size; ++i){ need_device_hook[i] = false; }
  bool need_argc_argv = false;

  printf("|0100\n");
  for (Function *fn = prog->fns; fn; fn = fn->next) {
    for (i = 0; i < devices_size; i++) {
      if (*devices[i].name && !strncmp(fn->name, "on_", 3) &&
          !strcmp(fn->name + 3, devices[i].name)) {
        printf("  ;L.%s.hook #%02x DEO2\n", devices[i].name, devices[i].port);
        need_device_hook[i] = true;
      }
    }
    if (!strcmp(fn->name, "main") && fn->params) {
      if (!fn->params->next || (fn->params->next && fn->params->next->next)) {
        error("main() has incorrect parameter count: 0 or 2 expected");
      }
      Type* ty1 = fn->params->var->ty;
      Type* ty2 = fn->params->next->var->ty;
      if (ty1->kind != TY_INT || ty2->kind != TY_PTR || !ty2->base ||
          ty2->base->kind != TY_PTR || ty2->base->base->kind != TY_CHAR) {
        error("main() parameters have incorrect types: main(int argc, char *argv[]) expected");
      }
      need_argc_argv = true;
    }
  }
  if (!need_argc_argv) {
    printf("  LIT2r 0000 main_ POP2r BRK\n");
  } else {
    if (!console) {
      error("main(int, char*[]), but no console device");
    }
    if (console < devices || console >= devices + devices_size) {
      error("main(int, char*[]), but invalid console device pointer");
    }
    if (need_device_hook[console - devices]) {
      // TODO: passthrough for on_console() for stdin
      error("main(int, char*[]) clashes with console device hook");
    }
    if (!argc_argv_hook) {
      error("main(int, char*[]), but hook is missing");
    }
    argc_argv_hook();
  }
  for (i = 0; i < devices_size; i++) {
    if (need_device_hook[i]) {
      printf("  @L.%s.hook LIT2r 0000 on_%s_ POP2 POP2r BRK\n",
        devices[i].name, devices[i].name);
    }
  }
  emit_data(prog);

  Instruction out = {};
  emit_head = &out;
  emit_text(prog);
  if (do_opt)
    optimize(&out);
  output(&out);
}
