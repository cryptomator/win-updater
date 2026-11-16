#pragma once

#include "exitcode.h"

/**
 * Runs the update: locks and verifies the installer and reports READY while the app is running, then waits for the app to exit, runs the installer and relaunches the app.
 *
 * @param argc number of arguments, including the program name
 * @param argv command-line arguments, argv[0] being the program name
 * @return the exit code of the updater
 */
ExitCode Run(int argc, const wchar_t* const* argv);
