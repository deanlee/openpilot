# distutils: language = c++
# cython: language_level = 3

from libcpp.string cimport string
from libcpp cimport bool

cdef extern from "logreader.h":
  cdef cppclass LogReader:
    LogReader()
    bool load(string &url)


cdef class LogFileReader:
  cdef LogReader* reader

  def __init__(self, fn):
    self.reader = new LogReader()
    self.reader.load(fn)

  def __dealloc__(self):
    del self.reader
