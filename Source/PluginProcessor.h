#pragma once

#include <JuceHeader.h>

class VizBeatsAudioProcessor final : public juce::AudioProcessor
{
public:
  VizBeatsAudioProcessor();
  ~VizBeatsAudioProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  struct HostInfo
  {
    bool isPlaying = false;
    bool hasBpm = false;
    double bpm = 120.0;
    bool hasPpqPosition = false;
    double ppqPosition = 0.0;
  };

  HostInfo getHostInfo() const noexcept;
  void refreshHostInfo();

  juce::AudioProcessorValueTreeState apvts;

  static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
  void updateHostInfo();

  std::atomic<bool> hostIsPlaying { false };
  std::atomic<bool> hostHasBpm { false };
  std::atomic<double> hostBpm { 120.0 };
  std::atomic<bool> hostHasPpqPosition { false };
  std::atomic<double> hostPpqPosition { 0.0 };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VizBeatsAudioProcessor)
};
