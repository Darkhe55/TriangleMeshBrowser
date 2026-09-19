#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在 ASCII 工作区构建 HarmonyOS 工程。

背景: hvigor 会校验工程路径, 含非 ASCII 字符(如中文目录 "棱镜模型")时报
      "Invalid project path"。本脚本把工程与源码同步到纯 ASCII 工作区后构建,
      再把产物 .hap 拷回仓库, 使中文路径下的开发也能正常出包。

用法:
    python tools/build_ascii.py                # 同步 + assembleHap
    python tools/build_ascii.py --clean        # 先清理工作区
    python tools/build_ascii.py --task assembleApp   # 自定义 hvigor 任务

环境变量(可选):
    PRISM_OHOS_BUILD_WS   工作区根目录 (默认 <用户目录>/ohos-build/prism)
    DEVECO_HOME           command-line-tools 根目录
                          (默认 <用户目录>/Documents/ohos-tools/command-line-tools)
"""
import argparse
import os
import shutil
import subprocess
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

HERE = os.path.dirname(os.path.abspath(__file__))            # browser/harmony/tools
HARMONY_DIR = os.path.dirname(HERE)                          # browser/harmony
REPO_DIR = os.path.dirname(HARMONY_DIR)                      # browser
REPO_SRC = os.path.join(REPO_DIR, "src")                     # browser/src

HOME = os.path.expanduser("~")
WS = os.environ.get("PRISM_OHOS_BUILD_WS", os.path.join(HOME, "ohos-build", "prism"))
DEVECO = os.environ.get(
    "DEVECO_HOME",
    os.path.join(HOME, "Documents", "ohos-tools", "command-line-tools"),
)

IGNORE = shutil.ignore_patterns(
    "build", ".cxx", "oh_modules", "node_modules", ".git", "*.log", "*.hap",
)


def log(msg):
    print(f"[build_ascii] {msg}", flush=True)


def ensure_ascii(path, what):
    try:
        path.encode("ascii")
    except UnicodeEncodeError:
        raise SystemExit(
            f"[FAIL] {what} 含非 ASCII 字符, hvigor 无法构建: {path}\n"
            f"       请通过环境变量 PRISM_OHOS_BUILD_WS 指定纯 ASCII 工作区路径。"
        )


def sync(clean_ws):
    ensure_ascii(WS, "工作区路径")
    if clean_ws and os.path.exists(WS):
        log(f"清理工作区 {WS}")
        shutil.rmtree(WS, ignore_errors=True)
    os.makedirs(WS, exist_ok=True)

    dst_h = os.path.join(WS, "harmony")
    if os.path.exists(dst_h):
        shutil.rmtree(dst_h, ignore_errors=True)
    shutil.copytree(HARMONY_DIR, dst_h, ignore=IGNORE)
    log(f"同步 harmony -> {dst_h}")

    if os.path.isdir(REPO_SRC):
        dst_s = os.path.join(WS, "src")
        if os.path.exists(dst_s):
            shutil.rmtree(dst_s, ignore_errors=True)
        shutil.copytree(REPO_SRC, dst_s, ignore=IGNORE)
        log(f"同步 src     -> {dst_s}")


def build(task):
    hvigorw = os.path.join(DEVECO, "bin", "hvigorw.bat")
    if not os.path.exists(hvigorw):
        raise SystemExit(f"[FAIL] 未找到 hvigorw: {hvigorw}\n"
                         f"       可通过 DEVECO_HOME 指定 command-line-tools 目录。")
    env = dict(os.environ)
    env["DEVECO_SDK_HOME"] = os.path.join(DEVECO, "sdk")
    env["DEVECO_NODE_HOME"] = os.path.join(DEVECO, "tool", "node")

    cmd = ["cmd", "/c", hvigorw, task, "--no-daemon"]
    log("执行: " + " ".join(cmd))
    proc = subprocess.run(cmd, cwd=os.path.join(WS, "harmony"), env=env,
                          capture_output=True, text=True, errors="replace")
    sys.stdout.write(proc.stdout or "")
    sys.stderr.write(proc.stderr or "")
    return proc.returncode


def collect_hap():
    """把构建产物 .hap 拷回仓库 harmony/build-output/。"""
    out_root = os.path.join(WS, "harmony", "entry", "build")
    found = []
    for root, _dirs, files in os.walk(out_root):
        for f in files:
            if f.endswith(".hap"):
                found.append(os.path.join(root, f))
    if not found:
        log("未找到 .hap 产物")
        return
    dst_dir = os.path.join(HARMONY_DIR, "build-output")
    os.makedirs(dst_dir, exist_ok=True)
    for h in found:
        dst = os.path.join(dst_dir, os.path.basename(h))
        shutil.copy2(h, dst)
        log(f"产物: {dst}  ({os.path.getsize(dst)} bytes)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--clean", action="store_true", help="构建前清理工作区")
    ap.add_argument("--task", default="assembleHap", help="hvigor 任务 (默认 assembleHap)")
    args = ap.parse_args()

    log(f"工作区   : {WS}")
    log(f"工具链   : {DEVECO}")
    sync(args.clean)
    rc = build(args.task)
    if rc == 0:
        collect_hap()
    log("构建成功" if rc == 0 else f"构建失败 (exit={rc})")
    return rc


if __name__ == "__main__":
    sys.exit(main())
