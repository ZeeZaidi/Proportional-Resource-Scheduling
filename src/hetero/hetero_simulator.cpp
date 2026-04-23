#include "hetero_simulator.h"
#include <algorithm>
#include <stdexcept>

HeteroSimResult HeteroEventSimulator::run(
    HeteroScheduler&                       sched,
    const std::vector<SimClient>&           clients,
    std::vector<Core>&                      cores,
    int                                     num_quanta,
    const std::vector<HeteroDynamicEvent>&  events)
{
    HeteroSimResult result;
    result.scheduler_name = sched.name();
    result.total_quanta   = num_quanta;

    // Sort events by quantum.
    std::vector<HeteroDynamicEvent> sorted_events = events;
    std::stable_sort(sorted_events.begin(), sorted_events.end(),
                     [](const HeteroDynamicEvent& a, const HeteroDynamicEvent& b){
                         return a.at_quantum < b.at_quantum;
                     });
    int ev_idx = 0;

    // Add initial clients.
    for (const auto& c : clients) {
        sched.add_client(c.id, c.tickets);
        result.client_names[c.id] = c.name;
        result.tracks[c.id] = {c.id, c.tickets, 0, 1, num_quanta, 0.0, {}, -1};
    }

    double total_work = 0.0;

    // Main simulation loop.
    for (int q = 1; q <= num_quanta; ++q) {

        // Process dynamic events at this quantum.
        while (ev_idx < static_cast<int>(sorted_events.size()) &&
               sorted_events[ev_idx].at_quantum == q) {
            const auto& ev = sorted_events[ev_idx];

            switch (ev.type) {
                case HeteroDynamicEvent::JOIN: {
                    sched.add_client(ev.client_id, ev.new_tickets);
                    result.client_names[ev.client_id] =
                        ev.client_name.empty()
                            ? "client_" + std::to_string(ev.client_id)
                            : ev.client_name;
                    result.tracks[ev.client_id] =
                        {ev.client_id, ev.new_tickets, 0, q, num_quanta, 0.0, {}, -1};
                    break;
                }
                case HeteroDynamicEvent::LEAVE: {
                    auto it = result.tracks.find(ev.client_id);
                    if (it != result.tracks.end())
                        it->second.last_active = q - 1;
                    sched.remove_client(ev.client_id);
                    break;
                }
                case HeteroDynamicEvent::MODIFY: {
                    sched.modify_client(ev.client_id, ev.new_tickets);
                    auto it = result.tracks.find(ev.client_id);
                    if (it != result.tracks.end())
                        it->second.tickets_at_end = ev.new_tickets;
                    break;
                }
                case HeteroDynamicEvent::SCALE_CHANGE: {
                    // Maintain remain invariant: rescale inactive clients' remain
                    // by the ratio of new/old total work rate.
                    double old_total = total_scale(cores);
                    cores[ev.core_id].performance_scale = ev.new_scale;
                    double new_total = total_scale(cores);
                    if (old_total > 0.0)
                        sched.rescale_remains(new_total / old_total);
                    break;
                }
            }
            ++ev_idx;
        }

        // Allocate — one winner per core.
        auto pairs = sched.allocate_multi(cores);

        // Record results.
        for (const auto& [winner_id, core_id] : pairs) {
            double scale = 0.0;
            for (const auto& c : cores) {
                if (c.id == core_id) {
                    scale = c.performance_scale;
                    break;
                }
            }

            result.history.push_back({q, winner_id, core_id, scale});

            auto& track = result.tracks[winner_id];
            track.wins++;
            track.effective_work += scale;
            total_work += scale;

            if (track.last_win_quantum >= 0)
                track.inter_win_intervals.push_back(q - track.last_win_quantum);
            track.last_win_quantum = q;
        }
    }

    result.total_work_dispensed = total_work;
    return result;
}
