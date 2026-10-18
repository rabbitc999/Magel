'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const textmate = require('vscode-textmate');
const oniguruma = require('vscode-oniguruma');

/** 使用 VS Code 的真实 TextMate/Oniguruma 引擎验证独立宏区与注释、字符串边界。 */
async function main() {
  const wasm = fs.readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm'));
  await oniguruma.loadWASM(wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength));
  const file = path.join(__dirname, '../syntaxes/magel.tmLanguage.json');
  const registry = new textmate.Registry({
    onigLib: Promise.resolve({
      createOnigScanner: (patterns) => new oniguruma.OnigScanner(patterns),
      createOnigString: (text) => new oniguruma.OnigString(text),
    }),
    loadGrammar: async (scope) => scope === 'source.magel' ? textmate.parseRawGrammar(fs.readFileSync(file, 'utf8'), file) : null,
  });
  const grammar = await registry.loadGrammar('source.magel');
  /** 按真实的逐行状态进行词法着色。 */
  const tokenize = (lines) => {
    let state = textmate.INITIAL;
    return lines.map((line) => {
      const result = grammar.tokenizeLine(line, state);
      state = result.ruleStack;
      return result.tokens;
    });
  };
  const lines = [
    '$macro example(...A)(...B)', '{', '    print("A=[");', '    $for var in A',
    '    print("{} ", var);', '    $end_for', '    println("]");', '    print("B=[");',
    '    $for var in B', '    print("{} ", var);', '    $end_for', '    println("]");',
    '    int64 $123456789012345678901234567890 = 1;',
    '    // $for var in A $end_macro', '    println("$end_macro var A");', '}', '$end_macro',
    'int64 main() { int64 A=1; println("{}", A); return 0; }',
  ];
  const tokens = tokenize(lines);
  const scopes = (line, name) => tokens[line].find((token) => token.startIndex <= lines[line].indexOf(name) && lines[line].indexOf(name) < token.endIndex).scopes;
  assert(scopes(0, 'example').includes('entity.name.function.macro.magel'));
  assert(scopes(0, 'A').includes('variable.parameter.macro.magel'));
  assert(scopes(0, 'B').includes('variable.parameter.macro.magel'));
  assert(scopes(3, '$for').includes('keyword.control.macro.magel'));
  assert(scopes(3, 'var').includes('variable.other.macro-loop.magel'));
  assert(scopes(3, 'A').includes('variable.parameter.macro.magel'));
  assert(scopes(4, 'var').includes('variable.other.macro-body.magel'));
  assert(scopes(8, 'B').includes('variable.parameter.macro.magel'));
  assert(scopes(12, '$123').includes('variable.other.macro-temporary.magel'));
  assert(scopes(13, '$for').includes('comment.line.double-slash.magel'));
  assert(scopes(14, '$end_macro').includes('string.quoted.double.magel'));
  assert(scopes(15, '}').includes('meta.preprocessor.macro.body.magel'));
  assert(scopes(16, '$end_macro').includes('keyword.control.macro.magel'));
  assert(!scopes(17, 'int64').some((scope) => scope.startsWith('meta.preprocessor.macro')));
  const protectedLines = ['$macro safe(...A)', '/*', '$end_macro', '*/', 'println("A");', '$end_macro', 'int64 after=1;'];
  const protectedTokens = tokenize(protectedLines);
  assert(protectedTokens[2][0].scopes.includes('comment.block.magel'));
  assert(protectedTokens[4][0].scopes.includes('meta.preprocessor.macro.body.magel'));
  assert(!protectedTokens[6][0].scopes.includes('meta.preprocessor.macro.body.magel'));
  const incomplete = tokenize(['$macro vec<T>[...list]', '{', '    $for var in list', '    println("{}", var);']);
  assert(incomplete[3][0].scopes.includes('meta.preprocessor.macro.body.magel'));
  registry.dispose();
  console.log('真实 TextMate 宏头、独立宏体、两组循环、整数槽、字符串/注释保护与结束恢复高亮通过。');
}
main().catch((error) => { console.error(error); process.exitCode = 1; });
