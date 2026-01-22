#!/usr/bin/env python3
"""
增强版: 同时转换sclera和iris数据
"""

import re
import sys
import os

def extract_data(file_path):
    """提取sclera和iris数据"""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 检测sclera尺寸
    width_match = re.search(r'#define\s+SCLERA_WIDTH\s+(\d+)', content)
    height_match = re.search(r'#define\s+SCLERA_HEIGHT\s+(\d+)', content)

    if width_match and height_match:
        width = int(width_match.group(1))
        height = int(height_match.group(1))
    else:
        return None, None, None, None, None, None

    # 检测iris尺寸
    iris_width_match = re.search(r'#define\s+IRIS_MAP_WIDTH\s+(\d+)', content)
    iris_height_match = re.search(r'#define\s+IRIS_MAP_HEIGHT\s+(\d+)', content)

    iris_width = int(iris_width_match.group(1)) if iris_width_match else 0
    iris_height = int(iris_height_match.group(1)) if iris_height_match else 0

    # 提取sclera
    sclera_patterns = [
        r'const\s+uint16_t\s+sclera\[.*?\]\s*\[.*?\]\s*=\s*\{([^;]+)\};',
        r'const\s+uint16_t\s+sclera_\w+\[.*?\]\s*\[.*?\]\s*=\s*\{([^;]+)\};',
    ]

    sclera_data = None
    for pattern in sclera_patterns:
        match = re.search(pattern, content, re.DOTALL)
        if match:
            values_str = match.group(1)
            values = re.findall(r'0[xX][0-9A-Fa-f]+', values_str)
            sclera_data = [int(v, 16) for v in values]
            break

    # 提取iris
    iris_patterns = [
        r'const\s+uint16_t\s+iris\[.*?\]\s*\[.*?\]\s*=\s*\{([^;]+)\};',
        r'const\s+uint16_t\s+iris_\w+\[.*?\]\s*\[.*?\]\s*=\s*\{([^;]+)\};',
    ]

    iris_data = None
    for pattern in iris_patterns:
        match = re.search(pattern, content, re.DOTALL)
        if match:
            values_str = match.group(1)
            values = re.findall(r'0[xX][0-9A-Fa-f]+', values_str)
            iris_data = [int(v, 16) for v in values]
            break

    return sclera_data, iris_data, width, height, iris_width, iris_height

def resize_sclera(sclera_data, src_w, src_h, dst_w=375, dst_h=375):
    """调整sclera尺寸到375x375"""
    result = []
    x_ratio = src_w / dst_w
    y_ratio = src_h / dst_h

    for y in range(dst_h):
        for x in range(dst_w):
            src_x = min(int(x * x_ratio), src_w - 1)
            src_y = min(int(y * y_ratio), src_h - 1)
            result.append(sclera_data[src_y * src_w + src_x])

    return result

def resize_iris(iris_data, src_w, src_h, dst_w=256, dst_h=64):
    """调整iris尺寸到256x64"""
    result = []
    x_ratio = src_w / dst_w
    y_ratio = src_h / dst_h

    for y in range(dst_h):
        for x in range(dst_w):
            src_x = min(int(x * x_ratio), src_w - 1)
            src_y = min(int(y * y_ratio), src_h - 1)
            result.append(iris_data[src_y * src_w + src_x])

    return result

