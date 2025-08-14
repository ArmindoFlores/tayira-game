# Tayira

This is a minimal C project that uses **OpenGL** for GPU-accelerated 2D rendering, with a simple structure for window setup, rendering, and cleanup. It also supports custom shaders for advanced visual effects.

## 📦 Requirements

You need:

- A C compiler (e.g., `gcc` or `clang`)
- OpenGL development headers
- GLFW 3 (windowing and OpenGL context library)
- GLEW (OpenGL extension loader)

---

## 🔧 Installation

### Linux (Debian/Ubuntu)
```bash
sudo apt update
sudo apt install build-essential libglfw3-dev libglew-dev libgl1-mesa-dev
```

### Linux (Fedora)
```bash
sudo dnf install glfw-devel glew-devel mesa-libGL-devel
```

### MacOS (Homebrew)
```bash
brew install glfw glew
```

## Windows (MinGW + MSYS2)
1. Install MSYS2.
2. Open the MSYS2 MinGW64 terminal.
3. Install the required packages:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-glfw mingw-w64-x86_64-glew
```

