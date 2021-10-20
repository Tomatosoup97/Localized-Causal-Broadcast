#!/bin/zsh
set -e

fd ".*\.(c|h)pp" | xargs -I % sh -c "echo formatting %; clang-format -i %"
