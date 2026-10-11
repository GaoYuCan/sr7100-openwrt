#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu
R=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cc -O2 -Wall -Wextra -I "$R/tests" "$R/tests/test-bounded-policy.c" -o "$T/nand"
cc -O2 -Wall -Wextra -I "$R/drivers/ethernet" "$R/drivers/ethernet/test-policy.c" -o "$T/ports"
"$T/nand"
"$T/ports"
python3 "$R/tests/test-core-store.py"
python3 "$R/tools/flash-plan.py" > "$T/layout.json"
echo 'Offline policy and persistence tests passed; no router was contacted'
