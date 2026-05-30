// Barrel for the hydraulics domain.
// Owns: discrete valves (HLT/MASH/INLET/CHILLER), motorised boil valve,
//       mash pump, chiller pump, flow sensor, mash-water tracker.
// Owns store sections: 'valves', 'pumps', 'boilValve', 'flow', 'mashWater'.
// Cross-domain: pumps + stir read crane.state() synchronously as a safety
// interlock (motion → hydraulics state read). Reads only, never commands.
export * as valves       from './valves';
export * as mashPump     from './mashPump';
export * as chillerPump  from './chillerPump';
export * as boilValve    from './boilValve';
export * as flow         from './flow';
export * as mashWater    from './mashWater';
