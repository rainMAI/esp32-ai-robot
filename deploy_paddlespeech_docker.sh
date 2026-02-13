#!/bin/bash
# PaddleSpeech Docker 部署脚本

set -e

echo "=== PaddleSpeech Docker 部署 ==="

# 1. 检查 Docker 是否安装
if ! command -v docker &> /dev/null; then
    echo "❌ Docker 未安装，请先安装 Docker"
    echo "安装命令: curl -fsSL https://get.docker.com | sh"
    exit 1
fi

echo "✅ Docker 版本: $(docker --version)"

# 2. 创建 PaddleSpeech 专用目录
mkdir -p /opt/paddlespeech_tts
cd /opt/paddlespeech_tts

# 3. 创建 Dockerfile
cat > Dockerfile << 'EOF'
FROM python:3.9-slim

# 安装系统依赖
RUN apt-get update && apt-get install -y \
    gcc \
    g++ \
    libgomp1 \
    libglib2.0-0 \
    libsm6 \
    libxext6 \
    libxrender-dev \
    libgl1-mesa-glx \
    ffmpeg \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /app

# 安装 PaddleSpeech TTS
RUN pip install --no-cache-dir paddlepaddle-gpu==2.5.2 \
    && pip install --no-cache-dir paddlespeech \
    && pip install --no-cache-dir pydub

# 创建音频输出目录
RUN mkdir -p /app/output

# 复制 TTS 脚本
COPY tts_server.py /app/

# 暴露端口
EXPOSE 5000

# 启动 TTS 服务
CMD ["python", "-u", "tts_server.py"]
EOF

# 4. 创建 TTS 服务脚本
cat > tts_server.py << 'EOF'
#!/usr/bin/env python3
"""
PaddleSpeech TTS HTTP 服务
监听端口 5000，提供 /tts 接口
"""

from flask import Flask, request, jsonify, Response
import tempfile
import os
from paddlespeech.cli.tts.infer import TTSExecutor

app = Flask(__name__)

# 初始化 TTS 引擎
print("正在加载 PaddleSpeech TTS 模型...")
tts = TTSExecutor()
print("✅ TTS 模型加载完成")

@app.route('/tts', methods=['POST'])
def text_to_speech():
    """将文本转换为语音 (返回 WAV 格式)"""
    try:
        data = request.get_json()
        if not data or 'text' not in data:
            return jsonify({"error": "Missing 'text' parameter"}), 400

        text = data['text']
        print(f"\n[TTS] 收到请求: {text}")

        # 生成临时文件
        with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
            output_path = f.name

        # 使用 PaddleSpeech 生成语音
        tts(text=text, output=output_path)

        # 读取音频文件
        with open(output_path, 'rb') as f:
            audio_data = f.read()

        # 删除临时文件
        os.unlink(output_path)

        print(f"✅ [TTS] 生成成功: {len(audio_data)} 字节")

        return Response(
            audio_data,
            mimetype='audio/wav',
            headers={'Content-Length': str(len(audio_data))}
        )

    except Exception as e:
        print(f"❌ [TTS] 错误: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({"error": str(e)}), 500

@app.route('/health', methods=['GET'])
def health():
    """健康检查"""
    return jsonify({"status": "ok", "engine": "PaddleSpeech"})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False)
EOF

# 5. 构建 Docker 镜像
echo "📦 构建 PaddleSpeech Docker 镜像（这可能需要 10-20 分钟）..."
docker build -t paddlespeech-tts:latest .

# 6. 停止旧容器（如果存在）
echo "🛑 停止旧容器..."
docker stop paddlespeech-tts 2>/dev/null || true
docker rm paddlespeech-tts 2>/dev/null || true

# 7. 启动新容器
echo "🚀 启动 PaddleSpeech TTS 服务..."
docker run -d \
    --name paddlespeech-tts \
    --restart unless-stopped \
    -p 5000:5000 \
    paddlespeech-tts:latest

# 8. 等待服务启动
echo "⏳ 等待服务启动..."
sleep 10

# 9. 测试服务
echo "🧪 测试 TTS 服务..."
curl -s -X POST http://localhost:5000/tts \
    -H 'Content-Type: application/json' \
    -d '{"text":"喝水时间到了"}' \
    -o /tmp/test_tts.wav

if [ -f /tmp/test_tts.wav ] && [ -s /tmp/test_tts.wav ]; then
    echo "✅ TTS 测试成功！音频文件: /tmp/test_tts.wav"
    ls -lh /tmp/test_tts.wav
else
    echo "❌ TTS 测试失败"
    docker logs paddlespeech-tts --tail 50
    exit 1
fi

# 10. 显示容器日志
echo ""
echo "=== PaddleSpeech TTS 服务已启动 ==="
echo "容器名称: paddlespeech-tts"
echo "服务地址: http://localhost:5000"
echo ""
echo "查看日志: docker logs -f paddlespeech-tts"
echo "停止服务: docker stop paddlespeech-tts"
echo "重启服务: docker restart paddlespeech-tts"
echo ""
echo "使用示例:"
echo 'curl -X POST http://localhost:5000/tts -H "Content-Type: application/json" -d "{\"text\":\"你好\"}" -o output.wav'
EOF