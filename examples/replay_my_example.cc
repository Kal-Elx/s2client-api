#include "sc2api/sc2_api.h"

#include "sc2utils/sc2_manage_process.h"

#include <iostream>
#include <string>
#include <fstream>

const char* kReplayFolder = "FILE PATH";
const int step = 10;
const int save = 1000;
int count = 0;
int processed = 0;


// Copied from example bot.
struct IsArmy {
    IsArmy(const sc2::ObservationInterface* obs) : observation_(obs) {}

    bool operator()(const sc2::Unit& unit) {
        auto attributes = observation_->GetUnitTypeData().at(unit.unit_type).attributes;
        for (const auto& attribute : attributes) {
            if (attribute == sc2::Attribute::Structure) {
                return false;
            }
        }
        switch (unit.unit_type.ToType()) {
            case sc2::UNIT_TYPEID::ZERG_OVERLORD: return false;
            case sc2::UNIT_TYPEID::PROTOSS_PROBE: return false;
            case sc2::UNIT_TYPEID::ZERG_DRONE: return false;
            case sc2::UNIT_TYPEID::TERRAN_SCV: return false;
            case sc2::UNIT_TYPEID::ZERG_QUEEN: return false;
            case sc2::UNIT_TYPEID::ZERG_LARVA: return false;
            case sc2::UNIT_TYPEID::ZERG_EGG: return false;
            case sc2::UNIT_TYPEID::TERRAN_MULE: return false;
            case sc2::UNIT_TYPEID::TERRAN_NUKE: return false;
            default: return true;
        }
    }

    const sc2::ObservationInterface* observation_;
};

class CSVWriter {
public:
    std::string filePath = "FILE PATH";
    std::string delimeter = ";";
    bool first = true;
    std::fstream file;

    CSVWriter() {}

    void writeToFile(std::string x) {
        file << x;
    }

    void addColumn(std::string x) {
        std::string res = "";
        if (!first)
            res.append(delimeter);
        res.append(x);
        first = false;
        writeToFile(res);
    }

    void newRow() {
        writeToFile("\n");
        first = true;
    }

    void open() {
        file.open(filePath, std::ios_base::app);
    }

    void close() {
        file.close();
    }
};

#define DIM 5

class Bookkeeper {
public:
    int localStrength[DIM][DIM];
    int localUncertainty[DIM][DIM];
    int cnt = 0;
    int mapHeight;
    int mapWidth;

    Bookkeeper() {}

    void newGame(int h, int w) {
        mapHeight = h;
        mapWidth = w;
        cnt = 0;
        for (int i = 0; i < DIM; ++i)
            for (int j = 0; j < DIM; ++j) {
                localStrength[i][j] = 0;
                localUncertainty[i][j] = 0;
            }
    }

    void update() {
        ++cnt;
        for (int i = 0; i < DIM; ++i)
            for (int j = 0; j < DIM; ++j) {
                ++localUncertainty[i][j];
            }
    }

    int xToColumn(int x) {
        return x * DIM / mapWidth;
    }

    int yToRow(int y) {
        return y * DIM / mapHeight;
    }

    void unitIn(int x, int y) {
        int c = xToColumn(x);
        int r = yToRow(y);
        localUncertainty[r][c] = 0;
        localStrength[r][c] = 0;
    }

    void enemyIn(int x, int y, int value) {
        int c = xToColumn(x);
        int r = yToRow(y);
        // Border, count as visited.
        if (localUncertainty[r][c] != 0) {
            localUncertainty[r][c] = 0;
            localStrength[r][c] = 0;
        }
        localStrength[r][c] += value;
    }

    void saveFeatureVector(CSVWriter &writer, int strength) {
        for (int i = 0; i < DIM; ++i)
            for (int j = 0; j < DIM; ++j) {
                writer.addColumn(std::to_string(localStrength[i][j]));
            }
        for (int i = 0; i < DIM; ++i)
            for (int j = 0; j < DIM; ++j) {
                writer.addColumn(std::to_string(localUncertainty[i][j]));
            }
        writer.addColumn(std::to_string(cnt));
        writer.addColumn(std::to_string(strength));
        writer.newRow();
    }
};


class Replay : public sc2::ReplayObserver {
public:
    std::vector<uint32_t> count_units_built_;
    const sc2::ObservationInterface* obs;
    CSVWriter writer;
    Bookkeeper bk;

    Replay() : sc2::ReplayObserver() {}

    void OnGameStart() final {
        obs = Observation();
        bk.newGame(obs->GetGameInfo().height, obs->GetGameInfo().width);
        writer.open();
    }

    void OnUnitCreated(const sc2::Unit* unit) final {}

    void OnStep() final {
        obs = Observation();
        const sc2::UnitTypes& unit_types = obs->GetUnitTypeData();

        bk.update();

        sc2::Units units = obs->GetUnits(sc2::Unit::Alliance(1));
        for (auto it = units.begin(); it != units.end(); ++it) {
            bk.unitIn((*it)->pos.x, (*it)->pos.y);
        }

        // TODO: Just loop over army.
        sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance(4), IsArmy(obs));
        for (auto it = enemies.begin(); it != enemies.end(); ++it) {
            bk.enemyIn((*it)->pos.x, (*it)->pos.y, unit_types[(*it)->unit_type].food_required);
        }

        if (obs->GetGameLoop() % save == 0)
            bk.saveFeatureVector(writer, obs->GetFoodArmy());
    }

    void OnGameEnd() final {
        writer.close();
        std::cout << "Finished" << std::endl;
    }

    bool IgnoreReplay(const sc2::ReplayInfo& replay_info, uint32_t& /*player_id*/) {
        ++count;
        if (replay_info.num_players != 2)
            return true;

        // Only TvT.
        for (int i = 0; i < replay_info.num_players; ++i)
            if (replay_info.players[i].race != 0)
                return true;

        /*
        if (replay_info.map_name.find("Interloper LE") == std::string::npos)
            return true;
        */

        std::cout << "Count: " << count << ", Processed: " << ++processed << '\n';
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
    coordinator.SetStepSize(step);

    // NOTE You can modify perspective here.
    coordinator.SetReplayPerspective(2);
    Replay replay_observer;
    coordinator.AddReplayObserver(&replay_observer);

    while (coordinator.Update());
    while (!sc2::PollKeyPress());
}
