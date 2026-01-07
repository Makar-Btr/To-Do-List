#include <map>
#include <mutex>
#include <sqlite3.h>
#include <string>
#include <vector>

#include "Common.h"
#include "crow.h"

#ifdef DELETE
#undef DELETE
#endif

struct Task
{
    int id;
    std::string title;
    std::string description;
    std::string status;
};

sqlite3 *db;
std::mutex dbMutex;

bool executeSQL(const std::string &sql)
{
    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQL Error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

void initDB()
{
    int rc = sqlite3_open("todo.db", &db);
    if (rc)
    {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        exit(1);
    }
    else
    {
        std::cout << "Opened database successfully" << std::endl;
    }

    std::string sql = "CREATE TABLE IF NOT EXISTS tasks ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "title TEXT NOT NULL, "
                      "description TEXT, "
                      "status TEXT NOT NULL);";

    if (!executeSQL(sql))
    {
        exit(1);
    }
}

int main()
{
    initDB();

    crow::SimpleApp app;

    CROW_ROUTE(app, "/tasks")
        .methods(crow::HTTPMethod::POST)(
            [](const crow::request &req)
            {
                auto x = crow::json::load(req.body);
                if (!x)
                    return crow::response(400, "Invalid JSON");

                std::string title = x["title"].s();
                std::string desc = x["description"].s();
                std::string status = x["status"].s();

                std::lock_guard<std::mutex> lock(dbMutex);

                sqlite3_stmt *stmt;
                const char *sql = "INSERT INTO tasks (title, description, "
                                  "status) VALUES (?, ?, ?);";

                if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
                {
                    return crow::response(500, sqlite3_errmsg(db));
                }

                sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, desc.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, status.c_str(), -1, SQLITE_STATIC);

                if (sqlite3_step(stmt) != SQLITE_DONE)
                {
                    sqlite3_finalize(stmt);
                    return crow::response(500, "Failed to insert task");
                }

                int newId = (int)sqlite3_last_insert_rowid(db);
                sqlite3_finalize(stmt);

                crow::json::wvalue result;
                result["id"] = newId;
                result["title"] = title;
                result["description"] = desc;
                result["status"] = status;

                return crow::response(201, result);
            });

    CROW_ROUTE(app, "/tasks")
        .methods(crow::HTTPMethod::GET)(
            []()
            {
                std::lock_guard<std::mutex> lock(dbMutex);
                std::vector<crow::json::wvalue> tasksJsonList;

                sqlite3_stmt *stmt;
                const char *sql = "SELECT id, title, status FROM tasks;";

                if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
                {
                    while (sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        crow::json::wvalue t;
                        t["id"] = sqlite3_column_int(stmt, 0);
                        t["title"] = std::string(reinterpret_cast<const char *>(
                            sqlite3_column_text(stmt, 1)));
                        t["status"] =
                            std::string(reinterpret_cast<const char *>(
                                sqlite3_column_text(stmt, 2)));
                        tasksJsonList.push_back(t);
                    }
                }
                sqlite3_finalize(stmt);

                crow::json::wvalue result;
                result = std::move(tasksJsonList);
                return crow::response(200, result);
            });

    CROW_ROUTE(app, "/tasks/<int>")
        .methods(crow::HTTPMethod::GET)(
            [](int id)
            {
                std::lock_guard<std::mutex> lock(dbMutex);
                sqlite3_stmt *stmt;
                const char *sql = "SELECT id, title, description, status FROM "
                                  "tasks WHERE id = ?;";

                crow::json::wvalue result;
                bool found = false;

                if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
                {
                    sqlite3_bind_int(stmt, 1, id);

                    if (sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        found = true;
                        result["id"] = sqlite3_column_int(stmt, 0);
                        result["title"] =
                            std::string(reinterpret_cast<const char *>(
                                sqlite3_column_text(stmt, 1)));

                        const char *descText = reinterpret_cast<const char *>(
                            sqlite3_column_text(stmt, 2));
                        result["description"] =
                            descText ? std::string(descText) : "";

                        result["status"] =
                            std::string(reinterpret_cast<const char *>(
                                sqlite3_column_text(stmt, 3)));
                    }
                }
                sqlite3_finalize(stmt);

                if (!found)
                    return crow::response(404, "Task not found");
                return crow::response(200, result);
            });

    CROW_ROUTE(app, "/tasks/<int>")
        .methods(crow::HTTPMethod::PUT)(
            [](const crow::request &req, int id)
            {
                auto x = crow::json::load(req.body);
                if (!x)
                    return crow::response(400, "Invalid JSON");

                std::string title = x["title"].s();
                std::string desc = x["description"].s();
                std::string status = x["status"].s();

                std::lock_guard<std::mutex> lock(dbMutex);

                sqlite3_stmt *stmt;
                const char *sql = "UPDATE tasks SET title = ?, description = "
                                  "?, status = ? WHERE id = ?;";

                if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
                {
                    return crow::response(500, "DB Error");
                }

                sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, desc.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, status.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 4, id);

                int stepResult = sqlite3_step(stmt);
                sqlite3_finalize(stmt);

                if (sqlite3_changes(db) == 0)
                {
                    return crow::response(404, "Task not found");
                }

                return crow::response(200, "Task updated");
            });

    CROW_ROUTE(app, "/tasks/<int>")
        .methods(crow::HTTPMethod::DELETE)(
            [](int id)
            {
                std::lock_guard<std::mutex> lock(dbMutex);

                sqlite3_stmt *stmt;
                const char *sql = "DELETE FROM tasks WHERE id = ?;";

                if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
                {
                    return crow::response(500, "DB Error");
                }

                sqlite3_bind_int(stmt, 1, id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);

                if (sqlite3_changes(db) == 0)
                {
                    return crow::response(404, "Task not found");
                }

                return crow::response(200, "Task deleted");
            });

    app.port(18080).multithreaded().run();

    sqlite3_close(db);
}