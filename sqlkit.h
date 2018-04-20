// vim:foldenable fdm=marker
#pragma once

#include <cassert>
#include <string>

#ifndef _SQLITE3_H_
#include "sqlite3.h"
#endif

namespace sqlkit {

namespace detail {

// The `ColumnExtractor`s take a template type parameter and a 0-based index to
// call the correct sqlite3 function that extract the column value from the
// `sqlite3_stmt`.
//
// See https://www.sqlite.org/c3ref/column_blob.html for more information.
//
// T                     | sqlite3 API
// ----------------------+----------------------
// const void *          | sqlite3_column_blob
// double                | sqlite3_column_double
// int                   | sqlite3_column_int
// int64_t               | sqlite3_column_int64
// const unsigned char * | sqlite3_column_text
// const char *          | sqlite3_column_text
// std::string           | sqlite3_column_text
// sqlite3_value *       | sqlite3_column_value   https://www.sqlite.org/c3ref/value.html
//
// There's no type for sqlite3_column_text16 (UTF-16 strings). Special functions
// in the Stmt and PositionedRow classes are provided for it.
//
// 'ColumnExtractor's {{{

template <typename T>
struct ColumnExtractor {
  T operator()(sqlite3_stmt *stmt, unsigned int i);
};

template <>
struct ColumnExtractor<const void *> {
  const void *operator()(sqlite3_stmt *stmt, unsigned int i) {
    // Return value may be nullptr if column is NULL
    return sqlite3_column_blob(stmt, i);
  }
};

template <>
struct ColumnExtractor<double> {
  double operator()(sqlite3_stmt *stmt, unsigned int i) {
    assert(sqlite3_column_type(stmt, i) == SQLITE_FLOAT && "Column is not SQLITE_FLOAT or is NULL");
    return sqlite3_column_double(stmt, i);
  }
};

template <>
struct ColumnExtractor<int> {
  int operator()(sqlite3_stmt *stmt, unsigned int i) {
    assert(sqlite3_column_type(stmt, i) == SQLITE_INTEGER &&
           "Column is not SQLITE_INTEGER or is NULL");
    return sqlite3_column_int(stmt, i);
  }
};

template <>
struct ColumnExtractor<int64_t> {
  int64_t operator()(sqlite3_stmt *stmt, unsigned int i) {
    assert(sqlite3_column_type(stmt, i) == SQLITE_INTEGER &&
           "Column is not SQLITE_INTEGER or is NULL");
    return sqlite3_column_int64(stmt, i);
  }
};

template <>
struct ColumnExtractor<const unsigned char *> {
  const unsigned char *operator()(sqlite3_stmt *stmt, unsigned int i) {
    // Return value may be nullptr if column is NULL
    return sqlite3_column_text(stmt, i);
  }
};

template <>
struct ColumnExtractor<const char *> {
  const char *operator()(sqlite3_stmt *stmt, unsigned int i) {
    // Return value may be nullptr if column is NULL
    return (const char *)sqlite3_column_text(stmt, i);
  }
};

template <>
struct ColumnExtractor<std::string> {
  std::string operator()(sqlite3_stmt *stmt, unsigned int i) {
    const char *cstr = (const char *)sqlite3_column_text(stmt, i);
    assert(cstr != nullptr && "Column value is NULL");
    return std::string(cstr ? cstr : "");
  }
};

template <>
struct ColumnExtractor<sqlite3_value *> {
  sqlite3_value *operator()(sqlite3_stmt *stmt, unsigned int i) {
    // Return value may be nullptr if column is NULL
    return sqlite3_column_value(stmt, i);
  }
};

// }}}

}  // namespace detail

class Handle;
class PositionedRow;

class Stmt {
 public:
  Stmt() : _handle(nullptr) {}

  Stmt(Stmt &&rhs) : _handle(rhs._handle) { rhs._handle = nullptr; }

  Stmt &operator=(Stmt &&rhs) {
    if (this != &rhs) {
      if (_handle != rhs._handle) {
        finalize();
      }
      _handle = rhs._handle;
      rhs._handle = nullptr;
    }
    return *this;
  }

  ~Stmt() noexcept { finalize(); }

  bool isInitialized() const { return _handle != nullptr; }
  sqlite3_stmt *raw() { return _handle; };

  const char *sql() const { return sqlite3_sql(_handle); }

