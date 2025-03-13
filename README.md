# Storage Engine Architecture

## Overview

This repository implements a **storage engine** with a structured, layered architecture. The storage engine is designed
for efficient data storage, retrieval, and transaction management. It is composed of four key layers:

1. **B-Tree Layer** – Manages the structured organization of records using a B-Tree data structure.
2. **Pager Layer** – Handles page caching, transactions, and ensures data consistency.
3. **File I/O (OS) Layer** – Manages direct interactions with the operating system for reading and writing data to disk.
4. **Utility Layer** – Provides supporting functions such as serialization, logging, and error handling.

---


