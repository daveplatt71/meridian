#pragma once

class QGuiApplication;
class Clock;

// Runs the opt-in one-output layer-shell proof. It returns only after the
// surface is closed or the Wayland connection fails.
int runLayerShellProof(QGuiApplication &app, Clock &clock);
