// Shared types used by controllers, the store, and the server.

export type ValveName    = 'HLT' | 'MASH' | 'INLET' | 'CHILLER';
export type ValveState   = 'open' | 'closed';
export type PumpState    = 'pumping' | 'stopped';
export type MillState    = 'driving' | 'stopped';
export type StirState    = 'driving' | 'stopped';
export type CraneState   =
  | 'at_top'
  | 'at_bottom'
  | 'driving_up'
  | 'driving_down'
  | 'driving_down_incremental'
  | 'stopped';
export type HopDropperState = 'stopped' | 'driving_no_gap' | 'driving_gap';
export type BoilValveState  = 'opened' | 'closed' | 'opening' | 'closing' | 'stopped';
export type HltLevel        = 'low' | 'mid' | 'high';
export type HltCmd          = 'idle' | 'heat_and_fill' | 'drain';
export type BoilLevel       = 'high' | 'low';
export type BoilState       = 'off' | 'boiling' | 'waiting' | 'auto_boiling';
export type BrewRunning     = 'idle' | 'running' | 'paused';
export type LogLevel        = 'INFO' | 'WARN' | 'ERROR' | 'DEBUG';

// ── Persisted brew parameters (one-to-one with parameters.h) ────────────────
export interface Parameters {
  iGrindTime: number;
  fGrainWeightKilos: number;
  iPumpPrimingCycles: number;
  iPumpPrimingTime: number;
  fHLTMaxLitres: number;
  fStrikeTemp: number;
  fMashStage2Temp: number;
  fMashOutTemp: number;
  fSpargeTemp: number;
  fSpargeTemp2: number;
  fSpargeTemp3: number;
  fCleanTemp: number;
  fStrikeLitres: number;
  fMashStage2Litres: number;
  fMashOutLitres: number;
  fSpargeLitres: number;
  iMashTime: number;
  iMashStage2Time: number;
  iPumpTime1: number;
  iPumpTime2: number;
  iStirTime1: number;
  iStirTime2: number;
  uiInitialMixingTime: number;
  uiClearingTime: number;
  uiMixOnTime: number;
  uiMixOffTime: number;
  iMashOutTime: number;
  iMashOutPumpTime1: number;
  iMashOutPumpTime2: number;
  iMashOutStirTime1: number;
  iMashOutStirTime2: number;
  iSpargeTime: number;
  iSpargePumpTime1: number;
  iSpargePumpTime2: number;
  iSpargeStirTime1: number;
  iSpargeStirTime2: number;
  uiBoilTime: number;
  uiBringToBoilTime: number;
  uiHopDropperStopDelayms: number;
  uiHopTimes: number[];
  uiSettlingTime: number;
  uiSettlingRecircTime: number;
  uiChillerPumpPrimingCycles: number;
  uiChillerPumpPrimingTime: number;
  uiChillTime: number;
  uiChillingPumpRecircOnTime: number;
  uiChillingPumpRecircOffTime: number;
  uiPumpToBoilRecycleOnTime: number;
  uiPumpToBoilRecycleOffTime: number;
  uiPumpToFermenterTime: number;
  uiCurrentMashStage: number;
}

// ── Pinmap shapes (config/pinmap.js) ────────────────────────────────────────
export interface OutputPin {
  bcm: number | null;
}
export interface InputPin {
  bcm: number | null;
  pull?: 'up' | 'down' | 'none';
}
export interface PulseInputPin {
  bcm: number | null;
  edge?: 'rising' | 'falling' | 'both';
  debounceUs?: number;
}
export interface PwmPin {
  chip: number;
  channel: number;
  bcm: number;
  periodHz: number;
  name?: string;
}
export interface OneWireConfig {
  busPath: string;
  sensors: Record<string, string>;
}
export interface I2cConfig {
  busNumber: number;
}
export interface PinMap {
  outputs: Record<string, OutputPin>;
  inputs: Record<string, InputPin>;
  pwm: Record<string, PwmPin>;
  pulseInputs: Record<string, PulseInputPin>;
  oneWire: OneWireConfig;
  i2c: I2cConfig;
}

