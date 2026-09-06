// ============================================================================
// code.host.js - DSH Cordis Plugin 主体 (dsh-cc-orchestrator)
//
// 加载方式: dsh --patch /Volumes/ZT-1T/项目开发/ESP32-P5/.dsh-orchestration/cordis-plugin
//
// 注册:
//   - Service: cca.session, ccb.session
//   - Tool: agent_cca, agent_ccb, cc_status
//   - Timer: heartbeat (30s)
//   - Event listener: 无（监听由 DSH 内置）
//
// 通信方式:
//   - 通过 bash 脚本调用 cc-dispatch.sh 派发任务到 claude -p
//   - 通过 state/<role>.{pid,session-id} 读取 CC 状态
//   - 通过 logs/heartbeat.log 写心跳
//
// 设计: 一切 CC 状态由 shell 脚本管理, plugin 只做 RPC 包装
// ============================================================================

'use strict';

const { execFileSync, spawn } = require('node:child_process');
const { readFileSync, writeFileSync, existsSync } = require('node:fs');
const { join } = require('node:path');

// === 路径常量 ===
const PROJ_DIR = '/Volumes/ZT-1T/项目开发/ESP32-P4C5';
const ORCH_DIR = join(PROJ_DIR, '.dsh-orchestration');
const BIN_DIR = join(ORCH_DIR, 'bin');
const STATE_DIR = join(ORCH_DIR, 'state');
const LOG_DIR = join(ORCH_DIR, 'logs');

// === 工具函数 ===

function readState(name) {
  const path = join(STATE_DIR, name);
  if (!existsSync(path)) return null;
  try {
    return readFileSync(path, 'utf-8').trim();
  } catch (e) {
    return null;
  }
}

function writeState(name, value) {
  const path = join(STATE_DIR, name);
  try {
    writeFileSync(path, String(value), 'utf-8');
  } catch (e) {
    // 忽略
  }
}

function isProcessAlive(pid) {
  if (!pid) return false;
  try {
    process.kill(parseInt(pid, 10), 0);
    return true;
  } catch (e) {
    return false;
  }
}

function log(level, msg) {
  const ts = new Date().toISOString();
  const line = `[${ts}] [${level}] ${msg}\n`;
  try {
    writeFileSync(join(LOG_DIR, 'cordis-plugin.log'), line, { flag: 'a' });
  } catch (e) {
    // 忽略
  }
  if (level === 'ERROR') {
    console.error(line.trim());
  }
}

