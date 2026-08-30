## 9. Examples and Use Cases

### 9.1 Basic Examples

**Hello World**:
```
intent say_hello()
    greeting <= true
end

intent say_goodbye()
    greeting <= false
end
```

**Light Switch**:
```
intent turn_on()
    lit <= true
end

intent turn_off()
    lit <= false
end
```

**Greeter**:
```
intent greet(Person)
    greeted[Person] <= true
end

intent farewell(Person)
    greeted[Person] <= false
end
```

### 9.2 Advanced Examples

**Counter**:
```
intent reset()
    when count[N] do
        count[N] <= false
    end
end

intent inc(N)
    count[N] <= false
    count[N + 1] <= true
end
```

**Traffic Light**:
```
intent set_green()
    green <= true
    yellow <= false
    red <= false
end

intent set_yellow()
    yellow <= true
    green <= false
    red <= false
end

intent set_red()
    red <= true
    green <= false
    yellow <= false
end
```

### 9.3 Real-world Applications

Gil is particularly suitable for:
- **Access Control Systems**: Modeling authorization workflows with conflicting permissions
- **State Machines**: Complex control systems with multi-state transitions
- **Network Protocols**: Packet forwarding with convergence concepts
- **Game Systems**: RPG combat with state propagation
- **Inventory Management**: Multi-parameter tracking with complex business rules
- **Decision Making Systems**: Complex conditional logic with contradiction handling

### 9.4 Integration Scenarios

Gil integrates well with:
- **Event-driven architectures**: As state processors
- **Rule engines**: For formal business rule representation
- **AI/ML systems**: For constraint-based reasoning
- **IoT systems**: For device state management
- **Blockchain systems**: For transaction state modeling
- **Distributed systems**: For conflict resolution across nodes