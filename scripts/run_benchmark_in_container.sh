#!/usr/bin/env bash

ROOT_DIR="${ROOT_DIR:-"$(readlink -f "$(dirname "$0")"/..)"}"
IMAGE_TAG="${IMAGE_TAG:-cc-fyi-build-your-own-redis-server}"
DOCKER="${DOCKER:-docker}"
SOLUTION_CTR="${CONTAINER_NAME:-cc-fyi-solution}"
REDIS_CTR="${CONTAINER_NAME:-cc-fyi-redis}"

cd "$ROOT_DIR" || exit 1

"$DOCKER" run --name "$SOLUTION_CTR" -d -v $(pwd):$(pwd):rw "$IMAGE_TAG" /bin/bash -c "cd $(pwd) && cmake-build-release/src/main/redis_server" || exit 1
"$DOCKER" run --name "$REDIS_CTR" -d "$IMAGE_TAG" redis-server || exit 1

echo "solution benchmark:"
"$DOCKER" exec -it "$SOLUTION_CTR" bash -c "redis-benchmark -t set,get, -n 100000 -q" || exit 1

echo "redis benchmark:"
"$DOCKER" exec -it "$REDIS_CTR" bash -c "redis-benchmark -t set,get, -n 100000 -q" || exit 1

"$DOCKER" container rm -f "$SOLUTION_CTR" || exit 1

"$DOCKER" container rm -f "$REDIS_CTR" || exit 1
