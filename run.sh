#!/bin/bash

set -o pipefail

SCRIPT_DIR=$(dirname "$0")
cd "$SCRIPT_DIR"

IMAGE_TAG="reproduce-deepstream-segfault-c"

# Build the Docker image
echo "Building Docker image..."
docker build -t "$IMAGE_TAG" . || exit 1

# Create logs directory if it doesn't exist
mkdir -p logs

# Common Docker run arguments
DOCKER_ARGS=(
    --rm
    --gpus all
    -e GST_DEBUG=1
    -e THREAD_COUNT=8
    -e ENCODERS_PER_PIPELINE=2
    --name "container-$IMAGE_TAG"
)

if [ "$1" = "--gdb" ]; then
    echo "Running in GDB mode..."
    docker run "${DOCKER_ARGS[@]}" -it --cap-add=SYS_PTRACE "$IMAGE_TAG" gdb /app/src/pipeline_test
else
    echo "Running in normal mode..."
    docker run "${DOCKER_ARGS[@]}" "$IMAGE_TAG" /app/src/pipeline_test &> logs/single-run.log
    echo "Exit code: $?"
fi
