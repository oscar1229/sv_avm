#!/usr/bin/env bash
# 运行参数统一在工程根目录的 config.json 中配置。
# 摄像头不可用时自动读取 fallback_image_dir 下的图片；
# 无显示器时自动离屏渲染并在退出时保存 offscreen_output_path。
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -x "${SCRIPT_DIR}/build/sv_avm_render_test" ]]; then
    APP_DIR="${SCRIPT_DIR}/build"
elif [[ -x "${SCRIPT_DIR}/bin/sv_avm_render_test" ]]; then
    APP_DIR="${SCRIPT_DIR}/bin"
else
    echo "sv_avm_render_test not found under ${SCRIPT_DIR}/build or ${SCRIPT_DIR}/bin" >&2
    exit 1
fi
cd "${APP_DIR}"
exec ./sv_avm_render_test "$@"
