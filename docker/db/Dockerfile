FROM postgres:16

RUN apt-get update \
    && apt-get install -y --no-install-recommends postgresql-16-cron \
    && rm -rf /var/lib/apt/lists/*

COPY docker/db/init/001-init-graphing-server.sh /docker-entrypoint-initdb.d/001-init-graphing-server.sh
