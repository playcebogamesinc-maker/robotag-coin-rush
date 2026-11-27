// The HUD in play: the score at the top, a pause button in the top right corner and the premove
// arrow (the turn queued for the next intersection) near the bottom. Touches outside the pause
// button pass through to the game (swipes).
import { useModel, request, theme, Button, Label, SafeArea, Show } from "@robotag/ui";

const kArrows: Record<string, string> = { up: "↑", left: "←", right: "→", down: "↓" };

export function Hud() {
    const run = useModel("run");
    const visible = () => run()?.phase === "playing";
    return (
        <Show when={visible()}>
            <SafeArea style={{ flex: 1, padding: theme.space.lg }}>
                <view style={{ alignItems: "center" }}>
                    <Label variant="display">{String(run()?.score ?? 0)}</Label>
                </view>
                <Show when={run()?.phase === "playing"}>
                    <Button
                        variant="secondary"
                        title="II"
                        style={{ position: "absolute", top: theme.space.lg, right: theme.space.lg, minWidth: 48 }}
                        onPress={() => request("pause")}
                    />
                </Show>
                <view style={{ flex: 1 }} />
                <view style={{ alignItems: "center", paddingBottom: theme.space.xl, minHeight: 72 }}>
                    <Show when={run()?.phase === "playing" && (run()?.plan ?? "") !== ""}>
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
            </SafeArea>
        </Show>
    );
}
