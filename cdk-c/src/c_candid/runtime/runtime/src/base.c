#include <stddef.h>

#include "idl/base.h"

const char *idl_status_message(idl_status status) {
  switch (status) {
  case IDL_STATUS_OK:
    return "ok";
  case IDL_STATUS_ERR_OVERFLOW:
    return "overflow";
  case IDL_STATUS_ERR_TRUNCATED:
    return "truncated input";
  case IDL_STATUS_ERR_ALLOC:
    return "allocation failure";
  case IDL_STATUS_ERR_INVALID_ARG:
    return "invalid argument";
  case IDL_STATUS_ERR_UNSUPPORTED:
    return "unsupported operation";
  }
  return "unknown status";
}
