#include "crow.h"
#include "Common.h"


sqlite3 *db;
std::mutex dbMutex;
MemoryCache cache;

bool executeSQL(const std::string &sql)
{
    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg);
    if (rc != SQLITE_OK)
    {
        Logger::error(std::string("SQL Error: ") + errMsg);
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
        Logger::error(std::string("Can't open database: ") +
                      sqlite3_errmsg(db));
        exit(1);
    }
    else
    {
        Logger::info("Opened database successfully");
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

    CROW_ROUTE(app, "/metrics") ([]() { return Metrics::toJson(); });

    CROW_ROUTE(app, "/tasks")
        .methods(crow::HTTPMethod::POST)(
            [](const crow::request &req)
            {
                Metrics::incrementRequests();
                Logger::logRequest("POST", "/tasks");

                auto x = crow::json::load(req.body);
                if (!x)
                {
                    Metrics::recordError();
                    Logger::error("Invalid JSON provided in POST");
                    return crow::response(400, "Invalid JSON");
                }

                std::string title = x["title"].s();
                std::string desc = x["description"].s();
                std::string status = x["status"].s();

                std::lock_guard<std::mutex> lock(dbMutex);

                sqlite3_stmt *stmt;
                const char *sql = "INSERT INTO tasks (title, description, "
                                  "status) VALUES (?, ?, ?);";

                if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
                {
                    Metrics::recordError();
                    Logger::error("DB Prepare Error: " +
                                  std::string(sqlite3_errmsg(db)));
                    return crow::response(500, sqlite3_errmsg(db));
                }

                sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, desc.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, status.c_str(), -1, SQLITE_STATIC);

                if (sqlite3_step(stmt) != SQLITE_DONE)
                {
                    sqlite3_finalize(stmt);
                    Metrics::recordError();
                    Logger::error("Failed to insert task");
                    return crow::response(500, "Failed to insert task");
                }

                int newId = (int)sqlite3_last_insert_rowid(db);
                sqlite3_finalize(stmt);

                cache.clear();

                crow::json::wvalue result;
                result["id"] = newId;
                result["title"] = title;
                result["description"] = desc;
                result["status"] = status;

                Metrics::recordSuccess();
                Logger::info("Task created successfully with ID: " +
                             std::to_string(newId));
                return crow::response(201, result);
            });

    CROW_ROUTE(app, "/tasks")
        .methods(crow::HTTPMethod::GET)(
            []()
            {
                Metrics::incrementRequests();
                Logger::logRequest("GET", "/tasks");

                std::string cacheKey = "ALL_TASKS";
                auto cachedValue = cache.get(cacheKey);
                if (cachedValue)
                {
                    Metrics::recordSuccess();
                    crow::response res(200, *cachedValue);
                    res.set_header("Content-Type", "application/json");
                    return res;
                }

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

                std::string jsonStr = result.dump();
                cache.set(cacheKey, jsonStr);

                Metrics::recordSuccess();
                return crow::response(200, result);
            });

    CROW_ROUTE(app, "/tasks/<int>")
        .methods(crow::HTTPMethod::GET)(
            [](int id)
            {
                Metrics::incrementRequests();
                Logger::logRequest("GET", "/tasks/" + std::to_string(id));

                std::string cacheKey = "TASK_" + std::to_string(id);
                auto cachedValue = cache.get(cacheKey);
                if (cachedValue)
                {
                    Metrics::recordSuccess();
                    crow::response res(200, *cachedValue);
                    res.set_header("Content-Type", "application/json");
                    return res;
                }

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
                {
                    Metrics::recordError();
                    Logger::info("Task not found ID: " + std::to_string(id));
                    return crow::response(404, "Task not found");
                }

                cache.set(cacheKey, result.dump());

                Metrics::recordSuccess();
                return crow::response(200, result);
            });

    CROW_ROUTE(app, "/tasks/<int>")
        .methods(crow::HTTPMethod::PUT)(
            [](const crow::request &req, int id)
            {
                Metrics::incrementRequests();
                Logger::logRequest("PUT", "/tasks/" + std::to_string(id));

                auto x = crow::json::load(req.body);
                if (!x)
                {
                    Metrics::recordError();
                    return crow::response(400, "Invalid JSON");
                }

                std::string title = x["title"].s();
                std::string desc = x["description"].s();
                std::string status = x["status"].s();

                std::lock_guard<std::mutex> lock(dbMutex);

                sqlite3_stmt *stmt;
                const char *sql = "UPDATE tasks SET title = ?, description = "
                                  "?, status = ? WHERE id = ?;";

                if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
                {
                    Metrics::recordError();
                    return crow::response(500, "DB Error");
                }

                sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, desc.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, status.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 4, id);

                sqlite3_step(stmt);
                sqlite3_finalize(stmt);

                if (sqlite3_changes(db) == 0)
                {
                    Metrics::recordError();
                    return crow::response(404, "Task not found");
                }

                cache.clear();

                Metrics::recordSuccess();
                Logger::info("Task updated ID: " + std::to_string(id));
                return crow::response(200, "Task updated");
            });

    CROW_ROUTE(app, "/tasks/<int>")
        .methods(crow::HTTPMethod::DELETE)(
            [](int id)
            {
                Metrics::incrementRequests();
                Logger::logRequest("DELETE", "/tasks/" + std::to_string(id));

                std::lock_guard<std::mutex> lock(dbMutex);

                sqlite3_stmt *stmt;
                const char *sql = "DELETE FROM tasks WHERE id = ?;";

                if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
                {
                    Metrics::recordError();
                    return crow::response(500, "DB Error");
                }

                sqlite3_bind_int(stmt, 1, id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);

                if (sqlite3_changes(db) == 0)
                {
                    Metrics::recordError();
                    return crow::response(404, "Task not found");
                }

                cache.clear();

                Metrics::recordSuccess();
                Logger::info("Task deleted ID: " + std::to_string(id));
                return crow::response(200, "Task deleted");
            });

    app.port(18080).multithreaded().run();

    sqlite3_close(db);
}