# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Windows update helper that locks and verifies the downloaded installer (SHA-256 digest and Authenticode signature with signer EKU and root), confirms readiness to Cryptomator, waits for it to exit, runs the MSI or the EXE bundle (elevated from the start) and relaunches Cryptomator
