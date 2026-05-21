#ifndef NPC_MATH_H
#define NPC_MATH_H

namespace npc_math {

// Keep integer absolute value local to NPC code so unity builds don't depend
// on whichever `abs` overload transitive includes expose on a given compiler.
inline int AbsInt(int value) {
	return value < 0 ? -value : value;
}

} // namespace npc_math

#endif