// === Cordis Plugin 定义 ===
return {
  name: 'dsh-cc-orchestrator',
  version: '1.0.0',
  inject: ['subprocess', 'fs', 'timer', 'tools'],

  apply(ctx) {
    log('INFO', 'plugin applying...');

    // ================================================================
    // 1. 注册 Service: cca.session
    // ================================================================
    const ccaSession = {
      isAlive() {
        const pid = readState('cca.pid');
        return Boolean(pid) && isProcessAlive(pid);
      },
      getPid() {
        return readState('cca.pid');
      },
      getSessionId() {
        return readState('cca.session-id');
      },
      query(prompt, opts = {}) {
        if (!this.isAlive()) {
          throw new Error('cca process is not alive. Run: bash .dsh-orchestration/bin/cc-orchestrator.sh restart cca');
        }
        // 把 prompt 写到临时文件
        const tmpPrompt = join('/tmp', `cca-prompt-${Date.now()}.md`);
        writeFileSync(tmpPrompt, prompt, 'utf-8');

        const taskName = opts.taskName || `cca-query-${Date.now()}`;
        const timeoutMin = opts.timeoutMin || 30;
        const resultLog = `/tmp/${taskName}-result.log`;

        try {
          const stdout = execFileSync(
            'bash',
            [join(BIN_DIR, 'cc-dispatch.sh'), PROJ_DIR, tmpPrompt, taskName, '--role', 'manager', '--hard-timeout-min', String(timeoutMin)],
            { encoding: 'utf-8', timeout: timeoutMin * 60 * 1000 + 30000, maxBuffer: 50 * 1024 * 1024 }
          );
          return stdout || '(empty output)';
        } catch (e) {
          log('ERROR', `cca.query failed: ${e.message}`);
          throw e;
        } finally {
          // 清理临时 prompt
          try { require('node:fs').unlinkSync(tmpPrompt); } catch {}
        }
      },
      restart() {
        try {
          execFileSync('bash', [join(BIN_DIR, 'cc-orchestrator.sh'), 'restart', 'cca'], { encoding: 'utf-8' });
          return { ok: true, msg: 'cca restarted' };
        } catch (e) {
          return { ok: false, msg: e.message };
        }
      }
    };
    ctx.register('cca.session', ccaSession);

    // ================================================================
    // 2. 注册 Service: ccb.session (对称)
    // ================================================================
    const ccbSession = {
      isAlive() {
        const pid = readState('ccb.pid');
        return Boolean(pid) && isProcessAlive(pid);
      },
      getPid() {
        return readState('ccb.pid');
      },
      getSessionId() {
        return readState('ccb.session-id');
      },
      query(prompt, opts = {}) {
        if (!this.isAlive()) {
          throw new Error('ccb process is not alive. Run: bash .dsh-orchestration/bin/cc-orchestrator.sh restart ccb');
        }
        const tmpPrompt = join('/tmp', `ccb-prompt-${Date.now()}.md`);
        writeFileSync(tmpPrompt, prompt, 'utf-8');

        const taskName = opts.taskName || `ccb-query-${Date.now()}`;
        const timeoutMin = opts.timeoutMin || 30;

        try {
          const stdout = execFileSync(
            'bash',
            [join(BIN_DIR, 'cc-dispatch.sh'), PROJ_DIR, tmpPrompt, taskName, '--role', 'assistant', '--hard-timeout-min', String(timeoutMin)],
            { encoding: 'utf-8', timeout: timeoutMin * 60 * 1000 + 30000, maxBuffer: 50 * 1024 * 1024 }
          );
          return stdout || '(empty output)';
        } catch (e) {
          log('ERROR', `ccb.query failed: ${e.message}`);
          throw e;
        } finally {
          try { require('node:fs').unlinkSync(tmpPrompt); } catch {}
        }
      },
      restart() {
        try {
          execFileSync('bash', [join(BIN_DIR, 'cc-orchestrator.sh'), 'restart', 'ccb'], { encoding: 'utf-8' });
          return { ok: true, msg: 'ccb restarted' };
        } catch (e) {
          return { ok: false, msg: e.message };
        }
      }
    };
    ctx.register('ccb.session', ccbSession);

    // ================================================================
    // 3. 注册 Tool: agent_cca
    // ================================================================
    if (ctx.get('harness') && typeof ctx.get('harness').registerTool === 'function') {
      const harness = ctx.get('harness');

      harness.registerTool(ctx, harness.defineTool({
        name: 'agent_cca',
        description: '派发主要科研任务给 CCA（科研主管）。适用于：硬件代码 / HAL 驱动 / 4G 拨号集成 / PoC / docs/hw/ 规格书。最大超时 30 分钟（默认）。',
        parameters: {
          prompt: { type: 'string', required: true, description: '自包含任务提示词（含目标 / 输入 / 输出 / 验收）' },
          timeout_min: { type: 'number', required: false, description: '超时分钟数, 默认 30' }
        },
        output: {
          schema: { type: 'string' },
          render(_a, v) { return [{ type: 'text', text: String(v) }]; }
        },
        async execute(args) {
          log('INFO', `agent_cca invoked: ${args.prompt.substring(0, 80)}...`);
          const session = ctx.get('cca.session');
          if (!session.isAlive()) {
            return '❌ ERROR: cca process is dead. Run: bash .dsh-orchestration/bin/cc-orchestrator.sh restart cca';
          }
          try {
            return await session.query(args.prompt, { timeoutMin: args.timeout_min || 30 });
          } catch (e) {
            return `❌ ERROR: ${e.message}`;
          }
        }
      }));

      // ================================================================
      // 4. 注册 Tool: agent_ccb
      // ================================================================
      harness.registerTool(ctx, harness.defineTool({
        name: 'agent_ccb',
        description: '派发辅助科研任务给 CCB（科研助理）。适用于：调研报告 / PDF 抽取 / 文档归档 / commit 撰写 / benchmark。最大超时 30 分钟（默认）。',
        parameters: {
          prompt: { type: 'string', required: true, description: '自包含任务提示词' },
          timeout_min: { type: 'number', required: false, description: '超时分钟数, 默认 30' }
        },
        output: {
          schema: { type: 'string' },
          render(_a, v) { return [{ type: 'text', text: String(v) }]; }
        },
        async execute(args) {
          log('INFO', `agent_ccb invoked: ${args.prompt.substring(0, 80)}...`);
          const session = ctx.get('ccb.session');
          if (!session.isAlive()) {
            return '❌ ERROR: ccb process is dead. Run: bash .dsh-orchestration/bin/cc-orchestrator.sh restart ccb';
          }
          try {
            return await session.query(args.prompt, { timeoutMin: args.timeout_min || 30 });
          } catch (e) {
            return `❌ ERROR: ${e.message}`;
          }
        }
      }));

      // ================================================================
      // 5. 注册 Tool: cc_status
      // ================================================================
      harness.registerTool(ctx, harness.defineTool({
        name: 'cc_status',
        description: '查询 CCA 和 CCB 的运行状态（PID / session-id / alive / 启动时长）。',
        parameters: {},
        output: {
          schema: { type: 'object' },
          render(_a, v) { return [{ type: 'text', text: JSON.stringify(v, null, 2) }]; }
        },
        async execute(_args) {
          const cca = ctx.get('cca.session');
          const ccb = ctx.get('ccb.session');
          const result = {
            cca: {
              pid: cca.getPid(),
              session_id: cca.getSessionId(),
              alive: cca.isAlive()
            },
            ccb: {
              pid: ccb.getPid(),
              session_id: ccb.getSessionId(),
              alive: ccb.isAlive()
            },
            concurrency_slots: `${cca.isAlive() ? 1 : 0 + ccb.isAlive() ? 1 : 0} / 2`,
            orchestrator_root: ORCH_DIR
          };
          return result;
        }
      }));

      log('INFO', 'tools registered: agent_cca, agent_ccb, cc_status');
    } else {
      log('WARN', 'harness service not available, tools not registered');
    }

    // ================================================================
    // 6. 注册 Timer: heartbeat (30s)
    // ================================================================
    const timer = ctx.get('timer');
    if (timer && typeof timer.interval === 'function') {
      ctx.effect(() => timer.interval(() => {
        const cca = ctx.get('cca.session');
        const ccb = ctx.get('ccb.session');
        const line = `[heartbeat] cca=${cca.isAlive() ? 'alive' : 'dead'}(pid=${cca.getPid() || 'n/a'}) ccb=${ccb.isAlive() ? 'alive' : 'dead'}(pid=${ccb.getPid() || 'n/a'}) ts=${new Date().toISOString()}\n`;
        try {
          writeFileSync(join(LOG_DIR, 'heartbeat.log'), line, { flag: 'a' });
        } catch (e) {
          // 忽略
        }
      }, 30000));
      log('INFO', 'heartbeat timer registered (30s)');
    } else {
      log('WARN', 'timer service not available, heartbeat not registered');
    }

    log('INFO', 'plugin applied successfully');
  }
};