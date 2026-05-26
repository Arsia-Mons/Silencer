#include "frameevents.h"
#include "gasloader.h"
#include "audio/soundcue.h"
#include "world/world.h"

void FireFrameEvent(const std::string& event,
                    const ActorDef*    adef,
                    Uint16             platformId,
                    Object&            actor,
                    World&             world,
                    int                baseVol)
{
    if (event.empty()) return;

    if (event == "footstep:L" || event == "footstep:R") {
        bool isLeft = (event.back() == 'L');
        Platform* _cp = platformId ? world.map.platformids[platformId] : nullptr;
        const auto& mat = GASLoader::Get().GetPhysicsMaterialDef(
            _cp ? static_cast<uint8_t>(_cp->physicsMaterial) : 0);
        std::string cue;
        if (adef && !adef->footstepOverrides.empty()) {
            auto it = adef->footstepOverrides.find(mat.name);
            if (it != adef->footstepOverrides.end())
                cue = isLeft ? it->second.walkL : it->second.walkR;
        }
        if (cue.empty())
            cue = isLeft ? mat.footstepL : mat.footstepR;
        auto r = ResolveSound(cue, world.resources);
        if (r.chunk) actor.EmitSound(world, r.chunk, static_cast<int>(baseVol * r.volume));
        return;
    }

    // "cue:xxx", bare cue name, or legacy WAV filename
    auto r = ResolveSound(event, world.resources);
    if (r.chunk) actor.EmitSound(world, r.chunk, static_cast<int>(baseVol * r.volume));
}
