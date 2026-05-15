#!/usr/bin/env bash
set -euo pipefail

: "${APP_DB_NAME:=calc_db}"
: "${APP_DB_USER:=calc_app}"
: "${APP_DB_PASSWORD:=calc_app_password}"

psql -v ON_ERROR_STOP=1 \
    --username "$POSTGRES_USER" \
    --dbname postgres \
    -v app_db_name="$APP_DB_NAME" \
    -v app_db_user="$APP_DB_USER" \
    -v app_db_password="$APP_DB_PASSWORD" <<'SQL'
SELECT CASE
  WHEN EXISTS (SELECT 1 FROM pg_roles WHERE rolname = :'app_db_user')
    THEN format('ALTER ROLE %I WITH LOGIN PASSWORD %L', :'app_db_user', :'app_db_password')
  ELSE
    format('CREATE ROLE %I LOGIN PASSWORD %L', :'app_db_user', :'app_db_password')
END \gexec

SELECT format('CREATE DATABASE %I OWNER %I', :'app_db_name', :'app_db_user')
WHERE NOT EXISTS (SELECT 1 FROM pg_database WHERE datname = :'app_db_name') \gexec

SELECT format('ALTER DATABASE %I OWNER TO %I', :'app_db_name', :'app_db_user') \gexec
SQL
