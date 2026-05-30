// Barrel for the HLT domain.
// Owns: HLT tank — heater SSR, level monitor, fill/heat/drain commands.
// Owns store section: 'hlt'.
// Cross-domain: drives INLET + HLT valves (hydraulics) and reads
// tempSensors (sensing) — both legitimate for HLT operation; documented
// here so future maintainers don't flag them as boundary violations.
export * from './hlt';
