#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

python3 -m py_compile client.py server.py ipc_protocol.py
python3 -m unittest discover -s tests -p 'test_*.py'

kernel_dir="${KERNELDIR:-/lib/modules/$(uname -r)/build}"
if [[ ! -d "$kernel_dir" ]]; then
  echo "Skipping kernel build: KERNELDIR '$kernel_dir' does not exist"
  exit 0
fi

make KERNELDIR="$kernel_dir" W=1
make KERNELDIR="$kernel_dir" clean
