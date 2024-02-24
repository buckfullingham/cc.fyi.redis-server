#!/usr/bin/env bash

ROOT_DIR="${ROOT_DIR:-"$(readlink -f "$(dirname "$0")"/..)"}"
IMAGE_TAG="${IMAGE_TAG:-cc-fyi-build-your-own-redis-server}"
DOCKER="${DOCKER:-docker}"
CONTAINER_NAME="${CONTAINER_NAME:-cc-fyi-build}"

cd "$ROOT_DIR" || exit 1

exec "$DOCKER" run --rm --name "$CONTAINER_NAME" -it -v $(pwd):$(pwd):rw "$IMAGE_TAG" /bin/bash -c "cd $(pwd) && scripts/build.sh"