  // Called by the destructor.
  //
  // https://www.sqlite.org/c3ref/finalize.html
  int finalize() {
    // Invoking sqlite3_finalize() on a NULL pointer is a harmless no-op.
    int status = sqlite3_finalize(_handle);
    if (status != SQLITE_OK) {
#ifndef NDEBUG
      fprintf(stderr, "sqlkit: Something went wrong while finalizing prepared statement");
#endif
    }
    // From https://www.sqlite.org/c3ref/finalize.html
    //
    // The application must finalize every prepared statement in order to avoid
    // resource leaks. It is a grievous error for the application to try to use a
    // prepared statement after it has been finalized. Any use of a prepared
    // statement after it has been finalized can result in undefined and
    // undesirable behavior such as segfaults and heap corruption.
    _handle = nullptr;
    return status;
  }

  // Binding API {{{

  // 1-based index of binding parameter named `param`.
  //
  // @return 0 if parameter is not found
  unsigned int index(const char *param) {
    unsigned int i = (unsigned int)sqlite3_bind_parameter_index(_handle, param);
    if (i < 1) {
#ifndef NDEBUG
      fprintf(stderr, "sqlkit: Couldn't find binding parameter: %s", param);
#endif
    }
    return i;
  }

  // method       | typeof(value)         | sqlite3 API
  // -------------+-----------------------+----------------------
  // bind         | const void *          | sqlite3_bind_blob64
  // bindStatic   | const void *          | sqlite3_bind_blob64
  // bind         | double                | sqlite3_bind_double
  // bind         | int                   | sqlite3_bind_int
  // bind         | int64_t               | sqlite3_bind_int64
  // bindNull     | N/A                   | sqlite3_bind_null
  // bind         | const char *          | sqlite3_bind_text
  // bind         | std::string           | sqlite3_bind_text
  // bindStatic   | const char *          | sqlite3_bind_text
  // bindStatic   | std::string           | sqlite3_bind_text
  // bind         | sqlite3_value *       | sqlite3_bind_value
  // bindZeroblob | N/A                   | sqlite3_bind_zeroblob64
  //
  // `sqlite3_bind_text16` and `sqlite3_bind_text64` are not covered.
  // `sqlite3_bind_blob` and `sqlite3_bind_zeroblob` are not used in favor of
  // `sqlite3_bind_blob64` and `sqlite3_bind_zeroblob64`.

  // Bind BLOB

  int bind(unsigned int i, const void *value, size_t size) {
    int status = sqlite3_bind_blob64(_handle, i, value, size, SQLITE_TRANSIENT);
    assert(status == SQLITE_OK);
    return status;
  }

  int bind(const char *param, const void *value, size_t size) {
    return bind(index(param), value, size);
  }

  int bindStatic(unsigned int i, const void *value, size_t size) {
    int status = sqlite3_bind_blob64(_handle, i, value, size, SQLITE_STATIC);
    assert(status == SQLITE_OK);
    return status;
  }

  int bindStatic(const char *param, const void *value, size_t size) {
    return bindStatic(index(param), value, size);
  }

  // Bind double

  int bind(unsigned int i, double value) {
    int status = sqlite3_bind_double(_handle, i, value);
    assert(status == SQLITE_OK);
    return status;
  }

  int bind(const char *param, double value) { return bind(index(param), value); }

  // Bind int

  int bind(unsigned int i, int value) {
    int status = sqlite3_bind_int(_handle, i, value);
    assert(status == SQLITE_OK);
    return status;
  }

  int bind(const char *param, int value) { return bind(index(param), value); }

  // Bind int64

  int bind(unsigned int i, int64_t value) {
    int status = sqlite3_bind_int64(_handle, i, value);
    assert(status == SQLITE_OK);
    return status;
  }

  int bind(const char *param, int64_t value) { return bind(index(param), value); }

  // Bind NULL

  int bindNull(unsigned int i) {
    int status = sqlite3_bind_null(_handle, i);
    assert(status == SQLITE_OK);
    return status;
  }

  int bindNull(const char *param) { return bindNull(index(param)); }

  // Bind const char *

  int bind(unsigned int i, const char *value, size_t size) {
    int status = sqlite3_bind_text(_handle, i, value, size, SQLITE_TRANSIENT);
    assert(status == SQLITE_OK);
    return status;
  }

  int bindStatic(unsigned int i, const char *value, size_t size) {
    int status = sqlite3_bind_text(_handle, i, value, size, SQLITE_STATIC);
    assert(status == SQLITE_OK);
    return status;
  }

  int bind(const char *param, const char *value, size_t size) {
    return bind(index(param), value, size);
  }