def generate_theme_file(theme_name, sclera_data, iris_data, orig_w, orig_h, has_iris):
    """生成主题文件"""
    namespace = theme_name.capitalize() + 'Theme'

    # 生成头文件
    header_content = f"""#ifndef {theme_name.upper()}_THEME_DATA_H
#define {theme_name.upper()}_THEME_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {{
#endif

namespace {namespace} {{
constexpr int IRIS_MIN = 180;
constexpr int IRIS_MAX = 280;
constexpr int SCLERA_WIDTH = 375;
constexpr int SCLERA_HEIGHT = 375;
extern const uint16_t* const {theme_name}_sclera;
"""

    if has_iris:
        header_content += f"extern const uint16_t* const {theme_name}_iris;\n"

    header_content += """}

#ifdef __cplusplus
}
#endif

#endif
"""

    header_file = f'main/display/{theme_name}_theme_data.h'
    with open(header_file, 'w', encoding='utf-8') as f:
        f.write(header_content)

    # 生成实现文件
    impl_content = f"""#include "{theme_name}_theme_data.h"

namespace {namespace} {{
static const uint16_t {theme_name}_sclera_data[375 * 375] = {{
"""

    for i in range(0, len(sclera_data), 16):
        line_values = sclera_data[i:i+16]
        impl_content += '    ' + ''.join([f'0x{v:04X}, ' for v in line_values]) + '\n'

    impl_content += "};\n\n"

    if has_iris and iris_data:
        impl_content += f"static const uint16_t {theme_name}_iris_data[256 * 64] = {{\n"
        for i in range(0, len(iris_data), 16):
            line_values = iris_data[i:i+16]
            impl_content += '    ' + ''.join([f'0x{v:04X}, ' for v in line_values]) + '\n'
        impl_content += "};\n\n"

    impl_content += f"const uint16_t* const {theme_name}_sclera = {theme_name}_sclera_data;\n"

    if has_iris and iris_data:
        impl_content += f"const uint16_t* const {theme_name}_iris = {theme_name}_iris_data;\n"

    impl_content += "\n} // namespace " + namespace + "\n"

    impl_file = f'main/display/{theme_name}_theme_data.cc'
    with open(impl_file, 'w', encoding='utf-8') as f:
        f.write(impl_content)

    print(f"  {theme_name}: {orig_w}x{orig_h} -> 375x375")
    print(f"    ✓ sclera: {len(sclera_data)} 像素")
    if has_iris and iris_data:
        print(f"    ✓ iris: {len(iris_data)} 像素")
    print(f"    ✓ {header_file}")
    print(f"    ✓ {impl_file}")
    return namespace

def main():
    if len(sys.argv) < 2:
        print("用法: python convert_theme_with_iris.py <Eye.h文件名>")
        print("示例: python convert_theme_with_iris.py catEye.h")
        sys.exit(1)

    theme_file = sys.argv[1]
    graphics_dir = 'main/display/graphics'
    file_path = os.path.join(graphics_dir, theme_file)

    if not os.path.exists(file_path):
        print(f"错误: 文件不存在 {file_path}")
        sys.exit(1)

    theme_name = theme_file.replace('Eye.h', '').replace('.h', '')
    print(f"转换 {theme_file}...")

    # 提取数据
    sclera_data, iris_data, width, height, iris_width, iris_height = extract_data(file_path)

    if not sclera_data or not width or not height:
        print(f"  错误: 无法提取sclera数据或检测尺寸")
        sys.exit(1)

    print(f"  检测到sclera尺寸: {width}x{height}")
    print(f"  提取sclera: {len(sclera_data)} 像素")

    has_iris = iris_data is not None and len(iris_data) > 1
    if has_iris:
        print(f"  检测到iris尺寸: {iris_width}x{iris_height}")
        print(f"  提取iris: {len(iris_data)} 像素")

    # 调整尺寸
    sclera_data = resize_sclera(sclera_data, width, height)

    if has_iris:
        iris_data = resize_iris(iris_data, iris_width, iris_height)

    # 生成文件
    namespace = generate_theme_file(theme_name, sclera_data, iris_data, width, height, has_iris)

    print(f"\n✓ 完成!")
    print(f"\n添加到 eye_themes.cc:")
    iris_var = f"{namespace}::{theme_name}_iris" if has_iris else "iris_xingkong"
    iris_comment = "" if has_iris else "  // 使用星空虹膜"
    print(f"""    {{
        .name = "{theme_name}",
        .sclera = {namespace}::{theme_name}_sclera,
        .iris = {iris_var},{iris_comment}
        .width = 375,
        .height = 375,
        .iris_min = {namespace}::IRIS_MIN,
        .iris_max = {namespace}::IRIS_MAX
    }}""")

if __name__ == '__main__':
    main()
