#!/bin/bash
# This script configures environment for working on this codebase,
# Such as pre-hooks, git blame configurations and may more in future.

# This git blame command prevents git from showing commits like clang-format
# when working on the codebase.
git config blame.ignoreRevsFile .git-blame-ignore-revs

# Configures pre-hook location project-wide and allows the script to be executed.
chmod +x .githooks/*
git config core.hooksPath .githooks