# The build image: everything `cmake --build <build> --target check` needs.
#
#   docker build -t hmbtn-build .
#
# GCC 2.95.2 for the PlayStation (decompals/old-gcc 0.17), the PSY-Q Runtime Library 4.7
# (the release every library object in SLUS_011.15 comes from), MIPS GNU binutils, CMake,
# Ninja and Python. Downloads are SHA-256 pinned; nothing of Sony's ends up in this
# repository, only in the image. The SDK's headers are installed lower-case, with Unix
# line ends, in /opt/psyq/include.
FROM python:3.14-slim-trixie AS fetch

RUN apt-get update \
    && apt-get install -y --no-install-recommends curl ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /opt/psyq/gcc-2.95.2 /opt/psyq/sdk-4.7 /tmp/dl && cd /tmp/dl \
    && curl -fL --retry 5 --retry-all-errors -o gcc-2.95.2-psx.tar.gz \
        "https://github.com/decompals/old-gcc/releases/download/0.17/gcc-2.95.2-psx.tar.gz" \
    && echo "932ed3669710a82b12570c29c46ff42989b6505b2af10db84d4551d55dfc0b1c  gcc-2.95.2-psx.tar.gz" \
        | sha256sum -c - \
    && tar -xzf gcc-2.95.2-psx.tar.gz -C /opt/psyq/gcc-2.95.2 \
    && find /opt/psyq -type f -exec chmod a+rx {} + \
    && ( curl -fL --retry 5 --retry-all-errors -o psyq-4.7.zip \
           "https://psx.arthus.net/sdk/Psy-Q/Psy-Q_47.zip" \
      || curl -fL --retry 5 --retry-all-errors -o psyq-4.7.zip \
           "https://archive.org/download/ps1_sdks/Runtime%20Library%204.7.zip" ) \
    && echo "e3df241adde663fba1f5bff1f0070e0d32d9518ebb11d60a914e5e016f704745  psyq-4.7.zip" \
        | sha256sum -c - \
    && python3 -m zipfile -e psyq-4.7.zip /opt/psyq/sdk-4.7 \
    && cd /opt/psyq/sdk-4.7/INCLUDE && find . -type f | while read -r f; do \
         l=$(echo "$f" | tr A-Z a-z); mkdir -p "/opt/psyq/include/$(dirname "$l")"; \
         tr -d '\r' < "$f" > "/opt/psyq/include/$l"; \
       done \
    && chmod -R a+rX /opt/psyq

FROM python:3.14-slim-trixie

ENV PYTHONDONTWRITEBYTECODE=1
RUN apt-get update \
    && apt-get install -y --no-install-recommends binutils-mips-linux-gnu cmake ninja-build \
    && rm -rf /var/lib/apt/lists/*
COPY --from=fetch /opt/psyq /opt/psyq
WORKDIR /src
