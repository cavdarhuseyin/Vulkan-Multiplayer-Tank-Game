# Multiplayer build and run

## What was added
- `client/network_client.*`: TCP client using Boost.Asio
- `server/tank_server.*` and `server/main.cpp`: authoritative TCP server
- `shared/network_protocol.hpp`: shared packet structs
- `TankServer.vcxproj`: separate Visual Studio project for the server
- `first_app.*`: client now renders all connected players

## Run order
1. Build and run `TankServer.vcxproj`
2. Build and run `VulkanTest.vcxproj` on one or more machines
3. Set environment variables on clients if the server is on another machine:
   - `TANK_SERVER_HOST=SERVER_IP`
   - `TANK_SERVER_PORT=7777`

## Controls
- `H / Y`: move forward / backward
- `G / J`: rotate body left / right
- `O / I`: rotate turret left / right
- `SPACE`: fire
- `R`: reload missile
- `V`: free camera / FPS camera toggle

## Important limitations in this first multiplayer version
- The server is authoritative for tank movement and missile state.
- Scene collision and missile hit detection are **not yet server-side** in this version.
- The red sight is still local visual feedback for the local player.
- Remote tank interpolation is not yet added, so motion can look slightly stepped depending on latency.
