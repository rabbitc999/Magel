'use strict';

/**
 * Magel 插件冒烟测试（纯 Node，不需要 VS Code）。
 *
 * 两部分：
 *   1. 折叠配对：直接用扩展真正调用的 `src/folding.js`（纯计算），结果就是编辑器里
 *      折出来的层次；
 *   2. LSP 往返：真的起一个 `magel lsp`，走 initialize → didOpen → 诊断 / 补全 /
 *      悬停 / 跳转 / 大纲 / 格式化 → shutdown，并断言**未保存内容不会写盘**。
 *
 * 用法（在仓库任意目录执行都可以）：
 *   node magel/vscode/test/smoke.js                  # 自动找 magel
 *   node magel/vscode/test/smoke.js --magel <路径>   # 指定编译器
 *
 * 没找到编译器时只跳过 LSP 部分（折叠部分照样跑）；显式给了 `--magel` 却找不到则失败。
 */

const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const { LspClient } = require('../src/lspClient');
const locator = require('../src/locator');
const folding = require('../src/folding');

/** 仓库根（本文件在 <仓库>/magel/vscode/test/ 下）。 */
const REPO_ROOT = path.resolve(__dirname, '..', '..', '..');

let checks = 0;
let failures = 0;

/**
 * 记一条断言。
 *
 * @param {string} title 断言描述。
 * @param {boolean} ok 是否通过。
 * @param {string} [detail] 失败时的补充说明。
 */
function check(title, ok, detail) {
  checks += 1;
  if (!ok) failures += 1;
  console.log(`   [${ok ? 'OK' : '失败'}] ${title}${!ok && detail ? `（${detail}）` : ''}`);
}

/**
 * `.mgl` 折叠的配对（纯计算，不需要 VS Code）。
 *
 * 与 `vscode/src/folding.js` 是同一套规则，所以这里过了，编辑器里折出来的层次就是对的。
 */
/**
 * 「运行」命令的拼装（纯计算：加引号 + `run` 子命令）。
 */
function testRunCommand() {
  console.log('\n== 运行命令 ==');
  const runner = require('../src/runner');
  check('普通路径不加引号',
    runner.buildRunCommand('/usr/bin/magel', '/tmp/demo.mgl') === '/usr/bin/magel run /tmp/demo.mgl');
  const spaced = runner.buildRunCommand('C:\\Program Files\\magel\\magel.exe', 'C:\\my dir\\demo.mgl');
  check('带空格的路径加引号',
    spaced === '"C:\\Program Files\\magel\\magel.exe" run "C:\\my dir\\demo.mgl"', spaced);
  check('命令里带 run 子命令',
    runner.buildRunCommand('magel', 'a.mgl') === 'magel run a.mgl');
}

/**
 * 真的跑一次 `magel run`：临时写一个小程序，断言退出码与输出。
 *
 * @param {string} magel 编译器路径。
 * @returns {Promise<void>} 完成后返回。
 */
async function testMagelRun(magel) {
  console.log('\n== 运行（magel run）==');
  const { spawnSync } = require('node:child_process');
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'magel-run-'));
  const file = path.join(dir, 'demo.mgl');
  fs.writeFileSync(file, 'int64 main() { std::lang::println("run ok"); return 0; }\n', 'utf-8');
  const result = spawnSync(magel, ['run', file], { cwd: dir, encoding: 'utf-8' });
  check('magel run 退出码为 0', result.status === 0, result.stderr);
  check('magel run 输出正确', (result.stdout || '').trim() === 'run ok', result.stdout);
  fs.rmSync(dir, { recursive: true, force: true });
}

