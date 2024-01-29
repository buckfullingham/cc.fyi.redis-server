# Build Your Own Redis Server

This is a solution to the problem posed at https://codingchallenges.fyi/challenges/challenge-redis.

## Features

### Protocols

- **RESP2** - a limited version of RESP2 as per the challenge requirements
- **inline commands** - not specified in the challenge but quite important if you're testing with telnet, say

### Implemented Commands

- **PING**
- **ECHO**
- **SET** - supporting EX, PX, EXAT & PXAT expiry options
- **GET**
- **EXISTS**
- **DEL**
- **INCR**
- **DECR**
- **LPUSH**
- **RPUSH**
- **LRANGE**
- **SAVE**

### Benchmarks

### This Solution

```
bash-4.4$ redis-benchmark -t set,get, -n 100000 -q
SET: 52083.34 requests per second
GET: 52826.20 requests per second
```

### Real Redis

```
[root@4d6527960566 /]# redis-benchmark -t set,get, -n 100000 -q
SET: 54436.58 requests per second
GET: 52356.02 requests per second
```

### Getting Started

#### Pre-requisites

You will need to be able to build and run container images using docker or a docker compatible command line interface
(e.g. podman). Read the scripts below for further information.

#### Steps

1. ```scripts/build_container_image.sh``` - this will build a container image for you
2. ```scripts/build_in_container.sh``` - this will use the container image to do build the project for you
2. ```scripts/run_benchmark_in_container.sh``` - this will use the container image to run the benchmark for you
