# To-Do-List REST API
Данный проект представляет собой реализацию RESTful API для управления списком задач (To-Do List). Проект выполнен на языке C++ с использованием микрофреймворка Crow.

Каждая задача (Task) представлена в формате JSON и содержит следующие поля:
● id: Уникальный идентификатор (integer);
● title: Название задачи (string);
● description: Описание задачи (string);
● status: Статус ("todo", "in_progress", "done").

Эндпоинты (Endpoints) и Методы:
● GET /tasks - Получить список всех задач.
● POST /tasks - Создать новую задачу.
● GET /tasks/{id} - Получить одну задачу по ID.
● PUT /tasks/{id} - Полностью обновить задачу по ID.
● DELETE /tasks/{id} - Удалить задачу по ID.

Описание операций (Примеры):
  ● Создание задачи (POST /tasks):
    ○ Запрос: json
{
 "title": "Купить молоко",
 "description": "Обязательно 3.2% жирности",
 "status": "todo"
}
    ○ Ответ (201 Created): json
{
 "id": 1,
 "title": "Купить молоко",
 "description": "Обязательно 3.2% жирности",
 "status": "todo"
}
  ● Получение списка (GET /tasks):
    ○ Ответ (200 OK): json
[
 { "id": 1, "title": "Купить молоко", "status": "todo" },
 { "id": 2, "title": "Запустить API", "status": "in_progress" }
]
● Обновление статуса (PATCH /tasks/{id}):
○ Запрос (PATCH /tasks/2):json
{
 "status": "done"
}
    ○ Ответ (200 OK): json
{
 "id": 2,
 "title": "Запустить API",
 "description": "...",
 "status": "done"
}

Ожидаемые результаты:
● Реализована логика CRUD.
● Используются стандартные HTTP-методы.
● Применяются корректные HTTP- коды (200, 201, 404).
● Формат обмена данными — JSON.
