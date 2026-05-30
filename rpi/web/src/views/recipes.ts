// Recipes view — browse / activate / edit named brew sequences.
//
// Left column: list of recipes with the active one highlighted, plus
//   New / Duplicate / Delete / Activate buttons.
// Right column: the active editing recipe — name + description + list of
//   steps with up/down/delete/toggle-wait/toggle-enabled controls, an
//   "Add step" picker, and a per-step parameter form.

import type { Recipe, RecipesState, StepKindMeta, StepInstance } from '../types';

interface KindsCache { list: StepKindMeta[] | null; byId: Map<string, StepKindMeta> }
const kinds: KindsCache = { list: null, byId: new Map() };

let editingId: string | null = null;       // which recipe is open in the editor

function recipes(): RecipesState {
  return window.brewState.recipes ?? { activeRecipeId: null, recipes: [] };
}

function editingRecipe(): Recipe | null {
  const r = recipes().recipes;
  if (!r) return null;
  return r.find((x) => x.id === editingId) ?? r[0] ?? null;
}

async function loadKinds(): Promise<void> {
  if (kinds.list) return;
  const res = await fetch('/api/recipes/kinds');
  const list = (await res.json()) as StepKindMeta[];
  kinds.list = list;
  kinds.byId = new Map(list.map((k) => [k.id, k]));
}

