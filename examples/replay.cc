#include "sc2api/sc2_api.h"

#include "sc2utils/sc2_manage_process.h"

#include <iostream>
#include <string>
#include <fstream>

const char* kReplayFolder = "INSERT FILE PATH HERE";


class CSVWriter {
public:
    std::string filePath = "INSERT FILE PATH HERE";
    std::string delimiter = ";";
    bool first = true;

    CSVWriter() {
    }

    void writeToFile(std::string x) {
        std::fstream file;
        file.open(filePath, std::ios_base::app);
        file << x;
        file.close();
    }

    void addColumn(std::string x) {
        std::string res = "";
        if (!first)
            res.append(delimiter);
        res.append(x);
        first = false;
        writeToFile(res);
    }

    void newRow() {
        writeToFile("\n");
        first = true;
    }
};


class Replay : public sc2::ReplayObserver {
public:
    std::vector<uint32_t> count_units_built_;
    const sc2::ObservationInterface* obs;
    CSVWriter writer;

    std::vector<sc2::Point2D> base_locations;

    Replay() :
        sc2::ReplayObserver() {
    }

    void OnGameStart() final {
        obs = Observation();
        assert(obs->GetUnitTypeData().size() > 0);
        count_units_built_.resize(obs->GetUnitTypeData().size());
        std::fill(count_units_built_.begin(), count_units_built_.end(), 0);

        base_locations = obs->GetGameInfo().start_locations;
        for (sc2::Point2D base : base_locations) {
            std::cout << base.x << " " << base.y << '\n';
        }
    }

    void OnUnitCreated(const sc2::Unit* unit) final {
        assert(uint32_t(unit->unit_type) < count_units_built_.size());
        ++count_units_built_[unit->unit_type];
    }

    void OnStep() final {
        /*writer.addColumn(std::to_string(obs->GetPlayerID()));
        if (obs->GetGameLoop() % 3000 == 0)
            writer.newRow();*/
    }

    void OnGameEnd() final {
        std::cout << "Units created:" << std::endl;
        const sc2::ObservationInterface* obs = Observation();
        const sc2::UnitTypes& unit_types = obs->GetUnitTypeData();
        for (uint32_t i = 0; i < count_units_built_.size(); ++i) {
            if (count_units_built_[i] == 0) {
                continue;
            }

            std::cout << unit_types[i].name << ": " << std::to_string(count_units_built_[i]) << std::endl;
        }
        std::cout << "Finished" << std::endl;
    }

    bool IgnoreReplay(const sc2::ReplayInfo& replay_info, uint32_t& /*player_id*/) {
        /*
        if (replay_info.num_players != 2)
            return true;

        // Only TvT.
        for (int i = 0; i < replay_info.num_players; ++i)
            if (replay_info.players[i].race != 0)
                return true;

        if (replay_info.map_name.find("Interloper LE") == std::string::npos)
            return true;
        */
        return false;
    }
};


int main(int argc, char* argv[]) {
    sc2::Coordinator coordinator;
    if (!coordinator.LoadSettings(argc, argv)) {
        return 1;
    }

    if (!coordinator.SetReplayPath(kReplayFolder)) {
        std::cout << "Unable to find replays." << std::endl;
        return 1;
    }

    coordinator.SetRealtime(false);

    // NOTE You can modify step size here.
    coordinator.SetStepSize(1000);

    // NOTE You can modify perspective here.
    coordinator.SetReplayPerspective(1);
    Replay replay_observer;
    coordinator.AddReplayObserver(&replay_observer);

    while (coordinator.Update());
    while (!sc2::PollKeyPress());
}
