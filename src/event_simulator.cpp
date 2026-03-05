#include "event_simulator.h"
#include <algorithm>
#include <stdexcept>

SimResult EventSimulator::run(Scheduler&                       sched,
                               const std::vector<SimClient>&   clients,
                               int                             num_quanta,
                               const std::vector<DynamicEvent>& events)
{
    SimResult result;
    result.scheduler_name = sched.name();
    result.total_quanta   = num_quanta;

    // ---- Sort events by quantum for efficient lookup ----
    std::vector<DynamicEvent> sorted_events = events;
    std::stable_sort(sorted_events.begin(), sorted_events.end(),
                     [](const DynamicEvent& a, const DynamicEvent& b){
                         return a.at_quantum < b.at_quantum;
                     });
    int ev_idx = 0;

    // ---- Add initial clients ----
    for (const auto& c : clients) {
        sched.add_client(c.id, c.tickets);
        result.client_names[c.id] = c.name;
        result.tracks[c.id] = {c.id, c.tickets, 0, 1, num_quanta, {}, -1};
    }

    // ---- Main simulation loop ----
    for (int q = 1; q <= num_quanta; ++q) {

        // Process all dynamic events at quantum q.
        while (ev_idx < static_cast<int>(sorted_events.size()) &&
               sorted_events[ev_idx].at_quantum == q)
        {
            const DynamicEvent& ev = sorted_events[ev_idx];
            switch (ev.type) {
                case DynamicEvent::JOIN: {
                    sched.add_client(ev.client_id, ev.new_tickets);
                    result.client_names[ev.client_id] =
                        ev.client_name.empty() ? "client_" + std::to_string(ev.client_id)
                                               : ev.client_name;
                    result.tracks[ev.client_id] = {
                        ev.client_id, ev.new_tickets, 0, q, num_quanta, {}, -1};
                    break;
                }
                case DynamicEvent::LEAVE: {
                    auto it = result.tracks.find(ev.client_id);
                    if (it != result.tracks.end())
                        it->second.last_active = q - 1;
                    sched.remove_client(ev.client_id);
                    break;
                }
                case DynamicEvent::MODIFY: {
                    sched.modify_client(ev.client_id, ev.new_tickets);
                    auto it = result.tracks.find(ev.client_id);
                    if (it != result.tracks.end())
                        it->second.tickets_at_end = ev.new_tickets;
                    break;
                }
            }
            ++ev_idx;
        }

        // Allocate.
        int winner = sched.allocate();

        // Record allocation.
        result.history.push_back({q, winner});

        // Update winner's tracking data.
        auto& track = result.tracks.at(winner);
        track.wins++;
        if (track.last_win_quantum >= 0) {
            track.inter_win_intervals.push_back(q - track.last_win_quantum);
        }
        track.last_win_quantum = q;
    }

    return result;
}
