# win-updater

A small native Windows executable that applies an app update after the app has exited:
1. Locks and verifies the downloaded installer.
2. Sends a "ready" signal to the waiting app.
3. Waits for the app to exit.
4. Runs the installer.
5. Starts the app again.

## Usage

The app starts the updater directly (not via `cmd.exe`), with all paths absolute:

```
win-updater.exe --wait-pid <pid> --installer <path> --sha256 <hex> --signer-eku <oid> --root-thumbprint <hex> --relaunch <path> [--log-dir <path>]
```

- `--wait-pid`: process ID of the app, usually the caller's own.
- `--installer`: the downloaded installer, either an `.msi` (run with `msiexec`, receiving `INSTALLDIR=<directory of --relaunch>`) or an `.exe` bundle (started elevated with `/passive /norestart /log <file>`, as understood by WiX Burn bundles).
- `--sha256`: expected SHA-256 digest of the installer, 64 hex characters.
- `--signer-eku`: OID of an extended key usage that the installer's signing certificate must contain, e.g. the durable identity of an Azure Artifact Signing account.
- `--root-thumbprint`: SHA-1 thumbprint (40 hex characters) of the root certificate the signature must chain up to.
- `--relaunch`: the app's executable, started again after the installation.
- `--log-dir`: directory for `win-updater.log` and the installer's log; defaults to the installer's directory.

After verifying the installer, the updater writes `READY` to stdout. The app exits once it has read that line; if the updater exits without writing it, its exit code tells why. If the app doesn't exit within 60 s, the updater gives up without installing. See [docs/spec.md](docs/spec.md) for the exit codes and the design.

## Building

Requires Visual Studio with the C++ build tools (x64, and ARM64 for the ARM64 build).

```
build.cmd x64 0.1.0          :: target\win-updater-x64.exe
build.cmd arm64 0.1.0        :: target\win-updater-arm64.exe
build.cmd x64 0.1.0 test     :: builds the test binaries and runs the tests
```

The tests verify a real signature including its revocation status, so they need network access.

The version resource identifies the binary as "App Updater" (product "win-updater") by default. To brand it, set the environment variables `WIN_UPDATER_PRODUCT` (e.g. `Cryptomator`, which yields the description "Cryptomator Updater") and `WIN_UPDATER_COMPANY` before building; both may only contain letters, digits, spaces, dots and hyphens.

## License

This project is licensed under the AGPL-3.0, see [LICENSE](LICENSE).