// ── Recipes (editable brew sequences) ───────────────────────────────────────

/** Identifier of a step kind, e.g. 'hlt.heat_and_fill'. */
export type StepKindId =
  | 'wait'
  | 'crane.up'
  | 'crane.down'
  | 'crane.incremental'
  | 'mill.run_for'
  | 'hlt.heat_and_fill'
  | 'hlt.drain_to'
  | 'valves.close_all'
  | 'valve.set'
  | 'boil_valve.set'
  | 'mash_cycle'
  | 'sparge_cycle'
  | 'pump_to_boil_cycle'
  | 'bring_to_boil'
  | 'boil_cycle'
  | 'chill'
  | 'pump_to_fermenter'
  | 'safe_states';

/** Schema for one parameter on a step kind. Drives both validation and UI. */
export interface ParamSpec {
  key:     string;
  label:   string;
  type:    'number' | 'integer' | 'string' | 'boolean' | 'select';
  default: number | string | boolean | number[];
  min?:    number;
  max?:    number;
  step?:   number;
  unit?:   string;
  options?: string[];        // for type='select'
  description?: string;
}

/** Metadata about a step kind — declared in stepKinds.ts. */
export interface StepKindMeta {
  id:          StepKindId;
  displayName: string;
  description: string;
  params:      ParamSpec[];
}

/** One step instance inside a Recipe. */
export interface StepInstance {
  id:      string;                    // uuid for this instance
  kind:    StepKindId;
  wait:    boolean;                   // join with all prior pending steps?
  enabled: boolean;                   // skip when false
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

// ── Whole-app state snapshot ────────────────────────────────────────────────
export interface AppState {
  valves: Record<ValveName, ValveState>;
  pumps:  { mash: PumpState; chiller: PumpState };
  mill:        MillState;
  stir:        StirState;
  hopDropper:  HopDropperState;
  crane:       CraneState;
  boilValve:   BoilValveState;
  hlt: {
    level:    HltLevel;
    heating:  boolean;
    temp:     number;
    setpoint: number;
    cmd:      HltCmd;
  };
  boil: {
    state: BoilState;
    level: BoilLevel;
    duty:  number;
  };
  flow: {
    boilLitres: number;
    flowing:    boolean;
    measuring:  boolean;
  };
  temps: {
    HLT:  number;
    MASH: number;
  };
  mashWater: {
    inMash:   number;
    inBoiler: number;
  };
  brew: {
    running:        BrewRunning;
    step:           number;
    stepName:       string;
    secondsElapsed: number;
    stepElapsed:    number;
    maxSteps:       number;
  };
  parameters: Parameters;
  recipes:    RecipesState;
}

// ── Wire protocol (WebSocket) ───────────────────────────────────────────────
export interface LogEntry {
  ts:    number;
  level: LogLevel;
  msg:   string;
}

export type WsServerMessage =
  | { type: 'snapshot'; state: AppState }
  | { type: 'patch'; section: keyof AppState | '*'; value: unknown }
  | { type: 'log'; entry: LogEntry }
  | { type: 'error'; error: string };

export interface WsClientMessage {
  type: 'cmd';
  name: string;
  args?: Record<string, unknown>;
}

// ── Brew step machine ───────────────────────────────────────────────────────
export interface BrewStepDef {
  name: string;
  /**
   * If true: wait for ALL previously-launched steps to complete before
   * starting this one. If false: launch immediately in parallel.
   * Mirrors the `ucWait` flag in the original brew.c BrewSteps[].
   */
  wait: boolean;
  /**
   * Returns a promise that resolves when this step is fully done. Inside
   * `run`, fire off the work and await its completion (or just fire and
   * resolve immediately if the step is a one-shot command).
   */
  run: () => Promise<void>;
}