  int bindStatic(const char *param, const char *text, size_t size) {
    return bindStatic(index(param), text, size);
  }

  // Bind std::string

  int bind(unsigned int i, const std::string &value) {
    return bind(i, value.c_str(), value.size() + 1);
  }

  int bindStatic(unsigned int i, const std::string &value) {
    return bindStatic(i, value.c_str(), value.size() + 1);
  }

  int bind(const char *param, const std::string &value) { return bind(index(param), value); }

  int bindStatic(const char *param, const std::string &value) {
    return bindStatic(index(param), value);
  }

  // Bind const sqlite3_value *

  // TODO: test
  int bind(unsigned int i, const sqlite3_value *value) {
    int status = sqlite3_bind_value(_handle, i, value);
    assert(status == SQLITE_OK);
    return status;
  }

  int bind(const char *param, const sqlite3_value *value) { return bind(index(param), value); }

  // Bind zeroed BLOB

  int bindZeroblob(unsigned int i, size_t size) {
    int status = sqlite3_bind_zeroblob64(_handle, i, size);
    assert(status == SQLITE_OK);
    return status;
  }

  int bindZeroblob(const char *param, size_t size) { return bindZeroblob(index(param), size); }

  // }}}

  // Result extraction API {{{

  // Get the number of columns in the result set
  unsigned int numColumns() { return sqlite3_column_count(_handle); }

  // Byte size methods

  int blobColumnSize(unsigned int i) { return sqlite3_column_bytes(_handle, i); }
  int utf8ColumnSizeInBytes(unsigned int i) { return sqlite3_column_bytes(_handle, i); }
  int utf16ColumnSizeInBytes(unsigned int i) { return sqlite3_column_bytes16(_handle, i); }

  // Get the default datatype of the column in the result:
  //
  // SQLITE_INTEGER  1
  // SQLITE_FLOAT    2
  // SQLITE_BLOB     4
  // SQLITE_NULL     5
  // SQLITE_TEXT     3
  //
  // See https://www.sqlite.org/c3ref/c_blob.html
  int columnType(unsigned int i) { return sqlite3_column_type(_handle, i); }

  bool columnIsNull(unsigned int i) { return sqlite3_column_type(_handle, i) == SQLITE_NULL; }

  const char *columnName(unsigned int i) {
    assert(i < numColumns());
    return sqlite3_column_name(_handle, i);
  }

  const void *columnNameUtf16(unsigned int i) {
    assert(i < numColumns());
    return sqlite3_column_name16(_handle, i);
  }

  // Extract value of a single column.
  //
  // column() and column16() take a 0-based index.

  template <typename T>
  T column(unsigned int i) {
    assert(i < numColumns());
    detail::ColumnExtractor<T> extract;
    return extract(_handle, i);
  }

  const void *columnUtf16(unsigned int i) {
    assert(i < numColumns());
    return sqlite3_column_text16(_handle, i);
  }

  // Extract value of all columns as a tuple

  template <typename T0>
  std::tuple<T0> tuple() {
    assert(1 <= numColumns() && "Trying to extract too many columns");
    auto _0 = column<T0>(0);
    return std::make_tuple(_0);
  }

  template <typename T0, typename T1>
  std::tuple<T0, T1> tuple() {
    assert(2 <= numColumns() && "Trying to extract too many columns");
    auto _0 = column<T0>(0);
    auto _1 = column<T1>(1);
    return std::make_tuple(_0, _1);
  }

  template <typename T0, typename T1, typename T2>
  std::tuple<T0, T1, T2> tuple() {
    assert(3 <= numColumns() && "Trying to extract too many columns");
    auto _0 = column<T0>(0);
    auto _1 = column<T1>(1);
    auto _2 = column<T2>(2);
    return std::make_tuple(_0, _1, _2);
  }

  template <typename T0, typename T1, typename T2, typename T3>
  std::tuple<T0, T1, T2, T3> tuple() {
    assert(4 <= numColumns() && "Trying to extract too many columns");
    auto _0 = column<T0>(0);
    auto _1 = column<T1>(1);
    auto _2 = column<T2>(2);
    auto _3 = column<T3>(3);
    return std::make_tuple(_0, _1, _2, _3);
  }

