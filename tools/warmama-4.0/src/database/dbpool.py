# $Id: dbpool.py,v 4a021ec94450 2010/04/01 12:14:22 jon $

"""
Copyright (c) 2002 Jon Ribbens

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

"""
import weakref as _weakref
import Queue as _Queue
import thread as _thread
import time as _time
import atexit as _atexit


_log_level = 0
_log_name = "/tmp/dbpool.log"
_log_file = None
_log_lock = _thread.allocate_lock()

apilevel = "2.0"
threadsafety = 2

_dbmod = None
_lock = _thread.allocate_lock()
_refs = {}

_COPY_ATTRS = ("paramstyle", "Warning", "Error", "InterfaceError",
  "DatabaseError", "DataError", "OperationalError", "IntegrityError",
  "InternalError", "ProgrammingError", "NotSupportedError")


def _log(level, message, *args, **kwargs):
  global _log_file

  if _log_level >= level:
    if args or kwargs:
      argslist = [repr(arg) for arg in args]
      argslist.extend("%s=%r" % item for item in kwargs.items())
      message += "(" + ", ".join(argslist) + ")"
    _log_lock.acquire()
    try:
      if not _log_file:
        _log_file = open(_log_name, "a", 1)
      _log_file.write("%s %s\n" % (_time.strftime("%b %d %H:%M:%S"), message))
    finally:
      _log_lock.release()


def set_database(dbmod, minconns, timeout=0):
  if minconns < 1:
    raise ValueError("minconns must be greater than or equal to 1")
  if _dbmod is not None:
    if _dbmod is dbmod:
      return
    raise Exception("dbpool module is already in use")
  if len(dbmod.apilevel) != 3 or dbmod.apilevel[:2] != "2." or \
    not dbmod.apilevel[2].isdigit():
    raise ValueError("specified database module is not DB API 2.0 compliant")
  if dbmod.threadsafety < 1:
    raise ValueError("specified database module must have threadsafety level"
      " of at least 1")
  _log(1, "set_database", dbmod.__name__, minconns, timeout)
  g = globals()
  g["_dbmod"] = dbmod
  g["_available"] = {}
  g["_minconns"] = minconns
  g["_timeout"] = timeout
  for v in _COPY_ATTRS:
    g[v] = getattr(dbmod, v)


def connect(*args, **kwargs):
  if _dbmod is None:
    raise Exception("No database module has been specified")
  key = repr(args) + "\0" + repr(kwargs)
  _log(1, "connect", *args, **kwargs)
  try:
    while True:
      conn = _available[key].get(0)
      if _timeout == 0 or _time.time() - conn._lastuse < _timeout:
        _log(2, "connect: returning connection %r from _available" % conn)
        return conn
      else:
        conn._inner._connection = None
        _log(2, "connect: discarded connection %r from _available due to age" %
          conn)
  except (KeyError, _Queue.Empty):
    conn = _Connection(None, None, *args, **kwargs)
    _log(2, "connect: created new connection %r" % conn)
    return conn


def _make_available(conn):
  key = repr(conn._args) + "\0" + repr(conn._kwargs)
  _log(2, "_make_available", conn)
  _lock.acquire()
  try:
    try:
      _available[key].put(conn, 0)
      _log(3, "_make_available: put into existing _available slot")
    except KeyError:
      _log(3, "_make_available: created new _available slot")
      q = _Queue.Queue(_minconns)
      q.put(conn, 0)
      _available[key] = q
    except _Queue.Full:
      conn._inner._connection = None
      _log(3, "_make_available: discarded, _available slot full")
  finally:
    _lock.release()


def _connection_notinuse(ref):
  # if the Python interpreter is exiting, the globals might already have
  # been deleted, so check for them explicitly
  if _refs is None:
    return
  inner = _refs[ref]
  del _refs[ref]
  inner._cursorref = None
  if inner._connection is not None:
    if _make_available is not None and _Connection is not None:
      _make_available(_Connection(inner))


class _Connection(object):
  def __init__(self, inner, *args, **kwargs):
    self._inner = None
    _log(4, "_Connection", self, inner, *args, **kwargs)
    if inner is None:
      self._inner = _InnerConnection(*args, **kwargs)
      _log(5, "_Connection: new inner=%r" % self._inner)
    else:
      self._inner = inner
    self._inner._outerref = _weakref.ref(self)
    ref = _weakref.ref(self, _connection_notinuse)
    _log(5, "_Connection: ref=%r" % ref)
    _refs[ref] = self._inner

  def __repr__(self):
    return "<dbpool._Connection(%r) at %x>" % (self._inner, id(self))

  def cursor(self, *args, **kwargs):
    # this method would not be necessary (i.e. the __getattr__ would take
    # care of it) but if someone does dbpool.connect().cursor() all in one
    # expression, the outer _Connection class was getting garbage-collected
    # (and hence the actual database connection being put back in the pool)
    # *in the middle of the expression*, i.e. after connect() was called but
    # before cursor() was called. So you could end up with 2 cursors on the
    # same database connection.
    return self._inner.cursor(*args, **kwargs)

  def __getattr__(self, attr):
    return getattr(self._inner, attr)

    
class _InnerConnection(object):
  def __init__(self, connection, *args, **kwargs):
    self._connection = None
    _log(4, "_InnerConnection", self, connection, *args, **kwargs)
    self._args = args
    self._kwargs = kwargs
    if connection is None:
      _log(2, "_InnerConnection: Calling actual connect", *args, **kwargs)
      self._connection = _dbmod.connect(*args, **kwargs)
    else:
      _log(5, "_InnerConnection: Re-using connection %r" % connection)
      self._connection = connection
    self._cursorref = None
    self._outerref = None
    self._lock = _thread.allocate_lock()
    self._lastuse = _time.time()

  def __repr__(self):
    return "<dbpool._InnerConnection(%r) at %x>" % (self._connection, id(self))

  def close(self):
    _log(3, "_Connection.close", self)
    if self._cursorref is not None:
      c = self._cursorref()
      if c is not None:
        _log(4, "_Connection.close: closing cursor %r" % c)
        c.close()
    self._cursorref = None
    self._outerref = None
    conn = self._connection
    if conn:
      self._connection = None
      if _make_available is not None:
        _make_available(_Connection(None, conn, *self._args, **self._kwargs))

  def __getattr__(self, attr):
    return getattr(self._connection, attr)

  def cursor(self, *args, **kwargs):
    _log(3, "cursor", self, *args, **kwargs)
    self._lock.acquire()
    try:
      if self._cursorref is None or self._cursorref() is None:
        c = _Cursor(self, *args, **kwargs)
        self._cursorref = _weakref.ref(c)
        self._lastuse = _time.time()
        return c
    finally:
      self._lock.release()
    _log(3, "cursor: creating new connection")
    return connect(*self._args, **self._kwargs).cursor(*args, **kwargs)


class _Cursor(object):
  def __init__(self, connection, *args, **kwargs):
    self._cursor = None
    _log(4, "_Cursor", connection, *args, **kwargs)
    self._connection = connection
    self._outer = connection._outerref()
    self._cursor = connection._connection.cursor(*args, **kwargs)
  
  def __repr__(self):
    return "<dbpool._Cursor(%r) at %x>" % (self._cursor, id(self))

  def close(self):
    _log(4, "_Cursor.close", self)
    self._connection._cursorref = None
    self._connection = None
    self._cursor.close()
    self._outer = None

  def __getattr__(self, attr):
    return getattr(self._cursor, attr)


def _exiting():
  global _make_available
  _make_available = None

_atexit.register(_exiting)
