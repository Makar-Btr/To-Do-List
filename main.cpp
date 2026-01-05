#include <map>
#include <mutex>
#include <string>
#include <vector>

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

std::map<int, Task> tasks;
int nextId = 1;
std::mutex tasksMutex;

int main()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/tasks")
        .methods(crow::HTTPMethod::POST)(
            [](const crow::request &req)
            {
                auto x = crow::json::load(req.body);

                if (!x)
                {
                    return crow::response(400, "Invalid JSON");
                }

                std::lock_guard<std::mutex> lock(tasksMutex);

                Task newTask;
                newTask.id = nextId++;
                newTask.title = x["title"].s();
                newTask.description = x["description"].s();
                newTask.status = x["status"].s();

                tasks[newTask.id] = newTask;

                crow::json::wvalue result;
                result["id"] = newTask.id;
                result["title"] = newTask.title;
                result["description"] = newTask.description;
                result["status"] = newTask.status;

                return crow::response(201, result);
            });

    CROW_ROUTE(app, "/tasks")
        .methods(crow::HTTPMethod::GET)(
            []()
            {
                std::lock_guard<std::mutex> lock(tasksMutex);

                std::vector<crow::json::wvalue> tasksJsonList;

                for (const auto &pair : tasks)
                {
                    const Task &task = pair.second;
                    crow::json::wvalue t;
                    t["id"] = task.id;
                    t["title"] = task.title;
                    t["status"] = task.status;
                    tasksJsonList.push_back(t);
                }

                crow::json::wvalue result;
                result = std::move(tasksJsonList);

                return crow::response(200, result);
            });

    CROW_ROUTE(app, "/tasks/<int>")
        .methods(crow::HTTPMethod::GET)(
            [](int id)
            {
                std::lock_guard<std::mutex> lock(tasksMutex);

                if (tasks.find(id) == tasks.end())
                {
                    return crow::response(404, "Task not found");
                }

                const Task &task = tasks[id];
                crow::json::wvalue result;
                result["id"] = task.id;
                result["title"] = task.title;
                result["description"] = task.description;
                result["status"] = task.status;

                return crow::response(200, result);
            });

    CROW_ROUTE(app, "/tasks/<int>")
        .methods(crow::HTTPMethod::PUT)(
            [](const crow::request &req, int id)
            {
                auto x = crow::json::load(req.body);
                if (!x)
                {
                    return crow::response(400, "Invalid JSON");
                }

                std::lock_guard<std::mutex> lock(tasksMutex);

                if (tasks.find(id) == tasks.end())
                {
                    return crow::response(404, "Task not found");
                }

                // Обновляем поля
                tasks[id].title = x["title"].s();
                tasks[id].description = x["description"].s();
                tasks[id].status = x["status"].s();

                return crow::response(200, "Task updated");
            });

    CROW_ROUTE(app, "/tasks/<int>")
        .methods(crow::HTTPMethod::DELETE)(
            [](int id)
            {
                std::lock_guard<std::mutex> lock(tasksMutex);

                if (tasks.erase(id))
                {
                    return crow::response(200, "Task deleted");
                }
                else
                {
                    return crow::response(404, "Task not found");
                }
            });

    app.port(18080).multithreaded().run();
}