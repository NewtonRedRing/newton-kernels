import sys
from newton_kernels import NewtonKernels

if len(sys.argv) != 2:
    raise SystemExit("Usage: python test_smoke.py PATH_TO_SHARED_LIBRARY")
lib = NewtonKernels(sys.argv[1])
assert lib.count_byte(b"ababa\x00a", ord("a")) == 4
assert lib.sum_gt([0, 1, 7, 9, 3], 3) == (2, 16)
print("Python FFI smoke: PASS")
