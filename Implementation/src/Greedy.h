#pragma once

#include "Instance.h"
#include "Objective.h"
#include "Schedule.h"

#include <atomic>
#include <memory>
#include <optional>
#include <random>
#include <unordered_map>


namespace {
    void scaleVector(std::vector<double> &aVector) {
        const auto [myMinIt, myMaxIt] = std::minmax_element(aVector.begin(), aVector.end());
        const double myMin = *myMinIt;
        const double myMax = *myMaxIt;

        if (myMin == myMax) {
            std::fill(aVector.begin(), aVector.end(), 0.0);
            return;
        }

        const auto myDelta = myMax - myMin;
        for (auto &myElement: aVector) {
            myElement = (myElement - myMin) / myDelta;
        }
    }

    [[nodiscard]] std::vector<double> countTakeoffsPerVehicle(
            const std::vector<Takeoff> &aTakeoffs, const std::size_t aVehiclesCnt
    ) {
        std::vector<double> myCounts(aVehiclesCnt);

        for (const auto &myTakeoff: aTakeoffs) {
            myCounts[myTakeoff.theVehicleId]++;
        }

        return myCounts;
    }

    std::size_t stochasticallySelectFromRcl(
            const std::vector<double> &aFitness, double anAlpha, std::mt19937 &aGenerator
    ) {
        const auto [myMinIt, myMaxIt] = std::minmax_element(aFitness.begin(), aFitness.end());
        const double myMinFit = *myMinIt;
        const double myMaxFit = *myMaxIt;
        const double myThreshold = myMaxFit - anAlpha * (myMaxFit - myMinFit);

        std::vector<std::size_t> myRclIndices{};
        for (std::size_t i = 0; i < aFitness.size(); ++i) {
            if (aFitness[i] >= myThreshold) {
                myRclIndices.push_back(i);
            }
        }

        std::uniform_int_distribution<std::size_t> myDistribution{0, myRclIndices.size() - 1};
        const auto myRandomIndex = myDistribution(aGenerator);

        return myRclIndices[myRandomIndex];
    }
}

namespace greedy {
    std::optional<Takeoff> pickGreedyTakeoff(
            Schedule &aSchedule, double anAlpha, double aK1, double aK2, std::mt19937 &aGenerator
    ) {
        const auto myTakeoffs = aSchedule.findAllLegalTakeoffs();

        if (myTakeoffs.empty()) {
            return std::nullopt;
        }

        std::vector<double> myTakeoffsPerVehicle = countTakeoffsPerVehicle(
                myTakeoffs, aSchedule.getInstance().getVehiclesCnt()
        );

        std::vector<double> myObjectiveValues(myTakeoffs.size());
        std::vector<double> myReducedTakeoffCounts(myTakeoffs.size());

        for (std::size_t i = 0; i < myTakeoffs.size(); ++i) {
            const auto &myTakeoff = myTakeoffs[i];

            aSchedule.insertTakeoff(myTakeoff);

            myObjectiveValues[i] = evaluateObjective(aSchedule);
            myReducedTakeoffCounts[i] = aK2 != 0.0 ? aSchedule.findAllLegalTakeoffsCount() : 0.0;

            aSchedule.removeTakeoff(myTakeoff);
        }

        scaleVector(myTakeoffsPerVehicle);
        scaleVector(myObjectiveValues);
        scaleVector(myReducedTakeoffCounts);

        std::vector<double> myFitness(myTakeoffs.size());
        for (std::size_t i = 0; i < myTakeoffs.size(); ++i) {
            const auto &myTakeoff = myTakeoffs[i];
            myFitness[i] = myObjectiveValues[i]
                           - aK1 * myTakeoffsPerVehicle[myTakeoff.theVehicleId]
                           + aK2 * myReducedTakeoffCounts[i];
        }

        const auto myIdx = stochasticallySelectFromRcl(myFitness, anAlpha, aGenerator);
        return myTakeoffs[myIdx];
    }

    [[nodiscard]] Schedule createGreedySchedule(
            std::shared_ptr<const Instance> anInstance,
            std::atomic_bool &aKillSwitch,
            double anAlpha,
            double aK1,
            double aK2,
            std::mt19937 &aGenerator
    ) {
        Schedule mySchedule{std::move(anInstance)};

        while (auto myTakeoff = pickGreedyTakeoff(mySchedule, anAlpha, aK1, aK2, aGenerator)) {
            mySchedule.insertTakeoff(*myTakeoff);
            if (aKillSwitch) {
                break;
            }
        }

        return mySchedule;
    }
}
