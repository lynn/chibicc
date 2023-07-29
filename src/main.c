#include "chibi.h"

int total_alloc = 0;

int fsize(FILE *fp) {
  long previous = ftell(fp);
  fseek(fp, 0L, SEEK_END);
  long size = ftell(fp);
  fseek(fp, previous, SEEK_SET);
  return size;
}

// Returns the contents of a given file.
static char *read_file(char *path) {
  // Open and read the file.
  FILE *fp = fopen(path, "r");
  if (!fp)
    error("cannot open %s: %s", path, strerror(errno));

  int filemax = fsize(fp) + 3;
  char *buf = malloc(filemax);
  int size = fread(buf, 1, filemax - 2, fp);
  if (!feof(fp))
    error("%s: file too large");

  // Make sure that the string ends with "\n\0".
  if (size == 0 || buf[size - 1] != '\n')
    buf[size++] = '\n';
  buf[size] = '\0';
  return buf;
}

int main(int argc, char **argv) {
  bool do_opt = false;

  for (int i = 1; i < argc; i++) {
    if (!strcmp("-O", argv[i]) || !strcmp("-O1", argv[i])) {
      do_opt = true;
    } else if (!strcmp("-O0", argv[i])) {
      do_opt = false;
    } else if (argv[i][0] == '-') {
      error("%s: unrecognized option \"%s\"", argv[0], argv[i]);
    } else if (!filename) {
      filename = argv[i];
    } else {
      error("%s: can't specify more than one filename", argv[0]);
    }
  }
  if (!filename) {
    error("%s: a filename is required", argv[0]);
  }

  // Tokenize and parse.
  user_input = read_file(filename);
  fprintf(stderr, "read_file(): allocated %d bytes\n", total_alloc);
  total_alloc = 0;

  token = tokenize();
  fprintf(stderr, "tokenize(): allocated %d bytes (sizeof(Token) = %d)\n",
          total_alloc, (int)sizeof(Token));
  total_alloc = 0;

  Program *prog = program();
  fprintf(stderr, "program(): allocated %d bytes (sizeof(Node) = %d)\n",
          total_alloc, (int)sizeof(Node));
  total_alloc = 0;

  // Assign offsets to local variables.
  for (Function *fn = prog->fns; fn; fn = fn->next) {
    int offset = fn->has_varargs ? 56 : 0;
    for (VarList *vl = fn->locals; vl; vl = vl->next) {
      Var *var = vl->var;
      offset = align_to(offset, var->ty->align);
      var->offset = offset;
      offset += var->ty->size;
    }
    fn->stack_size = align_to(offset, 2);
  }

  // Traverse the AST to emit assembly.
  Device devices[] = {
    {"console", 0x10}, {"screen", 0x20}, {"audio1", 0x30},     {"audio2", 0x40},
    {"audio3", 0x50},  {"audio4", 0x60}, {"controller", 0x80}, {"mouse", 0x90},
  };
  codegen(prog, do_opt, sizeof(devices) / sizeof(Device), devices, &devices[0],
    varvara_argc_argv_hook);
  fprintf(stderr, "codegen(): allocated %d bytes\n", total_alloc);

  return 0;
}
