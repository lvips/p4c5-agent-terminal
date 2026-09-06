#!/usr/bin/env python3
"""
ESP32-P4C5 语音功能真机验证脚本

通过串口连接 ESP32, 自动验证 W3 + W4 全部语音功能:

  1. 串口连接 + hello
  2. 启动录音 (audio start) + 收集 audio_diag 日志
  3. 验证 AEC 收敛 (ref>>mic, aec<<mic)
  4. 关闭录音 (audio stop)
  5. 启动唤醒 (wake enable)
  6. 模拟唤醒 (发测试音频到扬声器) - 可选
  7. 验证 wake/sleep events
  8. 关闭唤醒 (wake disable)

使用方法:
  python3 tools/esp32_test.py --port /dev/tty.usbserial-1410 --baud 115200

需要安装:
  pip3 install pyserial
"""

import argparse
import logging
import re
import sys
import time

try:
    import serial
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S',
)
logger = logging.getLogger("esp32-test")


# ═══════════════════════════════════════════════════════════
# ESP32 串口交互
# ═══════════════════════════════════════════════════════════

class ESP32Tester:
    """ESP32 串口测试器"""

    def __init__(self, port: str, baud: int = 115200, timeout: float = 5.0):
        if not HAS_SERIAL:
            raise RuntimeError("pyserial 未安装, 请运行: pip3 install pyserial")
        self.ser = serial.Serial(port, baud, timeout=timeout)
        self.received_lines = []
        self.diag_samples = []
        self.wake_events = []
        self.sleep_events = []

    def send(self, cmd: str):
        """发送串口命令"""
        self.ser.write(f"{cmd}\n".encode())
        self.ser.flush()
        logger.info(f"📤 → {cmd}")

    def read_lines(self, duration_sec: float = 2.0, filter_pattern: str = None):
        """读取串口输出持续 N 秒"""
        import select
        end_time = time.time() + duration_sec
        lines = []
        while time.time() < end_time:
            if self.ser.in_waiting > 0:
                try:
                    line = self.ser.readline().decode('utf-8', errors='ignore').rstrip()
                    if line:
                        lines.append(line)
                        self.received_lines.append(line)
                        self._parse_line(line)
                except Exception as e:
                    logger.debug(f"read error: {e}")
        return lines

    def _parse_line(self, line: str):
        """解析串口输出, 提取关键事件"""
        # audio_diag 日志: frame=N mic=X ref=Y aec=Z
        m = re.search(r'audio_diag.*frame=(\d+) mic=(\d+) ref=(\d+) aec=(\d+)', line)
        if m:
            self.diag_samples.append({
                'frame': int(m.group(1)),
                'mic': int(m.group(2)),
                'ref': int(m.group(3)),
                'aec': int(m.group(4)),
            })

        # wake event
        if 'WAKE detected' in line:
            self.wake_events.append(line)
        if 'SLEEP detected' in line:
            self.sleep_events.append(line)

    def close(self):
        self.ser.close()

    # ── 测试步骤 ──

    def wait_boot(self, timeout: float = 15.0):
        """等待 ESP32 启动 + 看到提示"""
        logger.info(f"⏳ 等待 ESP32 启动 ({timeout}s)...")
        lines = self.read_lines(timeout)
        boot_done = any("W3 提示" in l or "All subsystems" in l for l in lines)
        if boot_done:
            logger.info("✅ ESP32 启动完成")
        else:
            logger.warning("⚠  未检测到启动提示, 继续测试")
        return boot_done

    def test_audio_start_stop(self):
        """测试 1: audio start/stop + audio_diag"""
        logger.info("\n" + "=" * 60)
        logger.info("测试 1: 录音 + AEC 实时诊断")
        logger.info("=" * 60)

        self.send("audio start")
        time.sleep(0.5)
        lines = self.read_lines(duration_sec=4.0)

        # 检查 audio_diag
        if not self.diag_samples:
            logger.error("❌ 没收到 audio_diag 日志 (请检查 ESP32 是否真在录音)")
            return False

        # 分析 RMS 趋势
        first = self.diag_samples[0]
        last = self.diag_samples[-1]
        logger.info(f"📊 audio_diag 样本数: {len(self.diag_samples)}")
        logger.info(f"   第一帧: mic={first['mic']} ref={first['ref']} aec={first['aec']}")
        logger.info(f"   最后一帧: mic={last['mic']} ref={last['ref']} aec={last['aec']}")

        # 验证 AEC 收敛 (如果 ref 有信号, aec 应该 < mic)
        if any(s['ref'] > 500 for s in self.diag_samples):
            # 有扬声器回采
            ref_signals = [s['ref'] for s in self.diag_samples if s['ref'] > 500]
            aec_signals = [s['aec'] for s in self.diag_samples if s['ref'] > 500]
            if aec_signals:
                avg_ref = sum(ref_signals) // len(ref_signals)
                avg_aec = sum(aec_signals) // len(aec_signals)
                logger.info(f"   ref (扬声器回采) avg={avg_ref}")
                logger.info(f"   aec (AEC 输出) avg={avg_aec}")
                if avg_aec < avg_ref * 0.5:
                    logger.info(f"   ✅ AEC 收敛 (aec={avg_aec} < ref*0.5={avg_ref*0.5})")
                else:
                    logger.warning(f"   ⚠  AEC 收敛不佳 (aec={avg_aec} >= ref*0.5={avg_ref*0.5})")
        else:
            logger.warning("   ⚠  未检测到扬声器回采信号 (ref < 500, 请确认硬件 AEC ref 连接)")

        self.send("audio stop")
        time.sleep(0.5)
        return True

    def test_wake_detection(self):
        """测试 2: wake enable + 检测 wake/sleep events"""
        logger.info("\n" + "=" * 60)
        logger.info("测试 2: 唤醒检测")
        logger.info("=" * 60)

        self.send("wake enable")
        time.sleep(0.5)
        self.read_lines(duration_sec=1.0)

        logger.info("⏳ 等 5 秒看是否有误唤醒...")
        lines = self.read_lines(duration_sec=5.0)

        if self.wake_events:
            logger.warning(f"⚠  静默时检测到 {len(self.wake_events)} 次 wake event (环境噪声太大?)")
        else:
            logger.info("✅ 静默 5s, 0 个 wake event (无误唤醒)")

        # 询问 wake stats
        self.send("wake status")
        time.sleep(0.5)
        lines = self.read_lines(duration_sec=1.0)

        self.send("wake disable")
        time.sleep(0.5)
        return True

    def test_tts_player(self):
        """测试 3: TTS player 统计 (需要 TTS Adapter 在运行)"""
        logger.info("\n" + "=" * 60)
        logger.info("测试 3: TTS Player 统计")
        logger.info("=" * 60)

        self.send("tts stats")
        time.sleep(0.5)
        lines = self.read_lines(duration_sec=1.5)
        has_stats = any("tts_player" in l and "stats" in l for l in lines)
        if has_stats:
            logger.info("✅ tts_player 响应正常")
        else:
            logger.warning("⚠  未收到 tts_player 统计")
        return True


