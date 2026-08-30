## Appendix B: Error Handling

### B.1 Parse Errors

Parse errors occur during script loading and include:
- **Syntax errors**: Invalid grammar constructs
- **Missing keywords**: Missing `intent`, `do`, `end`, etc.
- **Invalid identifiers**: Reserved words or malformed names
- **Arithmetic errors**: Invalid expressions in arguments
- **Structure errors**: Improper nesting or repetition

Error messages include line numbers and specific descriptions.

### B.2 Runtime Errors

Runtime errors occur during intent execution:
- **Argument count mismatches**: Wrong number of parameters passed
- **Unbound variables**: Variables not provided in execution context
- **Division by zero**: Arithmetic errors during execution
- **Integer overflow**: When integer operations exceed limits
- **Memory allocation failures**: System resource exhaustion

### B.3 API Errors

API-level errors include:
- **Null pointer violations**: Passing NULL where valid objects required
- **Invalid argument combinations**: Parameter misuse
- **State transitions**: Attempting operations on freed objects
- **Resource limits**: Exceeding system constraints

### B.4 Recovery Considerations

Error recovery strategies:
- **Partial recovery**: Continue with remaining valid operations
- **Complete failure**: Rollback state to previous consistent point
- **Notification**: Provide detailed error context to calling applications
- **Logging**: Record errors for diagnostic purposes

### B.5 Best Practices for Error Handling

1. Always check return values from Gil API calls
2. Use error pointers to get detailed diagnostic information
3. Provide context to error messages for easier debugging
4. Implement appropriate logging for production systems
5. Design error recovery into critical applications