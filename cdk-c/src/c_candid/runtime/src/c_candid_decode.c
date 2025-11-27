#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Temporary stub decoder for Phase 0-A.
// For now this simply delegates to `didc decode --format hex`,
// so that the oracle pipeline and compare.py can be verified.
// Later this program will be replaced by a pure C implementation
// that reads hex and writes Candid text.

int main(int argc, char **argv) {
  const char *bin = getenv("DIDC_BIN");
  if (bin == NULL || bin[0] == '\0') {
    bin = "didc"; // rely on PATH
  }

  // Build argv: bin decode --format hex [extra args...]
  int extra = argc - 1;
  int base = 4; // decode --format hex
  char **args = calloc((size_t)(base + extra + 1), sizeof(char *));
  if (!args) {
    perror("calloc");
    return 1;
  }

  int i = 0;
  args[i++] = (char *)bin;
  args[i++] = "decode";
  args[i++] = "--format";
  args[i++] = "hex";
  for (int j = 1; j < argc; ++j) {
    args[i++] = argv[j];
  }
  args[i] = NULL;

  execvp(bin, args);
  perror("execvp didc decode");
  free(args);
  return 1;
}
