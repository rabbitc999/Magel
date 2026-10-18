/*
 * app.js —— 学生成绩信息管理系统（前后端分离版）前端逻辑
 *
 * 纯原生 JS，没有框架、没有构建步骤：所有数据都来自后端 `/api/*` 的 JSON 接口，
 * 页面本身只负责渲染与收集表单。
 *
 * 两条硬约定：
 *   1. **权限不在前端做主**。菜单与按钮的可见性来自后端 `/api/session` 返回的
 *      `modules[].actions[].needLevel`（后端已按登录人等级过滤过），前端只是
 *      「后端说能做什么就显示什么」；越权请求仍会被后端 403 挡住。
 *   2. **不用 innerHTML 拼数据**。所有后端文本一律走 textContent，避免把库里的
 *      内容当 HTML 解释。
 */

'use strict';

/* ── 配置 ─────────────────────────────────────────────────────────────── */

const query = new URLSearchParams(location.search);
const API_BASE = (query.get('api') || window.MAGEL_API_BASE || 'http://127.0.0.1:8080')
  .replace(/\/+$/, '');

/* 登录尝试次数；与控制台版一致：连续 3 次失败即停止。 */
const MAX_ATTEMPTS = 3;

/* ── 模块描述表 ───────────────────────────────────────────────────────── */

/*
 * 每个模块声明：怎么取列表、表格显示哪些列、新增/编辑表单有哪些字段。
 * 键名与后端 `allModules()` 的模块键一一对应；字段键名与后端接口的 JSON 字段一致。
 */
