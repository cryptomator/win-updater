#pragma once

#include "handle.h"
#include "log.h"

#include <string>

// The System32 directory from GetSystemDirectoryW, without trailing backslash.
std::wstring SystemDirectory();

// Command line for msiexec: passive install into the given directory with a verbose log. All paths must be free of double quotes.
std::wstring BuildMsiCommandLine(const std::wstring& msiexec, const std::wstring& package, const std::wstring& installDir, const std::wstring& logFile);

// Parameters for the EXE bundle (WiX Burn): passive install with a log.
std::wstring BuildBundleParameters(const std::wstring& logFile);

/**
 * Runs msiexec unelevated (it requests elevation itself) and waits for it to exit.
 *
 * @param commandLine from BuildMsiCommandLine
 * @param msiexec absolute path of msiexec.exe
 * @param workingDir working directory of the new process
 * @param exitCode receives msiexec's exit code
 * @param log receives errors
 * @return whether msiexec could be started
 */
bool RunMsiexec(const std::wstring& msiexec, std::wstring commandLine, const std::wstring& workingDir, DWORD& exitCode, Log& log);

/**
 * Starts an EXE bundle elevated from the beginning (ShellExecuteExW with the verb "runas") and waits for it to exit.
 *
 * @param bundle path of the verified, locked bundle
 * @param parameters from BuildBundleParameters
 * @param workingDir working directory of the new process
 * @param exitCode receives the bundle's exit code, or ERROR_CANCELLED if the user declined the UAC prompt
 * @param log receives errors
 * @return whether the bundle was started or the user declined the UAC prompt
 */
bool RunElevated(const std::wstring& bundle, const std::wstring& parameters, const std::wstring& workingDir, DWORD& exitCode, Log& log);

/**
 * Starts a program without arguments and without waiting for it.
 *
 * @param exe absolute path of the program, also determines the working directory
 * @param log receives errors
 * @return whether the program could be started
 */
bool Launch(const std::wstring& exe, Log& log);
