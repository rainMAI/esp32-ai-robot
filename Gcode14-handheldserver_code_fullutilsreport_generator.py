"""
Report Generator - AI思维报告生成器
使用DeepSeek API生成报告内容
"""
import os


class ReportGenerator:
    """AI思维报告生成器"""

    def __init__(self):
        # 优先使用环境变量，否则使用默认值
        self.api_key = os.environ.get('LLM_API_KEY', 'sk-6dda6739fdc244379ac1109fdc9734ce')
        self.base_url = os.environ.get('LLM_BASE_URL', 'https://api.deepseek.com')
        self.model = os.environ.get('LLM_MODEL', 'deepseek-chat')

    def generate_report(self, chats: list, date: str, device_name: str) -> str:
        """
        生成AI思维报告

        Args:
            chats: 对话列表 [{"user_text": "...", "ai_text": "..."}]
            date: 报告日期
            device_name: 设备名称

        Returns:
            HTML格式的报告内容
        """
        if not chats:
            return self._generate_empty_report(date)

        # 构建对话上下文
        conversation_text = self._build_conversation_context(chats)

        # 调用DeepSeek API生成报告
        html_content = self._call_deepseek_api(conversation_text, date, device_name)

        return self._wrap_html_template(html_content, date, device_name)

    def _build_conversation_context(self, chats: list) -> str:
        """构建对话上下文字符串"""
        lines = []
        for i, chat in enumerate(chats, 1):
            lines.append(f"对话{i}:")
            lines.append(f"  用户: {chat['user_text']}")
            lines.append(f"  AI: {chat['ai_text']}")
            lines.append("")
        return "\n".join(lines)

    def _call_deepseek_api(self, conversation: str, date: str, device_name: str) -> str:
        """调用DeepSeek API生成报告"""
        try:
            # 禁用SSL警告，使用旧版OpenSSL兼容模式
            import urllib3
            urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

            import requests
            from requests.adapters import HTTPAdapter
            from urllib3.util.retry import Retry

            # 创建session，重试机制
            session = requests.Session()
            retry = Retry(total=3, backoff_factor=0.5)
            adapter = HTTPAdapter(max_retries=retry)
            session.mount('http://', adapter)
            session.mount('https://', adapter)

            headers = {
                'Authorization': f'Bearer {self.api_key}',
                'Content-Type': 'application/json'
            }

            prompt = f"""你是一个专业的AI思维分析师。基于以下用户与AI的对话记录，生成一份包含4个部分的思维报告：

对话记录（{date}，设备：{device_name}）：
{conversation}

请生成HTML格式的报告，包含以下4个部分：

1. **今日思维热点图** (Conversation Heatmap)
   - 使用词云或标签云展示对话主题
   - 列出出现频率最高的5个关键词

2. **关键概念网络** (Concept Network)
   - 提取对话中的3-5个关键概念
   - 展示概念之间的关联关系

3. **思维模式小奖章** (Thinking Medals)
   - 颁发3个有趣的思维亮点奖章
   - 每个奖章包含：名称、描述、获得的对话片段引用

4. **给明天的挑战** (Growth Challenges)
   - 基于对话内容，提出3个成长建议
   - 每个挑战包含：标题、具体建议、可执行步骤

要求：
- 使用HTML + CSS（内联样式）实现
- 使用响应式设计（适配移动端）
- 使用emoji或图标增强可读性
- 色彩搭配友好（避免过于鲜艳）
- 所有内容基于真实对话，不要编造

现在请生成报告（仅返回HTML内容，不要其他说明文字）："""

            payload = {
                'model': self.model,
                'messages': [
                    {
                        'role': 'system',
                        'content': '你是一个专业的AI思维分析师，擅长分析对话并生成可视化报告。'
                    },
                    {
                        'role': 'user',
                        'content': prompt
                    }
                ],
                'temperature': 0.7,
                'max_tokens': 4000
            }

            response = session.post(
                f"{self.base_url}/chat/completions",
                headers=headers,
                json=payload,
                timeout=60,
                verify=False  # 跳过SSL验证，兼容旧版OpenSSL
            )

            if response.status_code == 200:
                result = response.json()
                html_content = result['choices'][0]['message']['content']

                # 清理可能出现的markdown代码块标记
                if html_content.startswith('```html'):
                    html_content = html_content[7:]
                if html_content.startswith('```'):
                    html_content = html_content[3:]
                if html_content.endswith('```'):
                    html_content = html_content[:-3]

                return html_content.strip()
            else:
                print(f"DeepSeek API error: {response.status_code} - {response.text}")
                return self._generate_error_report("AI服务暂时不可用")

        except Exception as e:
            print(f"Error calling DeepSeek API: {e}")
            return self._generate_error_report(str(e))

    def _wrap_html_template(self, content: str, date: str, device_name: str) -> str:
        """包装完整HTML文档"""
        return f"""
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AI思维报告 - {date}</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Microsoft YaHei", sans-serif;
            line-height: 1.6;
            color: #333;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        }}
        .container {{
            background: white;
            border-radius: 16px;
            padding: 30px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.1);
        }}
        .header {{
            text-align: center;
            margin-bottom: 30px;
            padding-bottom: 20px;
            border-bottom: 2px solid #f0f0f0;
        }}
        .header h1 {{
            color: #2c3e50;
            margin: 0 0 10px 0;
            font-size: 28px;
        }}
        .header p {{
            color: #7f8c8d;
            margin: 5px 0;
            font-size: 14px;
        }}
        .content {{
            line-height: 1.8;
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📊 AI思维报告</h1>
            <p>📅 {date}</p>
            <p>🤖 {device_name}</p>
        </div>
        <div class="content">
            {content}
        </div>
    </div>
</body>
</html>
        """

    def _generate_empty_report(self, date: str) -> str:
        """生成空报告（当天无对话）"""
        return f"""
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AI思维报告 - {date}</title>
</head>
<body style="font-family: sans-serif; padding: 40px; text-align: center; background: #f5f5f5;">
    <div style="background: white; border-radius: 16px; padding: 40px; box-shadow: 0 4px 20px rgba(0,0,0,0.1);">
        <h2 style="color: #2c3e50;">📊 今日思维报告</h2>
        <p style="font-size: 18px; margin-top: 40px; color: #7f8c8d;">今天还没有对话记录</p>
        <p style="font-size: 14px; color: #95a5a6; margin-top: 20px;">明天再来查看吧~</p>
    </div>
</body>
</html>
        """

    def _generate_error_report(self, error_msg: str) -> str:
        """生成错误报告"""
        return f"""
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <title>报告生成失败</title>
</head>
<body style="font-family: sans-serif; padding: 40px; text-align: center; background: #fff5f5;">
    <div style="background: white; border-radius: 16px; padding: 40px; box-shadow: 0 4px 20px rgba(0,0,0,0.1);">
        <h2 style="color: #e74c3c;">⚠️ 报告生成失败</h2>
        <p style="font-size: 14px; margin-top: 20px; color: #7f8c8d;">错误信息：{error_msg}</p>
        <p style="font-size: 12px; color: #95a5a6; margin-top: 10px;">请稍后重试</p>
    </div>
</body>
</html>
        """
