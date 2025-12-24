#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class VizBeatsAudioProcessor;

class VizBeatsAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
  explicit VizBeatsAudioProcessorEditor(VizBeatsAudioProcessor&);
  ~VizBeatsAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  void timerCallback() override;
  void updateVisualizerVisibility();

  VizBeatsAudioProcessor& processor;

  class PulseVisualizer;
  class TrafficVisualizer;
  class TransportBar;
  class SettingsPanel;

  std::unique_ptr<PulseVisualizer> pulseVisualizer;
  std::unique_ptr<TrafficVisualizer> trafficVisualizer;
  std::unique_ptr<TransportBar> transportBar;
  std::unique_ptr<SettingsPanel> settingsPanel;

  bool settingsVisible = false;
  bool lastInternalPlayState = false;
  double internalStartTimeSeconds = 0.0;
  float lastBeatPhaseUi = 0.0f;
  bool lastUiRunning = false;
  int currentBeatInBar = 0;
  ColorTheme lastColorTheme = ColorTheme::HighContrast;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VizBeatsAudioProcessorEditor)
};
