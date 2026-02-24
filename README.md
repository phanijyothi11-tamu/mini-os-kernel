# 🖥️ Mini OS Kernel from Scratch

A lightweight **x86-based operating system kernel** built from scratch in C++, implementing core OS concepts such as memory management, paging, scheduling, and file systems.

---

## 🚀 Overview

This project demonstrates low-level system design by building a functional OS kernel capable of managing memory, scheduling processes, and handling disk operations. It provides hands-on experience with **x86 architecture**, **hardware interrupts**, and **kernel-level programming**.

---

## 💡 Key Features

* 🧠 **Memory Management**

  * Bitmap-based physical memory allocation
  * Efficient tracking of free/used memory blocks

* 📄 **Paging System**

  * Two-level paging mechanism
  * Demand paging using **recursive page tables**

* ⏱️ **Process Scheduling**

  * Kernel threads implementation
  * Preemptive **round-robin scheduler**
  * Timer interrupt-driven context switching

* 💾 **Disk & File System**

  * Non-blocking interrupt-driven disk driver
  * Custom **inode-based file system**
  * Supports files up to 64KB

* ⚡ **Performance & Reliability**

  * Efficient memory access and process switching
  * Persistent storage with structured file management

---

## 🏗️ System Architecture

* **x86 Architecture**
* Kernel runs in protected mode
* Uses hardware interrupts for scheduling and I/O
* Modular design:

  * Memory Manager
  * Scheduler
  * Disk Driver
  * File System

---

## 🛠️ Tech Stack

* **Language:** C++
* **Architecture:** x86
* **Concepts:**

  * Paging & Virtual Memory
  * Interrupt Handling
  * Thread Scheduling
  * File Systems (inode-based)

---

## ⚙️ How to Run

### 🔹 Prerequisites

* GCC (cross-compiler recommended)
* QEMU / Bochs (for OS emulation)
* Make

### 🔹 Steps

```bash
# Clone the repository
git clone https://github.com/yourusername/mini-os-kernel.git

cd mini-os-kernel

# Build the kernel
make

# Run using QEMU
qemu-system-i386 -kernel kernel.bin
```

---

## 📂 Project Structure

```
/kernel        # Core kernel logic
/memory        # Memory management (bitmap, paging)
/scheduler     # Thread scheduling
/filesystem    # Inode-based file system
/drivers       # Disk driver & I/O handling
/boot          # Bootloader and initialization
```

---

## 📈 Learning Outcomes

* Deep understanding of **operating system internals**
* Hands-on experience with **low-level programming**
* Practical implementation of:

  * Paging
  * Scheduling
  * Interrupt handling
  * File systems

---

## ⚠️ Notes

* This is an educational OS kernel and not intended for production use.
* Some hardware interactions are simulated using emulators like QEMU.

---

## 🔮 Future Improvements

* Add multi-level scheduling (priority-based)
* Support larger file sizes and directories
* Improve virtual memory management
* Add system call interface

---

## 👩‍💻 Author

**Phani Jyothi Kurada**

---
