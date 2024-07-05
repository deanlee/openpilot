# distutils: language = c++
# cython: language_level=3
from libcpp.vector cimport vector
from libcpp.string cimport string
from libcpp cimport bool
from libc.stdint cimport uint64_t

cdef extern from "logreader.h":
  cdef cppclass Event:
    uint64_t mono_time

  cdef cppclass LogReader:
    LogReader()
    bool load(string &url)
    vector[Event] events


cdef class LogFileReader:
  cdef LogReader* reader

  def __cinit__(self, fn):
    self.reader = new LogReader()
    self.reader.load(fn.encode('utf8'))

  def __dealloc__(self):
    del self.reader
