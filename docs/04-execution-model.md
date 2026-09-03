## 5. Execution Model

### 5.1 Intent Execution Process

When an intent executes, the following process occurs:

1. **Parameter Binding**: Determine the values of intent parameters from the execution arguments
2. **Phase 1 Evaluation**: Execute all statements (including repeat block bodies) against the current frontier snapshot, collecting assignments. Resolve any conflicting assignments and commit them atomically to the frontier.
3. **Phase 2 Convergence**: If the intent has top-level `repeat` blocks, iterate all of their bodies together until no new assignments are produced.

### 5.2 Two-Phase Execution

**Phase 1 (First Pass)**:
- All statements (including repeat body bodies) execute against snapshot frontier
- No frontier changes during evaluation
- Assignments collected, resolved (`true` + `false` → `both`), then committed atomically

**Phase 2 (Repeat Convergence)**:
- If repeat blocks exist, iterate all repeat body blocks together as a group
- Each iteration is a full snapshot → resolve → commit cycle
- Converges when no new assignments are produced
- All repeat blocks see each others' changes from the previous iteration

The convergence model is based on the iterative fixed-point approach, ensuring deterministic execution behavior.

### 5.3 Convergence Mechanism

Convergence occurs when no predicate values change between iterations:
- The system iterates until fixed point is reached
- Multiple repeat blocks iterate together in a synchronized manner
- The process has a well-defined termination condition

Each convergence iteration is a complete snapshot/resolve/commit cycle.

### 5.4 Repeat Blocks

Repeat blocks provide iterative convergence for complex state evolution:
- Only top-level repeat blocks are supported
- Nested repeat blocks are not allowed 
- All repeat blocks run together in a synchronized manner
- Convergence continues until no new assignments are produced

This creates the basis for complex behavior modeling like graph traversal or system state propagation.