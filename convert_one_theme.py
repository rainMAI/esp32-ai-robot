#!/usr/bin/env python3
"""
单个转换graphics文件夹中的眼睛主题
自动检测尺寸,转换为375x375
"""

import re
import sys
import os

def detect_theme_size(file_path):
    """自动检测主题文件的实际尺寸"""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 方法1: 从宏定义获取
    width_match = re.search(r'#define\s+SCLERA_WIDTH\s+(\d+)', content)
    height_match = re.search(r'#define\s+SCLERA_HEIGHT\s+(\d+)', content)

    if width_match and height_match:
        return int(width_match.group(1)), int(height_match.group(1))

    # 方法2: 从数组声明获取
    array_match = re.search(r'const\s+uint16_t\s+sclera\[(\d+)\]\s*\[(\d+)\]', content)
    if array_match:
        return int(array_match.group(2)), int(array_match.group(1))

    return None, None

def extract_sclera_data(file_path):
    """提取sclera数据"""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 尝试多种可能的数组名称
    patterns = [
        r'const\s+uint16_t\s+sclera\[.*?\]\s*\[.*?\]\s*=\s*\{([^;]+)\};',
        r'const\s+uint16_t\s+sclera_\w+\[.*?\]\s*\[.*?\]\s*=\s*\{([^;]+)\};',
    ]

    for pattern in patterns:
        match = re.search(pattern, content, re.DOTALL)
        if match:
            values_str = match.group(1)
            values = re.findall(r'0[xX][0-9A-Fa-f]+', values_str)
            return [int(v, 16) for v in values]

    return None

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

def generate_theme_file(theme_name, sclera_data, orig_w, orig_h):
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
}}

#ifdef __cplusplus
}}
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
    impl_content += f"const uint16_t* const {theme_name}_sclera = {theme_name}_sclera_data;\n"
    impl_content += "\n} // namespace " + namespace + "\n"

    impl_file = f'main/display/{theme_name}_theme_data.cc'
    with open(impl_file, 'w', encoding='utf-8') as f:
        f.write(impl_content)

    print(f"  {theme_name}: {orig_w}x{orig_h} -> 375x375")
    print(f"    ✓ {header_file}")
    print(f"    ✓ {impl_file}")
    return namespace

def main():
    if len(sys.argv) < 2:
        print("用法: python convert_one_theme.py <Eye.h文件名>")
        print("示例: python convert_one_theme.py catEye.h")
        sys.exit(1)

    theme_file = sys.argv[1]
    graphics_dir = 'main/display/graphics'
    file_path = os.path.join(graphics_dir, theme_file)

    if not os.path.exists(file_path):
        print(f"错误: 文件不存在 {file_path}")
        sys.exit(1)

    theme_name = theme_file.replace('Eye.h', '').replace('.h', '')
    print(f"转换 {theme_file}...")

    # 检测尺寸
    width, height = detect_theme_size(file_path)
    if not width or not height:
        print(f"  错误: 无法检测尺寸")
        sys.exit(1)

    print(f"  检测到尺寸: {width}x{height}")

    # 提取数据
    sclera_data = extract_sclera_data(file_path)
    if not sclera_data or len(sclera_data) == 0:
        print(f"  错误: 无sclera数据")
        sys.exit(1)

    print(f"  提取到 {len(sclera_data)} 像素")

    # 调整尺寸并生成文件
    namespace = generate_theme_file(theme_name, sclera_data, width, height)

    print(f"\n✓ 完成!")
    print(f"\n添加到 CMakeLists.txt:")
    print(f'    "display/{theme_name}_theme_data.cc"')
    print(f"\n添加到 eye_themes.h:")
    print(f'    EYE_THEME_{theme_name.upper()} = 4,')
    print(f"\n添加到 eye_themes.cc:")
    print(f"""    {{
        .name = "{theme_name}",
        .sclera = {namespace}::{theme_name}_sclera,
        .iris = iris_xingkong,
        .width = 375,
        .height = 375,
        .iris_min = {namespace}::IRIS_MIN,
        .iris_max = {namespace}::IRIS_MAX
    }}""")

if __name__ == '__main__':
    main()
