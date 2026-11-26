// Coin Rush's game UI: the score at the top (and "Caught!" when a hunter ends the run; the
// results screen comes with milestone 5), from the game code's "run" model
// (code/core/coin_rush.cpp). The score is the only number shown in play (docs/gameplay.md); the
// level shows only as the coins' colour. Built by `rt ui build games/coin-rush` (the editor rebuilds it on
// save). Skill: add-ui-screen.
import { mount, useModel, FpsCounter, Label, SafeArea, Show } from "@robotag/ui";

declare module "@robotag/ui" {
    interface Models {
        run: {
            score: number;
            level: number;
            coins_left: number;
            won: boolean;
            caught: boolean; // A hunter touched the player: the run is over (milestone 4).
            caught_by: string;
            hunters: number;
        };
    }
}

function Score() {
    const run = useModel("run");
    return (
        <SafeArea style={{ flex: 1, alignItems: "center", paddingTop: 16 }}>
            <Label variant="title">{String(run()?.score ?? 0)}</Label>
            <Show when={run()?.caught}>
                <Label variant="title">Caught!</Label>
            </Show>
        </SafeArea>
    );
}

mount("hud", () => <Score />);
mount("debug", () => <FpsCounter />);