  template <typename T0, typename T1, typename T2, typename T3, typename T4>
  std::tuple<T0, T1, T2, T3, T4> tuple() {
    assert(5 <= numColumns() && "Trying to extract too many columns");
    auto _0 = column<T0>(0);
    auto _1 = column<T1>(1);
    auto _2 = column<T2>(2);
    auto _3 = column<T3>(3);
    auto _4 = column<T4>(4);
    return std::make_tuple(_0, _1, _2, _3, _4);
  }

  // We use variadic templates (more complicated to debug when things go awry)
  // to support any number of parameters.
  template <typename T0,
            typename T1,
            typename T2,
            typename T3,
            typename T4,
            typename T5,
            typename... TailTypes>
  std::tuple<T0, T1, T2, T3, T4, T5, TailTypes...> tuple() {
    assert(6 + sizeof...(TailTypes) <= numColumns() && "Trying to extract too many columns");
    typedef std::tuple<T0, T1, T2, T3, T4, T5, TailTypes...> TupleType;
    TupleType tuple;
    tailColumns<TupleType, 0, T0, T1, T2, T3, T4, T5, TailTypes...>(tuple);
    return tuple;
  }

 private:
  template <typename TupleType, unsigned int i>
  void tailColumns(TupleType &) {}

  template <typename TupleType, unsigned int i, typename T, typename... TailTypes>
  void tailColumns(TupleType &tuple) {
    detail::ColumnExtractor<T> extract;
    std::get<i>(tuple) = extract(_handle, i);
    tailColumns<TupleType, i + 1, TailTypes...>(tuple);
  }

  // }}}

 public:
  int execute(Handle &db);
  int query(Handle &db);
  int step(Handle &db);
  PositionedRow row();

  int reset() { return sqlite3_reset(_handle); }

  int clearBindings() {
    int status = sqlite3_clear_bindings(_handle);
    assert(status == SQLITE_OK);
    return status;
  }

 private:
  sqlite3_stmt *_handle;

  // Disallow copy constructors
  Stmt(const Stmt &);
  void operator=(const Stmt &);

  // Allow database Handle methods to get access to the sqlite3_stmt handle.
  friend class Handle;
};

class PositionedRow {
 public:
  explicit PositionedRow(Stmt *stmt) : _stmt(stmt), _pos(0) {}

  bool nextIsNull() { return _stmt->columnIsNull(_pos); }

  template <typename T>
  T next() {
    return _stmt->column<T>(_pos++);
  }

  const void *nextBlob(size_t *size) {
    if (size) {
      *size = _stmt->blobColumnSize(_pos);
    }
    return next<const void *>();
  }

  double nextDouble() {
    assert(!nextIsNull());
    return next<double>();
  }

  int nextInt() {
    assert(!nextIsNull());
    return next<int>();
  }

  int64_t nextInt64() {
    assert(!nextIsNull());
    return next<int64_t>();
  }

  std::string nextString() {
    assert(!nextIsNull());
    return next<std::string>();
  }

  // Return value may be nullptr
  const char *nextCStr(size_t *size) {
    if (size) {
      *size = _stmt->blobColumnSize(_pos);
    }
    return next<const char *>();
  }

  // Return value may be nullptr
  const char *nextCStr() { return nextCStr(nullptr); }

  // Return value may be nullptr
  const sqlite3_value *nextValue() { return next<sqlite3_value *>(); }

  unsigned int currentPos() const { return _pos; }
  unsigned int numColumns() { return _stmt->numColumns(); }
  bool hasMoreColumns() { return _pos < _stmt->numColumns(); }
  void restart() { _pos = 0; }

  void skip() {
    assert(_pos < _stmt->numColumns());
    _pos++;
  }

  void rewind() {
    assert(_pos > 0);
    if (_pos > 0) {
      _pos--;
    }
  }

  sqlite3_stmt *rawStmt() { return _stmt->raw(); }

 private:
  Stmt *_stmt;
  unsigned int _pos;
};

class Handle {
 public:
  Handle() noexcept : _handle(nullptr) {}
  Handle(Handle &&other) noexcept : _handle(other._handle) { other._handle = nullptr; }

  Handle &operator=(Handle &&rhs) {
    if (this != &rhs) {
      if (_handle != rhs._handle) {
        if (_handle) {
          close();
        }
        _handle = rhs._handle;
      }
      rhs._handle = nullptr;
    }
    return *this;
  }

  ~Handle() noexcept {
    if (_handle) {
      close();
    }
  }

  int open(const char *filename) {
    int status = sqlite3_open(filename, &_handle);
    if (status != SQLITE_OK) {
      // TODO: log debug
      /*
      assert(false && "Couldn't open/create database! filename=(%s), error=(%s)",
              filename,
              sqlite3_errmsg(_handle));
      */
      close();
    }
    return status;
  }

