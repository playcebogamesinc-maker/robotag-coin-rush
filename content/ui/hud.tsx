// The HUD in play: the score at the top, a pause button in the top right corner, the premove
// arrow (the turn queued for the next intersection) and the map button at the bottom middle.
// Holding the map button shows the full-screen map (map.tsx) until it is let go; the robot keeps
// driving but takes no input meanwhile (docs/map.md). Touches outside the buttons pass through to
// the game (swipes).
import { useModel, request, theme, Button, Label, SafeArea, Show } from "@robotag/ui";
import { MapView } from "./map";

const kArrows: Record<string, string> = { up: "↑", left: "←", right: "→", down: "↓" };

export function Hud() {
    const run = useModel("run");
    const playing = () => run()?.phase === "playing";
    const mapOpen = () => playing() && run()?.map_open === true;
    return (
        <Show when={playing()}>
            <view style={{ position: "absolute", left: 0, top: 0, right: 0, bottom: 0 }}>
                <SafeArea style={{ flex: 1, padding: theme.space.lg }}>
                    <view style={{ alignItems: "center" }}>
                        <Label variant="display">{String(run()?.score ?? 0)}</Label>
                    </view>
                    <Show when={!mapOpen()}>
                        <Button
                            variant="secondary"
                            title="II"
                            style={{ position: "absolute", top: theme.space.lg, right: theme.space.lg, minWidth: 48 }}
                            onPress={() => request("pause")}
                        />
                    </Show>
                </SafeArea>
                <MapView open={mapOpen()} />
                <SafeArea
                    edges={["left", "right", "bottom"]}
                    style={{ position: "absolute", left: 0, right: 0, bottom: 0, alignItems: "center", paddingBottom: theme.space.xl, gap: theme.space.md }}
                >
                    <view style={{ height: 64, justifyContent: "center" }}>
                        <Show when={(run()?.plan ?? "") !== ""}>
                            <view
                                style={{
                                    width: 64,
                                    height: 64,
                                    borderRadius: theme.radius.round,
                                    backgroundColor: "#0b1220b0",
                                    alignItems: "center",
                                    justifyContent: "center",
                                }}
                            >
                                <Label variant="display" color={theme.colors.warning}>
                                    {kArrows[run()?.plan ?? ""] ?? ""}
                                </Label>
                            </view>
                        </Show>
                    </view>
                    <Button
                        variant={mapOpen() ? "primary" : "secondary"}
                        title="MAP"
                        style={{ minWidth: 96 }}
                        onPressIn={() => request("map_open")}
                        onPressOut={() => request("map_close")}
                    />
                </SafeArea>
            </view>
        </Show>
    );
}
