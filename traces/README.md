# traces — append-only decision log

One file per merged change: what changed, why, evidence consulted, verify
result + git hash. (Distinct from the plugin's runtime TraceWriter, which
records engine generations — see the contract §6.)
