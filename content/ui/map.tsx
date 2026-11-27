// The hold-to-view map (docs/map.md, docs/gameplay.md "HUD and map"): a full-screen map over a
// dark see-through backdrop while the map button is held. North up; the map pans so the
// player's marker stays in the centre, with a dot in front of it showing the way it faces.
// Walls are sandstone outlines, rails teal, over a faint floor; the coins still out are dots in
// the level's colour, the hunters dots in their own colours. Drawn from three models the game
// code sends (code/core/coin_rush.cpp): "map" (the layout, once), "map_coins" (on pickups) and
// "map_live" (positions, only while the map is open). It never takes touches.
import { useModel, Index, Show } from "@robotag/ui";

// dp per metre on the map (the screen is 390 dp wide; the maze with its walls ~62 m).
const kScale = 7;

const kColors = {
    backdrop: "#05070cec",
    floor: "#c9a77018",
    wallFill: "#d9a96a30",
    wallLine: "#e0b47a",
    rail: "#2ec4b6",
    player: "#ffffff",
    playerLine: "#0b1220",
};
// Coin colour per level (content/materials/coin_*.material.json).
const kLevelColors = ["#ffd11a", "#33f24d", "#3380ff", "#ff2619", "#b840ff"];
// Hunter colours (content/materials/hunter_*.material.json), by entity name.
const kHunterColors: Record<string, string> = {
    Blinky: "#e61f1a",
    Pinky: "#ff8cbf",
    Inky: "#1ad9e6",
    Clyde: "#ff8c1a",
};

declare module "@robotag/ui" {
    interface Models {
        // Map coordinates: metres from the map's north-west corner, x east, z south.
        map: {
            width: number;
            depth: number;
            walls: [number, number, number, number][];
            rails: [number, number, number, number, number][]; // Last: 1 = a round dot.
            coins: [number, number][];
        };
        map_coins: { level: number; out: string }; // out[i] = "1": coin i is still there.
        map_live: {
            x: number;
            z: number;
            hx: number; // The way the player faces (unit vector, map axes).
            hz: number;
            hunters: { name: string; x: number; z: number }[];
        };
    }
}

function Rect(props: { r: number[]; fill: string; line?: string; round?: boolean }) {
    return (
        <view
            style={{
                position: "absolute",
                left: props.r[0] * kScale,
                top: props.r[1] * kScale,
                width: Math.max(1, (props.r[2] - props.r[0]) * kScale),
                height: Math.max(1, (props.r[3] - props.r[1]) * kScale),
                backgroundColor: props.fill,
                borderColor: props.line ?? "transparent",
                borderWidth: props.line ? 1.5 : 0,
                borderRadius: props.round ? kScale : 1,
            }}
        />
    );
}

// A round dot of `size` dp centred on map point (x, z).
function Dot(props: { x: number; z: number; size: number; color: string; line?: string }) {
    return (
        <view
            style={{
                position: "absolute",
                left: props.x * kScale - props.size / 2,
                top: props.z * kScale - props.size / 2,
                width: props.size,
                height: props.size,
                borderRadius: props.size / 2,
                backgroundColor: props.color,
                borderColor: props.line ?? "transparent",
                borderWidth: props.line ? 1.5 : 0,
            }}
        />
    );
}

export function MapView(props: { open: boolean }) {
    const map = useModel("map");
    const coins = useModel("map_coins");
    const live = useModel("map_live");
    const coinColor = () => kLevelColors[(coins()?.level ?? 1) - 1] ?? kLevelColors[0];
    return (
        <Show when={props.open && map()}>
            <view
                style={{
                    position: "absolute",
                    left: 0,
                    top: 0,
                    right: 0,
                    bottom: 0,
                    backgroundColor: kColors.backdrop,
                    overflow: "hidden",
                    pointerEvents: "none",
                }}
            >
                {/* The map, its top-left corner on the screen's centre, moved back by the
                    player's position: the player is always in the centre. */}
                <view
                    style={{
                        position: "absolute",
                        left: "50%",
                        top: "50%",
                        width: (map()?.width ?? 0) * kScale,
                        height: (map()?.depth ?? 0) * kScale,
                        backgroundColor: kColors.floor,
                        translateX: -(live()?.x ?? (map()?.width ?? 0) / 2) * kScale,
                        translateY: -(live()?.z ?? (map()?.depth ?? 0) / 2) * kScale,
                        transition: "transform:0.06",
                    }}
                >
                    <Index each={map()?.walls ?? []}>
                        {(r) => <Rect r={r()} fill={kColors.wallFill} line={kColors.wallLine} />}
                    </Index>
                    <Index each={map()?.rails ?? []}>
                        {(r) => <Rect r={r()} fill={kColors.rail} round={r()[4] === 1} />}
                    </Index>
                    <Index each={map()?.coins ?? []}>
                        {(c, i) => (
                            <Show when={coins()?.out[i] !== "0"}>
                                <Dot x={c()[0]} z={c()[1]} size={6} color={coinColor()} />
                            </Show>
                        )}
                    </Index>
                    <Index each={live()?.hunters ?? []}>
                        {(h) => (
                            <Dot x={h().x} z={h().z} size={14} color={kHunterColors[h().name] ?? "#ffffff"} line={kColors.playerLine} />
                        )}
                    </Index>
                </view>
                {/* The player: always in the centre; the small dot ahead is the way it faces. */}
                <view style={{ position: "absolute", left: "50%", top: "50%", width: 0, height: 0 }}>
                    <Dot x={((live()?.hx ?? 0) * 11) / kScale} z={((live()?.hz ?? -1) * 11) / kScale} size={7} color={kColors.player} />
                    <Dot x={0} z={0} size={16} color={kColors.player} line={kColors.playerLine} />
                </view>
            </view>
        </Show>
    );
}
