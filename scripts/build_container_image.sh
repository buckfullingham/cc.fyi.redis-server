#!/usr/bin/env bash

ROOT_DIR="${ROOT_DIR:-"$(readlink -f "$(dirname "$0")"/..)"}"
IMAGE_TAG="${IMAGE_TAG:-cc-fyi-build-your-own-redis-server}"
DOCKER="${DOCKER:-docker}"

cd "$ROOT_DIR" || exit 1

"$DOCKER" build -t "$IMAGE_TAG" .
