FROM ghcr.io/wiiu-env/devkitppc:20260504

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20260503 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:20260404 /artifacts $DEVKITPRO

WORKDIR project
