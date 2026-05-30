// Barrel for the brewing domain.
// Owns: the brew step orchestrator (Architect 2's "aggregate root").
// Owns store section: 'brew'.
// Cross-domain: calls commands on hlt, hydraulics, motion, boil (the
// orchestrator is the only place this is allowed by the dependency rule).
export * from './brew';
