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

    if (event.rfind("footstep:", 0) == 0) {
        // Determine which material field to use based on tag variant.
        // Priority: actor per-material override (walkL/R only) → material field.
        bool isLeft = (event.back() == 'L');
        Platform* _cp = platformId ? world.map.platformids[platformId] : nullptr;
        const auto& mat = GASLoader::Get().GetPhysicsMaterialDef(
            _cp ? static_cast<uint8_t>(_cp->physicsMaterial) : 0);

        // Per-material actor override (walk only — crouch/stair use material defaults).
        std::string cue;
        if ((event == "footstep:L" || event == "footstep:R") &&
            adef && !adef->footstepOverrides.empty()) {
            auto it = adef->footstepOverrides.find(mat.name);
            if (it != adef->footstepOverrides.end())
                cue = isLeft ? it->second.walkL : it->second.walkR;
        }

        // Fall back to the appropriate material field by tag variant.
        if (cue.empty()) {
            if      (event == "footstep:L"        || event == "footstep:R")
                cue = isLeft ? mat.footstepL        : mat.footstepR;
            else if (event == "footstep:crouch:L" || event == "footstep:crouch:R")
                cue = isLeft ? mat.footstepCrouchL  : mat.footstepCrouchR;
            else if (event == "footstep:stair:L"  || event == "footstep:stair:R")
                cue = isLeft ? mat.footstepStairL   : mat.footstepStairR;
        }

        auto r = ResolveSound(cue, world.resources);
        if (r.chunk) actor.EmitSound(world, r.chunk, static_cast<int>(baseVol * r.volume));
        return;
    }

    // "cue:xxx", bare cue name, or legacy WAV filename
    auto r = ResolveSound(event, world.resources);
    if (r.chunk) actor.EmitSound(world, r.chunk, static_cast<int>(baseVol * r.volume));
}
