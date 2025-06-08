# Software Design Manual

## 1 Functional Module

### 1.1 Encryption Layer

encrypt data using AES-256-GCM(Based on libsoduim implementation)

### 1.2 Message Format

1. sequence number defined by application layer;
2. timestamp
3. encryption payload
4. verification tag

### 1.3 Retransmission Mechanism

ACK-based timeout retransmission(support sliding window scalability)

### 1.4 Replay Defense

Timestemp + sequence number window

### 1.5 Key Initialization

pre-shared key(configuration or manual input)



