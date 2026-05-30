// Browser-side mirror of the server types. Kept in sync by hand — small and
// stable enough that a shared package isn't worth the build complexity.

export type ValveName    = 'HLT' | 'MASH' | 'INLET' | 'CHILLER';
export type ValveState   = 'open' | 'closed';
export type PumpState    = 'pumping' | 'stopped';
export type BoilValveState = 'opened' | 'closed' | 'opening' | 'closing' | 'stopped';

export interface ParamSpec {
  key:     string;
  label:   string;
  type:    'number' | 'integer' | 'string' | 'boolean' | 'select';
  default: number | string | boolean | number[];
  min?:    number;
  max?:    number;
  step?:   number;
  unit?:   string;
  options?: string[];
  description?: string;
}

export interface StepKindMeta {
  id:          string;
  displayName: string;
  description: string;
  params:      ParamSpec[];
}

export interface StepInstance {
  id:      string;
  kind:    string;
  wait:    boolean;
  enabled: boolean;
  params:  Record<string, unknown>;
}

export interface Recipe {
  id:          string;
  name:        string;
  description: string;
  steps:       StepInstance[];
  createdAt:   number;
  updatedAt:   number;
}

export interface RecipesState {
  activeRecipeId: string | null;
  recipes:        Recipe[];
}

export interface AppStateLite {
  valves?: Partial<Record<ValveName, ValveState>>;
  pumps?:  { mash?: PumpState; chiller?: PumpState };
  mill?: string;
  stir?: string;
  hopDropper?: string;
  crane?: string;
  boilValve?: BoilValveState;
  hlt?: { level?: string; heating?: boolean; temp?: number; setpoint?: number; cmd?: string };
  boil?: { state?: string; level?: string; duty?: number };
  flow?: { boilLitres?: number; flowing?: boolean; measuring?: boolean };
  temps?: { HLT?: number; MASH?: number };
  mashWater?: { inMash?: number; inBoiler?: number };
  brew?: {
    running?: 'idle' | 'running' | 'paused';
    step?: number;
    stepName?: string;
    secondsElapsed?: number;
    stepElapsed?: number;
    maxSteps?: number;
  };
  parameters?: Record<string, unknown>;
  recipes?: RecipesState;
}

export interface LogEntry {
  ts: number;
  level: 'INFO' | 'WARN' | 'ERROR' | 'DEBUG';
  msg: string;
}

export type WsServerMessage =
  | { type: 'snapshot'; state: AppStateLite }
  | { type: 'patch'; section: keyof AppStateLite | '*'; value: unknown }
  | { type: 'log'; entry: LogEntry }
  | { type: 'error'; error: string };

export interface BrewBusEvent {
  type: 'snapshot' | 'patch';
  section?: string;
}

export interface BrewBus {
  on:  (cb: (e: BrewBusEvent) => void) => void;
  off: (cb: (e: BrewBusEvent) => void) => void;
  emit: (e: BrewBusEvent) => void;
}

declare global {
  interface Window {
    brewState: AppStateLite;
    brewBus:   BrewBus;
    brewWs:    WebSocket | undefined;
    brewSend:  (name: string, args?: Record<string, unknown>) => void;

    buildDashboardView:   (id: string) => void;
    buildManualView:      (id: string) => void;
    buildValvesView:      (id: string) => void;
    buildDiagnosticsView: (id: string) => void;
    buildParametersView:  (id: string) => void;
    buildBrewView:        (id: string) => void;
    buildRecipesView:     (id: string) => void;
  }
}

export {};
