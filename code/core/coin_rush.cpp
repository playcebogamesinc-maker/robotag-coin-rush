#include "core/coin_rush.hpp"

#include <memory>
#include <string>

#include "runtime/session.hpp"

namespace coin_rush {

namespace {

// One run of Coin Rush (milestone 1: the score the HUD shows). Coins, levels and hunters
// add to this run in later milestones.
class Run : public runtime::Instance {
public:
    Run() { publish(); }

    void tick(scene::Scene& /*scene*/, float /*dt*/) override {}

private:
    void publish() const {
        runtime::set_ui_model("run", "{\"score\":" + std::to_string(score_) + "}");
    }

    int score_ = 0;
};

}  // namespace

void register_game(doc::Registry& /*registry*/) {
    static bool added = false;  // Apps may build several registries.
    if (added) return;
    added = true;
    runtime::add_system({"CoinRushRun", nullptr,
                         [](scene::Scene&, const runtime::StartContext&)
                             -> std::unique_ptr<runtime::Instance> {
                             return std::make_unique<Run>();
                         }});
}

}  // namespace coin_rush
