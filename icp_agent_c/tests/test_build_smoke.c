#include <assert.h>
#include <stddef.h>

#include "icp_agent/agent.h"

int main(void) {
    const char *version = ic_agent_version();
    assert(version != NULL);
    assert(version[0] != '\0');
    return 0;
}
