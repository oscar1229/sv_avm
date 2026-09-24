# sv_avm_lib

本目录由 `sv_avm` 工程的 `cmake --install build` 生成；示例程序使用本目录的预编译库、头文件和资源，以及同级 `../mpp` 仓库中的 MPP 头文件和 `build/lib/libmpp.so`。

先构建共享 MPP，再在 `sv_avm` 中构建并安装：

```bash
cmake -S ../mpp -B ../mpp/build
cmake --build ../mpp/build -j8
cmake -S . -B build
cmake --build build -j8
cmake --install build
```

默认安装目录为 `../sv_avm_lib`。安装完成后，在此目录中构建并运行示例：

```bash
cmake -S . -B build
cmake --build build -j8
./run_live_vi.sh
```

运行配置在 `config.json`；`frames` 为 0 时持续运行直至退出。无摄像头时可将 `use_fallback_image` 设为 `true`，无显示器时可将 `force_offscreen` 设为 `true`。图片、标定文件和模型资源位于 `sv_avm_test/`。共享 MPP 不复制到本目录，运行时从 `../mpp/build/lib` 加载。
