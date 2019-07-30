#define FOC_DEBUGGER_IMPLEMENTATION
#include "../foc/debugger.h"

#define FOC_LOGGING_IMPLEMENTATION
#include "../foc/logging.h"

int main(int argc, char *argv[]) {
  DCHECK(argc == 1) << "argc expected to be 1, but" << argc << "was provided";
}
