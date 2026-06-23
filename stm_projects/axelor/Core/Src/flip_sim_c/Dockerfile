FROM debian:bookworm-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends mingw-w64 make && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src

ENTRYPOINT ["make"]
CMD ["CC=x86_64-w64-mingw32-gcc"]
