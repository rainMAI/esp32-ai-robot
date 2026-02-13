#!/usr/bin/env python3
"""
优化的 PNG 序列转 C 数组工具
生成更紧凑的数组格式，减少文件大小

使用方法:
    python png_to_array_optimized.py <png_folder> <output_name>

示例:
    python png_to_array_optimized.py ./eye anim_eye
"""

import os
import sys
from pathlib import Path
from PIL import Image
import argparse

def rgb888_to_rgb565(r, g, b):
    """将 RGB888 转换为 RGB565"""
    # RGB格式: 红色在高5位, 绿色在中6位, 蓝色在低5位
    # 注意: 不在这里做字节交换,而是在绘制时交换(和眼睛渲染逻辑一致)
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def process_png_sequence(folder_path, output_name, width=240, height=240, fps=12):
    """
    处理 PNG 序列并生成优化的 C 数组
    """
    folder = Path(folder_path)

    # 查找所有 PNG 文件并排序
    png_files = sorted(folder.glob("*.png"),
                      key=lambda x: int(x.stem.split('_')[-1]) if x.stem.split('_')[-1].isdigit() else 0)

    if not png_files:
        print(f"错误: 在 {folder} 中未找到 PNG 文件")
        return None

    print(f"找到 {len(png_files)} 个 PNG 文件")
    print(f"分辨率: {width}x{height}")
    print(f"帧率: {fps} FPS")

    # 收集所有帧数据
    all_frames = []
    for idx, png_file in enumerate(png_files):
        try:
            img = Image.open(png_file)
            if img.size != (width, height):
                img = img.resize((width, height), Image.Resampling.LANCZOS)
            if img.mode != 'RGB':
                img = img.convert('RGB')

            pixels = img.load()
            frame_data = []
            for y in range(height):
                for x in range(width):
                    r, g, b = pixels[x, y]
                    pixel = rgb888_to_rgb565(r, g, b)
                    frame_data.append(pixel)

            all_frames.append(frame_data)
            print(f"处理帧 {idx + 1}/{len(png_files)}: {png_file.name}")

        except Exception as e:
            print(f"处理 {png_file} 时出错: {e}")
            continue

    if not all_frames:
        print("错误: 没有成功处理任何帧")
        return None

    # 生成优化的 C 头文件
    header_content = generate_optimized_header(output_name, all_frames,
                                              width, height, fps, len(all_frames))
    return header_content

def generate_optimized_header(name, frames_data, width, height, fps, frame_count):
    """生成优化的 C 头文件内容"""

    bytes_per_frame = width * height * 2
    total_bytes = bytes_per_frame * frame_count
    kb_size = total_bytes / 1024

    header = f"""/**
 * 自动生成的动画数据（优化版）
 *
 * 动画信息:
 * - 帧数: {frame_count}
 * - 分辨率: {width}x{height}
 * - 帧率: {fps} FPS
 * - 总时长: {frame_count/fps:.2f} 秒
 * - 数据大小: {kb_size:.1f} KB
 * - 格式: RGB565 (紧凑格式)
 */

#ifndef ANIM_{name.upper()}_H
#define ANIM_{name.upper()}_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {{
#endif

// 动画配置
#define ANIM_{name.upper()}_FRAME_COUNT     {frame_count}
#define ANIM_{name.upper()}_WIDTH           {width}
#define ANIM_{name.upper()}_HEIGHT          {height}
#define ANIM_{name.upper()}_FPS             {fps}
#define ANIM_{name.upper()}_DURATION_MS     {(frame_count * 1000 / fps)}
#define ANIM_{name.upper()}_BYTES_PER_FRAME {bytes_per_frame}
#define ANIM_{name.upper()}_TOTAL_BYTES     {total_bytes}

// 所有帧数据（紧凑存储）
const uint16_t anim_{name}_data[{frame_count} * {width * height}] = {{
"""

    # 生成紧凑的像素数据
    for frame_idx, frame in enumerate(frames_data):
        # 每行16个像素，减少换行
        for i in range(0, len(frame), 16):
            line_pixels = frame[i:i+16]
            hex_values = ','.join(f'0x{p:04X}' for p in line_pixels)
            if frame_idx < frame_count - 1 or i + 16 < len(frame):
                header += f"    {hex_values},\n"
            else:
                header += f"    {hex_values}\n"

    # 结束数组定义，继续使用普通字符串拼接
    header += "};\n\n"

    # 帧指针数组
    header += f"// 帧指针数组\n"
    header += f"const uint16_t* anim_{name}_frames[{frame_count}] = {{\n"
    for i in range(frame_count):
        if i < frame_count - 1:
            header += f"    &anim_{name}_data[{i * width * height}],\n"
        else:
            header += f"    &anim_{name}_data[{i * width * height}]\n"
    header += "};\n\n"

    # 动画结构体
    header += f"// 动画结构体\n"
    header += "typedef struct {\n"
    header += "    const uint16_t** frames;\n"
    header += "    uint16_t frame_count;\n"
    header += "    uint16_t width;\n"
    header += "    uint16_t height;\n"
    header += "    uint8_t fps;\n"
    header += "    uint32_t duration_ms;\n"
    header += f"}} anim_{name}_t;\n\n"

    # 动画实例
    header += f"// 动画实例\n"
    header += f"const anim_{name}_t anim_{name} = {{\n"
    header += f"    .frames = anim_{name}_frames,\n"
    header += f"    .frame_count = {frame_count},\n"
    header += f"    .width = {width},\n"
    header += f"    .height = {height},\n"
    header += f"    .fps = {fps},\n"
    header += f"    .duration_ms = {int(frame_count * 1000 / fps)},\n"
    header += "};\n\n"

    # 结束头文件
    header += "#ifdef __cplusplus\n"
    header += "}\n"
    header += "#endif\n"
    header += f"#endif // ANIM_{name.upper()}_H\n"

    return header

def main():
    parser = argparse.ArgumentParser(
        description='优化的 PNG 序列转 C 数组工具（生成更小的文件）',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument('folder', help='PNG 序列文件夹路径')
    parser.add_argument('name', help='输出数组名称')
    parser.add_argument('--fps', type=int, default=12, help='帧率（默认 12）')
    parser.add_argument('--width', type=int, default=240, help='宽度（默认 240）')
    parser.add_argument('--height', type=int, default=240, help='高度（默认 240）')
    parser.add_argument('--output', '-o', help='输出文件路径')

    args = parser.parse_args()

    if not os.path.isdir(args.folder):
        print(f"错误: 文件夹 '{args.folder}' 不存在")
        sys.exit(1)

    # 处理 PNG 序列
    header_content = process_png_sequence(
        args.folder,
        args.name,
        width=args.width,
        height=args.height,
        fps=args.fps
    )

    if header_content is None:
        sys.exit(1)

    # 确定输出路径
    if args.output:
        output_path = args.output
    else:
        output_dir = Path(__file__).parent.parent / "main" / "display" / "animations"
        output_dir.mkdir(parents=True, exist_ok=True)
        output_path = output_dir / f"anim_{args.name}.h"

    # 写入文件
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(header_content)

    # 显示文件大小
    file_size = os.path.getsize(output_path) / (1024 * 1024)
    print(f"\n✅ 成功生成动画文件: {output_path}")
    print(f"📊 文件大小: {file_size:.2f} MB")

if __name__ == "__main__":
    main()
