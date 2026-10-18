'use strict';

/**
 * Magel（`.mgl`）语言能力：把 VS Code 的请求转给 `magel lsp`，把结果翻译回来。
 *
 * 与根目录 `vscode/src/providers.js` 同一套写法（那边管 `.gram` / `.metro`），
 * 这里只留 Magel 需要的部分：悬停、补全、跳转定义、文档大纲、格式化，
 * 以及 Magel 宏块折叠（`src/folding.js`，纯计算，不依赖服务器）。
 *
 * 诊断不走这里：它们由 `magel lsp` 主动 `textDocument/publishDiagnostics` 推过来，
 * 入口在 `extension.js` 的 `publishDiagnostics`。
 */

const vscode = require('vscode');
const folding = require('./folding');

/** Magel 的语言 id（与 `package.json` 的 `contributes.languages` 一致）。 */
const LANGUAGE_ID = 'magel';

/** 语言选择器：只认 `.mgl`，文件与未命名缓冲区都算。 */
const SELECTOR = [
  { language: LANGUAGE_ID, scheme: 'file' },
  { language: LANGUAGE_ID, scheme: 'untitled' },
];

/**
 * 把 LSP 位置换成 VS Code 位置（两边都是 0 基、UTF-16 代码单元，直接映射）。
 *
 * @param {{line: number, character: number}} position LSP 位置。
 * @returns {vscode.Position} VS Code 位置。
 */
function toPosition(position) {
  return new vscode.Position(position.line, position.character);
}

/**
 * 把 LSP 范围换成 VS Code 范围。
 *
 * @param {{start: object, end: object}} range LSP 范围。
 * @returns {vscode.Range} VS Code 范围。
 */
function toRange(range) {
  return new vscode.Range(toPosition(range.start), toPosition(range.end));
}

/**
 * LSP severity → VS Code severity。
 *
 * @param {number|undefined} severity LSP 级别（1..4）。
 * @returns {vscode.DiagnosticSeverity} VS Code 级别。
 */
function toDiagnosticSeverity(severity) {
  switch (severity) {
    case 1:
      return vscode.DiagnosticSeverity.Error;
    case 2:
      return vscode.DiagnosticSeverity.Warning;
    case 3:
      return vscode.DiagnosticSeverity.Information;
    default:
      return vscode.DiagnosticSeverity.Hint;
  }
}

/**
 * LSP CompletionItemKind → VS Code CompletionItemKind。
 *
 * @param {number|undefined} kind LSP 种类。
 * @returns {vscode.CompletionItemKind} VS Code 种类。
 */
function toCompletionKind(kind) {
  const map = {
    2: vscode.CompletionItemKind.Method,
    3: vscode.CompletionItemKind.Function,
    5: vscode.CompletionItemKind.Field,
    6: vscode.CompletionItemKind.Variable,
    7: vscode.CompletionItemKind.Class,
    8: vscode.CompletionItemKind.Interface,
    9: vscode.CompletionItemKind.Module,
    10: vscode.CompletionItemKind.Property,
    13: vscode.CompletionItemKind.Enum,
    14: vscode.CompletionItemKind.Keyword,
    15: vscode.CompletionItemKind.Snippet,
    21: vscode.CompletionItemKind.Constant,
    22: vscode.CompletionItemKind.Struct,
    25: vscode.CompletionItemKind.TypeParameter,
  };
  return map[kind] || vscode.CompletionItemKind.Keyword;
}

/**
 * LSP SymbolKind → VS Code SymbolKind。
 *
 * @param {number|undefined} kind LSP 种类。
 * @returns {vscode.SymbolKind} VS Code 种类。
 */
