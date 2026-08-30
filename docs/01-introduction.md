# Gnosis Intent Language (Gil) Specification

## Version 1.0

**Authors**: Zyven Labs
**Date**: August 2026
**License**: MIT

---

## Table of Contents

1. Executive Summary
2. Introduction to Gil
3. Core Concepts
4. Language Syntax and Grammar
5. Execution Model
6. Three-valued Logic
7. Advanced Features
8. Implementation Details
9. Examples and Use Cases
10. System Integration
11. API Reference
12. Appendix A: Grammar Reference
13. Appendix B: Error Handling

---

## 1. Executive Summary

The Gnosis Intent Language (Gil) is a small, three-valued logic language designed for describing state transitions in a deterministic, concurrent manner. It provides a clean syntax for defining *intents*—named, parameterized transitions that transform a *frontier* of predicate values.

Gil is uniquely suited for modeling complex state systems where traditional binary logic may be insufficient. It supports:
- Three-valued logic (true, false, both) for representing contradictions
- Concurrent execution through iterative convergence
- Atomic state transactions
- Pattern matching for sophisticated querying
- Integer arithmetic in predicate arguments
- Nested conditional statements (when-blocks)
- Iterative convergence through repeat blocks

This specification defines the complete syntax, semantics, and execution model of Gil, ensuring consistent implementation across different platforms and languages.

---

## 2. Introduction to Gil

### 2.1 What is Gil?

Gil stands for Gnosis Intent Language. It is a domain-specific language designed to model and execute state transition systems while capturing the logical complexity that simple binary logic can't adequately represent.

### 2.2 Design Philosophy

Gil is designed with the following principles in mind:
- **Conciseness**: Efficient expression of complex state systems
- **Safety**: Prevention of undefined behavior and memory issues
- **Expressiveness**: Capacity to model sophisticated logical relationships
- **Determinism**: Predictable execution characteristics
- **Concurrency Safety**: Support for multi-threaded environments

### 2.3 Core Concepts

The Gil language is built around three fundamental concepts:
1. **Predicates** - Represent propositions with identifiers and arguments
2. **Frontiers** - Collections of predicate/value bindings
3. **Intents** - Named executable transitions over frontiers