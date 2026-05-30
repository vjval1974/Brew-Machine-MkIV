// Barrel for the motion domain.
// Owns: crane (H-bridge + limit switches), stir motor, grain mill, hop
//       dropper.
// Owns store sections: 'crane', 'stir', 'mill', 'hopDropper'.
// Intra-domain coupling: crane <-> stir interlock during incremental
// descent (mash-in). Cross-domain: mashPump + stir publish 'okToX'
// predicates that read crane.state() — same-process synchronous reads.
export * as crane       from './crane';
export * as stir        from './stir';
export * as mill        from './mill';
export * as hopDropper  from './hopDropper';
