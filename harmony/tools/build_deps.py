#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""交叉编译第三方依赖到 HarmonyOS (OHOS) 静态库。

用途: 鸿蒙版需要 Assimp / laszip / pugixml 等库, 但这些库没有 OHOS 预编译包,
      本脚本用 SDK 自带的 clang + sysroot 直接编译源码为静态库 (.a), 输出到
      构建工作区的 third_party/<lib>/{include,lib/<abi>}。

源码来源: vcpkg buildtrees (桌面构建时已下载), 或 --src 指定目录。

用法:
    python tools/build_deps.py                 # 编译全部已实现的目标
    python tools/build_deps.py pugixml laszip
    python tools/build_deps.py --list

环境变量:
    PRISM_OHOS_BUILD_WS  工作区 (默认 ~/ohos-build/prism)
    DEVECO_HOME          command-line-tools 根目录
    VCPKG_BUILDTREES     vcpkg buildtrees (默认 ~/Documents/vcpkg/buildtrees)
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

HERE = os.path.dirname(os.path.abspath(__file__))
HARMONY_DIR = os.path.dirname(HERE)
REPO_DIR = os.path.dirname(HARMONY_DIR)

HOME = os.path.expanduser("~")
WS = os.environ.get("PRISM_OHOS_BUILD_WS", os.path.join(HOME, "ohos-build", "prism"))
DEVECO = os.environ.get("DEVECO_HOME",
                        os.path.join(HOME, "Documents", "ohos-tools", "command-line-tools"))
VCPKG_BT = os.environ.get("VCPKG_BUILDTREES",
                          os.path.join(HOME, "Documents", "vcpkg", "buildtrees"))

SDK_NATIVE = os.path.join(DEVECO, "sdk", "default", "openharmony", "native")
CLANGXX = os.path.join(SDK_NATIVE, "llvm", "bin", "clang++.exe")
CLANG = os.path.join(SDK_NATIVE, "llvm", "bin", "clang.exe")
AR = os.path.join(SDK_NATIVE, "llvm", "bin", "llvm-ar.exe")
SYSROOT = os.path.join(SDK_NATIVE, "sysroot")

ABI_TARGET = {
    "arm64-v8a": "aarch64-linux-ohos",
    "x86_64": "x86_64-linux-ohos",
}
DEFAULT_ABIS = ["arm64-v8a", "x86_64"]

CXXFLAGS = ["-O2", "-fPIC", "-fstack-protector-strong", "-D__MUSL__",
            "-ffunction-sections", "-fdata-sections"]


def log(msg):
    print(f"[build_deps] {msg}", flush=True)


def find_src(lib, marker):
    """在 vcpkg buildtrees 下定位含 marker 的源码目录。"""
    root = os.path.join(VCPKG_BT, lib, "src")
    if not os.path.isdir(root):
        return None
    for d in sorted(os.listdir(root)):
        p = os.path.join(root, d)
        if os.path.exists(os.path.join(p, marker)):
            return p
    return None


def out_dir(lib):
    return os.path.join(WS, "third_party", lib)


def compile_objects(sources, outdir, include_dirs, abi, cxx=True, extra=None):
    os.makedirs(outdir, exist_ok=True)
    cc = CLANGXX if cxx else CLANG
    objs = []
    for src in sources:
        obj = os.path.join(outdir, os.path.splitext(os.path.basename(src))[0] + ".o")
        cmd = [cc, "--target=" + ABI_TARGET[abi], f"--sysroot={SYSROOT}"] + CXXFLAGS
        cmd += ["-std=gnu++17"] if cxx else []
        for inc in include_dirs:
            cmd += ["-I" + inc]
        if extra:
            cmd += extra
        cmd += ["-c", src, "-o", obj]
        r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
        if r.returncode != 0:
            log(f"  编译失败: {os.path.basename(src)}")
            log((r.stderr or "")[-1500:])
            raise SystemExit(1)
        objs.append(obj)
    return objs


def archive(objs, libfile):
    if os.path.exists(libfile):
        os.remove(libfile)
    r = subprocess.run([AR, "rcs", libfile] + objs,
                       capture_output=True, text=True, errors="replace")
    if r.returncode != 0:
        log(f"  ar 失败: {(r.stderr or '')[-500:]}")
        raise SystemExit(1)
    log(f"  -> {libfile} ({os.path.getsize(libfile)} bytes)")