function toSymbolKind(kind) {
  const map = {
    2: vscode.SymbolKind.Module,
    3: vscode.SymbolKind.Namespace,
    5: vscode.SymbolKind.Class,
    6: vscode.SymbolKind.Method,
    7: vscode.SymbolKind.Property,
    8: vscode.SymbolKind.Field,
    9: vscode.SymbolKind.Constructor,
    10: vscode.SymbolKind.Enum,
    11: vscode.SymbolKind.Interface,
    12: vscode.SymbolKind.Function,
    13: vscode.SymbolKind.Variable,
    14: vscode.SymbolKind.Constant,
    22: vscode.SymbolKind.EnumMember,
    23: vscode.SymbolKind.Struct,
  };
  return map[kind] || vscode.SymbolKind.Variable;
}

/**
 * 把 `magel lsp` 推送的诊断转成 VS Code 诊断（一次推送即全量）。
 *
 * @param {Array<any>} diagnostics LSP 诊断列表。
 * @returns {vscode.Diagnostic[]} VS Code 诊断列表。
 */
function toDiagnostics(diagnostics) {
  return (diagnostics || []).map((diagnostic) => {
    const item = new vscode.Diagnostic(
      toRange(diagnostic.range),
      diagnostic.message || '',
      toDiagnosticSeverity(diagnostic.severity),
    );
    item.source = diagnostic.source || 'magel-lsp';
    if (diagnostic.code) item.code = diagnostic.code;
    item.relatedInformation = (diagnostic.relatedInformation || []).map((related) =>
      new vscode.DiagnosticRelatedInformation(
        new vscode.Location(vscode.Uri.parse(related.location.uri), toRange(related.location.range)),
        related.message));
    return item;
  });
}

/**
 * 注册全部语言能力。
 *
 * @param {vscode.ExtensionContext} context 扩展上下文。
 * @param {{getClient: () => any, logLine: (text: string) => void}} deps 依赖：
 *        getClient 取当前 LSP 客户端（可能为 null），logLine 写输出面板。
 */