  int close() {
    int status = sqlite3_close(_handle);
    if (status == SQLITE_OK) {
      _handle = nullptr;
    } else {
#ifndef NDEBUG
      fprintf(stderr,
              "sqlkit: Something wrong happened when trying to close database! (%s)",
              lastErrorMessage());
#endif
    }
    return status;
  }

  bool isInitialized() const { return _handle != nullptr; }
  sqlite3 *raw() { return _handle; }

  // Prepare (compile) statements

  Stmt prepare(const char *sql) { return prepare(sql, -1); }

  Stmt prepare(const std::string &sql) { return prepare(sql.c_str(), sql.size()); }

  Stmt prepare(const char *sql, int num_sql_bytes) {
    Stmt stmt;
    int status = sqlite3_prepare_v2(_handle, sql, num_sql_bytes, &stmt._handle, nullptr);

    if (status != SQLITE_OK) {
#ifndef NDEBUG
      fprintf(stderr,
              "sqlkit: Failed to prepare \"%s\". error_code %d, error_msg \"%s\"",
              sql,
              status,
              lastErrorMessage());
#endif
    }

    return stmt;
  }

  // Execute result-less statements

  int execute(const char *sql, int num_sql_bytes) {
    Stmt stmt = prepare(sql, num_sql_bytes);
    return execute(stmt);
  }

  int execute(const char *sql) { return execute(sql, -1); }
  int execute(const std::string &sql) { return execute(sql.c_str(), sql.size()); }

  int execute(Stmt &stmt) {
    int status = sqlite3_step(stmt._handle);

    assert(status != SQLITE_ROW && stmt.numColumns() == 0 &&
           "Use query() for queries and execute() for result-less statements.");

    switch (status) {
      case SQLITE_OK:
      case SQLITE_DONE:
      case SQLITE_ROW:
        break;
      default:
#ifndef NDEBUG
        fprintf(stderr, "sqlkit: Failed to execute prepared statement: %s", lastErrorMessage());
#endif
        break;
    }

    // sqlite3_reset() the statement when done, otherwise subsequent calls to
    // sqlite3_bind_* will crash.
    return sqlite3_reset(stmt._handle);
  }

  // Perform queries and step through query results

  int query(Stmt &stmt) {
    int status = sqlite3_step(stmt._handle);

    assert(status != SQLITE_OK && stmt.numColumns() > 0 &&
           "Use execute() instead of query() for result-less statements.");

    switch (status) {
      case SQLITE_OK:
      case SQLITE_DONE:
      case SQLITE_ROW:
        break;
      default:
#ifndef NDEBUG
        fprintf(stderr, "sqlkit: Failed to perform query: %s", lastErrorMessage());
#endif
        break;
    }
    return status;
  }

  int step(Stmt &stmt) {
    int status = sqlite3_step(stmt._handle);
    switch (status) {
      case SQLITE_OK:
      case SQLITE_DONE:
      case SQLITE_ROW:
        break;
      default:
#ifndef NDEBUG
        fprintf(stderr, "sqlkit: Failed to step through query results: %s", lastErrorMessage());
#endif
        break;
    }
    return status;
  }

  int64_t lastInsertRowId() {
    // If a separate thread performs a new INSERT on the same database
    // handle while the sqlite3_last_insert_rowid() function is
    // running and thus changes the last insert rowid, then the value
    // returned by sqlite3_last_insert_rowid() is unpredictable and might
    // not equal either the old or the new last insert rowid.
    return sqlite3_last_insert_rowid(_handle);
  }

  // Utility methods

  const char *lastErrorMessage() { return sqlite3_errmsg(_handle); }

  int vacuum() {
    auto stmt = prepare("VACUUM;");
    return execute(stmt);
  }

 private:
  sqlite3 *_handle;  //> SQLite3 database handle

  // Disallow copy constructors
  Handle(const Handle &);
  void operator=(const Handle &);
};

#ifdef SQLKIT_IMPLEMENTATION

int Stmt::execute(Handle &db) { return db.execute(*this); }

int Stmt::query(Handle &db) { return db.query(*this); }

int Stmt::step(Handle &db) { return db.step(*this); }

PositionedRow Stmt::row() { return PositionedRow{this}; }

#endif  // SQLKIT_IMPLEMENTATION

}  // namespace sqlkit