# ---------------------------------------------------------------------------
# 各库的构建配方
# ---------------------------------------------------------------------------
def build_pugixml(abis):
    src = find_src("pugixml", os.path.join("src", "pugixml.cpp"))
    if not src:
        log("跳过 pugixml: 未找到源码")
        return False
    out = out_dir("pugixml")
    inc_dst = os.path.join(out, "include")
    os.makedirs(inc_dst, exist_ok=True)
    shutil.copy2(os.path.join(src, "src", "pugixml.hpp"), inc_dst)
    shutil.copy2(os.path.join(src, "src", "pugiconfig.hpp"), inc_dst)
    for abi in abis:
        # 注意: 不要定义 PUGIXML_WCHAR_MODE —— pugixml 用 #ifdef 判断(不看值),
        # 定义了会切换成 wchar_t 接口, 与使用方(宽字节)的符号不匹配。
        objs = compile_objects([os.path.join(src, "src", "pugixml.cpp")],
                               os.path.join(out, "obj", abi),
                               [os.path.join(src, "src")], abi,
                               extra=[])
        libdir = os.path.join(out, "lib", abi)
        os.makedirs(libdir, exist_ok=True)
        archive(objs, os.path.join(libdir, "libpugixml.a"))
    log(f"pugixml 完成 -> {out}  [源: {src}]")
    return True


def build_laszip(abis):
    """编译 LASzip 3.5.0 完整源码 (含空间索引, laszip_dll.cpp 依赖它)。

    上游缺陷: src/ 缺 lasreader.hpp, 但 lasindex.hpp 已前向声明 class LASreader,
    故生成最小桩头即可编译。体积优化: -Os + 分段。
    """
    src = None
    for cand in (os.path.join(WS, "third_party", "_src", "laszip", "LASzip-3.5.0"),
                 os.path.join(REPO_DIR, "third_party", "_src", "laszip", "LASzip-3.5.0")):
        if os.path.isfile(os.path.join(cand, "src", "laszip_dll.cpp")):
            src = cand
            break
    if not src:
        src = find_src("laszip", os.path.join("src", "laszip_dll.cpp"))
    if not src:
        log("跳过 laszip: 未找到完整源码 (需先解压 _src/laszip/LASzip-3.5.0)")
        return False

    sdir = os.path.join(src, "src")
    ddir = os.path.join(src, "dll")

    # 注意: 编译时定义 LASZIPDLL_EXPORTS —— lasindex.cpp 据此走 LASreadPoint 分支
    # (DLL 模式), 从而不需要上游未随包分发的 LASlib 头 lasreader.hpp。
    # 旧生成的桩头若存在则删除, 避免混淆。
    stub = os.path.join(sdir, "lasreader.hpp")
    if os.path.isfile(stub):
        os.remove(stub)

    inc_dirs = [
        os.path.join(src, "include"),
        os.path.join(src, "include", "laszip"),
        ddir,
        sdir,
        src,
    ]
    # 官方 src/CMakeLists.txt 的源列表
    LASZIP_SOURCES = [
        "mydefs.cpp", "lasmessage.cpp",
        "arithmeticdecoder.cpp", "arithmeticencoder.cpp", "arithmeticmodel.cpp",
        "integercompressor.cpp",
        "lasindex.cpp", "lasinterval.cpp", "lasquadtree.cpp",
        "lasreaditemcompressed_v1.cpp", "lasreaditemcompressed_v2.cpp",
        "lasreaditemcompressed_v3.cpp", "lasreaditemcompressed_v4.cpp",
        "lasreadpoint.cpp",
        "laswriteitemcompressed_v1.cpp", "laswriteitemcompressed_v2.cpp",
        "laswriteitemcompressed_v3.cpp", "laswriteitemcompressed_v4.cpp",
        "laswritepoint.cpp",
        "laszip.cpp", "laszip_dll.cpp",
    ]
    cpp = [os.path.join(sdir, f) for f in LASZIP_SOURCES
           if os.path.isfile(os.path.join(sdir, f))]
    # 只编译官方列表: 目录里的 lasunzipper.cpp / laszipper.cpp 是 .laz 文件级封装,
    # 不在 LASZIP_SOURCES 内 (它们引用未随包分发的 LASlib/宏), 编译会失败。
    if not cpp:
        log("跳过 laszip: 未找到源文件")
        return False
    log(f"  laszip 源文件 {len(cpp)} 个 (官方列表)")

    out = out_dir("laszip")
    inc_lz = os.path.join(out, "include", "laszip")
    os.makedirs(inc_lz, exist_ok=True)
    if os.path.isdir(os.path.join(src, "include", "laszip")):
        shutil.copytree(os.path.join(src, "include", "laszip"), inc_lz, dirs_exist_ok=True)
    if os.path.isfile(os.path.join(ddir, "laszip_api.h")):
        shutil.copy2(os.path.join(ddir, "laszip_api.h"), inc_lz)

    for abi in abis:
        objdir = os.path.join(out, "obj", abi)
        objs = compile_objects(cpp, objdir, inc_dirs, abi, cxx=True,
                               extra=["-DLASZIPDLL_EXPORTS", "-Os"])
        libdir = os.path.join(out, "lib", abi)
        os.makedirs(libdir, exist_ok=True)
        archive(objs, os.path.join(libdir, "liblaszip.a"))
    log(f"laszip 完成 -> {out}  [源: {src}]")
    return True


