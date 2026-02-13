"""
Report Service - AI思维报告业务逻辑层
"""
import os
import sys
from datetime import datetime, timedelta
from typing import List, Dict, Optional

# 添加项目路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from database.connection import get_db
from services.chat_service import ChatService


class ReportService:
    """AI思维报告业务逻辑层"""

    def __init__(self):
        self.chat_service = ChatService()

    def generate_daily_report(self, device_mac: str, target_date: str) -> Dict:
        """
        生成每日AI思维报告

        Args:
            device_mac: 设备MAC地址
            target_date: 目标日期 (YYYY-MM-DD)

        Returns:
            {"success": bool, "report_id": int, "chat_count": int}
        """
        try:
            # 1. 获取设备信息
            device = self.chat_service._get_device_by_mac(device_mac)
            if not device:
                return {'success': False, 'error': 'Device not found'}

            device_id = device['id']
            device_name = device['device_name']

            # 2. 检查是否已生成
            existing = self._get_report_by_device_and_date(device_id, target_date)
            if existing:
                return {
                    'success': True,
                    'already_generated': True,
                    'report_id': existing['id'],
                    'chat_count': existing['chat_count']
                }

            # 3. 获取对话数据
            chats = self.chat_service.get_daily_chats(device_mac, target_date)
            if not chats:
                return {'success': False, 'error': 'No conversations found'}

            # 4. 生成报告（这里先创建一个简单版本，后续集成LLM）
            from utils.report_generator import ReportGenerator
            generator = ReportGenerator()

            html_content = generator.generate_report(
                chats=chats,
                date=target_date,
                device_name=device_name
            )

            # 5. 保存报告
            report_id = self._save_report(
                device_id=device_id,
                report_date=target_date,
                html_content=html_content,
                chat_count=len(chats)
            )

            print(f"✅ [ReportService] Report generated for {device_mac} on {target_date} ({len(chats)} chats)")

            return {
                'success': True,
                'report_id': report_id,
                'chat_count': len(chats)
            }

        except Exception as e:
            print(f"❌ [ReportService] Error generating report: {e}")
            import traceback
            traceback.print_exc()
            return {'success': False, 'error': str(e)}

    def get_report(self, device_mac: str, date: str) -> Optional[Dict]:
        """查询指定日期的报告"""
        try:
            device = self.chat_service._get_device_by_mac(device_mac)
            if not device:
                return None

            return self._get_report_by_device_and_date(device['id'], date)

        except Exception as e:
            print(f"[ReportService] Error in get_report: {e}")
            return None

    def _get_report_by_device_and_date(self, device_id: int, report_date: str) -> Optional[Dict]:
        """通过设备ID和日期查询报告"""
        try:
            db = get_db()
            row = db.execute(
                """SELECT id, device_id, report_date, html_content, chat_count,
                          generation_status, created_at
                   FROM ai_reports
                   WHERE device_id = ? AND report_date = ?""",
                (device_id, report_date),
                fetch_one=True
            )

            if row:
                return {
                    'id': row[0],
                    'device_id': row[1],
                    'report_date': row[2],
                    'html_content': row[3],
                    'chat_count': row[4],
                    'generation_status': row[5],
                    'created_at': row[6]
                }
            return None

        except Exception as e:
            print(f"[ReportService] Error in _get_report_by_device_and_date: {e}")
            return None

    def _save_report(self, device_id: int, report_date: str,
                     html_content: str, chat_count: int) -> int:
        """保存报告到数据库"""
        try:
            db = get_db()
            now = datetime.now().isoformat()

            with db.get_connection() as conn:
                cursor = conn.cursor()
                cursor.execute(
                    """INSERT INTO ai_reports
                       (device_id, report_date, html_content, chat_count,
                        generation_status, created_at, updated_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?)""",
                    (device_id, report_date, html_content, chat_count,
                     'success', now, now)
                )
                conn.commit()
                return cursor.lastrowid

        except Exception as e:
            print(f"[ReportService] Error in _save_report: {e}")
            raise

    def get_all_active_devices(self) -> List[Dict]:
        """获取所有活跃设备（最近7天有上线记录）"""
        try:
            db = get_db()
            rows = db.execute(
                """SELECT id, device_name, mac_address
                   FROM devices
                   WHERE last_online_at >= datetime('now', '-7 days')
                     AND is_online = 1""",
                fetch_all=True
            )

            return [
                {
                    'id': row[0],
                    'device_name': row[1],
                    'mac_address': row[2]
                }
                for row in rows
            ]

        except Exception as e:
            print(f"[ReportService] Error in get_all_active_devices: {e}")
            return []
