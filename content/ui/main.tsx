// Coin Rush's game UI. Milestone 1: the score at the top, from the game code's "run" model
// (code/core/coin_rush.cpp). Built by `rt ui build games/coin-rush` (the editor rebuilds it on
// save). Skill: add-ui-screen.
import { mount, useModel, FpsCounter, Label, SafeArea } from "@robotag/ui";

declare module "@robotag/ui" {
    interface Models {
        run: { score: number };
    }
}

function Score() {
    const run = useModel("run");
    return (
        <SafeArea style={{ flex: 1, alignItems: "center", paddingTop: 16 }}>
            <Label variant="title">{String(run()?.score ?? 0)}</Label>
        </SafeArea>
    );
}

mount("hud", () => <Score />);
mount("debug", () => <FpsCounter />);
