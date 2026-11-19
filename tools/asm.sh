#!/usr/bin/env bash
# Print the assembly g++ generates for matching a regex at runtime.
#
#   tools/asm.sh 'a(b|c)*d'            # -O2 by default
#   tools/asm.sh 'a(b|c)*d' -O3        # extra arguments are passed to g++
#   CXX=clang++ tools/asm.sh 'a(b|c)*d'
#
# It compiles a wrapper with external linkage,
#   bool test(std::string_view text) { return re::ct::match<"...">(text).success; }
# which forces the matcher to be emitted, then demangles the names, drops
# assembler directives, and shortens the long template names.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  sed -n '2,7s/^# \{0,1\}//p' "$0" >&2
  exit 2
fi

regex=$1
shift

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# A raw string literal, so the regex needs no escaping (unless it contains
# the delimiter itself).
if [[ $regex == *')__re__"'* ]]; then
  echo "regex contains the raw string delimiter )__re__\"" >&2
  exit 2
fi
cat >"$tmp/asm.cpp" <<EOF
#include "toyctre/compile_time.hpp"
#include <string_view>

bool test(std::string_view text) {
  return re::ct::match<R"__re__($regex)__re__">(text).success;
}
EOF

"${CXX:-g++}" -std=c++23 -O2 -S -o "$tmp/asm.s" -I"$repo/include" \
  -masm=intel -fno-asynchronous-unwind-tables "$@" "$tmp/asm.cpp"

# Everything from the first .note section on is toolchain metadata.
awk '/^[[:space:]]*\.section[[:space:]]+\.note/ { exit } { print }' "$tmp/asm.s" |
  c++filt |
  grep -vE '^\s*\.(file|ident|section|p2align|globl|weak|type|size|text|cfi|intel_syntax|addrsig|addrsig_sym)|^\.LF[BE][0-9]+:|^\s*$' |
  perl -pe '
    s/re::ct::detail::match::CapturingMatcher<re::ct::detail::match::CaptureData<\d+ul> >:://g;
    s/re::ct::detail::match:://g;
    s/re::ct::detail:://g;
    s/re::ctn:://g;
    s/re::ct:://g;
    s/std::basic_string_view<char, std::char_traits<char> >/string_view/g;
    s/CaptureData<\d+ul>&/CaptureData&/g;
    s/\(char\)(\d+)/($1 >= 33 && $1 < 127) ? "\x27" . chr($1) . "\x27" : "(char)$1"/ge;
    s/(\d+)ul\b/$1/g;
    s/> >/>>/g while />\s>/;
  '
