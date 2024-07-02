#include "chibi.h"

char *opcode_names[0x20] = {
    "BRK", "INC", "POP", "NIP", "SWP", "ROT", "DUP", "OVR", "EQU", "NEQ", "GTH",
    "LTH", "JMP", "JCN", "JSR", "STH", "LDZ", "STZ", "LDR", "STR", "LDA", "STA",
    "DEI", "DEO", "ADD", "SUB", "MUL", "DIV", "AND", "ORA", "EOR", "SFT",
};

Instruction *emit_head;

void emit(Opcode opcode, int literal, char *label) {
  emit_head->opcode = opcode;
  emit_head->literal = literal;
  if (label)
    emit_head->label = strdup(label);
  emit_head->next = calloc(1, sizeof(Instruction));
  emit_head = emit_head->next;
}

char buf[4096];
void op(Opcode o) { emit(o, 0, ""); }
void jci(char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vsprintf(buf, fmt, va);
  va_end(va);
  emit(JCI, 0, buf);
}
void jmi(char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vsprintf(buf, fmt, va);
  va_end(va);
  emit(JMI, 0, buf);
}
void jsi(char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vsprintf(buf, fmt, va);
  va_end(va);
  emit(JSI, 0, buf);
}
void lit(unsigned char n) { emit(LIT, n, ""); }
void lit2(unsigned short n) { emit(LIT2, n, ""); }
void lit2r(unsigned short n) { emit(LIT2r, n, ""); }
void at(char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vsprintf(buf, fmt, va);
  va_end(va);
  emit(AT, 0, buf);
}
void semi(char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vsprintf(buf, fmt, va);
  va_end(va);
  emit(SEMI, 0, buf);
}
void bar(unsigned short n) { emit(BAR, n, ""); }
#define IsLit(p, n) ((p) && (p)->opcode == LIT && (p)->literal == n)
#define IsLit2(p, n) ((p) && (p)->opcode == LIT2 && (p)->literal == n)

#define ConstantFolding(target_opcode, c_operator, result_opcode)              \
  if (prog->opcode == LIT2 && prog->next->opcode == LIT2 &&                    \
      prog->next->next->opcode == (target_opcode | flag_2)) {                  \
    prog->opcode = result_opcode;                                              \
    prog->literal = prog->literal c_operator prog->next->literal;              \
    prog->next = prog->next->next->next;                                       \
    changed = true;                                                            \
    continue;                                                                  \
  }                                                                            \
  if (prog->opcode == LIT && prog->next->opcode == LIT &&                      \
      prog->next->next->opcode == target_opcode) {                             \
    prog->literal = (unsigned char)prog->literal c_operator(unsigned char)     \
                        prog->next->literal;                                   \
    prog->next = prog->next->next->next;                                       \
    changed = true;                                                            \
    continue;                                                                  \
  }

