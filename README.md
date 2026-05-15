# Сервер программы по отрисовке графиков параметризованной функции

Сервер написан на C++/Qt, хранит пользователей в стандартном PostgreSQL через ODB ORM и ходит во внешний
почтовый сервис для восстановления пароля. В Docker Compose поднимаются только
сервер и база данных. Почтовый сервис задаётся URL-адресом и должен быть
запущен отдельно. Таблицы и расширение `pgcrypto` создаёт сам сервер при
старте, поэтому отдельные SQL-скрипты для схемы запускать не нужно. Запросы
на восстановление пароля не удаляются из базы:
для аудита у них сохраняются даты создания, замены, отмены, ошибки отправки,
верификации и успешного использования.

## Быстрый запуск через Docker

1. Установите Docker Desktop (если домашний ПК) или docker (если сервер).

2. Откройте терминал в папке проекта.

3. Создайте файл настроек:

   ```bash
   cp .env.example .env
   ```

   Если вы запускаете команды в Windows PowerShell:

   ```powershell
   Copy-Item .env.example .env
   ```

4. Откройте `.env` обычным текстовым редактором.
   Для первого запуска можно оставить значения как есть. Если у вас уже есть
   почтовый сервис, поменяйте `MAIL_SERVICE_BASE_URL` на его адрес.

5. Соберите и запустите проект:

   ```bash
   docker compose up --build -d
   ```

6. Проверьте, что оба контейнера работают:

   ```bash
   docker compose ps
   ```

   У `graphing-server-server-1` и `graphing-server-db-1` должен быть статус
   `healthy`.

Сервер будет доступен на TCP-порту `13579` вашего компьютера.
PostgreSQL будет доступен только локально на `127.0.0.1:5432`.

## Проверка сервера

После запуска можно отправить пару тестовых команд. На Linux, macOS или в WSL:

```bash
{
  printf 'request 1 register docker_user|secret123|Docker User|docker_user@example.com\n'
  sleep 1
  printf 'request 2 calculate 1|2|3\n'
  sleep 1
} | nc -w 5 127.0.0.1 13579
```

Ожидаемый результат: первая строка `response 1 ok`, вторая строка
`response 2 ok ...` с рассчитанными точками графика.

Если пользователь уже существует, поменяйте `docker_user` и email на любые
другие значения.

## Почтовый сервис

Compose-файл специально не запускает почтовый сервис. Серверу нужен внешний
HTTP-сервис с endpoint:

```text
POST <MAIL_SERVICE_BASE_URL>/message?from=<user>&to=<email>
Content-Type: application/json
```

Тело запроса:

```json
{
  "title": "Password reset code",
  "content": "text version",
  "html_content": "html version",
  "sender_alias": "Graphing Server"
}
```

Успешный ответ должен быть JSON с полем:

```json
{ "success": true }
```

Если почтовый сервис запущен на вашем компьютере, используйте в `.env` адрес
вида:

```env
MAIL_SERVICE_BASE_URL=http://host.docker.internal:8025
```

Без реально запущенного почтового сервиса сервер всё равно стартует, но запросы
на восстановление пароля будут завершаться ошибкой отправки письма.

## Полезные команды

Посмотреть логи:

```bash
docker compose logs -f server
docker compose logs -f db
```

Остановить контейнеры без удаления базы:

```bash
docker compose down
```

Остановить контейнеры и удалить локальную базу данных:

```bash
docker compose down -v
```

Пересобрать с нуля:

```bash
docker compose build --no-cache
docker compose up -d
```

## Настройки

Основные настройки лежат в `.env`:

- `SERVER_PORT` - порт TCP-сервера на вашем компьютере.
- `POSTGRES_PORT` - локальный порт PostgreSQL.
- `APP_DB_NAME`, `APP_DB_USER`, `APP_DB_PASSWORD` - база и пользователь приложения.
  Схему таблиц внутри этой базы сервер создаёт сам через ODB ORM.
- `DB_SSLMODE` - режим SSL для подключения к PostgreSQL. Для локального Docker
  оставьте `disable`.
- `MAIL_SERVICE_BASE_URL` - адрес внешнего почтового сервиса.
- `GRAPHING_CALCULATION_POINTS` - количество точек при расчёте графика.
