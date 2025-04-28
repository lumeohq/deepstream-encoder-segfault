FROM nvcr.io/nvidia/deepstream:7.1-triton-multiarch

RUN apt-get update && apt-get install -y \
    build-essential \
    pkg-config \
    libglib2.0-dev \
    libgstreamer1.0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY src/ /app/src/

RUN gcc -o /app/src/pipeline_test \
    /app/src/main.c \
    $(pkg-config --cflags --libs gstreamer-1.0) \
    -Wall -Wextra -g

ENTRYPOINT []
