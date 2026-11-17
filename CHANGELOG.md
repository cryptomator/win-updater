# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0](https://github.com/cryptomator/win-updater/releases/tag/0.2.0) - 2026-11-17

### Changed
- Removed signing: Released binaries must be signed externally

## [0.1.0](https://github.com/cryptomator/win-updater/releases/tag/0.1.0) - 2026-11-16

### Added

- Windows updater: Locks and verifies the downloaded installer (SHA-256 digest and Authenticode signature with signer EKU and root), confirms readiness to the app, waits for it to exit, runs the MSI or the EXE bundle (elevated from the start) and relaunches the app
