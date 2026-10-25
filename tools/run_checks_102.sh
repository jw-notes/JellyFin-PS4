#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
g++ -std=c++17 -O2 -Wall -Wextra -Isource tests/keyboard_test.cpp source/keyboard_model.cpp -o build/keyboard-test
./build/keyboard-test > logs/keyboard-model-102-final.log 2>&1
JF_TEST_DATA="$PWD/build/auth-test-data" ./build/auth-integration-test http://127.0.0.1:18096 ps4test 'LocalPortTest-2026!' > logs/auth-tests-102.log 2>&1
bash tools/test.sh http://127.0.0.1:18096 ps4test 'LocalPortTest-2026!' > logs/regression-tests-102.log 2>&1
cat logs/keyboard-model-102-final.log logs/auth-tests-102.log logs/regression-tests-102.log