const MODULES = {
  student: {
    title: '学生管理',
    endpoint: '/api/students',
    canQuery: true,
    modes: [{ value: 'name', label: '按姓名' }, { value: 'class', label: '按班级' }],
    keywordPlaceholder: '学生姓名或班级名称',
    columns: [
      { key: 'id', label: '学号' },
      { key: 'name', label: '姓名' },
      { key: 'sexLabel', label: '性别' },
      { key: 'sclassName', label: '班级' },
      { key: 'gradeLabel', label: '年级' },
      { key: 'courseCount', label: '课程数' },
      { key: 'averageScore', label: '平均分' },
    ],
    fields: [
      { key: 'name', label: '姓名', type: 'text', required: true, maxlength: 24, placeholder: '如：李航超' },
      {
        key: 'sex', label: '性别', type: 'select', required: true,
        options: [{ value: 'MALE', label: '男' }, { value: 'FEMALE', label: '女' }],
      },
      { key: 'sclassId', label: '班级', type: 'dselect', source: 'sclasses', empty: '不分班' },
    ],
    canCreate: true,
    canEdit: true,
  },

  sclass: {
    title: '班级管理',
    endpoint: '/api/sclasses',
    canQuery: true,
    modes: [],
    keywordPlaceholder: '班级名称',
    columns: [
      { key: 'id', label: '班级ID' },
      { key: 'name', label: '班级名称' },
      { key: 'administrator', label: '班主任' },
      { key: 'gradeLabel', label: '年级' },
      { key: 'studentCount', label: '学生数' },
      { key: 'averageScore', label: '平均分' },
    ],
    fields: [
      { key: 'name', label: '班级名称', type: 'text', required: true, maxlength: 36 },
      { key: 'administrator', label: '班级负责人', type: 'text', required: true, maxlength: 24 },
      {
        key: 'grade', label: '年级', type: 'select', required: true,
        options: [{ value: 'ONE', label: '高一' }, { value: 'TWO', label: '高二' }, { value: 'THREE', label: '高三' }],
      },
    ],
    canCreate: true,
    canEdit: true,
  },

  course: {
    title: '课程管理',
    endpoint: '/api/courses',
    canQuery: true,
    modes: [],
    keywordPlaceholder: '课程名称',
    columns: [
      { key: 'id', label: '课程ID' },
      { key: 'name', label: '课程名称' },
      { key: 'stateLabel', label: '状态' },
      { key: 'studentCount', label: '选课人数' },
      { key: 'averageScore', label: '平均分' },
    ],
    fields: [
      { key: 'name', label: '课程名称', type: 'text', required: true, maxlength: 36 },
      {
        key: 'state', label: '课程状态', type: 'select', required: true,
        options: [{ value: 'START', label: '开课' }, { value: 'COMPLETION', label: '结课' }],
      },
    ],
    canCreate: true,
    canEdit: true,
  },

  selection: {
    title: '成绩管理',
    endpoint: '/api/selections',
    canQuery: true,
    modes: [{ value: 'student', label: '按学生' }, { value: 'course', label: '按课程' }],
    keywordPlaceholder: '学生姓名 / 学号 / 课程名称',
    columns: [
      { key: 'id', label: '成绩ID' },
      { key: 'studentId', label: '学号' },
      { key: 'studentName', label: '学生姓名' },
      { key: 'courseId', label: '课程ID' },
      { key: 'courseName', label: '课程' },
      { key: 'testDate', label: '考试日期' },
      { key: 'grade', label: '成绩' },
      { key: 'gradeLabel', label: '等级' },
    ],
    fields: [
      { key: 'studentId', label: '学号', type: 'number', required: true, min: 1 },
      { key: 'courseId', label: '课程', type: 'dselect', source: 'courses', required: true },
      { key: 'testDate', label: '考试日期', type: 'date', required: true },
      { key: 'grade', label: '成绩（0-100）', type: 'number', required: true, min: 0, max: 100 },
    ],
    /* 后端 PUT 只改分数，编辑表单也就只有一项。 */
    editFields: [
      { key: 'grade', label: '成绩（0-100）', type: 'number', required: true, min: 0, max: 100 },
    ],
    canCreate: true,
    canEdit: true,
    hasStats: true,
  },

  user: {
    title: '用户管理',
    endpoint: '/api/users',
    canQuery: false,
    columns: [
      { key: 'id', label: '用户ID' },
      { key: 'userName', label: '用户名' },
      { key: 'permission', label: '权限（枚举）' },
      { key: 'permissionLabel', label: '权限' },
    ],
    fields: [
      { key: 'userName', label: '用户名', type: 'text', required: true, maxlength: 18, placeholder: '最长 18 个字符' },
      { key: 'password', label: '密码', type: 'password', required: true, placeholder: '4~18 位字母或数字' },
      {
        key: 'permission', label: '权限', type: 'select', required: true,
        options: [
          { value: 'STUDENT', label: '学生' },
          { value: 'TEACHER', label: '教师' },
          { value: 'ADMINISTRATOR', label: '管理员' },
        ],
        note: '超级管理员只能由 admin 担任',
      },
    ],
    canCreate: true,
    canEdit: false,          /* 改权限、重置密码是两条独立动作，走专用表单 */
    rowActions: ['permission', 'password'],
  },

  password: {
    title: '修改我的密码',
    endpoint: '',
    standalone: true,
    fields: [
      { key: 'currentPassword', label: '当前密码', type: 'password', required: true },
      { key: 'newPassword', label: '新密码', type: 'password', required: true, placeholder: '4~18 位字母或数字' },
    ],
  },
};

/* ── 状态 ─────────────────────────────────────────────────────────────── */

const state = {
  token: null,
  user: null,
  modules: [],
  moduleKey: 'student',
  rows: [],
  editingId: null,
  view: 'list',
  keyword: '',
  mode: '',
  options: {},
  attempts: 0,
};

/* ── 小工具 ───────────────────────────────────────────────────────────── */

const $ = id => document.getElementById(id);

