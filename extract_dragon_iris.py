#!/usr/bin/env python3
"""
提取龙眼的虹膜数据并调整尺寸
龙眼虹膜: 80x512 -> 需要调整为 256x64 以匹配其他主题
"""

import re
import sys

def extract_iris_from_dragon_eye():
    """从dragonEye.h提取虹膜数据"""
    input_file = 'main/display/graphics/dragonEye.h'

    with open(input_file, 'r', encoding='utf-8') as f:
        content = f.read()

    # 查找虹膜数组
    pattern = r'const\s+uint16_t\s+iris\[IRIS_MAP_HEIGHT\]\s*\[IRIS_MAP_WIDTH\]\s*=\s*\{([^;]+)\};'
    match = re.search(pattern, content, re.DOTALL)

    if not match:
        print("错误: 无法找到虹膜数据")
        sys.exit(1)

    data_str = match.group(1)
    values = re.findall(r'0X[0-9A-Fa-f]+', data_str)
    iris_80x512 = [int(v, 16) for v in values]

    expected_size = 80 * 512  # 40960
    if len(iris_80x512) != expected_size:
        print(f"错误: 虹膜数据大小不匹配,期望{expected_size},实际{len(iris_80x512)}")
        sys.exit(1)

    print(f"✓ 提取到虹膜数据: 80x512 ({len(iris_80x512)} 像素)")
    return iris_80x512

def resize_iris_data(iris_80x512):
    """将80x512的虹膜数据调整为256x64"""
    # 从80行512列 调整为 64行256列
    # 需要重新采样: 80->64行 (压缩0.8倍), 512->256列 (压缩0.5倍)

    new_iris = []
    src_height = 80
    src_width = 512
    dst_height = 64
    dst_width = 256

    for dst_y in range(dst_height):
        # 计算源图像中的对应行
        src_y = int(dst_y * src_height / dst_height)

        for dst_x in range(dst_width):
            # 计算源图像中的对应列
            src_x = int(dst_x * src_width / dst_width)

            # 获取源像素
            src_idx = src_y * src_width + src_x
            new_iris.append(iris_80x512[src_idx])

    print(f"✓ 虹膜数据调整: 80x512 -> {dst_height}x{dst_width} ({len(new_iris)} 像素)")
    return new_iris

def update_dragon_theme_files(iris_256x64):
    """更新龙眼主题文件,添加虹膜数据"""

    # 更新头文件
    header_file = 'main/display/dragon_theme_data.h'
    with open(header_file, 'r', encoding='utf-8') as f:
        header_content = f.read()

    # 检查是否已经添加了虹膜声明
    if 'dragon_iris' in header_content:
        print("虹膜数据已存在,跳过头文件更新")
    else:
        # 在虹膜数据注释行后添加声明
        new_header = header_content.replace(
            '// 虹膜数据 - 暂时使用xingkong的虹膜\n// extern const uint16_t iris[];',
            '// 虹膜数据 (256x64 = 16384 pixels)\n// 从原始80x512龙眼虹膜调整而来\nextern const uint16_t* const dragon_iris;'
        )
        with open(header_file, 'w', encoding='utf-8') as f:
            f.write(new_header)
        print(f"✓ 更新头文件: {header_file}")

    # 更新实现文件
    impl_file = 'main/display/dragon_theme_data.cc'
    with open(impl_file, 'r', encoding='utf-8') as f:
        impl_content = f.read()

    # 检查是否已经添加了虹膜数据
    if 'dragon_iris_data' in impl_content:
        print("虹膜数据已存在,跳过实现文件更新")
        return

    # 在dragon_sclera定义后添加虹膜数据
    iris_data_str = '\n'.join([
        '',
        '// 虹膜数据 (256x64 = 16384 pixels)',
        '// 从原始80x512龙眼虹膜通过最近邻插值调整',
        'static const uint16_t dragon_iris_data[256 * 64] = {'
    ])

    # 每16个值一行
    for i in range(0, len(iris_256x64), 16):
        line_values = iris_256x64[i:i+16]
        line = '    ' + ''.join([f'0x{v:04X}, ' for v in line_values]) + '\n'
        iris_data_str += line

    iris_data_str += '};\n\n'
    iris_data_str += '// 导出虹膜数据指针\n'
    iris_data_str += 'const uint16_t* const dragon_iris = dragon_iris_data;\n'

    # 在命名空间结束前插入
    new_impl = impl_content.replace(
        '} // namespace DragonTheme',
        iris_data_str + '\n} // namespace DragonTheme'
    )

    with open(impl_file, 'w', encoding='utf-8') as f:
        f.write(new_impl)

    print(f"✓ 更新实现文件: {impl_file}")
    print(f"  - 添加了虹膜数据数组 (256x64)")
    print(f"  - 添加了虹膜数据指针导出")

def main():
    print("提取龙眼虹膜数据...")

    # 1. 提取原始虹膜数据
    iris_80x512 = extract_iris_from_dragon_eye()

    # 2. 调整尺寸
    iris_256x64 = resize_iris_data(iris_80x512)

    # 3. 更新文件
    update_dragon_theme_files(iris_256x64)

    print("\n✓ 龙眼虹膜数据添加完成!")
    print("\n下一步: 更新 eye_themes.cc 使用龙眼虹膜")
    print("  将 .iris = iris_xingkong 改为 .iris = DragonTheme::dragon_iris")

if __name__ == '__main__':
    main()
