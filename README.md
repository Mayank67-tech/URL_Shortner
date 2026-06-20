# Thread-Safe Distributed URL Shortener

A high-performance, horizontally-scalable URL shortener built in **C++17**, designed for 100K+ URLs and millions of reads per day.

## Architecture

```
              Client
                │
          Load Balancer
           /         \
      Server 1    Server 2
           \         /
      Thread-safe LRU Cache
                │
             Redis
                │
          PostgreSQL
```

| Layer | Technology | Role |
|---|---|---|
| API Server | Crow (C++17) | Multithreaded REST endpoints |
| Database | PostgreSQL 16 | Persistent storage, analytics |
| Cache L1 | LRU Cache | In-memory, O(1), mutex-protected |
| Cache L2 | Redis 7 | Distributed cache with TTL |
| Rate Limiter | Token Bucket | 100 req/min per IP |
| Load Balancing | Consistent Hashing | Minimal key redistribution |
| Analytics | Thread Pool | Async producer-consumer pipeline |
| Monitoring | Prometheus + Grafana | Real-time metrics and dashboards |
| Deployment | Docker Compose | Single-command full stack |

## Quick Start

```bash
docker compose up --build --detach
```

| Service | Port | Credentials |
|---|---|---|
| API (Primary) | `localhost:8080` | — |
| API (Secondary) | `localhost:8081` | — |
| PostgreSQL | `localhost:5432` | urlapp / secretpass123 |
| Redis | `localhost:6379` | — |
| Prometheus | `localhost:9092` | — |
| Grafana | `localhost:3000` | admin / admin |

## API

### Shorten URL

```
POST /shorten
Content-Type: application/json

{"url": "https://youtube.com/watch?v=abc"}
```

```json
{
  "shortCode": "A7Kd91",
  "shortUrl": "http://localhost:8080/r/A7Kd91",
  "originalUrl": "https://youtube.com/watch?v=abc"
}
```

### Redirect

```
GET /r/A7Kd91  →  302 Redirect
```

### Analytics

```
GET /analytics/A7Kd91
```

```json
{
  "shortCode": "A7Kd91",
  "clicks": 5400,
  "topCountries": ["India", "US", "UK"],
  "lastAccess": "2026-06-20T12:00:00Z"
}
```

### Delete

```
DELETE /r/A7Kd91
```

### Health Check

```
GET /health
```

```json
{
  "status": "healthy",
  "database": "connected",
  "redis": "connected"
}
```

## Project Structure

```
url-shortener/
├── CMakeLists.txt
├── Dockerfile
├── docker-compose.yml
├── src/
│   ├── main.cpp
│   ├── controllers/
│   │   ├── ShortenController.h
│   │   ├── RedirectController.h
│   │   ├── AnalyticsController.h
│   │   └── HealthController.h
│   ├── services/
│   │   ├── UrlService.h
│   │   └── AnalyticsService.h
│   ├── cache/
│   │   └── LRUCache.h
│   ├── rateLimiter/
│   │   └── TokenBucket.h
│   ├── hashing/
│   │   ├── Base62.h
│   │   └── ConsistentHash.h
│   ├── models/
│   │   └── Url.h
│   ├── database/
│   │   ├── PostgresPool.h
│   │   └── PostgresPool.cpp
│   ├── redis/
│   │   ├── RedisClient.h
│   │   └── RedisClient.cpp
│   ├── metrics/
│   │   ├── Metrics.h
│   │   └── Metrics.cpp
│   └── threadpool/
│       └── ThreadPool.h
├── tests/
│   ├── test_base62.cpp
│   ├── test_lru_cache.cpp
│   ├── test_rate_limiter.cpp
│   ├── test_consistent_hash.cpp
│   └── test_thread_pool.cpp
└── config/
    ├── init.sql
    ├── prometheus.yml
    └── grafana/
```

## Database Schema

```sql
CREATE TABLE urls (
    id          BIGSERIAL PRIMARY KEY,
    short_code  VARCHAR(8) UNIQUE NOT NULL,
    long_url    TEXT NOT NULL,
    created_at  TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE TABLE analytics (
    id          BIGSERIAL PRIMARY KEY,
    short_code  VARCHAR(8) REFERENCES urls(short_code) ON DELETE CASCADE,
    accessed_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    country     VARCHAR(50),
    browser     VARCHAR(100),
    device      VARCHAR(50)
);
```

## Design

### Short Code Generation

Database auto-increment IDs are encoded into compact alphanumeric strings using Base62 (`0-9a-zA-Z`). Six characters support 56 billion unique codes.

### Cache Strategy

Three-tier cache-aside lookup:

1. **LRU Cache** — In-process, O(1), mutex-protected, configurable capacity
2. **Redis** — Shared across instances, sub-millisecond reads, 1-hour TTL
3. **PostgreSQL** — Source of truth, queried only on double cache miss

### Rate Limiting

Token bucket algorithm enforcing 100 requests per minute per client IP. Tokens refill lazily on each request. Stale buckets are periodically cleaned up.

### Consistent Hashing

150 virtual nodes per server ensure even key distribution. Adding or removing a server redistributes only ~1/N of keys.

### Async Analytics

Redirects return immediately with a 302. Analytics events are pushed to a thread pool queue and inserted into PostgreSQL by background workers, keeping redirect latency under 1ms.

## Monitoring

### Prometheus Metrics

| Metric | Type |
|---|---|
| `http_requests_total` | Counter (method, path, status) |
| `http_request_duration_seconds` | Histogram (method, path) |
| `cache_operations_total` | Counter (hit, miss) |
| `active_connections` | Gauge |

### Grafana Dashboard

Pre-provisioned dashboard at `localhost:3000` with panels for request rate, p95 latency, cache hit ratio, error rate, and active connections.

## Testing

Five GoogleTest suites covering all core algorithms:

```bash
docker compose exec backend ctest --output-on-failure
```

| Suite | Coverage |
|---|---|
| Base62 | Encode/decode roundtrip, uniqueness, character validation |
| LRU Cache | Eviction policy, concurrent read/write with 8 threads |
| Rate Limiter | Token refill, per-IP isolation, 10-thread concurrency |
| Consistent Hash | Distribution evenness, minimal redistribution on add/remove |
| Thread Pool | Future results, exception propagation, 10K-task stress test |

## Configuration

| Variable | Default | Description |
|---|---|---|
| `DB_HOST` | postgres | PostgreSQL host |
| `DB_PORT` | 5432 | PostgreSQL port |
| `DB_NAME` | url_shortener | Database name |
| `DB_USER` | urlapp | Database user |
| `DB_PASSWORD` | secretpass123 | Database password |
| `REDIS_HOST` | redis | Redis host |
| `REDIS_PORT` | 6379 | Redis port |
| `SERVER_PORT` | 8080 | API port |
| `METRICS_PORT` | 9090 | Prometheus metrics port |
| `CACHE_SIZE` | 10000 | LRU cache capacity |
| `POOL_SIZE` | 10 | DB connection pool size |
| `THREAD_POOL_SIZE` | 4 | Analytics worker threads |

## Tech Stack

| Component | Technology |
|---|---|
| Language | C++17 |
| REST Framework | Crow |
| Database | PostgreSQL 16 |
| Cache | Redis 7 |
| Metrics | prometheus-cpp |
| Testing | GoogleTest |
| Build | CMake 3.20+ |
| Containers | Docker, Docker Compose |
| Dashboards | Grafana 11 |

## License

MIT
