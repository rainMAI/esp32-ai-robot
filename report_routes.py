"""
Report Routes - AI思维报告API路由
"""
from flask import Blueprint, request, jsonify, Response
from services.report_service import ReportService

report_bp = Blueprint('report', __name__)
report_service = ReportService()


@report_bp.route('/api/reports/generate', methods=['POST'])
def generate_report():
    """
    手动触发报告生成

    Request Body:
        {
            "device_mac": "aa:bb:cc:dd:ee:ff",
            "date": "2026-01-17"
        }

    Response:
        {"success": true, "report_id": 1, "chat_count": 10}
    """
    try:
        data = request.get_json()
        if not data:
            return jsonify({'success': False, 'error': 'Missing request body'}), 400

        device_mac = data.get('device_mac')
        target_date = data.get('date')

        if not device_mac or not target_date:
            return jsonify({'success': False, 'error': 'Missing device_mac or date'}), 400

        # 生成报告
        result = report_service.generate_daily_report(device_mac, target_date)

        if result['success']:
            status_code = 200 if not result.get('already_generated') else 201
            return jsonify(result), status_code
        else:
            return jsonify(result), 500

    except Exception as e:
        print(f"[ReportRoutes] Error in generate_report: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500


@report_bp.route('/api/reports/<device_mac>/<date>', methods=['GET'])
def get_report(device_mac: str, date: str):
    """
    获取指定日期的AI报告（HTML）

    Response:
        HTML内容
    """
    try:
        report = report_service.get_report(device_mac, date)

        if not report:
            return jsonify({'error': 'Report not found'}), 404

        # 返回HTML内容
        return Response(report['html_content'], mimetype='text/html')

    except Exception as e:
        print(f"[ReportRoutes] Error in get_report: {e}")
        return jsonify({'error': str(e)}), 500


@report_bp.route('/api/reports/<device_mac>/<date>/json', methods=['GET'])
def get_report_json(device_mac: str, date: str):
    """
    获取指定日期的AI报告（JSON格式）

    Response:
        {
            "report_date": "2026-01-17",
            "chat_count": 10,
            "generation_status": "success",
            "created_at": "2026-01-17T23:05:00"
        }
    """
    try:
        report = report_service.get_report(device_mac, date)

        if not report:
            return jsonify({'error': 'Report not found'}), 404

        # 返回JSON（不包含HTML内容）
        return jsonify({
            'report_id': report['id'],
            'device_mac': device_mac,
            'report_date': report['report_date'],
            'chat_count': report['chat_count'],
            'generation_status': report['generation_status'],
            'created_at': report['created_at']
        }), 200

    except Exception as e:
        print(f"[ReportRoutes] Error in get_report_json: {e}")
        return jsonify({'error': str(e)}), 500


def register_report_routes(app):
    """注册报告路由"""
    app.register_blueprint(report_bp)
    print("[OK] Report routes registered:")
    print("     - POST /api/reports/generate")
    print("     - GET  /api/reports/<device_mac>/<date>")
    print("     - GET  /api/reports/<device_mac>/<date>/json")