function testMagelFolding() {
  console.log('\n== Magel 折叠 ==');
  const lines = [
    '$macro SHAPE(...items)',       // 0
    '$for v in items',              // 1
    '$for w in items',              // 2
    'std::lang::println("{}", w);',      // 3
    '$end_for',                     // 4
    '$end_for',                     // 5
    '$end_macro',                   // 6
  ];
  const blocks = folding.planFoldingRanges(lines);
  check('折叠：7 行里的块数', blocks.length === 3);
  check('折叠：宏块最外层', blocks[0].start === 0 && blocks[0].end === 6 && blocks[0].name === '宏定义');
  check('折叠：外层 for 块', blocks[1].start === 1 && blocks[1].end === 5 && blocks[1].name === 'for 展开');
  check('折叠：内层 for 块', blocks[2].start === 2 && blocks[2].end === 4 && blocks[2].name === 'for 展开');

  // 收尾没写完（编辑到一半）：按文件末尾收，块不消失。
  const half = folding.planFoldingRanges(['$macro X()', 'std::lang::println("x");']);
  check('折叠：缺 $end_macro 也能折到末尾', half.length === 1 && half[0].end === 1);

  // 词面要完整，单行开合不折，多余的收尾不炸。
  check('折叠：$match_if 不是指令', folding.planFoldingRanges(['$match_if kind', 'x', '$end_match']).length === 0);
  check('折叠：同一行开合不折', folding.planFoldingRanges(['$macro A()']).length === 0);
  check('折叠：多余收尾不报错', folding.planFoldingRanges(['$end_for']).length === 0);
}

/**
 * 打开文档并等第一次诊断推送。
 *
 * @param {LspClient} client 客户端。
 * @param {string} uri 文档 URI。
 * @param {string} text 文档内容。
 * @returns {Promise<Array<any>>} 诊断列表（超时返回空数组）。
 */
function openAndWait(client, uri, text) {
  return new Promise((resolve) => {
    const onDiagnostics = (event) => {
      if (event.uri !== uri) return;
      client.off('diagnostics', onDiagnostics);
      clearTimeout(timer);
      resolve(event.diagnostics || []);
    };
    client.on('diagnostics', onDiagnostics);
    const timer = setTimeout(() => {
      client.off('diagnostics', onDiagnostics);
      resolve([]);
    }, 30000);
    client.notify('textDocument/didOpen', {
      textDocument: { uri, languageId: 'magel', version: 1, text },
    });
  });
}

/**
 * 跑一遍 `magel lsp` 的 LSP 往返。
 *
 * @param {string} magel magel 编译器路径。
 * @returns {Promise<void>} 完成后返回。
 */