static bool optimize_pass(Instruction *prog, int stage) {
  bool changed = false;
  while (prog->opcode) {
    if (prog->next && prog->next->next) {
      ConstantFolding(ADD, +, LIT2);
      ConstantFolding(SUB, -, LIT2);
      ConstantFolding(MUL, *, LIT2);
      ConstantFolding(DIV, /, LIT2);
      ConstantFolding(AND, &, LIT2);
      ConstantFolding(ORA, |, LIT2);
      ConstantFolding(EOR, ^, LIT2);
      ConstantFolding(EQU, ==, LIT);
      ConstantFolding(NEQ, !=, LIT);
      ConstantFolding(GTH, >, LIT);
      ConstantFolding(LTH, <, LIT);
    }

    // Fold INC2
    if (prog->opcode == LIT2 && prog->next && prog->next->opcode == INC2) {
      ++prog->literal;
      prog->next = prog->next->next;
      changed = true;
      continue;
    }

    // #0001 ADD2 -> INC2
    if (IsLit2(prog, 1) && prog->next && prog->next->opcode == ADD2) {
      prog->opcode = INC2;
      prog->next = prog->next->next;
      changed = true;
      continue;
    }

    // #0002 ADD2 -> INC2 INC2
    if (IsLit2(prog, 2) && prog->next && prog->next->opcode == ADD2) {
      prog->opcode = INC2;
      prog->next->opcode = INC2;
      changed = true;
      continue;
    }

    // Commutativity
    if (prog->opcode == SWP2 && prog->next &&
            (prog->next->opcode == ORA2 || prog->next->opcode == AND2 ||
             prog->next->opcode == ADD2 || prog->next->opcode == MUL2 ||
             prog->next->opcode == EOR2 || prog->next->opcode == EQU2 ||
             prog->next->opcode == NEQ2) ||
        prog->opcode == SWP && prog->next &&
            (prog->next->opcode == ORA || prog->next->opcode == AND ||
             prog->next->opcode == ADD || prog->next->opcode == MUL ||
             prog->next->opcode == EOR || prog->next->opcode == EQU ||
             prog->next->opcode == NEQ)) {
      prog->opcode = prog->next->opcode;
      prog->next = prog->next->next;
      changed = true;
      continue;
    }

    // #0000 +-|^ -> nothing
    if (IsLit2(prog, 0) && prog->next &&
        (prog->next->opcode == ADD2 || prog->next->opcode == SUB2 ||
         prog->next->opcode == ORA2 || prog->next->opcode == EOR2)) {
      memcpy(prog, prog->next->next, sizeof(Instruction));
      changed = true;
      continue;
    }

    // #xxxx POP2 cancels out
    if (prog->opcode == LIT2 && prog->next && prog->next->opcode == POP2) {
      memcpy(prog, prog->next->next, sizeof(Instruction));
      changed = true;
      continue;
    }

    // sext NIP cancels out
    if (prog->opcode == JSI && !strcmp("sext", prog->label) && prog->next &&
        prog->next->opcode == NIP) {
      memcpy(prog, prog->next->next, sizeof(Instruction));
      changed = true;
      continue;
    }

    // LIT SWP NIP cancels out
    if (prog->opcode == LIT && prog->next && prog->next->opcode == SWP &&
        prog->next->next && prog->next->next->opcode == NIP) {
      memcpy(prog, prog->next->next->next, sizeof(Instruction));
      changed = true;
      continue;
    }

    // #0001 muldiv -> nothing
    if (IsLit2(prog, 1) && prog->next &&
        (prog->next->opcode == MUL2 || prog->next->opcode == DIV2)) {
      memcpy(prog, prog->next->next, sizeof(Instruction));
      changed = true;
      continue;
    }

    // pow2 muldiv -> SFT2
    if (prog->opcode == LIT2 && prog->literal > 1 &&
        (prog->literal & (prog->literal - 1)) == 0 && prog->next &&
        (prog->next->opcode == MUL2 || prog->next->opcode == DIV2)) {
      prog->opcode = LIT;
      int log2 = 0;
      while (prog->literal >>= 1)
        log2++;
      prog->literal = log2 << (prog->next->opcode == MUL2 ? 4 : 0);
      prog->next->opcode = SFT2;
      changed = true;
      continue;
    }

    // Bypass divisor sign check for known signs
    if (prog->opcode == LIT2 && prog->next && prog->next->opcode == JSI &&
        !strcmp("sdiv", prog->next->label)) {
      prog->next->label =
          (prog->literal & 0x8000) ? "sdiv/b_neg" : "sdiv/b_pos";
      changed = true;
      continue;
      // modulo variant
    } else if (prog->opcode == LIT2 && prog->next &&
               prog->next->opcode == OVR2 && prog->next->next &&
               prog->next->next->opcode == OVR2 && prog->next->next->next &&
               prog->next->next->next->opcode == JSI &&
               !strcmp("sdiv", prog->next->next->next->label)) {
      prog->next->next->next->label =
          (prog->literal & 0x8000) ? "sdiv/b_neg" : "sdiv/b_pos";
      changed = true;
    }

    // #10 SFT2 -> DUP2 ADD2
    if (IsLit2(prog, 10) && prog->next && prog->next->opcode == SFT2) {
      prog->opcode = DUP2;
      prog->next->opcode = ADD2;
      changed = true;
      continue;
    }

    // Shrink literals
    if (prog->opcode == LIT2 && prog->next && prog->next->opcode == NIP) {
      prog->opcode = LIT;
      prog->literal &= 0xff;
      prog->next = prog->next->next;
      changed = true;
      continue;
    }

    // Combine literals
    if (stage >= 1 && prog->opcode == LIT && prog->next &&
        prog->next->opcode == LIT) {
      prog->opcode = LIT2;
      prog->literal = prog->literal << 8 | prog->next->literal;
      prog->next = prog->next->next;
      changed = true;
      continue;
    }

    // Associative addition: x ADD y ADD = (x+y) ADD
    if (prog->opcode == LIT2 && prog->next && prog->next->opcode == ADD2 &&
        prog->next->next && prog->next->next->opcode == LIT2 &&
        prog->next->next->next && prog->next->next->next->opcode == ADD2) {
      prog->literal += prog->next->next->literal;
      prog->next->next = prog->next->next->next->next;
      changed = true;
      continue;
    }

    // A common sequence: STA2k POP2 POP2 -> STA2
    if (prog->opcode == STA2k && prog->next && prog->next->opcode == POP2 &&
        prog->next->next && prog->next->next->opcode == POP2) {
      prog->opcode = STA2;
      prog->next = prog->next->next->next;
      changed = true;
      continue;
    }

    // A common sequence: STAk POP2 POP2 -> STA POP
    if (prog->opcode == (STA | flag_k) && prog->next &&
        prog->next->opcode == POP2 && prog->next->next &&
        prog->next->next->opcode == POP2) {
      prog->opcode = STA;
      prog->next->opcode = POP;
      prog->next->next = prog->next->next->next;
      changed = true;
      continue;
    }

    // DUP2 (INC2|LDA2|DUP2) -> +k
    if (prog->opcode == DUP2 && prog->next &&
        (prog->next->opcode == INC2 || prog->next->opcode == LDA2 ||
         prog->next->opcode == LDA || prog->next->opcode == DUP2)) {
      prog->opcode = (prog->next->opcode | flag_k);
      prog->next = prog->next->next;
      changed = true;
      continue;
    }

    // A common sequence:
    // STH2kr #xxxx ADD2 #yyyy SWP2 -> #yyyy STH2kr #xxxx ADD2
    if (prog->opcode == STH2kr && prog->next && prog->next->opcode == LIT2 &&
        prog->next->next && prog->next->next->opcode == ADD2 &&
        prog->next->next->next && prog->next->next->next->opcode == LIT2 &&
        prog->next->next->next->next &&
        prog->next->next->next->next->opcode == SWP2) {
      int x = prog->next->literal;
      prog->opcode = LIT2;
      prog->literal = prog->next->next->next->literal;
      prog->next->opcode = STH2kr;
      prog->next->next->opcode = LIT2;
      prog->next->next->literal = x;
      prog->next->next->next->opcode = ADD2;
      prog->next->next->next->next = prog->next->next->next->next->next;
      changed = true;
      continue;
    }

    // EQU2 #00 SWP #0000 EQU2 -> NEQ2
    if (prog->opcode == EQU2 && IsLit(prog->next, 0) && prog->next->next &&
        prog->next->next->opcode == SWP && IsLit2(prog->next->next->next, 0) &&
        prog->next->next->next->next &&
        prog->next->next->next->next->opcode == EQU2) {
      prog->opcode = NEQ2;
      prog->next = prog->next->next->next->next->next;
      changed = true;
      continue;
    }

    // #00 SWP #0000 EQU2 ? -> #00 EQU ?
    if (IsLit(prog, 0) && prog->next && prog->next->opcode == SWP &&
        IsLit2(prog->next->next, 0) && prog->next->next->next &&
        prog->next->next->next->opcode == EQU2 &&
        prog->next->next->next->next->opcode == JCI) {
      prog->next->opcode = EQU;
      prog->next->next = prog->next->next->next->next;
      changed = true;
      continue;
    }

    // sext #0000 NEQ2 ? -> ?
    if (prog->opcode == JSI && !strcmp("sext", prog->label) &&
        IsLit2(prog->next, 0) && prog->next->next &&
        prog->next->next->opcode == NEQ2 &&
        prog->next->next->next->opcode == JCI) {
      memcpy(prog, prog->next->next->next, sizeof(Instruction));
      changed = true;
      continue;
      // same with zext (#00 SWP)
    } else if (prog->opcode == LIT && prog->literal == 0 && prog->next &&
               prog->next->opcode == SWP && IsLit2(prog->next->next, 0) &&
               prog->next->next->next &&
               prog->next->next->next->opcode == NEQ2 &&
               prog->next->next->next->next->opcode == JCI) {
      memcpy(prog, prog->next->next->next->next, sizeof(Instruction));
      changed = true;
      continue;
    }

    // #0000 NEQ2 ? -> ORA ?
    if (IsLit2(prog, 0) && prog->next && prog->next->opcode == NEQ2 &&
        prog->next->next && prog->next->next->opcode == JCI) {
      prog->opcode = ORA;
      prog->next = prog->next->next;
      changed = true;
      continue;
    }

    // #ffff NEQ2 ? -> INC2 ORA ?
    if (IsLit2(prog, 0xffff) && prog->next && prog->next->opcode == NEQ2 &&
        prog->next->next && prog->next->next->opcode == JCI) {
      prog->opcode = INC2;
      prog->next->opcode = ORA;
      changed = true;
      continue;
    }

    // sext #00xx AND2 -> #xx AND #00 SWP
    if (prog->opcode == JSI && !strcmp("sext", prog->label) && prog->next &&
        prog->next->opcode == LIT2 && !(prog->next->literal & 0xff00) &&
        prog->next->next && prog->next->next->opcode == AND2) {
      prog->opcode = LIT;
      prog->literal = prog->next->literal & 0xff;
      prog->next->opcode = AND;
      prog->next->next->opcode = LIT;
      prog->next->next->literal = 0;
      Instruction *new = calloc(1, sizeof(Instruction));
      new->opcode = SWP;
      new->next = prog->next->next->next;
      prog->next->next->next = new;
      changed = true;
      continue;
      // ditto with zext (#00 SWP)
    } else if (IsLit(prog, 0) && prog->next && prog->next->opcode == SWP &&
               prog->next->next && prog->next->next->opcode == LIT2 &&
               !(prog->next->next->literal & 0xff00) &&
               prog->next->next->next &&
               prog->next->next->next->opcode == AND2) {
      prog->opcode = LIT;
      prog->literal = prog->next->next->literal & 0xff;
      prog->next->opcode = AND;
      prog->next->next->opcode = LIT;
      prog->next->next->literal = 0;
      prog->next->next->next->opcode = SWP;
      changed = true;
      continue;
    }

    // STH2kr LDA -> LDAkr STHr is slightly faster
    if (prog->opcode == STH2kr && prog->next && prog->next->opcode == LDA) {
      prog->opcode = LDA | flag_k | flag_r;
      prog->next->opcode = STH | flag_r;
      changed = true;
      continue;
    }

    //
    // https://github.com/lynn/chibicc/issues/27
    //

    // instances of #00 ORA, [can] be removed.
    if (IsLit(prog, 0) && prog->next->opcode == ORA) {
      memcpy(prog, prog->next->next, sizeof(Instruction));
      changed = true;
      continue;
    }

    // [SWP] static arithmetic operation e.g. #abcd SWP, could be #cdab.
    if (prog->opcode == LIT2 && prog->next->opcode == SWP) {
      unsigned short tmp = prog->literal;
      prog->literal = ((tmp & 0xFF) << 8) | ((tmp & 0xFF00) >> 8 );
      prog->next = prog->next->next;
      changed = true;
      continue;
    }

    // DUP2 #0000 EQU2, [can] be turned into ORAk #00 EQU
    if (prog->opcode == DUP2 && IsLit2(prog->next, 0) && prog->next->next->opcode == EQU2) {
      prog            ->opcode = ORAk;
      prog->next      ->opcode = LIT;
      prog->next->next->opcode = EQU;
      changed = true;
      continue;
    }

    // #0000 SWP, to #0000
    if (IsLit2(prog, 0) && prog->next->opcode == SWP) {
      memcpy(prog->next, prog->next->next, sizeof(Instruction));
      changed = true;
      continue;
    }

    prog = prog->next;
  }
  return changed;
}

