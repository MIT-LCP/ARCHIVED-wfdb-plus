#include "wfdb.hh"

#include <absl/strings/str_format.h>

#include <iostream>
#include <string>

// Error tracking state
static std::string error_message =
    absl::StrFormat("WFDB library version %d.%d.%d (%s).\n", WFDB_MAJOR,
                    WFDB_MINOR, WFDB_RELEASE, WFDB_BUILD_DATE);
static bool print_error = true;

/* Handles error messages, normally by printing them on the standard error
output. It can be silenced by invoking wfdbquiet(), or re-enabled by invoking
wfdbverbose().
*/
void wfdb_error(std::string_view msg) {
  error_message = msg;

  if (print_error) {
    std::cerr << error_message << std::flush;
  }
}

/*
Returns the most recent error message passed to wfdb_error (even if output was
suppressed by wfdbquiet). This feature permits programs to handle errors
somewhat more flexibly (in windowing environments, for example, where using the
standard error output may be inappropriate).
*/
std::string wfdberror() {
  // TODO: Figure out memory strategy
  if (error_message.empty()) {
    return "WFDB: cannot allocate memory for error message";
  }
  return error_message;
}

/* wfdbquiet can be used to suppress error messages from the WFDB library. */
void wfdbquiet() { print_error = false; }

/* wfdbverbose enables error messages from the WFDB library. */
void wfdbverbose() { print_error = true; }
