// Recipe persistence + CRUD.
//
// Recipes are user-editable sequences of `StepInstance`s. They live on disk
// at `rpi/data/recipes.json`, seeded from `config/recipes.default.json` on
// first run. The brew engine reads the active recipe at start() time, so
// edits between brews take effect without a process restart.

import fs   from 'fs';
import path from 'path';
import log  from '../../platform/util/logger';
import store from '../../platform/store/store';
import type { Recipe, RecipesState, StepInstance, StepKindId } from '../../types';
import { listKinds, getKind, defaultParams } from './stepKinds';

const DATA_DIR  = path.join(__dirname, '..', '..', '..', 'data');
const DATA_FILE = path.join(DATA_DIR, 'recipes.json');
const DEFAULTS  = path.join(__dirname, '..', '..', '..', 'config', 'recipes.default.json');

let cache: RecipesState | null = null;

function uuid(): string {
  // Compact ID — not crypto-secure, just unique within the recipe collection.
  return Date.now().toString(36) + Math.random().toString(36).slice(2, 8);
}

function publish(): void {
  if (!cache) return;
  store.set('recipes', cache, 'recipes');
}

function persist(): void {
  if (!cache) throw new Error('recipes.load() must be called first');
  fs.writeFileSync(DATA_FILE, JSON.stringify(cache, null, 2));
  publish();
}

/** Validate that a recipe references only known step kinds. */
function validate(r: Recipe): void {
  for (const s of r.steps) {
    try { getKind(s.kind as StepKindId); }
    catch (err) { throw new Error(`Recipe '${r.name}' step '${s.id}': ${(err as Error).message}`); }
  }
}

export function load(): RecipesState {
  if (!fs.existsSync(DATA_DIR)) fs.mkdirSync(DATA_DIR, { recursive: true });

  if (!fs.existsSync(DATA_FILE)) {
    try {
      const defaults = JSON.parse(fs.readFileSync(DEFAULTS, 'utf8')) as RecipesState;
      // Stamp createdAt/updatedAt for seeded recipes if they're 0.
      const now = Date.now();
      for (const r of defaults.recipes) {
        if (!r.createdAt) r.createdAt = now;
        if (!r.updatedAt) r.updatedAt = now;
      }
      fs.writeFileSync(DATA_FILE, JSON.stringify(defaults, null, 2));
      log.info(`Created ${DATA_FILE} from defaults`);
      cache = defaults;
    } catch (err) {
      log.error(`Failed to seed recipes from ${DEFAULTS}: ${(err as Error).message}`);
      cache = { activeRecipeId: null, recipes: [] };
    }
  } else {
    try {
      cache = JSON.parse(fs.readFileSync(DATA_FILE, 'utf8')) as RecipesState;
    } catch (err) {
      log.error(`Failed to parse ${DATA_FILE}: ${(err as Error).message}`);
      cache = { activeRecipeId: null, recipes: [] };
    }
  }
  for (const r of cache.recipes) validate(r);
  // If active recipe id is dangling, pick the first available.
  if (cache.activeRecipeId && !cache.recipes.find((r) => r.id === cache!.activeRecipeId)) {
    cache.activeRecipeId = cache.recipes[0]?.id ?? null;
  }
  publish();
  return cache;
}

export function get(): RecipesState {
  if (!cache) throw new Error('recipes.load() must be called first');
  return cache;
}

export function getActive(): Recipe | null {
  const s = get();
  if (!s.activeRecipeId) return null;
  return s.recipes.find((r) => r.id === s.activeRecipeId) ?? null;
}

export function activate(id: string): void {
  const s = get();
  if (!s.recipes.find((r) => r.id === id)) {
    throw new Error(`No recipe with id '${id}'`);
  }
  s.activeRecipeId = id;
  persist();
  log.info(`Active recipe: ${id}`);
}

export function create(name: string, description = ''): Recipe {
  const s = get();
  const now = Date.now();
  const r: Recipe = {
    id: uuid(),
    name,
    description,
    steps: [],
    createdAt: now,
    updatedAt: now,
  };
  s.recipes.push(r);
  if (!s.activeRecipeId) s.activeRecipeId = r.id;
  persist();
  log.info(`Created recipe '${name}' (${r.id})`);
  return r;
}

export function duplicate(srcId: string, newName?: string): Recipe {
  const s = get();
  const src = s.recipes.find((r) => r.id === srcId);
  if (!src) throw new Error(`No recipe with id '${srcId}'`);
  const now = Date.now();
  const copy: Recipe = {
    id: uuid(),
    name: newName ?? `${src.name} (copy)`,
    description: src.description,
    steps: src.steps.map((step) => ({ ...step, id: uuid(), params: { ...step.params } })),
    createdAt: now,
    updatedAt: now,
  };
  s.recipes.push(copy);
  persist();
  log.info(`Duplicated recipe '${src.name}' → '${copy.name}' (${copy.id})`);
  return copy;
}

export function remove(id: string): void {
  const s = get();
  const i = s.recipes.findIndex((r) => r.id === id);
  if (i < 0) return;
  s.recipes.splice(i, 1);
  if (s.activeRecipeId === id) s.activeRecipeId = s.recipes[0]?.id ?? null;
  persist();
  log.info(`Deleted recipe ${id}`);
}

export function rename(id: string, name: string, description?: string): void {
  const r = mustGet(id);
  r.name = name;
  if (description !== undefined) r.description = description;
  r.updatedAt = Date.now();
  persist();
}

export function addStep(recipeId: string, kind: StepKindId, position?: number): StepInstance {
  const r = mustGet(recipeId);
  const step: StepInstance = {
    id: uuid(),
    kind,
    wait: false,
    enabled: true,
    params: defaultParams(kind),
  };
  if (position === undefined || position < 0 || position > r.steps.length) {
    r.steps.push(step);
  } else {
    r.steps.splice(position, 0, step);
  }
  r.updatedAt = Date.now();
  persist();
  return step;
}

export function removeStep(recipeId: string, stepId: string): void {
  const r = mustGet(recipeId);
  const i = r.steps.findIndex((s) => s.id === stepId);
  if (i < 0) return;
  r.steps.splice(i, 1);
  r.updatedAt = Date.now();
  persist();
}

export function moveStep(recipeId: string, stepId: string, delta: number): void {
  const r = mustGet(recipeId);
  const i = r.steps.findIndex((s) => s.id === stepId);
  if (i < 0) return;
  const j = Math.max(0, Math.min(r.steps.length - 1, i + delta));
  if (i === j) return;
  const [s] = r.steps.splice(i, 1);
  r.steps.splice(j, 0, s!);
  r.updatedAt = Date.now();
  persist();
}

export function updateStep(
  recipeId: string,
  stepId: string,
  patch: Partial<Pick<StepInstance, 'wait' | 'enabled' | 'params'>>,
): void {
  const r = mustGet(recipeId);
  const s = r.steps.find((x) => x.id === stepId);
  if (!s) return;
  if (patch.wait !== undefined)    s.wait    = patch.wait;
  if (patch.enabled !== undefined) s.enabled = patch.enabled;
  if (patch.params !== undefined)  s.params  = { ...s.params, ...patch.params };
  r.updatedAt = Date.now();
  persist();
}

function mustGet(id: string): Recipe {
  const r = get().recipes.find((x) => x.id === id);
  if (!r) throw new Error(`No recipe with id '${id}'`);
  return r;
}

/** Step-kind catalog, exported for the UI. */
export const kinds = listKinds;

export default {
  load, get, getActive, activate,
  create, duplicate, remove, rename,
  addStep, removeStep, moveStep, updateStep,
  kinds,
};
