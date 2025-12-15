#pragma once

#include <JuceHeader.h>

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

  VizBeatsAudioProcessor& processor;

  class PulseVisualizer;
  class TransportBar;

  std::unique_ptr<PulseVisualizer> visualizer;
  std::unique_ptr<TransportBar> transportBar;

  bool lastInternalPlayState = false;
  double internalStartTimeSeconds = 0.0;
  float lastBeatPhaseUi = 0.0f;
  bool lastUiRunning = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VizBeatsAudioProcessorEditor)
};