void optimize(Instruction *prog) {
  while (optimize_pass(prog, 0))
    ;
  while (optimize_pass(prog, 1))
    ;
}

void output_one(Instruction *ins) {
  if (ins->opcode == AT) {
    printf("%s%s", ins->label[0] == '&' ? "" : "@", ins->label);
  } else if (ins->opcode == BAR) {
    printf("|%04x", ins->literal);
  } else if (ins->opcode == SEMI) {
    printf(";%s", ins->label);
  } else if (ins->opcode == ASM) {
    printf("%s", ins->label);
  } else if (ins->opcode == JCI) {
    printf("?%s", ins->label);
  } else if (ins->opcode == JMI) {
    printf("!%s", ins->label);
  } else if (ins->opcode == JSI) {
    printf("%s", ins->label);
  } else if (ins->opcode == LIT) {
    printf("#%02x", ins->literal);
  } else if (ins->opcode == LIT2) {
    printf("#%04x", ins->literal);
  } else if (ins->opcode == LIT2r) {
    printf("LIT2r %04x", ins->literal);
  } else {
    printf("%s%s%s%s", opcode_names[ins->opcode & 0x1f],
           ins->opcode & flag_2 ? "2" : "", ins->opcode & flag_k ? "k" : "",
           ins->opcode & flag_r ? "r" : "");
  }
}

void output(Instruction *prog) {
  bool new_line = true; // did we just have a \n and need to re-indent?
  while (prog->opcode) {
    if (prog->opcode == AT || prog->opcode == BAR) {
      printf("\n");  // double newline for all labels
      if (!new_line) // avoid triple newline after certain instructions
        printf("\n");
      if (prog->opcode == AT && prog->label[0] == '&')
        printf("  "); // half-indent (subordinate label)
    } else if (new_line) {
      printf("    "); // indent
      new_line = false;
    } else {
      printf(" "); // separate
    }

    output_one(prog);

    // Labels definitions get their own lines. Other instructions get collected
    // on one line, and we insert newlines at stores and branches (we don't know
    // statement boundaries).
    if (prog->opcode == AT || prog->opcode == BAR || prog->opcode == JCI ||
        prog->opcode == JMI || prog->opcode == JSI || prog->opcode == JMP2r ||
        (prog->opcode & 0x1f) == STA) {
      printf("\n");
      new_line = true;
    }

    prog = prog->next;
  }
}
