#include "simulation.h"

SimStep Simulation::Advance(float realSeconds)
{
    const SimStep step = m_state.clock.Advance(realSeconds);
    if (step.simSeconds <= 0.0f)
        return step;

    for (CrewMember& member : m_state.crew)
        if (!member.away)
            UpdateCharacterMotion(member.motion, m_state.world, step.simSeconds);

    m_state.warehouse.Tick(m_state, step);
    m_state.lair.Tick(m_state, step);
    m_state.city.Tick(m_state, step);
    m_state.law.Tick(m_state, step);
    return step;
}
