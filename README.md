# WFDB Plus

This is NOT the official WFDB software package. See https://github.com/bemoody/wfdb instead.

This is a project attempting to advance the WFDB package into a more modern form, by converting the main code into C++, refactoring and reorganizing the content, and using CMake as the build system generator. If this effort proves successful, this will become the reference WFDB package. If not, you never read this.

## Repo Content

Directories:

- `src`: The main WFDB codebase.
- `data`: Data used for tests.
- `examples`: Example usage scripts.
- `fortran`: Fortran wrappers for the WFDB library.
- `wave`: The Linux desktop waveform viewer, WAVE.

## Installation

WFDB is officially supported on Linux.

Prerequisites: gcc, cmake, libcurl.

Eg. For Debian and Ubuntu:

```sh
sudo apt-get update && sudo apt-get install -y gcc curl cmake

# TODO after cmake integration

```

See also the [Dockerfile](./Dockerfile) for a containerized installation. To build the container:

```sh
docker build . -t wfdb -f Dockerfile
```

## Developing WFDB

### Containerized Development Environment

This section describes a potential containerized development environment that can be used on Linux, MacOS, and Windows. You are not required to use this setup.

A recommended environment is shown in [Dockerfile.dev](./Dockerfile.dev). To set up this system:

- [Install](https://docs.docker.com/get-docker/) Docker or Docker Desktop.

- If you wish to use VSCode and its Remote Explorer extension, follow the [Developing inside a Container guide](https://code.visualstudio.com/docs/remote/containers). When you create your dev container, select "From Dockerfile.dev". Inside the container, you may also want to install the "CMake" and "CMake Tools" extensions.

- If you do not wish to use VSCode and/or the extension, you can just build and run the container:

```sh
docker build . -t wfdbdev -f Dockerfile.dev
docker run -it wfdbdev /bin/bash
```

### Style Guide

Follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) as specified in [.clang-format](.clang-format).
