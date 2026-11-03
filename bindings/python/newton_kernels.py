"""Opt-in ctypes wrapper around NewtonKernels' in-process C ABI.

Pass the full path to the explicitly built newton_c shared library.
No network, background service, JIT, package import side effects or threads.
The byte-buffer wrapper copies immutable Python bytes into ctypes memory;
benchmark Python overhead separately from the native kernel.
"""
import ctypes
from array import array

class NewtonKernels:
    def __init__(self, library_path):
        self.lib = ctypes.CDLL(str(library_path))
        self.lib.newton_count_byte.argtypes = [
            ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint8,
            ctypes.POINTER(ctypes.c_size_t)]
        self.lib.newton_count_byte.restype = ctypes.c_int
        self.lib.newton_sum_gt_u64.argtypes = [
            ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint64,
            ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_uint64)]
        self.lib.newton_sum_gt_u64.restype = ctypes.c_int

    def count_byte(self, data: bytes, needle: int) -> int:
        if not isinstance(data, bytes) or not 0 <= needle <= 255:
            raise ValueError("Expected bytes and 8-bit needle")
        buf = ctypes.create_string_buffer(data)
        result = ctypes.c_size_t()
        status = self.lib.newton_count_byte(
            buf, len(data), needle, ctypes.byref(result))
        if status:
            raise RuntimeError(f"newton_count_byte status {status}")
        return result.value

    def sum_gt(self, values, threshold: int):
        if not 0 <= threshold <= 2**64 - 1:
            raise ValueError("Threshold outside uint64")
        values = array("Q", values)
        if values.itemsize != 8:
            raise RuntimeError("Platform does not use 64-bit unsigned array items")
        buffer = (ctypes.c_uint64 * len(values)).from_buffer(values)
        count = ctypes.c_uint64()
        total = ctypes.c_uint64()
        status = self.lib.newton_sum_gt_u64(
            buffer, len(values), threshold, ctypes.byref(count), ctypes.byref(total))
        if status == 2:
            raise OverflowError("Unsigned 64-bit accumulator overflow")
        if status:
            raise RuntimeError(f"newton_sum_gt_u64 status {status}")
        return count.value, total.value
