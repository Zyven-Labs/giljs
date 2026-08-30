## 11. API Reference

### 11.1 Script Management

**Script Loading**:
```c
GilScript* gil_load(const char *source, const char **error);
GilScript* gil_load_file(const char *path, const char **error);
void gil_script_free(GilScript *script);
```

**Intent Access**:
```c
GilIntent* gil_intent_get(const GilScript *script, const char *name);
```

### 11.2 Frontier Operations

**Creation and Destruction**:
```c
GilFrontier* gil_frontier_new(GilLocks *locks);
void gil_frontier_free(GilFrontier *f);
```

**Predicate Operations**:
```c
void gil_frontier_set(GilFrontier *f, const char *name,
                      const char *args[], size_t argc, GilVal value);
GilVal gil_frontier_get(const GilFrontier *f, const char *name,
                        const char *args[], size_t argc);
void gil_frontier_del(GilFrontier *f, const char *name,
                      const char *args[], size_t argc);
```

**Locking**:
```c
void gil_frontier_lock(GilFrontier *f);
void gil_frontier_unlock(GilFrontier *f);
```

### 11.3 Query Operations

```c
GilResult gil_frontier_query(const GilFrontier *f, const char *name,
                             const char *pattern_args[], size_t argc);
void gil_result_free(GilResult *r);
```

### 11.4 Execution

```c
int gil_intent_execute(GilIntent *intent, GilFrontier *frontier,
                       const char *args[], size_t argc);
```

### 11.5 Constants

**Truth Values**:
```c
#define GIL_FALSE 0
#define GIL_TRUE  1
#define GIL_BOTH  2
```

### 11.6 Error Handling

Gil functions return appropriate error codes and may populate error pointers:
- Parse errors during loading
- Runtime errors during execution
- Argument count mismatches
- Memory allocation failures