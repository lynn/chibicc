// How to run:
//
// $ make
// $ gcc -I. -P -E examples/nqueen.c > tmp.c
// $ ./chibicc tmp.c > tmp.tal
// $ uxnasm tmp.tal tmp.rom
// $ uxncli tmp.rom

#include <varvara.h>

int print_board(int (*board)[10]) {
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      if (board[i][j])
        putchar('Q');
      else
        putchar('.');
      putchar(' ');
    }
    putchar('\n');
  }
  putchar('\n');
  putchar('\n');
}

int conflict(int (*board)[10], int row, int col) {
  for (int i = 0; i < row; i++) {
    if (board[i][col])
      return 1;
    int j = row - i;
    if (0 < col - j + 1 && board[i][col - j])
      return 1;
    if (col + j < 10 && board[i][col + j])
      return 1;
  }
  return 0;
}

int solve(int (*board)[10], int row) {
  if (row > 9) {
    print_board(board);
    return 0;
  }
  for (int i = 0; i < 10; i++) {
    if (!conflict(board, row, i)) {
      board[row][i] = 1;
      solve(board, row + 1);
      board[row][i] = 0;
    }
  }
}

int main() {
  int board[100];
  for (int i = 0; i < 100; i++)
    board[i] = 0;
  solve(board, 0);
  exit(0);
  return 0;
}