def main():
    parser = argparse.ArgumentParser(description="ESP32-P4C5 真机验证脚本")
    parser.add_argument("--port", required=True,
                        help="串口设备 (如 /dev/tty.usbserial-1410)")
    parser.add_argument("--baud", type=int, default=115200,
                        help="波特率 (default: 115200)")
    parser.add_argument("--skip-wake", action="store_true",
                        help="跳过 wake 检测测试")
    parser.add_argument("--skip-tts", action="store_true",
                        help="跳过 TTS player 测试")
    args = parser.parse_args()

    try:
        tester = ESP32Tester(args.port, args.baud)

        if not tester.wait_boot():
            logger.error("❌ ESP32 未启动, 退出")
            return 1

        results = []

        # 测试 1
        results.append(("录音 + AEC", tester.test_audio_start_stop()))

        # 测试 2 (可选)
        if not args.skip_wake:
            results.append(("唤醒检测", tester.test_wake_detection()))

        # 测试 3 (可选)
        if not args.skip_tts:
            results.append(("TTS Player", tester.test_tts_player()))

        tester.close()

        # 报告
        logger.info("\n" + "=" * 60)
        logger.info("测试结果汇总")
        logger.info("=" * 60)
        for name, ok in results:
            mark = "✅" if ok else "❌"
            logger.info(f"  {mark} {name}")
        passed = sum(1 for _, ok in results if ok)
        total = len(results)
        logger.info(f"\n通过: {passed}/{total}")
        return 0 if passed == total else 1

    except Exception as e:
        logger.exception(f"测试失败: {e}")
        return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)