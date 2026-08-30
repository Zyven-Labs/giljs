## 5. Execution Model

### 5.1 Intent Execution Process

When an intent executes, the following process occurs:

1. **Parameter Binding**: Determine the values of intent parameters from the execution arguments
2. **Phase 1 Evaluation**: Execute all statements against the current frontier snapshot, collecting assignments
3. **Conflict Resolution**: Resolve any conflicting assignments
4. **Atomic Commit**: Commit resolved assignments to the frontier
5. **Phase 2 Convergence**: If the intent has top-level `repeat` blocks, iterate these blocks until convergence

### 5.2 Three-Phase Execution

**Phase 1 (Evaluation)**:
- All statements execute against snapshot frontier
- No frontier changes during evaluation
- Assignments collected for resolution

**Phase 2 (Assignment Resolution)**:
- Resolve conflicting assignments (`true` + `false` → `both`)
- Apply resolution to determine final values

**Phase 3 (Atomically Commit)**:
- Commit final assignments atomically to frontier
- Locking ensures concurrent safety

### 5.3 Convergence Mechanism

Convergence occurs when no predicate values change between iterations:
- The system iterates until fixed point is reached
- For intents with repeat blocks, convergence phase 2 repeats until stable
- Multiple repeat blocks iterate together in a synchronized manner
- The process has a well-defined termination condition

The convergence model is based on the iterative fixed-point approach, ensuring deterministic execution behavior. Each convergence iteration is a complete snapshot/resolve/commit cycle.

### 5.4 Repeat Blocks

Repeat blocks provide iterative convergence for complex state evolution:
- Only top-level repeat blocks are supported
- Nested repeat blocks are not allowed 
- All repeat blocks run together in a synchronized manner
- Convergence continues until no new assignments are produced

This creates the basis for complex behavior modeling like graph traversal or system state propagation.