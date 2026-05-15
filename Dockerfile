FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        libboost-date-time-dev \
        libodb-boost-dev \
        libodb-dev \
        libodb-pgsql-dev \
        qt6-base-dev \
        qt6-base-dev-tools \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cd libfn \
    && ./build-linux.sh /usr/bin \
    && cd /app \
    && mkdir -p build-docker \
    && cd build-docker \
    && qmake6 ../graphing-server.pro \
    && make -j"$(nproc)"

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libboost-date-time1.83.0 \
        libodb-2.4 \
        libodb-boost-2.4t64 \
        libodb-pgsql-2.4 \
        libqt6core6t64 \
        libqt6network6t64 \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/graphing-server

COPY --from=build /app/build-docker/graphing-server /usr/local/bin/graphing-server
COPY --from=build /app/libfn/libfn.so /opt/graphing-server/libfn.so

RUN useradd --system --no-create-home --home-dir /opt/graphing-server graphing \
    && chown -R graphing:graphing /opt/graphing-server

ENV LIBFN_PATH=/opt/graphing-server/libfn.so
ENV GRAPHING_CALCULATION_POINTS=1000

EXPOSE 13579

USER graphing

CMD ["graphing-server"]
