// Coin Rush's game UI, from the game code's "run" model (code/core/coin_rush.cpp) and back to
// it with requests ("play", "pause", "resume", "menu"). The run flow (docs/run-flow.md): start
// screen -> 3-2-1 countdown -> play (HUD: score, pause button, premove arrow) <-> pause menu ->
// results (Caught! / Cleared!, score, best, Play Again, Menu). Holding the HUD's map button shows
// the full-screen map (map.tsx, docs/map.md). The score is the only number shown
// in play (docs/gameplay.md); the level shows only as the coins' colour. Built by
// `rt ui build games/coin-rush` (the editor rebuilds it on save). Skill: add-ui-screen.
import { mount, FpsCounter } from "@robotag/ui";
import { Hud } from "./hud";
import { Countdown, PauseMenu, Results, StartScreen } from "./screens";

declare module "@robotag/ui" {
    interface Models {
        run: {
            score: number;
            level: number;
            coins_left: number;
            won: boolean; // The last coin of level 5 is gone: Cleared!
            caught: boolean; // A hunter touched the player: Caught!
            caught_by: string;
            hunters: number;
            phase: "menu" | "countdown" | "playing" | "paused" | "over";
            countdown: number; // 3, 2, 1 in the countdown, else 0.
            best: number; // Best score so far.
            new_best: boolean; // This run beat it (results).
            plan: "" | "up" | "left" | "right" | "down"; // The queued premove.
            spawn: string; // The node this run started on.
            map_open: boolean; // The map button is held: the full-screen map shows (map.tsx).
        };
    }
}

mount("hud", () => <Hud />);
mount("menu", () => <StartScreen />);
mount("menu", () => <PauseMenu />);
mount("menu", () => <Results />);
mount("modal", () => <Countdown />);
mount("debug", () => <FpsCounter />);
