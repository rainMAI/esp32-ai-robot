"""
Chat Service - 对话记录业务逻辑层
"""
import os
import sys
from datetime import datetime
from typing import List, Dict, Optional

# 添加项目路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from database.connection import get_db


class ChatService:
    """对话记录业务逻辑层"""

    def __init__(self):
        pass

    def batch_create(self, device_mac: str, messages: List[Dict]) -> Dict:
        """
        批量创建对话记录

        Args:
            device_mac: 设备MAC地址
            messages: 消息列表 [{"user_text": "...", "ai_text": "..."}]

        Returns:
            {"success": bool, "created_count": int, "device_id": int}
        """
        try:
            # 1. 验证设备是否存在
            device = self._get_device_by_mac(device_mac)
            if not device:
                return {'success': False, 'error': 'Device not found'}

            device_id = device['id']

            # 2. 服务器时间覆盖（毫秒时间戳）
            server_timestamp = int(datetime.now().timestamp() * 1000)

            # 3. 批量插入
            created_count = 0
            db = get_db()

            for msg in messages:
                if 'user_text' not in msg or 'ai_text' not in msg:
                    continue

                db.execute(
                    """INSERT INTO chat_messages
                       (device_id, user_text, ai_text, server_timestamp, created_at)
                       VALUES (?, ?, ?, ?, ?)""",
                    (device_id, msg['user_text'], msg['ai_text'],
                     server_timestamp, datetime.now().isoformat())
                )
                created_count += 1

            # 4. 更新设备最后在线时间
            db.execute(
                "UPDATE devices SET last_online_at = ? WHERE id = ?",
                (datetime.now().isoformat(), device_id)
            )

            return {
                'success': True,
                'created_count': created_count,
                'device_id': device_id
            }

        except Exception as e:
            print(f"[ChatService] Error in batch_create: {e}")
            import traceback
            traceback.print_exc()
            return {'success': False, 'error': str(e)}

    def get_daily_chats(self, device_mac: str, date: str) -> List[Dict]:
        """
        获取指定日期的对话记录

        Args:
            device_mac: 设备MAC地址
            date: 日期 (YYYY-MM-DD)

        Returns:
            对话记录列表
        """
        try:
            device = self._get_device_by_mac(device_mac)
            if not device:
                return []

            device_id = device['id']

            # 计算时间范围
            from datetime import timedelta
            start_dt = datetime.strptime(date, "%Y-%m-%d")
            end_dt = start_dt + timedelta(days=1)

            start_ts = int(start_dt.timestamp() * 1000)
            end_ts = int(end_dt.timestamp() * 1000)

            # 查询对话
            db = get_db()
            rows = db.execute(
                """SELECT user_text, ai_text, server_timestamp, created_at
                   FROM chat_messages
                   WHERE device_id = ? AND server_timestamp >= ? AND server_timestamp < ?
                   ORDER BY server_timestamp ASC""",
                (device_id, start_ts, end_ts),
                fetch_all=True
            )

            return [
                {
                    'user_text': row[0],
                    'ai_text': row[1],
                    'server_timestamp': row[2],
                    'created_at': row[3]
                }
                for row in rows
            ]

        except Exception as e:
            print(f"[ChatService] Error in get_daily_chats: {e}")
            return []

    def get_recent_chats(self, device_mac: str, limit: int = 100, offset: int = 0) -> List[Dict]:
        """
        获取最近的对话记录

        Args:
            device_mac: 设备MAC地址
            limit: 返回数量限制
            offset: 偏移量

        Returns:
            对话记录列表
        """
        try:
            device = self._get_device_by_mac(device_mac)
            if not device:
                return []

            device_id = device['id']

            # 查询最近的对话
            db = get_db()
            rows = db.execute(
                """SELECT user_text, ai_text, server_timestamp, created_at
                   FROM chat_messages
                   WHERE device_id = ?
                   ORDER BY server_timestamp DESC
                   LIMIT ? OFFSET ?""",
                (device_id, limit, offset),
                fetch_all=True
            )

            return [
                {
                    'user_text': row[0],
                    'ai_text': row[1],
                    'server_timestamp': row[2],
                    'created_at': row[3]
                }
                for row in rows
            ]

        except Exception as e:
            print(f"[ChatService] Error in get_recent_chats: {e}")
            return []

    def _get_device_by_mac(self, mac_address: str) -> Optional[Dict]:
        """通过MAC地址获取设备"""
        try:
            # 标准化MAC地址
            import re
            mac_clean = re.sub(r'[:.-]', '', mac_address)
            mac_normalized = ':'.join([mac_clean[i:i+2] for i in range(0, 12, 2)]).lower()

            db = get_db()
            row = db.execute(
                "SELECT id, mac_address, device_name FROM devices WHERE mac_address = ?",
                (mac_normalized,),
                fetch_one=True
            )

            if row:
                return {'id': row[0], 'mac_address': row[1], 'device_name': row[2]}
            return None

        except Exception as e:
            print(f"[ChatService] Error in _get_device_by_mac: {e}")
            return None