async function testMagelLsp(magel) {
  console.log('\n== LSP（magel lsp）==');
  const client = new LspClient(magel, ['lsp'], { cwd: REPO_ROOT });
  client.on('log', () => {});
  await client.start();
  const capabilities = await client.initialize();
  check('初始化成功', client.running);
  check('声明了 hover / completion / definition / documentSymbol / formatting',
    !!(capabilities
      && capabilities.capabilities
      && capabilities.capabilities.hoverProvider
      && capabilities.capabilities.completionProvider
      && capabilities.capabilities.definitionProvider
      && capabilities.capabilities.documentSymbolProvider
      && capabilities.capabilities.documentFormattingProvider));
  check('位置编码是 utf-16', capabilities.capabilities.positionEncoding === 'utf-16');

  // 1. 干净文件：不该有诊断。
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'magel-vscode-smoke-'));
  const file = path.join(dir, 'probe.mgl');
  const clean = 'int64 main() {\n    return 0;\n}\n';
  fs.writeFileSync(file, clean, 'utf8');
  const uri = `file:///${file.replace(/\\/g, '/')}`;
  const cleanDiagnostics = await openAndWait(client, uri, clean);
  check('干净文件没有诊断', cleanDiagnostics.length === 0,
    JSON.stringify(cleanDiagnostics.map((item) => item.message)));

  // 2. 悬停 / 补全 / 跳转：用一份带声明的文档。
  const text = 'int64 main() {\n    int64 value = 3;\n    return value;\n}\n';
  const changed = openAndWait(client, uri, text);
  client.notify('textDocument/didChange', {
    textDocument: { uri, version: 2 },
    contentChanges: [{ text }],
  });
  check('未保存内容也没有诊断', (await changed).length === 0);

  const declaration = { line: 2, character: 12 };
  const hover = await client.request('textDocument/hover', {
    textDocument: { uri }, position: declaration,
  });
  const hoverText = hover && hover.contents ? JSON.stringify(hover.contents) : '';
  // 服务器的悬停就是声明那一行的原文（含空格），这里只要求它确实给了 `int64 value`。
  check('悬停有说明', hoverText.includes('int64 value'), hoverText);

  const completion = await client.request('textDocument/completion', {
    textDocument: { uri }, position: declaration,
  });
  const items = Array.isArray(completion) ? completion : (completion && completion.items) || [];
  check('补全含当前文档的声明', items.some((item) => item.label === 'value'),
    JSON.stringify(items.map((item) => item.label).slice(0, 8)));

  const semantic = await client.request('textDocument/semanticTokens/full', { textDocument: { uri } });
  check('语义着色返回五元组数据', semantic && Array.isArray(semantic.data)
    && semantic.data.length > 0 && semantic.data.length % 5 === 0, JSON.stringify(semantic));
  check('语义着色区分类型与变量', semantic && semantic.data.some((value, index) => index % 5 === 3 && value === 1)
    && semantic.data.some((value, index) => index % 5 === 3 && value === 4));

  const definition = await client.request('textDocument/definition', {
    textDocument: { uri }, position: declaration,
  });
  // `magel lsp` 返回单个 Location（协议允许单个或数组），两种都归一。
  const locations = Array.isArray(definition) ? definition : definition ? [definition] : [];
  check('跳转定义落在声明行',
    locations.length > 0 && locations[0].range.start.line === 1, JSON.stringify(definition));

  const symbols = await client.request('textDocument/documentSymbol', { textDocument: { uri } });
  check('大纲含 main 与 value',
    Array.isArray(symbols)
      && symbols.some((symbol) => symbol.name === 'main')
      && symbols.some((symbol) => symbol.name === 'value'),
    JSON.stringify((symbols || []).map((symbol) => symbol.name)));

  // 3. 格式化：服务器只规范缩进与行尾空白。
  const bad = 'int64 main() {\nint64 value=3;\nreturn value;\n}\n';
  const forFormat = 'file:///' + path.join(dir, 'format.mgl').replace(/\\/g, '/');
  fs.writeFileSync(path.join(dir, 'format.mgl'), bad, 'utf8');
  await openAndWait(client, forFormat, bad);
  const edits = await client.request('textDocument/formatting', {
    textDocument: { uri: forFormat }, options: { tabSize: 4, insertSpaces: true },
  });
  check('格式化补了缩进', Array.isArray(edits)
    && edits.length > 0
    && edits[0].newText.includes('    int64 value=3;'),
    JSON.stringify(edits));

  // 4. 坏文档：报错，且**不写盘**。
  const broken = 'int64 main(){ int64 value = ; return 0; }\n';
  const forBroken = 'file:///' + path.join(dir, 'broken.mgl').replace(/\\/g, '/');
  fs.writeFileSync(path.join(dir, 'broken.mgl'), 'int64 main(){ return 0; }\n', 'utf8');
  const diagnostics = await openAndWait(client, forBroken, broken);
  const errors = diagnostics.filter((item) => item.severity === 1);
  check('坏文档报错诊断', errors.length > 0, JSON.stringify(diagnostics));
  check('诊断带稳定错误码', errors.every((item) => !!item.code),
    JSON.stringify(errors.map((item) => item.code)));
  check('未保存内容没有写盘',
    fs.readFileSync(path.join(dir, 'broken.mgl'), 'utf8') === 'int64 main(){ return 0; }\n');

  await client.stop();
  fs.rmSync(dir, { recursive: true, force: true });
}

/**
 * 入口。
 *
 * @returns {Promise<void>} 完成后返回。
 */
async function main() {
  const index = process.argv.indexOf('--magel');
  const given = index >= 0 ? process.argv[index + 1] : '';
  if (index >= 0 && !given) {
    console.error('--magel 后面要跟编译器路径');
    process.exit(2);
  }

  testMagelFolding();
  testRunCommand();

  if (given) {
    if (!locator.isFile(given)) {
      console.error(`找不到编译器：${given}`);
      process.exit(1);
    }
    await testMagelLsp(given);
  } else {
    const resolved = locator.resolveMagel(REPO_ROOT, {}, path.join(__dirname, '..'));
    if (!resolved.magel) {
      console.log(`\n[跳过] 没找到 magel（${resolved.source}）：折叠部分已跑，LSP 部分用 --magel 指定后重跑。`);
    } else {
      console.log(`\n（编译器：${resolved.magel}）`);
      await testMagelLsp(resolved.magel);
      await testMagelRun(resolved.magel);
    }
  }

  console.log(`\n共 ${checks} 项检查，失败 ${failures} 项`);
  process.exit(failures === 0 ? 0 : 1);
}

main().catch((error) => {
  console.error('冒烟测试异常：', error);
  process.exit(1);
});
