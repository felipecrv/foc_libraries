#include <algorithm>
#include <cstdarg>
#include <utility>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#define SQLKIT_IMPLEMENTATION
#include "sqlkit.h"

namespace sqlkit {

TEST_CASE("SQLKit handle lifecycle", "[SQLKit]") {
  SECTION("failure to open database is reported") {
    Handle fail_db;
    int status;

    status = fail_db.open("///");
    REQUIRE(status != SQLITE_OK);
    REQUIRE(!fail_db.isInitialized());

    status = fail_db.close();
    REQUIRE(status == SQLITE_OK);
  }

  SECTION("should not close a database that's not open") {
    Handle empty_db;

    int status = empty_db.close();
    REQUIRE(status == SQLITE_OK);
  }

  SECTION("double closing is a noop") {
    Handle db;
    int status;

    status = db.close();
    REQUIRE(status == SQLITE_OK);

    status = db.close();
    REQUIRE(status == SQLITE_OK);
  }

  SECTION("Handle constructors") {
    int status;
    Handle db1;  // Default constructor
    Handle db2;  // Default constructor
    REQUIRE(!db1.isInitialized());
    REQUIRE(!db2.isInitialized());
    db1 = std::move(*(&db1));  // Handle::operator=(Handle &&)
    db1 = std::move(db2);      // Handle::operator=(Handle &&)
    db2 = std::move(db1);      // Handle::operator=(Handle &&)

    db1.open("test1.db");
    REQUIRE(db1.isInitialized());
    Handle db3(std::move(db1));  // Move constructor
    REQUIRE(!db1.isInitialized());
    REQUIRE(!db2.isInitialized());
    REQUIRE(db3.isInitialized());
    db1 = std::move(db3);  // Handle::operator=(Handle &&)
    REQUIRE(db1.isInitialized());
    REQUIRE(!db2.isInitialized());
    REQUIRE(!db3.isInitialized());

    db2.open("test2.db");
    REQUIRE(db1.isInitialized());
    REQUIRE(db2.isInitialized());
    auto raw1 = db1.raw();
    // The current handle in db2 should be closed
    db2 = std::move(db1);  // Handle::operator=(Handle &&)
    REQUIRE(!db1.isInitialized());
    REQUIRE(db2.isInitialized());
    REQUIRE(db2.raw() == raw1);
  }
}

TEST_CASE("SQLKit API", "[SQLKit]") {
  Handle db;
  db.open("test.db");

  SECTION("Stmt constructors") {
    int status;
    Stmt stmt1;  // Default constructor
    Stmt stmt2;  // Default constructor
    REQUIRE(!stmt1.isInitialized());
    REQUIRE(!stmt2.isInitialized());
    stmt1 = std::move(*(&stmt1));  // Stmt::operator=(Stmt &&)
    stmt1 = std::move(stmt2);      // Stmt::operator=(Stmt &&)
    stmt2 = std::move(stmt1);      // Stmt::operator=(Stmt &&)
    REQUIRE(!stmt1.isInitialized());
    REQUIRE(!stmt2.isInitialized());

    // An idempotent statement
    stmt1 = db.prepare("DROP TABLE IF EXISTS no_table");  // Stmt::operator=(Stmt &&)
    REQUIRE(stmt1.isInitialized());
    status = stmt1.execute(db);
    REQUIRE(status == SQLITE_OK);

    stmt2 = std::move(stmt1);  // Stmt::operator=(Stmt &&)
    REQUIRE(!stmt1.isInitialized());
    REQUIRE(stmt2.isInitialized());

    auto raw2 = stmt2.raw();

    // Move constructor
    Stmt stmt3(std::move(stmt2));
    REQUIRE(!stmt2.isInitialized());
    REQUIRE(stmt3.isInitialized());
    REQUIRE(stmt3.raw() == raw2);

    status = stmt3.execute(db);
    REQUIRE(status == SQLITE_OK);

    stmt3.finalize();
    REQUIRE(!stmt3.isInitialized());

    stmt1 = db.prepare("DROP TABLE IF EXISTS foo");
    stmt2 = db.prepare("DROP TABLE IF EXISTS bar");
    REQUIRE(stmt1.isInitialized());
    REQUIRE(stmt2.isInitialized());

    auto raw1 = stmt1.raw();

    // The sqlite3_stmt in stmt2 should have been finalized otherwise the
    // Handle::close() call in the Fixture will fail.
    stmt2 = std::move(stmt1);
    REQUIRE(!stmt1.isInitialized());
    REQUIRE(stmt2.isInitialized());
    REQUIRE(stmt2.raw() == raw1);
  }

  SECTION("Handle::prepare()") {
    int status;
    status = db.execute("DROP TABLE IF EXISTS bar");
    REQUIRE(status == SQLITE_OK);
    status = db.execute("CREATE TABLE bar(id INT)");
    REQUIRE(status == SQLITE_OK);

    std::string sql = "DROP TABLE bar";
    auto stmt1 = db.prepare(sql.c_str());
    auto stmt2 = db.prepare("DROP TABLE barZZZZ", sql.size());
    auto stmt3 = db.prepare(sql);

    REQUIRE(sql == stmt1.sql());
    REQUIRE(sql == stmt2.sql());
    REQUIRE(sql == stmt3.sql());

    status = stmt1.execute(db);
    REQUIRE(status == SQLITE_OK);
  }

  SECTION("Handle::execute()") {
    int status;
    status = db.execute("DROP TABLE IF EXISTS bar");
    REQUIRE(status == SQLITE_OK);

    std::string sql = "DROP TABLE bar";

    status = db.execute("CREATE TABLE bar(id INT)");
    REQUIRE(status == SQLITE_OK);
    status = db.execute(sql.c_str());
    REQUIRE(status == SQLITE_OK);

    status = db.execute("CREATE TABLE bar(id INT)");
    REQUIRE(status == SQLITE_OK);
    status = db.execute("DROP TABLE barZZZZ", sql.size());
    REQUIRE(status == SQLITE_OK);

    status = db.execute("CREATE TABLE bar(id INT)");
    REQUIRE(status == SQLITE_OK);
    status = db.execute(sql);
    REQUIRE(status == SQLITE_OK);
  }

  SECTION("multi column extraction") {
    {
      auto stmt = db.prepare("SELECT 100");
      db.query(stmt);
      auto cols = stmt.tuple<int>();
      REQUIRE(cols == std::make_tuple(100));
    }

    {
      auto stmt = db.prepare("SELECT 1, 2");
      db.query(stmt);
      auto cols = stmt.tuple<int, int>();
      REQUIRE(cols == std::make_tuple(1, 2));
    }

    {
      auto stmt = db.prepare("SELECT 1, 2, 3");
      db.query(stmt);
      auto cols = stmt.tuple<int, int, int>();
      REQUIRE(cols == std::make_tuple(1, 2, 3));
    }

    {
      auto stmt = db.prepare("SELECT 1, 2, 3, 4");
      db.query(stmt);
      auto cols = stmt.tuple<int, int, int, int>();
      REQUIRE(cols == std::make_tuple(1, 2, 3, 4));
    }

    {
      auto stmt = db.prepare("SELECT 1, 2, 3, 4, 5");
      db.query(stmt);
      auto cols = stmt.tuple<int, int, int, int, int>();
      REQUIRE(cols == std::make_tuple(1, 2, 3, 4, 5));
    }

    {
      auto stmt = db.prepare("SELECT 1, 2, 3, 4, 5, 6");
      db.query(stmt);
      auto cols = stmt.tuple<int, int, int, int, int, int>();
      REQUIRE(cols == std::make_tuple(1, 2, 3, 4, 5, 6));
    }

    {
      auto stmt = db.prepare("SELECT 1, 2, 3, 4, 5, 6, 3 + 4");
      db.query(stmt);
      auto cols = stmt.tuple<int, int, int, int, int, int>();
      REQUIRE(cols == std::make_tuple(1, 2, 3, 4, 5, 6, 7));
    }

    {
      auto stmt = db.prepare("SELECT 1, 2, 3, 4, 5, 6, 3 + 4, 8");
      db.query(stmt);
      auto cols = stmt.tuple<int, int, int, int, int, int, int>();
      REQUIRE(cols == std::make_tuple(1, 2, 3, 4, 5, 6, 7, 8));
    }

    {
      auto stmt = db.prepare("SELECT 1, 2, 3, 4, 5, 6, 3 + 4, 8, 9");
      db.query(stmt);
      auto cols = stmt.tuple<int, int, int, int, int, int, int, int>();
      REQUIRE(cols == std::make_tuple(1, 2, 3, 4, 5, 6, 7, 8, 9));
    }
  }

  SECTION("result set iteration") {
    // Navigate a result set with a single result
    {
      auto stmt = db.prepare("SELECT 100, 'Hundred'");
      for (int s = stmt.query(db); s == SQLITE_ROW; s = stmt.step(db)) {
        auto row = stmt.row();
        REQUIRE(row.nextInt() == 100);
        REQUIRE(row.nextString() == "Hundred");
      }
      REQUIRE(stmt.reset() == SQLITE_OK);

      // Ensure another call to Stmt::query() resets the internal SQLite cursor.

      for (int s = stmt.query(db); s == SQLITE_ROW; s = stmt.step(db)) {
        auto row = stmt.row();
        REQUIRE(row.nextInt() == 100);
        REQUIRE(row.nextString() == "Hundred");
      }
      REQUIRE(stmt.reset() == SQLITE_OK);
    }
    {
      int status;
      status = db.execute("DROP TABLE IF EXISTS numbers");
      status = db.execute("CREATE TABLE numbers(id INT PRIMARY KEY, name TEXT)");
      REQUIRE(status == SQLITE_OK);

      std::vector<std::pair<int, std::string>> numbers;
      numbers.push_back(std::make_pair(1, "one"));
      numbers.push_back(std::make_pair(2, "two"));
      numbers.push_back(std::make_pair(3, "three"));

      Stmt insert = db.prepare("INSERT INTO numbers(id, name) VALUES(?, ?)");
      for (const auto &number : numbers) {
        insert.bind(1, number.first);
        insert.bind(2, number.second);
        status = insert.execute(db);
        REQUIRE(status == SQLITE_OK);
      }

      // Navigate a result set with many results
      std::vector<std::pair<int, std::string>> results;
      Stmt q = db.prepare("SELECT id, name FROM numbers ORDER BY id");
      for (int s = q.query(db); s == SQLITE_ROW; s = q.step(db)) {
        auto row = q.row();
        int id = row.nextInt();
        std::string name = row.nextString();
        results.push_back(std::make_pair(id, name));
      }
      REQUIRE(q.reset() == SQLITE_OK);
      REQUIRE(numbers == results);

      // Navigate an empty result set
      q = db.prepare("SELECT id, name FROM numbers WHERE id > ?");
      q.bind(1, 1000);
      for (int s = q.query(db); s == SQLITE_ROW; s = q.step(db)) {
        REQUIRE(false);
      }
      q.reset();
      // ...again
      q.bind(1, 1000);
      for (int s = q.query(db); s == SQLITE_ROW; s = q.step(db)) {
        REQUIRE(false);
      }
      q.reset();
    }
  }

  SECTION("full API") {
    int status;

    status = db.execute("DROP TABLE IF EXISTS test");
    REQUIRE(status == SQLITE_OK);

    status = db.execute(
        "CREATE TABLE test(\n"
        "  id            INT PRIMARY KEY,\n"
        "  bytes         BLOB,\n"
        "  bytes_static  BLOB,\n"
        "  f64           FLOAT,\n"
        "  i32           INT,\n"
        "  i64           INT,\n"
        "  i_null        INT,\n"
        "  s_size        TEXT,\n"
        "  s_std         TEXT,\n"
        "  s_size_static TEXT,\n"
        "  s_std_static  TEXT,\n"
        "  zeroblob      BLOB\n"
        ")");
    REQUIRE(status == SQLITE_OK);

    const char blob[] = "blob";
    const char zeroblob[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    const char str[] = "Hello, World!";
    const std::string std_str = str;

    Stmt insert = db.prepare(
        "INSERT INTO test VALUES("
        "  :id,"
        "  :bytes,"
        "  :bytes_static,"
        "  :f64,"
        "  :i32,"
        "  :i64,"
        "  :i_null,"
        "  :s_size,"
        "  :s_std,"
        "  :s_size_static,"
        "  :s_std_static,"
        "  :zeroblob"
        ")");
    insert.bind(":id", 1);
    insert.bind(":bytes", blob, sizeof(blob));
    insert.bindStatic(":bytes_static", blob, sizeof(blob));
    insert.bind(":f64", 3.14);
    insert.bind(":i32", (int)5);
    insert.bind(":i64", (int64_t)6);
    insert.bindNull(":i_null");
    insert.bind(":s_size", str, sizeof(str));
    insert.bind(":s_std", std_str);
    insert.bindStatic(":s_size_static", str, sizeof(str));
    insert.bindStatic(":s_std_static", std_str);
    insert.bindZeroblob(":zeroblob", sizeof(zeroblob));

    status = insert.execute(db);
    REQUIRE(status == SQLITE_OK);

    Stmt q = db.prepare("SELECT * FROM test");
    status = q.query(db);
    REQUIRE(status == SQLITE_ROW);
    auto row = q.row();
    REQUIRE(row.nextInt() == 1);  // id
    size_t size = 0;
    auto blob_result = row.nextBlob(&size);  // bytes
    REQUIRE(size == sizeof(blob));
    REQUIRE(memcmp(blob, blob_result, size) == 0);
    blob_result = row.nextBlob(&size);  // bytes_static
    REQUIRE(size == sizeof(blob));
    REQUIRE(memcmp(blob, blob_result, size) == 0);
    REQUIRE(row.nextDouble() == 3.14);  // f64
    REQUIRE(row.nextInt() == 5);        // i32
    REQUIRE(row.nextInt64() == 6);      // i64
    REQUIRE(row.nextIsNull());          // i_null
    row.skip();
    auto cstr = row.nextCStr(&size);  // s_size
    REQUIRE(size == sizeof(str));
    REQUIRE(strcmp(cstr, str) == 0);
    REQUIRE(row.nextString() == std_str);  // s_std
    cstr = row.nextCStr();                 // s_size_static
    REQUIRE(strcmp(cstr, str) == 0);
    REQUIRE(row.nextString() == std_str);  // s_std_static
    blob_result = row.nextBlob(&size);     // zeroblob
    REQUIRE(size == sizeof(zeroblob));
    REQUIRE(memcmp(zeroblob, blob_result, size) == 0);
  }
}

}  // namespace sqlkit
