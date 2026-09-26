#!/usr/bin/env sh
set -eu

shell=${1:-./mymsh}
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/minishell-test.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM

assert_equal() {
    expected=$1
    actual=$2
    label=$3
    if [ "$expected" != "$actual" ]; then
        printf 'FAIL: %s\nexpected: <%s>\nactual:   <%s>\n' "$label" "$expected" "$actual" >&2
        exit 1
    fi
}

actual=$($shell "printf 'hola mundo\\n'")
assert_equal "hola mundo" "$actual" "quoted arguments"

expected=$(bash -c "printf 'uno\\ndos\\n' | wc -l" | tr -d ' ')
actual=$($shell "printf 'uno\\ndos\\n' | wc -l" | tr -d ' ')
assert_equal "$expected" "$actual" "pipeline compared with sh"

MINISHELL_TEST_VALUE="variable expandida" actual=$(MINISHELL_TEST_VALUE="variable expandida" $shell 'printf "%s\n" "$MINISHELL_TEST_VALUE"')
assert_equal "variable expandida" "$actual" "environment expansion"

$shell "printf 'primera\\n' > $tmp_dir/output.txt"
$shell "printf 'segunda\\n' >> $tmp_dir/output.txt"
actual=$(cat "$tmp_dir/output.txt")
assert_equal "primera
segunda" "$actual" "truncate and append redirection"

printf 'entrada\n' > "$tmp_dir/input.txt"
actual=$($shell "cat < $tmp_dir/input.txt")
assert_equal "entrada" "$actual" "input redirection"

if $shell "echo hola |" >/dev/null 2>&1; then
    printf 'FAIL: invalid syntax returned success\n' >&2
    exit 1
fi

printf 'pwd\ncd /\npwd\nexit\n' | $shell > "$tmp_dir/session.txt"
actual=$(tail -n 1 "$tmp_dir/session.txt")
assert_equal "/" "$actual" "built-ins keep shell state"

printf 'shell integration tests: OK\n'
