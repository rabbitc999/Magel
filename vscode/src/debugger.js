'use strict';

/**
 * Magel 的调试适配器（Debug Adapter Protocol）。
 *
 * 编译器本体就是适配器：`magel --dap <文件>` 走 stdio 收发 DAP，所以这里不需要
 * 另外写一个 node 版的 debug adapter——只负责**找到编译器并把参数拼出来**，
 * 交给 VS Code 去拉起进程。
 *
 * 断点 / 单步 / 调用栈由编译器那边的 VM 提供；变量面板暂时是空的
 * （VM 还没有读栈帧槽的接口）。
 */

const vscode = require('vscode');
const path = require('node:path');

const locator = require('./locator');

/** 调试类型：`launch.json` 里的 `"type": "magel"`。 */
const DEBUG_TYPE = 'magel';

/**
 * 注册调试适配器工厂。
 *
 * @param {vscode.ExtensionContext} context 扩展上下文。
 * @param {{logLine: (text: string) => void, workspaceDir: () => string,
 *          readConfig: () => {magelPath: string}}} deps 宿主提供的读设置 / 写日志入口。
 * @returns {void}
 */
function registerDebugger(context, deps) {
  const factory = {
    /**
     * 拉起一次调试：把编译器当适配器进程起起来。
     *
     * @param {vscode.DebugSession} session 本次调试会话。
     * @returns {vscode.ProviderResult<vscode.DebugAdapterDescriptor>} 适配器描述。
     */
    createDebugAdapterDescriptor(session) {
      const config = deps.readConfig();
      const folder = session.workspaceFolder ? session.workspaceFolder.uri.fsPath : deps.workspaceDir();
      const resolved = locator.resolveMagel(folder, config, context.extensionPath);
      if (!resolved.magel) {
        deps.logLine(`调试启动失败：没找到 magel 编译器（${resolved.source}）`);
        void vscode.window.showErrorMessage(
          `Magel：没找到 magel 编译器，无法调试。${resolved.source}`,
          '打开设置',
        ).then((choice) => {
          if (choice === '打开设置') {
            void vscode.commands.executeCommand('workbench.action.openSettings', 'magel.path');
          }
        });
        return null;
      }

      const program = resolveProgram(session);
      if (!program) {
        deps.logLine('调试启动失败：没拿到要调试的 .mgl 文件（先打开一个文件再启动调试）');
        void vscode.window.showErrorMessage('Magel：请先打开一个 .mgl 文件再启动调试。');
        return null;
      }

      const args = ['--dap', program];
      const extra = Array.isArray(session.configuration.args) ? session.configuration.args : [];
      for (const item of extra) args.push(String(item));

      // 工作目录取源文件所在目录：调试表里记的是相对路径，两边一致才对得上断点。
      const cwd = session.configuration.cwd || path.dirname(program);
      deps.logLine(`调试：${resolved.magel} ${args.join(' ')}（工作目录 ${cwd}）`);
      return new vscode.DebugAdapterExecutable(resolved.magel, args, { cwd });
    },
  };

  context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory(DEBUG_TYPE, factory));
  context.subscriptions.push(
    vscode.debug.onDidTerminateDebugSession((session) => {
      if (session.type === DEBUG_TYPE) deps.logLine('调试会话结束。');
    }),
  );
}

/**
 * 从调试配置里取要调试的文件；`${file}` 这类变量 VS Code 已经展开过了，
 * 没给就退回到当前编辑器里打开的 `.mgl`。
 *
 * @param {vscode.DebugSession} session 调试会话。
 * @returns {string} 文件绝对路径；拿不到返回空串。
 */
function resolveProgram(session) {
  const configured = session.configuration && session.configuration.program;
  if (configured && typeof configured === 'string' && configured.trim()) {
    return configured.trim();
  }
  const editor = vscode.window.activeTextEditor;
  if (editor && editor.document.languageId === 'magel') return editor.document.uri.fsPath;
  return '';
}

module.exports = { registerDebugger, resolveProgram, DEBUG_TYPE };
