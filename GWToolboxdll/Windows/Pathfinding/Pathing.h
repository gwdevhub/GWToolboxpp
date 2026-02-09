#pragma once

#include <cstdint>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Pathing.h>
#include "MapSpecificData.h"

namespace Pathing {
    #define PATHING_MAX_PLANE_COUNT 192 // Be sure to ASSERT if this is ever higher!

    using BlockedPlaneBitset = std::bitset<PATHING_MAX_PLANE_COUNT>;

    enum class Error : uint32_t {
        OK,
        Unknown,
        FailedToFindStartPathingTrapezoid,
        FailedToFindGoalPathingTrapezoid,
        FailedToFinializePath,
        InvalidMapContext,
        BuildPathLengthExceeded,
        FailedToGetPathingMapBlock
    };

	typedef uint16_t PointId;

    class MilePath {
        volatile bool m_processing = false;
        volatile bool m_done = false;
        volatile int m_progress = 0;

        std::thread* worker_thread = nullptr;

    public:	
        MilePath(GW::MapContext*);
        ~MilePath();

        // Signals terminate to worker thread. Usually followed late by shutdown() to grab the thread again.
        void stopProcessing();
        bool isProcessing() { return m_processing; }
        // Signals terminate to worker thread, waits for thread to finish. Blocking.
        void shutdown()
        {
            stopProcessing();
            while (isProcessing())
                Sleep(10);
            if (worker_thread) {
                ASSERT(worker_thread->joinable());
                worker_thread->join();
                delete worker_thread;
                worker_thread = nullptr;
            }
        }

        int progress()
        {
            return m_progress;
        }

        bool ready()
        {
            return m_progress >= 100;
        }

        void* GetImpl() { return opaque; };
    private:
        void LoadMapSpecificData();

        int opaque[336 / sizeof(int)];
    };

    class AStar {
    public:
        class Path {
        public:
            Path(AStar* _parent)
                : m_astar(_parent),
				  m_mp(),
                  m_cost(0.0f) {}

            const std::vector<GW::GamePos>& points()
            {
                return m_points;
            }

            float cost() const
            {
                return m_cost;
            }

            bool ready() const
            {
                return finalized;
            }

            void clear()
            {
                finalized = false;
                m_points.clear();
            }

            void insertPoint(const GW::GamePos& point)
            {
                m_points.emplace_back(point);
            }

            void setCost(float cost)
            {
                m_cost = cost;
            }

            void finalize()
            {
                if (!finalized)
                    std::ranges::reverse(m_points);
                finalized = true;
            }

        MilePath* m_mp; //TODO: move to private?
		
        private:
            bool finalized = false;
            AStar* m_astar;
            std::vector<GW::GamePos> m_points;
            float m_cost; // distance
        };

        Path m_path;

        AStar(MilePath* mp);

        Error Search(const GW::GamePos& start_pos, const GW::GamePos& goal_pos);
    };
}
