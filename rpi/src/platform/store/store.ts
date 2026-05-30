import { EventEmitter } from 'events';
import type { AppState, Parameters } from '../../types';

const emptyParameters: Parameters = {
  iGrindTime: 0, fGrainWeightKilos: 0,
  iPumpPrimingCycles: 0, iPumpPrimingTime: 0,
  fHLTMaxLitres: 0,
  fStrikeTemp: 0, fMashStage2Temp: 0, fMashOutTemp: 0,
  fSpargeTemp: 0, fSpargeTemp2: 0, fSpargeTemp3: 0, fCleanTemp: 0,
  fStrikeLitres: 0, fMashStage2Litres: 0, fMashOutLitres: 0, fSpargeLitres: 0,
  iMashTime: 0, iMashStage2Time: 0,
  iPumpTime1: 0, iPumpTime2: 0, iStirTime1: 0, iStirTime2: 0,
  uiInitialMixingTime: 0, uiClearingTime: 0, uiMixOnTime: 0, uiMixOffTime: 0,
  iMashOutTime: 0, iMashOutPumpTime1: 0, iMashOutPumpTime2: 0,
  iMashOutStirTime1: 0, iMashOutStirTime2: 0,
  iSpargeTime: 0, iSpargePumpTime1: 0, iSpargePumpTime2: 0,
  iSpargeStirTime1: 0, iSpargeStirTime2: 0,
  uiBoilTime: 0, uiBringToBoilTime: 0, uiHopDropperStopDelayms: 0,
  uiHopTimes: [],
  uiSettlingTime: 0, uiSettlingRecircTime: 0,
  uiChillerPumpPrimingCycles: 0, uiChillerPumpPrimingTime: 0,
  uiChillTime: 0,
  uiChillingPumpRecircOnTime: 0, uiChillingPumpRecircOffTime: 0,
  uiPumpToBoilRecycleOnTime: 0, uiPumpToBoilRecycleOffTime: 0,
  uiPumpToFermenterTime: 0,
  uiCurrentMashStage: 0,
};

const initial: AppState = {
  valves: { HLT: 'closed', MASH: 'closed', INLET: 'closed', CHILLER: 'closed' },
  pumps:  { mash: 'stopped', chiller: 'stopped' },
  mill: 'stopped',
  stir: 'stopped',
  hopDropper: 'stopped',
  crane: 'stopped',
  boilValve: 'stopped',
  hlt:  { level: 'low', heating: false, temp: NaN, setpoint: 74.5, cmd: 'idle' },
  boil: { state: 'off', level: 'high', duty: 0 },
  flow: { boilLitres: 0, flowing: false, measuring: false },
  temps: { HLT: NaN, MASH: NaN },
  mashWater: { inMash: 0, inBoiler: 0 },
  brew: { running: 'idle', step: 0, stepName: 'Idle', secondsElapsed: 0, stepElapsed: 0, maxSteps: 0 },
  parameters: emptyParameters,
};

type Section = keyof AppState;

/**
 * Soft section-ownership registry. Every domain module that writes a
 * particular section should call `store.declareOwner('<section>', '<domain>')`
 * once during init. If another module later writes that section, we log a
 * warning. It's deliberately non-fatal: it surfaces accidental coupling in
 * test runs and PR review without blocking legitimate compositions (e.g.
 * the safety/quit path in index.ts touching the store directly during
 * shutdown). Architect 2 specified this discipline in the synthesis.
 */
type OwnerName = string;
const owners = new Map<Section, OwnerName>();
const ownerWarned = new Set<string>();   // section:writer pairs already warned

export class Store extends EventEmitter {
  state: AppState;
  constructor() {
    super();
    this.state = JSON.parse(JSON.stringify(initial)) as AppState;
    this.setMaxListeners(50);
  }

  /**
   * Register the canonical writer for a section. First registration wins —
   * subsequent registrations log a conflict warning but don't override.
   */
  declareOwner<K extends Section>(section: K, owner: OwnerName): void {
    const existing = owners.get(section);
    if (existing && existing !== owner) {
      // eslint-disable-next-line no-console
      console.warn(`[store] section '${section}' already owned by '${existing}', '${owner}' refused`);
      return;
    }
    owners.set(section, owner);
  }

  /** True if `writer` is the declared owner of `section` (or no owner declared). */
  private ownsSection(section: Section, writer: OwnerName | undefined): boolean {
    const owner = owners.get(section);
    if (!owner) return true;            // no owner declared → anyone allowed
    if (writer === undefined) return true;   // legacy call site (e.g. server)
    return owner === writer;
  }

  private warnIfForeign(section: Section, writer: OwnerName | undefined): void {
    if (this.ownsSection(section, writer)) return;
    const key = `${section}:${writer ?? '<unknown>'}`;
    if (ownerWarned.has(key)) return;
    ownerWarned.add(key);
    const owner = owners.get(section);
    // eslint-disable-next-line no-console
    console.warn(
      `[store] section '${section}' is owned by '${owner}'. ` +
      `Writer '${writer}' is mutating it — likely a boundary violation.`
    );
  }

  /** Merge a partial object into a section and emit a change event. */
  patch<K extends Section>(section: K, patch: Partial<AppState[K]>, writer?: OwnerName): void {
    this.warnIfForeign(section, writer);
    const current = this.state[section] as object;
    const next = { ...current, ...patch } as AppState[K];
    this.state[section] = next;
    this.emit('change', { section, value: next });
  }

  /** Replace an entire section. */
  set<K extends Section>(section: K, value: AppState[K], writer?: OwnerName): void {
    this.warnIfForeign(section, writer);
    this.state[section] = value;
    this.emit('change', { section, value });
  }

  get(): AppState;
  get<K extends Section>(section: K): AppState[K];
  get<K extends Section>(section?: K): AppState | AppState[K] {
    if (section === undefined) return this.state;
    return this.state[section];
  }
}

const store = new Store();
export default store;
