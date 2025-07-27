#!/usr/bin/env sh

# Also good but untagged: CLANG_FORMAT=unibeautify/clang-format
# This might provide more control in the future: silkeh/clang:8 (etc)
CLANG_FORMAT=saschpe/clang-format:5.0.1

set -evx

docker run --rm ${CLANG_FORMAT} --version
docker run --rm --user $(id -u):$(id -g) -v "$(pwd)":/workdir -w /workdir ${CLANG_FORMAT} -style=file -sort-includes -i $(git ls-files -- '*.cpp' '*.hpp')

git diff --exit-code --color

set +evx
