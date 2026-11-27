// The run flow's screens: start screen, countdown, pause menu and results. Each is a full-screen
// dark backdrop that takes touches (so swipes don't reach the game behind it) with a card on top.
import { useModel, request, theme, Button, Label, Panel, Show, type ElementProps } from "@robotag/ui";

function Backdrop(props: { dark?: boolean; children?: ElementProps["children"] }) {
    return (
        <view
            style={{
                position: "absolute",
                left: 0,
                top: 0,
                right: 0,
                bottom: 0,
                alignItems: "center",
                justifyContent: "center",
                padding: theme.space.xl,
                backgroundColor: props.dark ? "#000000d0" : theme.colors.overlay,
            }}
            onPress={() => {}}
        >
            {props.children}
        </view>
    );
}

function Card(props: { children?: ElementProps["children"] }) {
    return (
        <Panel style={{ alignSelf: "stretch", alignItems: "center", gap: theme.space.md, padding: theme.space.xl }}>
            {props.children}
        </Panel>
    );
}

export function StartScreen() {
    const run = useModel("run");
    return (
        <Show when={(run()?.phase ?? "menu") === "menu"}>
            <Backdrop>
                <Card>
                    <Label variant="display">Coin Rush</Label>
                    <Label muted style={{ textAlign: "center" }}>
                        Grab every coin. Dodge the four hunters. One touch ends the run.
                    </Label>
                    <Show when={(run()?.best ?? 0) > 0}>
                        <Label variant="title">Best: {String(run()?.best ?? 0)}</Label>
                    </Show>
                    <Button title="Play" style={{ alignSelf: "stretch" }} onPress={() => request("play")} />
                </Card>
            </Backdrop>
        </Show>
    );
}

export function Countdown() {
    const run = useModel("run");
    return (
        <Show when={run()?.phase === "countdown"}>
            <Backdrop dark>
                <Label variant="display" style={{ fontSize: 120 }}>
                    {String(run()?.countdown ?? 3)}
                </Label>
            </Backdrop>
        </Show>
    );
}

export function PauseMenu() {
    const run = useModel("run");
    return (
        <Show when={run()?.phase === "paused"}>
            <Backdrop>
                <Card>
                    <Label variant="title">Paused</Label>
                    <Button title="Resume" style={{ alignSelf: "stretch" }} onPress={() => request("resume")} />
                    <Button
                        variant="secondary"
                        title="Restart"
                        style={{ alignSelf: "stretch" }}
                        onPress={() => request("play")}
                    />
                    <Button variant="ghost" title="Menu" style={{ alignSelf: "stretch" }} onPress={() => request("menu")} />
                </Card>
            </Backdrop>
        </Show>
    );
}

export function Results() {
    const run = useModel("run");
    return (
        <Show when={run()?.phase === "over"}>
            <Backdrop>
                <Card>
                    <Label variant="display" color={run()?.won ? theme.colors.success : theme.colors.danger}>
                        {run()?.won ? "Cleared!" : "Caught!"}
                    </Label>
                    <Show when={!run()?.won && (run()?.caught_by ?? "") !== ""}>
                        <Label muted>by {run()?.caught_by ?? ""}</Label>
                    </Show>
                    <Label muted>Score</Label>
                    <Label variant="display">{String(run()?.score ?? 0)}</Label>
                    <Show
                        when={run()?.new_best}
                        fallback={<Label muted>Best: {String(run()?.best ?? 0)}</Label>}
                    >
                        <Label variant="title" color={theme.colors.warning}>
                            New best!
                        </Label>
                    </Show>
                    <Button title="Play Again" style={{ alignSelf: "stretch" }} onPress={() => request("play")} />
                    <Button variant="ghost" title="Menu" style={{ alignSelf: "stretch" }} onPress={() => request("menu")} />
                </Card>
            </Backdrop>
        </Show>
    );
}
