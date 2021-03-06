"""
Galois Python is a Python library designed to simplify parallel programming. It is especially
focused on computations that are difficult to parallelize efficiently, such as
loops with

* irregular amount of work per iteration
* irregular memory accesses and branching patterns
* dependencies between iterations
* dynamic work creation

A typical Galois user is a Python programmer who understands parallelism in his/her
algorithm and wishes to express it using high-level constructs such as parallel
loops and concurrent data structures, without having to deal with low-level
parallel programming details such as threads, mutexes, barriers, condition
variables, work stealing, etc.

Galois Python utilizes the underlying Galois C++ library for most operations.
Galois Python also leverages numba to compile the "operators" which are run
by Galois C++.
"""

# Initialize the galois runtime immediately.
import galois.shmem


def load_ipython_extension(ipython):
    import cython

    cython.load_ipython_extension(ipython)
    from .ipython import GaloisMagics

    ipython.register_magics(GaloisMagics)