function escape(s: string): string {
  return s.replace(/[&<>"']/g, (c) =>
    c === '&' ? '&amp;' :
    c === '<' ? '&lt;' :
    c === '>' ? '&gt;' :
    c === '"' ? '&quot;' :
                '&#39;');
}

function paramInputHtml(stepId: string, p: StepKindMeta['params'][number], current: unknown): string {
  const v = current === undefined ? p.default : current;
  const unit = p.unit ? ` <span style="color:#8b949e;font-size:12px;">${escape(p.unit)}</span>` : '';
  const label = `<label style="font-size:13px;color:#c0caf5;">${escape(p.label)}${unit}</label>`;
  switch (p.type) {
    case 'select': {
      const opts = (p.options ?? []).map((o) =>
        `<option value="${escape(o)}"${o === v ? ' selected' : ''}>${escape(o)}</option>`
      ).join('');
      return `${label}<select class="recipe-param" data-step="${stepId}" data-key="${p.key}">${opts}</select>`;
    }
    case 'boolean':
      return `${label}<input type="checkbox" class="recipe-param" data-step="${stepId}" data-key="${p.key}" data-type="boolean"${v ? ' checked' : ''}>`;
    case 'integer':
    case 'number':
      return `${label}<input type="number" class="recipe-param numeric" data-step="${stepId}" data-key="${p.key}" data-type="${p.type}" value="${escape(String(v))}"${p.min !== undefined ? ` min="${p.min}"` : ''}${p.max !== undefined ? ` max="${p.max}"` : ''}${p.step !== undefined ? ` step="${p.step}"` : ''}>`;
    default:
      return `${label}<input type="text" class="recipe-param numeric" data-step="${stepId}" data-key="${p.key}" data-type="string" value="${escape(String(v))}">`;
  }
}

function stepRowHtml(r: Recipe, s: StepInstance, idx: number): string {
  const k = kinds.byId.get(s.kind);
  const displayName = k?.displayName ?? s.kind;
  const description = k?.description ?? '';
  const params = (k?.params ?? []).map((p) => paramInputHtml(s.id, p, s.params[p.key])).join('');
  const enabledClass = s.enabled ? '' : 'opacity:0.4;';
  return `
    <div class="recipe-step card" data-step="${s.id}" style="${enabledClass}padding:10px;margin-bottom:8px;">
      <div style="display:flex;align-items:center;gap:8px;">
        <span style="min-width:28px;color:#8b949e;font-variant-numeric:tabular-nums;">${idx}</span>
        <b style="flex:1;font-size:14px;color:#ffb454;">${escape(displayName)}</b>
        <button class="btn" data-act="up"     data-step="${s.id}" title="Move up"   style="min-width:42px;min-height:32px;padding:4px 8px;">↑</button>
        <button class="btn" data-act="down"   data-step="${s.id}" title="Move down" style="min-width:42px;min-height:32px;padding:4px 8px;">↓</button>
        <button class="btn ${s.wait    ? 'warn' : ''}" data-act="wait"    data-step="${s.id}" title="Wait for prior steps to complete" style="min-width:60px;min-height:32px;padding:4px 8px;">${s.wait ? '⏳ wait' : '∥ par'}</button>
        <button class="btn ${s.enabled ? 'on'   : 'off'}" data-act="enabled" data-step="${s.id}" title="Enable / disable" style="min-width:50px;min-height:32px;padding:4px 8px;">${s.enabled ? 'on' : 'off'}</button>
        <button class="btn off" data-act="delete" data-step="${s.id}" title="Delete step" style="min-width:42px;min-height:32px;padding:4px 8px;">×</button>
      </div>
      ${description ? `<div style="font-size:12px;color:#8b949e;margin:4px 0 6px 36px;">${escape(description)}</div>` : ''}
      ${params ? `<div style="display:grid;grid-template-columns:max-content 1fr;gap:4px 10px;margin-left:36px;align-items:center;">${params}</div>` : ''}
    </div>
  `;
}

function recipeListHtml(state: RecipesState): string {
  if (state.recipes.length === 0) {
    return '<p style="color:#8b949e;font-size:13px;">No recipes yet. Click "New" to create one.</p>';
  }
  return state.recipes.map((r) => {
    const isActive  = r.id === state.activeRecipeId;
    const isEditing = r.id === editingId;
    return `
      <div class="param-row ${isEditing ? 'selected' : ''}" data-recipe="${r.id}" style="display:flex;align-items:center;gap:8px;">
        <span style="flex:1;">
          <b style="display:block;color:${isActive ? '#7ee787' : '#e6edf3'};">${escape(r.name)}${isActive ? ' ●' : ''}</b>
          <small style="color:#8b949e;">${r.steps.length} steps</small>
        </span>
      </div>
    `;
  }).join('');
}

function render(root: HTMLElement): void {
  const state = recipes();
  if (!editingId || !state.recipes.find((r) => r.id === editingId)) {
    editingId = state.activeRecipeId ?? state.recipes[0]?.id ?? null;
  }
  const r = editingRecipe();

  const kindOptions = (kinds.list ?? []).map((k) =>
    `<option value="${escape(k.id)}">${escape(k.displayName)}</option>`
  ).join('');

  root.innerHTML = `
    <div class="grid cols-2" style="grid-template-columns: 1fr 2fr; gap: 12px;">
      <div class="card">
        <h3>Recipes</h3>
        <div class="param-list" id="recipe-list">${recipeListHtml(state)}</div>
        <div class="row" style="margin-top:10px;flex-wrap:wrap;gap:6px;">
          <button class="btn primary" id="recipe-new"       style="min-width:80px;min-height:42px;">New…</button>
          <button class="btn primary" id="recipe-duplicate" style="min-width:90px;min-height:42px;" ${r ? '' : 'disabled'}>Duplicate</button>
          <button class="btn on"      id="recipe-activate"  style="min-width:80px;min-height:42px;" ${r && r.id !== state.activeRecipeId ? '' : 'disabled'}>Activate</button>
          <button class="btn off"     id="recipe-delete"    style="min-width:70px;min-height:42px;" ${r ? '' : 'disabled'}>Delete</button>
        </div>
      </div>

      <div class="card">
        ${r ? `
          <h3>Edit: ${escape(r.name)}${r.id === state.activeRecipeId ? ' <span style="color:#7ee787;">● active</span>' : ''}</h3>
          <div class="kv" style="margin-bottom:8px;">
            <span>Name</span><input class="numeric" id="recipe-name" value="${escape(r.name)}" style="text-align:left;">
            <span>Description</span><input class="numeric" id="recipe-desc" value="${escape(r.description)}" style="text-align:left;">
          </div>
          <div style="display:flex;gap:6px;align-items:center;margin-bottom:10px;">
            <select id="recipe-add-kind" class="numeric" style="flex:1;font-size:14px;padding:6px;">${kindOptions}</select>
            <button class="btn primary" id="recipe-add-step" style="min-width:80px;min-height:42px;">+ Step</button>
          </div>
          <div id="recipe-steps" style="max-height: 50vh; overflow-y: auto;">
            ${r.steps.map((s, i) => stepRowHtml(r, s, i)).join('')}
          </div>
        ` : `<p style="color:#8b949e;">No recipe selected.</p>`}
      </div>
    </div>
  `;

  bind(root);
}

function bind(root: HTMLElement): void {
  // Recipe list rows
  root.querySelectorAll<HTMLDivElement>('[data-recipe]').forEach((el) => {
    el.addEventListener('click', () => {
      editingId = el.dataset.recipe ?? null;
      render(root);
    });
  });

  document.getElementById('recipe-new')?.addEventListener('click', () => {
    const name = prompt('Recipe name?');
    if (name && name.trim()) {
      window.brewSend('recipe.create', { name: name.trim() });
    }
  });
  document.getElementById('recipe-duplicate')?.addEventListener('click', () => {
    const r = editingRecipe();
    if (!r) return;
    const name = prompt('New recipe name?', `${r.name} (copy)`);
    if (name && name.trim()) window.brewSend('recipe.duplicate', { id: r.id, name: name.trim() });
  });
  document.getElementById('recipe-activate')?.addEventListener('click', () => {
    const r = editingRecipe();
    if (r) window.brewSend('recipe.activate', { id: r.id });
  });
  document.getElementById('recipe-delete')?.addEventListener('click', () => {
    const r = editingRecipe();
    if (r && confirm(`Delete '${r.name}'?`)) window.brewSend('recipe.delete', { id: r.id });
  });

  // Recipe metadata
  const nameInput = document.getElementById('recipe-name') as HTMLInputElement | null;
  const descInput = document.getElementById('recipe-desc') as HTMLInputElement | null;
  const renameDebounced = debounce((name: string, description: string) => {
    const r = editingRecipe();
    if (r) window.brewSend('recipe.rename', { id: r.id, name, description });
  }, 500);
  nameInput?.addEventListener('input', () => renameDebounced(nameInput.value, descInput?.value ?? ''));
  descInput?.addEventListener('input', () => renameDebounced(nameInput?.value ?? '', descInput.value));

  // Add step
  document.getElementById('recipe-add-step')?.addEventListener('click', () => {
    const sel = document.getElementById('recipe-add-kind') as HTMLSelectElement | null;
    const r = editingRecipe();
    if (sel && r) window.brewSend('recipe.addStep', { recipeId: r.id, kind: sel.value });
  });

  // Step actions
  root.querySelectorAll<HTMLButtonElement>('.recipe-step button[data-act]').forEach((b) => {
    b.addEventListener('click', () => {
      const r = editingRecipe();
      if (!r) return;
      const stepId = b.dataset.step!;
      const step = r.steps.find((s) => s.id === stepId);
      if (!step) return;
      switch (b.dataset.act) {
        case 'up':      window.brewSend('recipe.moveStep',   { recipeId: r.id, stepId, delta: -1 }); break;
        case 'down':    window.brewSend('recipe.moveStep',   { recipeId: r.id, stepId, delta:  1 }); break;
        case 'wait':    window.brewSend('recipe.updateStep', { recipeId: r.id, stepId, patch: { wait: !step.wait } }); break;
        case 'enabled': window.brewSend('recipe.updateStep', { recipeId: r.id, stepId, patch: { enabled: !step.enabled } }); break;
        case 'delete':
          if (confirm('Delete this step?')) window.brewSend('recipe.removeStep', { recipeId: r.id, stepId });
          break;
      }
    });
  });

  // Per-param edits (debounced)
  const paramDebounced = debounce((recipeId: string, stepId: string, params: Record<string, unknown>) => {
    window.brewSend('recipe.updateStep', { recipeId, stepId, patch: { params } });
  }, 400);
  root.querySelectorAll<HTMLInputElement | HTMLSelectElement>('.recipe-param').forEach((el) => {
    el.addEventListener('input',  () => commitParam(el, paramDebounced));
    el.addEventListener('change', () => commitParam(el, paramDebounced));
  });
}

function commitParam(
  el: HTMLInputElement | HTMLSelectElement,
  send: (recipeId: string, stepId: string, params: Record<string, unknown>) => void,
): void {
  const r = editingRecipe();
  if (!r) return;
  const stepId = el.dataset.step!;
  const key    = el.dataset.key!;
  const type   = el.dataset.type ?? (el.tagName === 'SELECT' ? 'select' : 'string');
  let value: unknown;
  if (type === 'boolean') value = (el as HTMLInputElement).checked;
  else if (type === 'integer') value = parseInt((el as HTMLInputElement).value, 10);
  else if (type === 'number')  value = parseFloat((el as HTMLInputElement).value);
  else value = el.value;
  send(r.id, stepId, { [key]: value });
}

function debounce<T extends (...a: never[]) => void>(fn: T, ms: number): T {
  let h: ReturnType<typeof setTimeout> | null = null;
  return ((...args: never[]): void => {
    if (h) clearTimeout(h);
    h = setTimeout(() => fn(...args), ms);
  }) as T;
}

window.buildRecipesView = function (id: string): void {
  const root = document.getElementById(id);
  if (!root) return;

  void loadKinds().then(() => render(root));

  // Re-render only when the recipes section changes — input focus is preserved
  // for elements outside the changed subtree because we re-render the whole
  // card. Param edits are debounced + applied via WS so the server snapshot
  // arrives ~400 ms later; the input focus shift is acceptable.
  let lastJson = '';
  window.brewBus.on((e) => {
    if (!kinds.list) return;
    if (e.section !== 'recipes' && e.type !== 'snapshot') return;
    const json = JSON.stringify(window.brewState.recipes ?? {});
    if (json === lastJson) return;
    lastJson = json;
    render(root);
  });
};

export {};