function registerProviders(context, deps) {
  /**
   * 取一个可用的客户端；没有就返回 null。
   *
   * @returns {any|null} LSP 客户端。
   */
  const usableClient = () => {
    const client = deps.getClient();
    return client && client.running ? client : null;
  };

  /**
   * 发请求并兜住异常：语言服务出问题不该把编辑器搞崩。
   *
   * @param {string} method LSP 方法名。
   * @param {object} params 参数。
   * @returns {Promise<any>} 结果；失败时返回 null。
   */
  const request = async (method, params, token, document, throwError = false) => {
    const client = usableClient();
    if (!client) return null;
    try {
      const version = document && document.version;
      const result = await client.request(method, params, token);
      if ((token && token.isCancellationRequested) || (document && document.version !== version)) return null;
      return result;
    } catch (error) {
      if (error.code === -32800 || error.code === -32801) return null;
      deps.logLine(`[请求失败] ${method}：${error.message}`);
      if (throwError) throw error;
      return null;
    }
  };

  require('./semanticProviders').registerSemanticProviders(vscode, context, {
    request, selector: SELECTOR, toRange, toPosition,
  });

  context.subscriptions.push(
    vscode.languages.registerDocumentSemanticTokensProvider(SELECTOR, {
      /**
       * 将服务端 UTF-16 语义 token 转交编辑器，由主题区分类型、变量和其他角色。
       * @param {vscode.TextDocument} document 当前文档。
       * @returns {Promise<vscode.SemanticTokens>} 语义着色数据。
       */
      provideDocumentSemanticTokens: async (document, token) => {
        const result = await request('textDocument/semanticTokens/full', {
          textDocument: { uri: document.uri.toString() },
        }, token, document);
        return new vscode.SemanticTokens(new Uint32Array((result && result.data) || []));
      },
    }, new vscode.SemanticTokensLegend(
      ['namespace', 'type', 'typeParameter', 'parameter', 'variable', 'property', 'function',
        'keyword', 'comment', 'string', 'number', 'operator'], ['declaration', 'readonly'])),

    vscode.languages.registerHoverProvider(SELECTOR, {
      /**
       * 悬停：显示光标所在名字的声明。
       *
       * @param {vscode.TextDocument} document 文档。
       * @param {vscode.Position} position 位置。
       * @returns {Promise<vscode.Hover|null>} 悬停内容。
       */
      provideHover: async (document, position, token) => {
        const result = await request('textDocument/hover', {
          textDocument: { uri: document.uri.toString() },
          position: { line: position.line, character: position.character },
        }, token, document);
        if (!result || !result.contents) return null;

        // 协议允许 contents 是字符串、{value} 对象或数组，这里都兼容。
        const parts = [];
        const items = Array.isArray(result.contents) ? result.contents : [result.contents];
        for (const item of items) {
          if (typeof item === 'string') parts.push(item);
          else if (item && item.value) parts.push(item.value);
        }
        if (!parts.length) return null;

        const markdown = new vscode.MarkdownString(parts.join('\n\n'));
        markdown.isTrusted = false;
        const range = result.range ? toRange(result.range) : undefined;
        return new vscode.Hover(markdown, range);
      },
    }),

    vscode.languages.registerCompletionItemProvider(
      SELECTOR,
      {
        /**
         * 补全：当前文档可见的声明 + 关键字（由服务器算）。
         *
         * @param {vscode.TextDocument} document 文档。
         * @param {vscode.Position} position 位置。
         * @returns {Promise<vscode.CompletionItem[]>} 候选列表。
         */
        provideCompletionItems: async (document, position, token) => {
          const result = await request('textDocument/completion', {
            textDocument: { uri: document.uri.toString() },
            position: { line: position.line, character: position.character },
          }, token, document);
          const items = Array.isArray(result) ? result : (result && result.items) || [];
          return items.map((item) => {
            const completion = new vscode.CompletionItem(item.label, toCompletionKind(item.kind));
            if (item.detail) completion.detail = item.detail;
            completion.insertText = item.insertText || item.label;
            completion.additionalTextEdits = (item.additionalTextEdits || []).map((edit) => new vscode.TextEdit(toRange(edit.range), edit.newText));
            if (item.documentation && item.documentation.value) {
              completion.documentation = new vscode.MarkdownString(item.documentation.value);
            }
            return completion;
          });
        },
      },
      // 与服务器声明的 triggerCharacters 对齐（`::` / 成员访问会用到）。
      ':',
      '.',
    ),

    vscode.languages.registerSignatureHelpProvider(SELECTOR, {
      /**
       * 显示函数重载、形参和文档注释；参数位置由语言服务器计算。
       * @param {vscode.TextDocument} document 当前文档。
       * @param {vscode.Position} position 光标位置。
       * @returns {Promise<vscode.SignatureHelp|null>} 参数提示。
       */
      provideSignatureHelp: async (document, position, token) => {
        const result = await request('textDocument/signatureHelp', {
          textDocument: { uri: document.uri.toString() },
          position: { line: position.line, character: position.character },
        }, token, document);
        if (!result || !result.signatures) return null;
        const help = new vscode.SignatureHelp();
        help.signatures = result.signatures.map((item) => {
          const info = new vscode.SignatureInformation(item.label,
            new vscode.MarkdownString((item.documentation && item.documentation.value) || ''));
          info.parameters = (item.parameters || []).map((param) => new vscode.ParameterInformation(
            param.label, new vscode.MarkdownString((param.documentation && param.documentation.value) || '')));
          return info;
        });
        help.activeSignature = result.activeSignature || 0;
        help.activeParameter = result.activeParameter || 0;
        return help;
      },
    }, '(', ',', '[', '<', '{'),

    vscode.languages.registerDefinitionProvider(SELECTOR, {
      /**
       * 跳转定义：名字引用 → 当前文档里的声明处。
       *
       * @param {vscode.TextDocument} document 文档。
       * @param {vscode.Position} position 位置。
       * @returns {Promise<vscode.Location[]>} 定义位置。
       */
      provideDefinition: async (document, position, token) => {
        const result = await request('textDocument/definition', {
          textDocument: { uri: document.uri.toString() },
          position: { line: position.line, character: position.character },
        }, token, document);
        if (!result) return [];
        // 协议允许 Location / Location[] / LocationLink[]，`magel lsp` 返回单个 Location。
        const items = Array.isArray(result) ? result : [result];
        return items
          .filter((item) => item && item.uri && item.range)
          .map((item) => new vscode.Location(vscode.Uri.parse(item.uri), toRange(item.range)));
      },
    }),

    vscode.languages.registerWorkspaceSymbolProvider({
      /**
       * 搜索工程所有 Magel 文件中的函数与方法。
       * @param {string} query 查询文本。
       * @returns {Promise<vscode.SymbolInformation[]>} 跨文件符号位置。
       */
      provideWorkspaceSymbols: async (query, token) => {
        const result = await request('workspace/symbol', { query }, token);
        return (Array.isArray(result) ? result : []).map((item) => new vscode.SymbolInformation(
          item.name, toSymbolKind(item.kind), item.containerName || '',
          new vscode.Location(vscode.Uri.parse(item.location.uri), toRange(item.location.range))));
      },
    }),

    vscode.languages.registerDocumentSymbolProvider(SELECTOR, {
      /**
       * 文档大纲：当前文档显式声明的名字。
       *
       * @param {vscode.TextDocument} document 文档。
       * @returns {Promise<vscode.DocumentSymbol[]>} 符号树。
       */
      provideDocumentSymbols: async (document, token) => {
        const result = await request('textDocument/documentSymbol', {
          textDocument: { uri: document.uri.toString() },
        }, token, document);
        if (!Array.isArray(result)) return [];

        /**
         * 递归翻译一个符号节点。
         *
         * @param {any} node LSP 符号。
         * @returns {vscode.DocumentSymbol} VS Code 符号。
         */
        const convert = (node) => {
          const range = toRange(node.range || (node.location && node.location.range));
          const symbol = new vscode.DocumentSymbol(
            node.name || '?',
            '',
            toSymbolKind(node.kind),
            range,
            range,
          );
          for (const child of node.children || []) symbol.children.push(convert(child));
          return symbol;
        };
        return result.map(convert);
      },
    }),

    vscode.languages.registerDocumentFormattingEditProvider(SELECTOR, {
      /**
       * 格式化：服务器只规范缩进与行尾空白（注释 / 字符串 / 宏指令逐字保留）。
       *
       * @param {vscode.TextDocument} document 文档。
       * @returns {Promise<vscode.TextEdit[]>} 编辑列表。
       */
      provideDocumentFormattingEdits: async (document, options, token) => {
        const result = await request('textDocument/formatting', {
          textDocument: { uri: document.uri.toString() },
          options: { tabSize: 4, insertSpaces: true },
        }, token, document);
        if (!Array.isArray(result)) return [];
        return result.map(
          (edit) => new vscode.TextEdit(toRange(edit.range), edit.newText || ''),
        );
      },
    }),

    /* Magel 宏块折叠：`$macro` / `$for` 各自成块，
     * 靠 `src/folding.js` 的栈配对（纯计算，冒烟测试直接测它）。 */
    vscode.languages.registerFoldingRangeProvider(SELECTOR, {
      /**
       * 文档折叠区间。
       *
       * @param {vscode.TextDocument} document 文档。
       * @returns {vscode.FoldingRange[]} 折叠区间。
       */
      provideFoldingRanges: (document) => {
        const lines = [];
        for (let index = 0; index < document.lineCount; index++) {
          lines.push(document.lineAt(index).text);
        }
        return folding
          .planFoldingRanges(lines)
          .map((block) => new vscode.FoldingRange(block.start, block.end));
      },
    }),
  );
}

module.exports = {
  LANGUAGE_ID,
  SELECTOR,
  registerProviders,
  toDiagnostics,
};
