<h1 align="center">Brain Choice</h1>

<p align="center">
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows-0078D6?style=flat-square&logo=windows&logoColor=white">
  <img alt="OpenGL" src="https://img.shields.io/badge/OpenGL-3.3%20core-5586A4?style=flat-square&logo=opengl&logoColor=white">
  <img alt="C++" src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus&logoColor=white">
  <img alt="License" src="https://img.shields.io/badge/license-PolyForm%20Noncommercial%201.0.0-yellow?style=flat-square">
  <img alt="Distribute" src="https://img.shields.io/badge/ship-single%20exe-brightgreen?style=flat-square">
</p>

---

### 环境

- Windows
- MinGW-w64（或兼容工具链），`g++` 与 `windres` 在 **PATH** 中  
  也可临时指定：`set CXX=...` / `set WR=...`

### 源码下载

```bash
gh repo clone NeetheCheeBao/BrainChoice
```

```bash
git clone https://github.com/NeetheCheeBao/BrainChoice.git
```

### 一键编译

```bash
.\build.bat
```

输出：
```text
bin\BrainChoice.exe
```

### 产品说明

| 路径 | 说明 |
|------|------|
| `bin\BrainChoice.exe` | 产品文件 |
| `%TEMP%\BrainChoice_build\` | 临时构建文件 |

### 许可证

[PolyForm Noncommercial License 1.0.0](LICENSE) 