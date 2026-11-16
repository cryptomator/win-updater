#pragma once

#include "exitcode.h"

/**
 * Runs both phases of the update, see "Solution" in docs/spec.md.
 *
 * @param argc number of arguments, including the program name
 * @param argv command-line arguments, argv[0] being the program name
 * @return the exit code of the updater
 */
ExitCode Run(int argc, const wchar_t* const* argv);
