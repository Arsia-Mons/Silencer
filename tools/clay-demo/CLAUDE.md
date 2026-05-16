# tools/clay-demo

Dogfood executable for the shared Clay primitives. It links the Silencer client
runtime without the lobby-screen implementation so the primitives must stay
screen-agnostic.

Build through the main client CMake project:

```
cmake -B build clients/silencer
cmake --build build --target clay_demo
```

Keep demo-only code in this directory. Do not add game behavior, lobby flows, or
production UI ownership here; those belong under `clients/silencer/src/`.