def build_assimp(abis):
    """用 OHOS CMake 工具链交叉编译 Assimp (静态库, 仅 FBX/glTF/Collada/3MF)。

    使用未打 patch 的上游源码(自带 contrib/pugixml 与 contrib/utf8cpp)。
    上游 code/CMakeLists.txt 用 IF(1) 强制 find_package 外部 pugixml/utf8cpp,
    在无 vcpkg config 的环境下会失败 -> 改为 IF(0) 以走内置 contrib 副本。
    """
    src = None
    for cand in (os.path.join(WS, "third_party", "_src", "assimp", "assimp-6.0.4"),
                 os.path.join(REPO_DIR, "third_party", "_src", "assimp", "assimp-6.0.4")):
        if os.path.isfile(os.path.join(cand, "code", "CMakeLists.txt")):
            src = cand
            break
    if not src:
        src = find_src("assimp", os.path.join("code", "CMakeLists.txt"))
    if not src:
        log("跳过 assimp: 未找到源码")
        return False

    # patch: IF(1) -> IF(0) 使 assimp 使用内置 contrib/pugixml 与 contrib/utf8cpp
    cl = os.path.join(src, "code", "CMakeLists.txt")
    with open(cl, encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
    patched = 0
    for i, ln in enumerate(lines):
        if ("find_package(pugixml CONFIG REQUIRED)" in ln or
                "find_package(utf8cpp CONFIG REQUIRED)" in ln):
            for j in range(i - 1, max(i - 6, -1), -1):
                if lines[j].strip() == "IF(1)":
                    lines[j] = lines[j].replace("IF(1)", "IF(0)")
                    patched += 1
                    break
    if patched:
        with open(cl, "w", encoding="utf-8") as f:
            f.writelines(lines)
        log(f"  已 patch code/CMakeLists.txt ({patched} 处 IF(1)->IF(0), 改用内置 contrib)")

    cmake = os.path.join(SDK_NATIVE, "build-tools", "cmake", "bin", "cmake.exe")
    toolchain = os.path.join(SDK_NATIVE, "build", "cmake", "ohos.toolchain.cmake")
    if not (os.path.exists(cmake) and os.path.exists(toolchain)):
        log("跳过 assimp: 缺少 OHOS cmake 或 ohos.toolchain.cmake")
        return False

    out = out_dir("assimp")
    ninja = os.path.join(SDK_NATIVE, "build-tools", "cmake", "bin", "ninja.exe")
    for abi in abis:
        # OHOS_ARCH 只接受 arm64-v8a / armeabi-v7a / x86_64 (传 "arm64" 会 FATAL_ERROR)
        bdir = os.path.join(out, "build", abi)

        # 源码目录可能变化(vcpkg buildtrees -> 上游 tar), 过期缓存会让 cmake 拒绝复用,
        # 报 "does not match the source ... used to generate cache" -> 检测并清理。
        cache = os.path.join(bdir, "CMakeCache.txt")
        if os.path.isfile(cache):
            try:
                with open(cache, encoding="utf-8", errors="replace") as f:
                    cached = f.read()
            except OSError:
                cached = ""
            want = os.path.normpath(src).replace("\\", "/")
            if want not in cached.replace("\\", "/"):
                log(f"  源码目录已变, 清理过期缓存: {bdir}")
                shutil.rmtree(bdir, ignore_errors=True)

        cfg = [
            "-S", src, "-B", bdir, "-G", "Ninja",
            f"-DCMAKE_MAKE_PROGRAM={ninja}",
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
            f"-DOHOS_ARCH={abi}",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_SHARED_LIBS=OFF",
            "-DASSIMP_BUILD_TESTS=OFF",
            "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF",
            "-DASSIMP_BUILD_SAMPLES=OFF",
            "-DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF",
            "-DASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=OFF",
            "-DASSIMP_BUILD_FBX_IMPORTER=ON",
            "-DASSIMP_BUILD_GLTF_IMPORTER=ON",
            "-DASSIMP_BUILD_COLLADA_IMPORTER=ON",
            "-DASSIMP_BUILD_3MF_IMPORTER=ON",
            "-DASSIMP_BUILD_ZLIB=ON",
            "-DASSIMP_NO_EXPORT=ON",
            "-DASSIMP_INSTALL=OFF",
            "-DASSIMP_WARNINGS_AS_ERRORS=OFF",
            # 体积优先: -Os + 函数/数据分段(配合链接期 --gc-sections)
            "-DCMAKE_C_FLAGS=-Os -fPIC -ffunction-sections -fdata-sections",
            "-DCMAKE_CXX_FLAGS=-Os -fPIC -ffunction-sections -fdata-sections",
            "-DCMAKE_EXE_LINKER_FLAGS=-Wl,--gc-sections",
            "-DCMAKE_SHARED_LINKER_FLAGS=-Wl,--gc-sections",
        ]
        log(f"配置 assimp ({abi}) ...")
        r = subprocess.run([cmake] + cfg, capture_output=True, text=True, errors="replace")
        if r.returncode != 0:
            log(f"  cmake 配置失败:\n{(r.stdout or '')[-1500:]}\n{(r.stderr or '')[-800:]}")
            raise SystemExit(1)

        log(f"编译 assimp ({abi}) ... (文件较多, 需要几分钟)")
        r = subprocess.run([cmake, "--build", bdir, "--target", "assimp", "--parallel"],
                           capture_output=True, text=True, errors="replace")
        if r.returncode != 0:
            log(f"  编译失败:\n{(r.stdout or '')[-1800:]}\n{(r.stderr or '')[-800:]}")
            raise SystemExit(1)

        found = glob.glob(os.path.join(bdir, "**", "libassimp*.a"), recursive=True)
        if not found:
            log("  未找到 libassimp*.a")
            raise SystemExit(1)
        libdir = os.path.join(out, "lib", abi)
        os.makedirs(libdir, exist_ok=True)
        shutil.copy2(found[0], os.path.join(libdir, "libassimp.a"))
        log(f"  -> {os.path.join(libdir, 'libassimp.a')} ({os.path.getsize(found[0])} bytes)")

        # ASSIMP_BUILD_ZLIB=ON 会把自带的 zlib 编成**独立**静态库 (contrib/zlib/
        # libzlibstatic.a), 其符号不在 libassimp.a 内 -> 必须一并收集并链接,
        # 否则最终会报 inflate*/crc32 未定义。
        zlibs = glob.glob(os.path.join(bdir, "**", "libzlibstatic.a"), recursive=True) \
            or glob.glob(os.path.join(bdir, "**", "libz.a"), recursive=True)
        if zlibs:
            shutil.copy2(zlibs[0], os.path.join(libdir, "libzlib.a"))
            log(f"  -> {os.path.join(libdir, 'libzlib.a')} ({os.path.getsize(zlibs[0])} bytes)")
        else:
            log("  警告: 未找到 assimp 自带的 zlib 静态库, 链接可能缺 zlib 符号")

    # 头文件 (含构建期生成的 config.h)
    inc_dst = os.path.join(out, "include")
    os.makedirs(inc_dst, exist_ok=True)
    shutil.copytree(os.path.join(src, "include", "assimp"),
                    os.path.join(inc_dst, "assimp"), dirs_exist_ok=True)
    gen_cfg = glob.glob(os.path.join(out, "build", "*", "include", "assimp", "config.h"))
    if gen_cfg:
        shutil.copy2(gen_cfg[0], os.path.join(inc_dst, "assimp", "config.h"))
        log("  已复制生成的 config.h")
    log(f"assimp 完成 -> {out}  [源: {src}]")
    return True


TARGETS = {
    "pugixml": build_pugixml,
    "laszip": build_laszip,
    "assimp": build_assimp,
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("targets", nargs="*", help="要编译的库 (默认全部)")
    ap.add_argument("--abis", default=",".join(DEFAULT_ABIS), help="ABI 列表, 逗号分隔")
    ap.add_argument("--list", action="store_true", help="列出可用目标")
    args = ap.parse_args()

    if args.list:
        print("可用目标:", ", ".join(TARGETS))
        return 0

    for p in (CLANGXX, AR):
        if not os.path.exists(p):
            log(f"未找到工具链: {p}\n  请设置 DEVECO_HOME")
            return 1
    os.makedirs(WS, exist_ok=True)

    abis = [a.strip() for a in args.abis.split(",") if a.strip()]
    log(f"工作区   : {WS}")
    log(f"工具链   : {SDK_NATIVE}")
    log(f"ABI      : {abis}")
    log(f"源码     : {VCPKG_BT}")

    names = args.targets or list(TARGETS)
    ok = 0
    for n in names:
        fn = TARGETS.get(n)
        if not fn:
            log(f"未知目标: {n}")
            continue
        if fn(abis):
            ok += 1
    log(f"完成 {ok}/{len(names)} 个目标")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
