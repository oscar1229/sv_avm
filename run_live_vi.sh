#!/usr/bin/env bash
set -e
cd ./build
exec ./sv_avm_render_test --live-vi --frames 0 --vi-timeout-ms 33 "$@"
