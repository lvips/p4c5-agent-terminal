#!/usr/bin/env python3
"""
Wake Word Detector 单元测试 (POC)

模拟 ESP32 wake_word_detector.cpp 的逻辑, 用 Python 验证:
1. 静默 → 不触发
2. 持续语音 → 触发 WAKE event
3. 一句话后静音 2s → 触发 SLEEP event
4. 多轮对话循环

完整模拟 ESP32 C++ 代码, 不依赖硬件。
"""

import logging
import sys
import time
from enum import Enum

import numpy as np

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S',
)
logger = logging.getLogger("wake_test")


class WakeEvent(Enum):
    NONE = "NONE"
    WAKE = "WAKE"
    SLEEP = "SLEEP"


class WakeDetector:
    """模拟 ESP32 wake_word_detector (C++ 实现)

    完整复刻 cpp 逻辑:
      - calc_rms 用整数牛顿法开方
      - 唤醒计数/睡眠计数
      - WAKE/SLEEP event 触发
    """

    def __init__(self, sample_rate=24000,
                 wake_rms_threshold=1500,
                 sleep_rms_threshold=500,
                 wake_hold_frames=5,
                 sleep_hold_frames=100):
        self.cfg = {
            'sample_rate': sample_rate,
            'wake_rms_threshold': wake_rms_threshold,
            'sleep_rms_threshold': sleep_rms_threshold,
            'wake_hold_frames': wake_hold_frames,
            'sleep_hold_frames': sleep_hold_frames,
        }
        self.wake_count = 0
        self.sleep_count = 0
        self.active = False
        self.frames_processed = 0
        self.wake_events = 0
        self.sleep_events = 0

    @staticmethod
    def calc_rms(buf):
        """整数牛顿法开方"""
        sum_sq = sum(int(v) * int(v) for v in buf)
        mean = sum_sq // len(buf)
        if mean == 0:
            return 0
        x = mean
        y = (x + 1) >> 1
        while y < x:
            x = y
            y = (x + mean // x) >> 1
        return x

    def feed(self, pcm):
        self.frames_processed += 1
        rms = self.calc_rms(pcm)

        if not self.active:
            if rms > self.cfg['wake_rms_threshold']:
                self.wake_count += 1
                if self.wake_count >= self.cfg['wake_hold_frames']:
                    self.active = True
                    self.wake_count = 0
                    self.sleep_count = 0
                    self.wake_events += 1
                    logger.info(f"🌟 WAKE detected! (RMS={rms}, events={self.wake_events})")
                    return WakeEvent.WAKE
            else:
                if self.wake_count > 0:
                    self.wake_count -= 1
        else:
            if rms < self.cfg['sleep_rms_threshold']:
                self.sleep_count += 1
                if self.sleep_count >= self.cfg['sleep_hold_frames']:
                    self.active = False
                    self.sleep_count = 0
                    self.wake_count = 0
                    self.sleep_events += 1
                    logger.info(f"💤 SLEEP detected (RMS={rms}, events={self.sleep_events})")
                    return WakeEvent.SLEEP
            else:
                if self.sleep_count > 0:
                    self.sleep_count -= 1

        return WakeEvent.NONE

    def print_stats(self):
        logger.info(f"📊 [stats] frames={self.frames_processed} "
                    f"wake={self.wake_events} sleep={self.sleep_events} "
                    f"active={self.active}")


def make_silence_frame(n_samples=480):
    """生成静默帧"""
    return np.zeros(n_samples, dtype=np.int16)


def make_speech_frame(n_samples=480, freq=1000, amplitude=12000):
    """生成 1kHz 语音帧"""
    t = np.arange(n_samples, dtype=np.float32) + np.random.randint(0, 1000)
    wave = (np.sin(2 * np.pi * freq * t / 24000) * amplitude).astype(np.int16)
    return wave


def test_idle_no_trigger():
    """测试 1: 持续静默, 不应触发任何 event"""
    logger.info("\n" + "=" * 60)
    logger.info("测试 1: 持续静默不应触发 wake/sleep")
    logger.info("=" * 60)
    detector = WakeDetector()
    events = []
    for _ in range(500):  # 10 秒静默
        evt = detector.feed(make_silence_frame())
        if evt != WakeEvent.NONE:
            events.append(evt)
    if events:
        logger.error(f"❌ 静默时不应触发 event, 但触发了: {events}")
        return False
    logger.info(f"✅ 通过: 500 帧静默, 0 个 event")
    return True


def test_speech_trigger_wake():
    """测试 2: 持续语音, 应触发 WAKE event"""
    logger.info("\n" + "=" * 60)
    logger.info("测试 2: 持续语音应触发 WAKE")
    logger.info("=" * 60)
    detector = WakeDetector()
    events = []
    for i in range(50):  # 1 秒语音
        evt = detector.feed(make_speech_frame())
        if evt != WakeEvent.NONE:
            events.append((i, evt))
    if not events:
        logger.error(f"❌ 持续语音应触发 WAKE, 但没触发")
        return False
    if events[0][1] != WakeEvent.WAKE:
        logger.error(f"❌ 第一个 event 应是 WAKE, 但实际: {events[0]}")
        return False
    logger.info(f"✅ 通过: 在帧 {events[0][0]} 触发 {events[0][1].value}")
    return True


def test_speech_then_silence_trigger_sleep():
    """测试 3: 语音 → 静音, 应触发 WAKE + SLEEP"""
    logger.info("\n" + "=" * 60)
    logger.info("测试 3: 语音 → 静音, 应触发 WAKE + SLEEP")
    logger.info("=" * 60)
    detector = WakeDetector()
    events = []

    # 阶段 1: 语音 30 帧 (~600ms)
    for i in range(30):
        evt = detector.feed(make_speech_frame())
        if evt != WakeEvent.NONE:
            events.append(('speech', i, evt))

    # 阶段 2: 静音 200 帧 (~4s, 超过 sleep_hold=100)
    for i in range(200):
        evt = detector.feed(make_silence_frame())
        if evt != WakeEvent.NONE:
            events.append(('silence', i, evt))

    if len(events) < 2:
        logger.error(f"❌ 应触发 WAKE + SLEEP, 但只触发了: {events}")
        return False
    if events[0][2] != WakeEvent.WAKE:
        logger.error(f"❌ 第一个 event 应是 WAKE, 但实际: {events[0]}")
        return False
    if events[-1][2] != WakeEvent.SLEEP:
        logger.error(f"❌ 最后 event 应是 SLEEP, 但实际: {events[-1]}")
        return False
    logger.info(f"✅ 通过: WAKE 在 speech 阶段触发, SLEEP 在 silence 阶段触发")
    return True


def test_multiple_dialog_turns():
    """测试 4: 多轮对话循环"""
    logger.info("\n" + "=" * 60)
    logger.info("测试 4: 模拟 3 轮对话 (语音→静音→语音→静音...)")
    logger.info("=" * 60)
    detector = WakeDetector()
    wake_count = 0
    sleep_count = 0

    for turn in range(3):
        logger.info(f"  第 {turn+1} 轮:")
        # 语音 50 帧
        for _ in range(50):
            evt = detector.feed(make_speech_frame())
            if evt == WakeEvent.WAKE:
                wake_count += 1
                logger.info(f"    🌟 WAKE (#{wake_count})")
        # 静音 150 帧 (超过 sleep_hold)
        for _ in range(150):
            evt = detector.feed(make_silence_frame())
            if evt == WakeEvent.SLEEP:
                sleep_count += 1
                logger.info(f"    💤 SLEEP (#{sleep_count})")

    if wake_count < 3 or sleep_count < 3:
        logger.error(f"❌ 应触发 3 次 WAKE + 3 次 SLEEP, "
                     f"实际: WAKE={wake_count}, SLEEP={sleep_count}")
        return False
    logger.info(f"✅ 通过: 3 轮对话, {wake_count} WAKE + {sleep_count} SLEEP")
    return True


def test_short_speech_no_trigger():
    """测试 5: 短促噪声 (1-2 帧) 不应触发 WAKE (避免误唤醒)"""
    logger.info("\n" + "=" * 60)
    logger.info("测试 5: 1-4 帧短促噪声不应触发 WAKE (wake_hold=5)")
    logger.info("=" * 60)
    detector = WakeDetector()
    events = []
    # 3 帧语音 + 50 帧静音 (避免触发 wake)
    for i in range(3):
        evt = detector.feed(make_speech_frame())
        if evt != WakeEvent.NONE:
            events.append((i, evt))
    for i in range(50):
        evt = detector.feed(make_silence_frame())
        if evt != WakeEvent.NONE:
            events.append((i+3, evt))
    if events:
        logger.error(f"❌ 短促噪声不应触发 event, 但触发了: {events}")
        return False
    logger.info(f"✅ 通过: 3 帧噪声 + 50 帧静音, 0 个 event (避免误唤醒)")
    return True


def main():
    logger.info("=" * 60)
    logger.info("Wake Word Detector 单元测试 (模拟 ESP32 C++)")
    logger.info("=" * 60)

    tests = [
        test_idle_no_trigger,
        test_short_speech_no_trigger,
        test_speech_trigger_wake,
        test_speech_then_silence_trigger_sleep,
        test_multiple_dialog_turns,
    ]

    passed = 0
    failed = 0
    for test in tests:
        try:
            if test():
                passed += 1
            else:
                failed += 1
        except Exception as e:
            logger.exception(f"❌ {test.__name__} 异常: {e}")
            failed += 1

    logger.info("\n" + "=" * 60)
    logger.info(f"测试结果: {passed} 通过, {failed} 失败 (共 {len(tests)})")
    logger.info("=" * 60)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)