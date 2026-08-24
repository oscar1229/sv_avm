#!/usr/bin/env bash
# 运行参数统一在工程根目录的 config.json 中配置。
# 摄像头不可用时自动读取 fallback_image_dir 下的图片；
# 无显示器时自动离屏渲染并在退出时保存 offscreen_output_path。
set -e
cd ./build
exec ./sv_avm_render_test "$@"
