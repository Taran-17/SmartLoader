
# SmartLoader
# Tarandeep Singh 111 2023553
## Introduction
**SmartLoader** is an upgraded ELF loader written in C, designed to load 32-bit ELF executables lazily. Rather than pre-loading all segments, this loader allocates and loads segments only when they are accessed. This approach mimics lazy loading and demand paging in Linux, making it a smart, memory-efficient loader.

This project builds upon the **SimpleLoader** from a previous assignment and is intended for educational purposes in an Operating Systems course.

## Features
- **Lazy Segment Loading**: Loads program segments only when accessed, reducing memory usage.
- **Page-by-Page Allocation**: Allocates memory in 4 KB pages on demand, handling multiple page faults for larger segments.
- **Segmentation Fault Handling**: Treats segmentation faults caused by unallocated memory as page faults and allocates memory dynamically using `mmap`.
- **Execution Resumption**: Resumes program execution after handling segmentation faults caused by accessing unallocated but valid segment addresses.

## Requirements
- **Hardware**: A system with any Unix-compatible OS. Avoid using macOS for this project.
- **Software**: 
  - C compiler (GCC recommended)
  - GNU Make
  - GitHub for version control (use a private repository)
  
Ensure you’re working on a Linux system or with WSL on Windows.

## Installation
1. Clone this repository:
   ```bash
   git clone https://github.com/yourusername/SimpleSmartLoader.git
   cd SimpleSmartLoader
   ```
2. Compile the loader and test cases using `make`:
   ```bash
   make
   ```

## Usage
1. Run the loader with a 32-bit ELF executable as follows:
   ```bash
   ./loader your_executable
   ```
2. After execution, the loader will display:
   - Total number of page faults
   - Total number of page allocations
   - Total internal fragmentation in KB

## Implementation Details
- **Segmentation Fault Handling**: Accessing an unallocated segment triggers a segmentation fault, which the loader interprets as a page fault.
- **Dynamic Memory Allocation**: Memory is allocated only when required, reducing initial memory usage. Segments are loaded page-by-page, allowing non-contiguous physical allocation while maintaining contiguous virtual memory.
- **Lazy Loading**: If a segment spans multiple pages, each page is allocated on access, leading to multiple page faults per segment.