function el(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

/**
 * @brief 在状态行里提示；error = true 时标红。
 *
 * 登录页与应用内各有一个状态行（登录页那个在登录后就藏起来了），所以两处都写。
 */
function notify(text, error) {
  for (const id of ['login-message', 'app-message']) {
    const box = document.getElementById(id);
    if (!box) continue;
    box.textContent = text;
    box.classList.toggle('error', !!error);
  }
}

function moduleSpec() {
  return state.modules.find(item => item.key === state.moduleKey) || null;
}

/** @brief 后端说当前登录人能不能做某个动作（后端已按等级过滤，这里只做查表）。 */
function allows(moduleKey, actionKey) {
  const module = state.modules.find(item => item.key === moduleKey);
  if (!module) return false;
  const action = (module.actions || []).find(item => item.key === actionKey);
  if (!action) return false;
  return state.user.level >= action.needLevel;
}

/* ── API 封装 ─────────────────────────────────────────────────────────── */

class ApiError extends Error {
  constructor(message, status) {
    super(message);
    this.status = status;
  }
}

/**
 * @brief 调后端 JSON 接口。
 * @param {string} path 以 / 开头的接口路径。
 * @param {{method?:string, body?:object, auth?:boolean}} options 请求选项。
 * @returns {Promise<any>} 解析后的 JSON（无正文时返回 null）。
 */
async function api(path, options = {}) {
  const method = options.method || 'GET';
  const headers = {};
  if (options.body !== undefined) headers['Content-Type'] = 'application/json';
  const auth = options.auth !== false;
  if (auth && state.token) headers.Authorization = 'Bearer ' + state.token;

  const response = await fetch(API_BASE + path, {
    method,
    headers,
    body: options.body === undefined ? undefined : JSON.stringify(options.body),
  });
  const text = await response.text();
  let payload = null;
  if (text) {
    try { payload = JSON.parse(text); } catch (error) { payload = null; }
  }
  if (!response.ok) {
    if (response.status === 401 && auth) {
      signOut('会话已失效，请重新登录。');
      throw new ApiError((payload && payload.error) || '会话已失效', 401);
    }
    throw new ApiError((payload && payload.error) || `请求失败（HTTP ${response.status}）`, response.status);
  }
  return payload;
}

/* ── 登录 / 注销 ──────────────────────────────────────────────────────── */

async function signIn(event) {
  event.preventDefault();
  const button = $('login-submit');
  button.disabled = true;
  const userName = $('login-user').value.trim();
  const password = $('login-password').value;
  if (!userName) { notify('用户名不能为空', true); button.disabled = false; return; }
  if (!password) { notify('用户密码不能为空', true); button.disabled = false; return; }
  try {
    const result = await api('/api/login', { method: 'POST', body: { userName, password }, auth: false });
    state.token = result.token;
    state.user = result.user;
    state.modules = result.modules;
    state.attempts = 0;
    state.moduleKey = (result.modules[0] && result.modules[0].key) || '';
    state.keyword = '';
    state.mode = '';
    document.body.classList.add('signed-in');
    $('login-password').value = '';
    notify('');
    renderShell();
    await openModule(state.moduleKey);
  } catch (error) {
    if (error.status === 401) {
      state.attempts += 1;
      const left = MAX_ATTEMPTS - state.attempts;
      if (left > 0) {
        notify(`用户名或密码错误，请重新输入（剩余 ${left} 次机会）`, true);
      } else {
        notify('用户名或密码错误，连续 3 次失败，请刷新页面重试', true);
        button.disabled = true;
        return;
      }
    } else {
      notify(error.message, true);
    }
  } finally {
    if (state.attempts < MAX_ATTEMPTS) button.disabled = false;
  }
}

function signOut(message) {
  state.token = null;
  state.user = null;
  state.modules = [];
  state.rows = [];
  state.attempts = 0;
  document.body.classList.remove('signed-in');
  $('login-password').value = '';
  notify(message || '已退出登录。');
  $('login-submit').disabled = false;
}

/* ── 外壳 ─────────────────────────────────────────────────────────────── */

function renderShell() {
  $('who-name').textContent = state.user.userName;
  $('who-role').textContent = state.user.permissionLabel;
  const nav = $('nav');
  nav.textContent = '';
  for (const module of state.modules) {
    if (!MODULES[module.key]) continue;
    const button = el('button', null, module.label);
    button.type = 'button';
    button.setAttribute('aria-current', String(module.key === state.moduleKey));
    button.addEventListener('click', () => {
      if (state.moduleKey === module.key) return;
      state.moduleKey = module.key;
      state.keyword = '';
      state.mode = '';
      notify('');
      renderShell();
      openModule(module.key);
    });
    nav.append(button);
  }
}

/** @brief 关闭表单面板并复位编辑态。 */
function closeForm() {
  state.editingId = null;
  state.view = 'list';
  $('form-panel').classList.remove('open');
  $('form-panel').textContent = '';
}

async function openModule(key) {
  const spec = MODULES[key];
  const module = moduleSpec();
  if (!spec || !module) return;
  closeForm();
  $('panel-title').textContent = module.label;
  if (spec.standalone) {
    renderStandaloneForm(key);
    return;
  }
  renderListPanel(key);
  await reload();
}

/* ── 列表面板 ─────────────────────────────────────────────────────────── */

function renderListPanel(key) {
  const spec = MODULES[key];
  const toolbar = $('toolbar');
  const head = $('table-head');
  const body = $('table-body');
  toolbar.textContent = '';
  head.textContent = '';
  body.textContent = '';

  if (spec.canQuery) {
    const modeSelect = el('select');
    modeSelect.id = 'query-mode';
    const all = el('option', null, '全部');
    all.value = '';
    modeSelect.append(all);
    for (const mode of spec.modes || []) {
      const option = el('option', null, mode.label);
      option.value = mode.value;
      modeSelect.append(option);
    }
    modeSelect.value = state.mode;
    modeSelect.addEventListener('change', () => { state.mode = modeSelect.value; });

    const keyword = el('input');
    keyword.id = 'query-keyword';
    keyword.type = 'search';
    keyword.placeholder = spec.keywordPlaceholder || '关键词';
    keyword.value = state.keyword;
    keyword.addEventListener('keydown', event => {
      if (event.key === 'Enter') { state.mode = modeSelect.value; state.keyword = keyword.value.trim(); reload(); }
    });
    const search = el('button', 'secondary', '查询');
    search.type = 'button';
    search.addEventListener('click', () => {
      state.mode = modeSelect.value;
      state.keyword = keyword.value.trim();
      reload();
    });
    toolbar.append(modeSelect, keyword, search);
  }

  const spacer = el('div', 'spacer');
  toolbar.append(spacer);

  if (spec.hasStats) {
    const statCourse = el('button', 'secondary', '按课程统计');
    statCourse.type = 'button';
    statCourse.disabled = !allows('selection', 'statCourse');
    statCourse.addEventListener('click', () => showStatCourse());
    const statClass = el('button', 'secondary', '按班级统计');
    statClass.type = 'button';
    statClass.disabled = !allows('selection', 'statClass');
    statClass.addEventListener('click', () => showStatClass());
    toolbar.append(statCourse, statClass);
  }

  if (spec.canCreate && allows(key, 'add')) {
    const add = el('button', null, '新增');
    add.type = 'button';
    add.addEventListener('click', () => openCreateForm());
    toolbar.append(add);
  }

  const headerRow = el('tr');
  for (const column of spec.columns) headerRow.append(el('th', null, column.label));
  headerRow.append(el('th', null, '操作'));
  head.append(headerRow);

  const refresh = el('button', 'link', '刷新');
  refresh.type = 'button';
  refresh.addEventListener('click', () => reload().then(() => notify('列表已刷新。')).catch(error => notify(error.message, true)));
  $('footer-note').textContent = '';
  $('footer-note').append(`列表最多显示 ${200} 条，按主键升序排列。`, refresh);
}

async function reload() {
  const spec = MODULES[state.moduleKey];
  if (!spec || spec.standalone) return;
  let payload;
  if (spec.canQuery && state.keyword) {
    payload = await api(spec.endpoint + '/query', {
      method: 'POST',
      body: spec.modes && spec.modes.length ? { mode: state.mode || spec.modes[0].value, keyword: state.keyword } : { keyword: state.keyword },
    });
  } else {
    payload = await api(spec.endpoint);
  }
  state.rows = payload.items || [];
  renderRows();
  $('count-note').textContent = state.keyword
    ? `命中 ${payload.count} 条 / 库中共 ${payload.total} 条`
    : `共 ${payload.count} 条 / 库中共 ${payload.total} 条`;
}

function renderRows() {
  const spec = MODULES[state.moduleKey];
  const body = $('table-body');
  body.textContent = '';
  $('empty').hidden = state.rows.length > 0;
  for (const row of state.rows) {
    const tr = el('tr');
    for (const column of spec.columns) {
      const td = el('td');
      const value = row[column.key];
      if (column.key === 'stateLabel' || column.key === 'gradeLabel') {
        const tag = el('span', column.key === 'stateLabel' && row.state === 'COMPLETION' ? 'tag off' : 'tag', value);
        td.append(tag);
      } else {
        td.textContent = value === undefined || value === null ? '' : String(value);
      }
      tr.append(td);
    }
    const actions = el('td', 'actions');
    if (spec.canEdit && allows(state.moduleKey, 'update')) {
      const edit = el('button', 'secondary', '编辑');
      edit.type = 'button';
      edit.addEventListener('click', () => openEditForm(row));
      actions.append(edit);
    }
    for (const action of spec.rowActions || []) {
      if (!allows('user', action)) continue;
      const button = el('button', 'secondary', action === 'permission' ? '改权限' : '重置密码');
      button.type = 'button';
      button.addEventListener('click', () => openUserAction(action, row));
      actions.append(button);
    }
    if (allows(state.moduleKey, 'delete')) {
      const remove = el('button', 'danger', '删除');
      remove.type = 'button';
      remove.addEventListener('click', () => removeRow(row, remove));
      actions.append(remove);
    }
    tr.append(actions);
    body.append(tr);
  }
}

async function removeRow(row, button) {
  const spec = MODULES[state.moduleKey];
  const label = row.name || row.userName || row.courseName || row.id;
  if (!window.confirm(`确定删除 ${label}（ID ${row.id}）？\n删除会连带影响关联数据，且不可撤销。`)) return;
  button.disabled = true;
  try {
    const result = await api(`${spec.endpoint}/${row.id}`, { method: 'DELETE' });
    if (state.editingId === row.id) closeForm();
    await reload();
    notify(result && result.message ? result.message : '已删除。');
  } catch (error) {
    notify(error.message, true);
    button.disabled = false;
  }
}

/* ── 表单 ─────────────────────────────────────────────────────────────── */

async function formOptions() {
  /* 动态下拉的数据源（班级 / 课程）按需拉取，缓存在 state.options。 */
  const cached = state.options;
  if (!cached.sclasses && allows('sclass', 'list')) {
    cached.sclasses = (await api('/api/options/sclasses')).items || [];
  }
  if (!cached.courses && allows('course', 'list')) {
    cached.courses = (await api('/api/options/courses')).items || [];
  }
  return cached;
}

function buildInput(field, value, options) {
  const wrap = el('label', 'field');
  wrap.append(el('span', null, field.label));
  let input;
  if (field.type === 'select' || field.type === 'dselect') {
    input = el('select');
    const list = field.type === 'select'
      ? (field.options || [])
      : (options[field.source] || []).map(item => ({ value: item.id, label: item.name }));
    if (!field.required) {
      const empty = el('option', null, field.empty || '（不指定）');
      empty.value = '';
      input.append(empty);
    }
    for (const item of list) {
      const option = el('option', null, item.label);
      option.value = String(item.value);
      input.append(option);
    }
  } else {
    input = el('input');
    input.type = field.type === 'number' ? 'number'
      : field.type === 'password' ? 'password'
        : field.type === 'date' ? 'date' : 'text';
    if (field.placeholder) input.placeholder = field.placeholder;
    if (field.maxlength) input.maxLength = field.maxlength;
    if (field.min !== undefined) input.min = String(field.min);
    if (field.max !== undefined) input.max = String(field.max);
  }
  input.name = field.key;
  input.required = !!field.required;
  if (value !== undefined && value !== null && value !== '') input.value = String(value);
  wrap.append(input);
  if (field.note) wrap.append(el('small', 'muted', field.note));
  return wrap;
}

function collect(form, fields) {
  const data = {};
  for (const field of fields) {
    const input = form.elements.namedItem(field.key);
    let value = input ? input.value : '';
    if (field.type === 'number') {
      value = value === '' ? null : Number(value);
    } else if (field.type === 'dselect') {
      /* 动态下拉的 value 一律是字符串，但后端按整数读 id：
         不转数字会被当成缺失，班级 / 课程就被静默丢掉了。空选项给 null。 */
      value = value === '' ? null : Number(value);
    }
    data[field.key] = value;
  }
  return data;
}

/**
 * @brief 打开表单面板。
 * @param {object} config {title, fields, values, submitLabel, onSubmit}
 */
async function openForm(config) {
  const panel = $('form-panel');
  const options = await formOptions();
  panel.textContent = '';
  const form = el('form');
  form.append(el('h2', null, config.title));
  for (const field of config.fields) {
    form.append(buildInput(field, config.values ? config.values[field.key] : undefined, options));
  }
  const actions = el('div', 'form-actions');
  const submit = el('button', null, config.submitLabel || '保存');
  submit.type = 'submit';
  const cancel = el('button', 'secondary', '取消');
  cancel.type = 'button';
  cancel.addEventListener('click', closeForm);
  actions.append(submit, cancel);
  form.append(actions);
  form.addEventListener('submit', async event => {
    event.preventDefault();
    submit.disabled = true;
    try {
      const message = await config.onSubmit(collect(form, config.fields));
      closeForm();
      await reload();
      notify(message || '已保存。');
    } catch (error) {
      notify(error.message, true);
    } finally {
      submit.disabled = false;
    }
  });
  panel.append(form);
  panel.classList.add('open');
  const first = form.querySelector('input, select');
  if (first) first.focus();
}

function openCreateForm() {
  const spec = MODULES[state.moduleKey];
  state.view = 'list';
  openForm({
    title: `新增${spec.title.replace('管理', '')}`,
    fields: spec.fields,
    submitLabel: '新增',
    onSubmit: async values => {
      const result = await api(spec.endpoint, { method: 'POST', body: values });
      return result && result.message ? result.message : '新增成功。';
    },
  });
}

function openEditForm(row) {
  const spec = MODULES[state.moduleKey];
  const fields = spec.editFields || spec.fields;
  state.editingId = row.id;
  const values = {};
  for (const field of fields) values[field.key] = row[field.key];
  openForm({
    title: `编辑${spec.title.replace('管理', '')} · ID ${row.id}`,
    fields,
    values,
    submitLabel: '保存修改',
    onSubmit: async payload => {
      const result = await api(`${spec.endpoint}/${row.id}`, { method: 'PUT', body: payload });
      return result && result.message ? result.message : '修改成功。';
    },
  });
}

function openUserAction(action, row) {
  if (action === 'permission') {
    openForm({
      title: `修改权限 · ${row.userName}（ID ${row.id}）`,
      fields: [{
        key: 'permission', label: '新权限', type: 'select', required: true,
        options: [
          { value: 'STUDENT', label: '学生' },
          { value: 'TEACHER', label: '教师' },
          { value: 'ADMINISTRATOR', label: '管理员' },
          { value: 'SUPER_ADMINISTRATOR', label: '超级管理员' },
        ],
        note: 'admin 只能是超级管理员；其他成员不能是超级管理员',
      }],
      values: { permission: row.permission },
      submitLabel: '保存权限',
      onSubmit: async payload => {
        const result = await api(`/api/users/${row.id}/permission`, { method: 'PUT', body: payload });
        return result && result.message ? result.message : '权限已更新。';
      },
    });
    return;
  }
  openForm({
    title: `重置密码 · ${row.userName}（ID ${row.id}）`,
    fields: [{ key: 'password', label: '新密码', type: 'password', required: true, placeholder: '4~18 位字母或数字' }],
    submitLabel: '重置密码',
    onSubmit: async payload => {
      const result = await api(`/api/users/${row.id}/password`, { method: 'PUT', body: payload });
      return result && result.message ? result.message : '密码已重置。';
    },
  });
}

function renderStandaloneForm(key) {
  const spec = MODULES[key];
  $('toolbar').textContent = '';
  $('table-head').textContent = '';
  $('table-body').textContent = '';
  $('empty').hidden = true;
  $('count-note').textContent = '';
  $('footer-note').textContent = '修改后请用新密码重新登录。';
  openForm({
    title: '修改我的密码',
    fields: spec.fields,
    submitLabel: '确认修改',
    onSubmit: async payload => {
      const result = await api('/api/password', { method: 'POST', body: payload });
      return result && result.message ? result.message : '密码修改成功！';
    },
  });
}

/* ── 统计 ─────────────────────────────────────────────────────────────── */

function statCard(label, value) {
  const cell = el('div', 'stat-cell');
  cell.append(el('div', 'k', label), el('div', 'v', String(value)));
  return cell;
}

async function showStatCourse() {
  closeForm();
  const spec = MODULES.selection;
  $('toolbar').textContent = '';
  const back = el('button', 'secondary', '返回列表');
  back.type = 'button';
  back.addEventListener('click', () => openModule('selection'));
  const toolbar = $('toolbar');
  toolbar.append(back, el('div', 'spacer'));

  let selector;
  const canListCourses = allows('course', 'list');
  if (canListCourses) {
    const options = await formOptions();
    selector = el('select');
    const all = el('option', null, '全部课程');
    all.value = '';
    selector.append(all);
    for (const item of options.courses || []) {
      const option = el('option', null, item.name);
      option.value = String(item.id);
      selector.append(option);
    }
    toolbar.append(el('label', null, '课程：'), selector);
  } else {
    selector = el('input');
    selector.type = 'number';
    selector.min = '0';
    selector.placeholder = '课程ID（留空统计全部）';
    toolbar.append(el('label', null, '课程ID：'), selector);
  }
  const run = el('button', null, '统计');
  run.type = 'button';
  toolbar.append(run);

  const render = async () => {
    run.disabled = true;
    try {
      const stat = await api('/api/selections/stats/course' + (selector.value ? `?courseId=${encodeURIComponent(selector.value)}` : ''));
      const body = $('table-body');
      body.textContent = '';
      $('empty').hidden = true;
      const tr = el('tr');
      const card = el('td');
      const row = el('div', 'stat-row');
      row.append(
        statCard('统计范围', stat.courseName),
        statCard('成绩记录数', stat.studentCount),
        statCard('平均分', stat.averageScore),
        statCard('最高分', stat.maxScore),
        statCard('最低分', stat.minScore),
        statCard('及格人数', stat.passCount),
        statCard('及格率(%)', stat.passRate),
      );
      card.colSpan = (spec.columns.length + 1);
      card.append(row);
      tr.append(card);
      body.append(tr);
      notify('');
    } catch (error) {
      notify(error.message, true);
    } finally {
      run.disabled = false;
    }
  };
  run.addEventListener('click', render);

  const headerRow = el('tr');
  headerRow.append(el('th', null, '按课程统计'));
  $('table-head').textContent = '';
  $('table-head').append(headerRow);
  $('count-note').textContent = '';
  $('footer-note').textContent = '统计直接在后端用 SQL 聚合，前端只渲染结果。';
  await render();
}

async function showStatClass() {
  closeForm();
  const toolbar = $('toolbar');
  toolbar.textContent = '';
  const back = el('button', 'secondary', '返回列表');
  back.type = 'button';
  back.addEventListener('click', () => openModule('selection'));
  toolbar.append(back);
  $('table-head').textContent = '';
  const headerRow = el('tr');
  for (const label of ['班级ID', '班级名称', '年级', '学生数', '成绩记录数', '平均分']) {
    headerRow.append(el('th', null, label));
  }
  $('table-head').append(headerRow);
  $('table-body').textContent = '';
  $('count-note').textContent = '';
  $('footer-note').textContent = '统计直接在后端用 SQL 聚合，前端只渲染结果。';
  try {
    const payload = await api('/api/selections/stats/class');
    const body = $('table-body');
    body.textContent = '';
    $('empty').hidden = (payload.items || []).length > 0;
    for (const item of payload.items || []) {
      const tr = el('tr');
      for (const value of [item.id, item.name, item.gradeLabel, item.studentCount, item.selectionCount, item.averageScore]) {
        tr.append(el('td', null, String(value)));
      }
      body.append(tr);
    }
    notify('');
  } catch (error) {
    notify(error.message, true);
  }
}

/* ── 启动 ─────────────────────────────────────────────────────────────── */

function boot() {
  $('api-base').textContent = API_BASE;
  $('login-form').addEventListener('submit', signIn);
  $('logout').addEventListener('click', async () => {
    try { await api('/api/logout', { method: 'POST' }); } catch (error) { /* 注销失败也要回登录页 */ }
    signOut('已退出登录。');
  });

  /* 带令牌时可以直接续用会话（刷新页面后不必重新登录）。 */
  const saved = window.sessionStorage.getItem('magel-ipage-token');
  if (saved) {
    state.token = saved;
    api('/api/session').then(result => {
      state.user = result.user;
      state.modules = result.modules;
      state.moduleKey = (result.modules[0] && result.modules[0].key) || '';
      document.body.classList.add('signed-in');
      renderShell();
      return openModule(state.moduleKey);
    }).catch(() => { state.token = null; window.sessionStorage.removeItem('magel-ipage-token'); });
  }
}

/* 记住令牌，便于刷新后续用会话。 */
window.addEventListener('beforeunload', () => {
  if (state.token) window.sessionStorage.setItem('magel-ipage-token', state.token);
  else window.sessionStorage.removeItem('magel-ipage-token');
});

document.addEventListener('DOMContentLoaded', boot);
